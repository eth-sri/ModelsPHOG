/*
   Copyright 2015 Software Reliability Lab, ETH Zurich

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#ifndef SYNTREE_TREE_H_
#define SYNTREE_TREE_H_

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <iomanip>

#include "glog/logging.h"
#include "json/json.h"


#include "base/base.h"
#include "base/readerutil.h"
#include "base/stringprintf.h"
#include "base/stringset.h"
#include "base/termcolor.h"

#include "gflags/gflags.h"

class FullTreeTraversal;
class LocalTreeTraversal;
class ConstLocalTreeTraversal;
class LocalEpsTreeTraversal;
class ConstLocalEpsTreeTraversal;
class TreeStorage;

struct TreeCompareInfo {
  int num_type_equalities;
  int num_type_diffs;
  int num_value_equalities;
  int num_value_diffs;
  int num_size_greater_diffs;
  int num_size_smaller_diffs;
  int num_aggregated_trees;

  TreeCompareInfo() : num_type_equalities(0), num_type_diffs(0), num_value_equalities(0),
      num_value_diffs(0), num_size_greater_diffs(0), num_size_smaller_diffs(0), num_aggregated_trees(1) {}

  int GetDifferences() const {
    return num_type_diffs + num_value_diffs + num_size_greater_diffs + num_size_smaller_diffs;
  }

  int GetEqualities() const {
    return num_type_equalities + num_value_equalities;
  }

  double AvgNodeDifference() const {
    // We divide by 2 because the diffs are shared for types and values and we want only number of nodes
    return ((num_size_greater_diffs - num_size_smaller_diffs)/2) / (double) num_aggregated_trees;
  }

  void Add(const TreeCompareInfo& other) {
    num_type_equalities += other.num_type_equalities;
    num_type_diffs += other.num_type_diffs;
    num_value_equalities += other.num_value_equalities;
    num_value_diffs += other.num_value_diffs;
    num_size_greater_diffs += other.num_size_greater_diffs;
    num_size_smaller_diffs += other.num_size_smaller_diffs;
    num_aggregated_trees++;
  }
};

struct PerTreeSizeTrainingStatistics {
  TreeCompareInfo stats_;
  std::map<int, std::map<int, TreeCompareInfo>> stats_per_predictor_;
  std::mutex stats_lock;

  PerTreeSizeTrainingStatistics() { }

  void AddTreeStats(int key, int tree_size, const TreeCompareInfo &info) {
    std::lock_guard<std::mutex> lock(stats_lock);
    stats_.Add(info);
    stats_per_predictor_[key][tree_size].Add(info);
  }

  std::string DebugString(const StringSet* ss, bool detailed) const;
};

struct TreeTrainingStatistics {
  TreeCompareInfo stats_;
  std::map<int, TreeCompareInfo> stats_per_predictor_;
  std::mutex stats_lock;
  int num_correct;
  int total_count;

  TreeTrainingStatistics() : num_correct(0), total_count(0) { }

  void AddTreeStats(int key, const TreeCompareInfo &info) {
    std::lock_guard<std::mutex> lock(stats_lock);
    stats_.Add(info);
    stats_per_predictor_[key].Add(info);
    if (info.GetDifferences() == 0) {
      num_correct++;
    }
    total_count++;
  }

  int NumCorrect() const {
    return num_correct;
  }

  double RatioCorrect() const {
    return static_cast<double>(num_correct) / total_count * 100;
  }

  std::string DebugStringShort(const StringSet* ss, bool detailed) const;
  std::string DebugString(const StringSet* ss, bool detailed, std::string header) const;
};

class ConstLocalTreeTraversal;

struct TreeNode {
  friend class FullTreeTraversal;
  friend class LocalTreeTraversal;
  friend class ConstLocalTreeTraversal;
  friend class LocalEpsTreeTraversal;
  friend class ConstLocalEpsTreeTraversal;
  friend class TreeStorage;
  friend bool operator==(const TreeNode& lhs, const TreeNode& rhs);
  friend void CompareTrees(ConstLocalTreeTraversal t1, ConstLocalTreeTraversal t2, TreeCompareInfo* info, bool only_types, int max_depth);
private:
  // Node properties.
  int type;
  int value;

public:
  TreeNode() :
    type(-1), value(-1), parent(-1), left_sib(-1), right_sib(-1), first_child(-1), last_child(-1), child_index(-1){}
  TreeNode(int type, int value, int parent, int left, int right, int first, int last, int index) :
    type(type), value(value), parent(parent), left_sib(left), right_sib(right), first_child(first), last_child(last), child_index(index) {}
  // Tree structure. Do not edit directly.
  int parent;
  int left_sib;
  int right_sib;
  int first_child;
  int last_child;
  int child_index;


  void SetValue(int v) {
    value = v;
  }

  void SetType(int t) {
    type = t;
  }

  bool HasNonTerminal() const {
    return IsUnknownType() || IsUnknownValue();
  }

  inline bool IsUnknownValue() const {
    return value == UNKNOWN_LABEL;
  }

  inline bool IsUnknownType() const {
    return type == UNKNOWN_LABEL;
  }

  inline int Value() const {
    if (value != UNKNOWN_LABEL) {
      return value;
    }
    return EMPTY_NODE_LABEL;
  }

  inline int Type() const {
    if (type != UNKNOWN_LABEL) {
      return type;
    }
    return EMPTY_NODE_LABEL;
  }

  inline void CopyNodeEps(const TreeNode& o) {
    type = o.type;
    value = o.value;
  }
  // 256 bits. Adding more fields will likely decrease performance by a lot.

  // Constants for type.
  static const int EMPTY_NODE_LABEL;  // Means that the node can be deleted.
  // Constants for type and value.
  static const int UNKNOWN_LABEL;

  static const TreeNode EMPTY_NODE;

  friend std::ostream& operator<<(std::ostream& os, const TreeNode& node);

  std::string DebugString(const StringSet* ss) const {
     return StringPrintf("%s%s%s:%s",
              type < 0 ? "-" : ss->getString(type),
              (first_child != -1) ? "+child" : "",
              (right_sib != -1) ? "+right_sib" : "",
              (value != -1) ? ss->getString(value) : "");
   }

  TreeCompareInfo CompareLabels(const TreeNode& other) const {
      TreeCompareInfo info;
      info.num_type_equalities = (type == other.type) ? 1 : 0;
      info.num_type_diffs = (type != other.type) ? 1 : 0;
      info.num_value_equalities = (value == other.value) ? 1 : 0;
      info.num_value_diffs = (value != other.value) ? 1 : 0;

      bool has_first_child = first_child != -1;
      bool other_has_first_child = other.first_child != -1;
      bool has_right_sib = right_sib != -1;
      bool other_has_right_sib = other.right_sib != -1;
      if (has_first_child && !other_has_first_child) {
        info.num_size_greater_diffs++;
      }
      if (has_right_sib && !other_has_right_sib) {
        info.num_size_greater_diffs++;
      }

      if (!has_first_child && other_has_first_child) {
        info.num_size_smaller_diffs++;
      }
      if (!has_right_sib && other_has_right_sib) {
        info.num_size_smaller_diffs++;
      }
      return info;
    }
};

inline bool operator==(const TreeNode& lhs, const TreeNode& rhs) {
  return lhs.type == rhs.type && lhs.value == rhs.value &&
      lhs.parent == rhs.parent && lhs.left_sib == rhs.left_sib &&
      lhs.right_sib == rhs.right_sib && lhs.first_child == rhs.first_child &&
      lhs.last_child == rhs.last_child && lhs.child_index == rhs.child_index;
}

static_assert(sizeof(TreeNode) == 32, "Inefficiency: TreeNode is not 256 bits");

// Constants for tree structure pointers.
const int TREEPOINTER_NO_VALUE = -1;
const int TREEPOINTER_VALUE_IN_PARENT = -2;
const int TREEPOINTER_DEALLOCATED = -3;

enum class TreeIteratorMode {
  PRE_ORDER_FORWARD_DFS,
  POST_ORDER_FORWARD_DFS
};

class TreeStorage;

template<class NodeType, class Traversal>
class TreeIterator {
public:
  explicit TreeIterator(Traversal&& t, TreeIteratorMode mode) : t_(t), mode_(mode), at_end_(false) { start(); }
  explicit TreeIterator(const Traversal& t, TreeIteratorMode mode) : t_(t), mode_(mode), at_end_(false) { start(); }
  explicit TreeIterator(Traversal&& t, TreeIteratorMode mode, bool at_end) : t_(t), mode_(mode), at_end_(true) { }
  explicit TreeIterator(const Traversal& t, TreeIteratorMode mode, bool at_end) : t_(t), mode_(mode), at_end_(true) { }

  TreeIterator<NodeType, Traversal>& operator++() {
    switch (mode_) {
    case TreeIteratorMode::PRE_ORDER_FORWARD_DFS: pre_order_forward(); break;
    case TreeIteratorMode::POST_ORDER_FORWARD_DFS: post_order_forward(); break;
    }
    return *this;
  }

  bool operator==(const TreeIterator<NodeType, Traversal>& o) const {
    if (at_end_ || o.at_end_) return at_end_ == o.at_end_;
    return t_ == o.t_;
  }

  bool operator!=(const TreeIterator<NodeType, Traversal>& o) const {
    return !(operator ==(o));
  }

  NodeType* operator->() {
    return &t_.node();
  }

  const NodeType& node() const {
    return t_.node();
  }

  void move_to_end() {
    at_end_ = true;
  }

  const TreeStorage* tree_storage() const {
    return t_.tree_storage();
  }

  int position() const {
    return t_.position();
  }

  bool at_end() const {
    return at_end_;
  }

private:
  void start() {
    switch (mode_) {
    case TreeIteratorMode::PRE_ORDER_FORWARD_DFS: pre_order_start(); break;
    case TreeIteratorMode::POST_ORDER_FORWARD_DFS: post_order_start(); break;
    }
  }

  void pre_order_start() {
  }

  void pre_order_forward() {
    if (t_.down_first_child()) return;
    do {
      if (t_.right()) return;
    } while (t_.up());
    at_end_ = true;
  }

  void post_order_start() {
    while (t_.down_first_child()) { }
  }

  void post_order_forward() {
    if (t_.right()) {
      while (t_.down_first_child()) { }
      return;
    }
    if (t_.up()) return;
    at_end_ = true;
  }

  Traversal t_;
  TreeIteratorMode mode_;
  bool at_end_;
};

class TreeSubstitution {
public:
  struct Node {
    int type;
    int value;
    int first_child;
    int right_sib;

    bool operator==(const TreeSubstitution::Node& o) const {
      return type == o.type && value == o.value && first_child == o.first_child && right_sib == o.right_sib;
    }

    friend std::ostream& operator<<(std::ostream& os, const TreeSubstitution::Node& f);
  };

  TreeSubstitution() {}
  TreeSubstitution(std::initializer_list<Node> l) : data(l) {}

  std::vector<Node> data;

  bool operator==(const TreeSubstitution& o) const {
    return data == o.data;
  }
};

struct TreeSubstitutionOnlyLabel {
  int type;
  bool has_first_child;
  bool has_right_sib;

  bool operator==(const TreeSubstitutionOnlyLabel& o) const {
    return type == o.type && has_first_child == o.has_first_child && has_right_sib == o.has_right_sib;
  }

  TreeSubstitution::Node ToSubstitutionNode() const {
    return TreeSubstitution::Node({type, TreeNode::UNKNOWN_LABEL, has_first_child ? -2 : -1, has_right_sib ? -2 : -1});
  }

  std::string DebugString(const StringSet* ss) const {
    if (ss == nullptr || type < 0)
      return StringPrintf("%d%s%s",
          type,
          has_first_child ? "+child" : "",
          has_right_sib ? "+right_sib" : "");
    return StringPrintf("%s%s%s",
             ss->getString(type),
             has_first_child ? "+child" : "",
             has_right_sib ? "+right_sib" : "");
  }
};

int EncodeTypeLabel(const TreeSubstitutionOnlyLabel& type_label);
TreeSubstitutionOnlyLabel DecodeTypeLabel(int encoded_label);

static_assert(sizeof(TreeSubstitutionOnlyLabel) == 8, "TreeSubstitutionOnlyLabel must have the booleans aligned such that it takes 8 bytes.");

namespace std {
template <> struct hash<TreeSubstitution> {
  size_t operator()(const TreeSubstitution& x) const {
    return FingerprintMem(static_cast<const void*>(x.data.data()), x.data.size() * sizeof(TreeSubstitution::Node));
  }
};

template <> struct hash<TreeSubstitution::Node> {
  size_t operator()(const TreeSubstitution::Node& n) const {
    return FingerprintMem(static_cast<const void*>(&n), sizeof(TreeSubstitution::Node));
  }
};

template <> struct hash<TreeSubstitutionOnlyLabel> {
  size_t operator()(const TreeSubstitutionOnlyLabel& n) const {
    return FingerprintCat(n.type, static_cast<int>(n.has_first_child) * 2 + static_cast<int>(n.has_right_sib));
  }
};
}

// Stores a tree.
class TreeStorage {
public:
  TreeStorage() : parent_(nullptr), position_in_parent_(-1), first_free_node_(-1) {
    AddFirstNode();
  }

  explicit TreeStorage(const TreeStorage* parent, int position_in_parent)
      : parent_(parent), position_in_parent_(position_in_parent), first_free_node_(-1) {
    AddFirstNode();
    AttachTo(parent, position_in_parent);
  }

  bool HasNonTerminal() const;

  void AttachTo(const TreeStorage* parent, int position_in_parent) {
    parent_ = parent;
    position_in_parent_ = position_in_parent;
    if (parent_ == nullptr) return;
    if (parent->nodes_[position_in_parent].left_sib >= 0)
      nodes_[0].left_sib = TREEPOINTER_VALUE_IN_PARENT;
    if (parent->nodes_[position_in_parent].right_sib >= 0)
      nodes_[0].right_sib = TREEPOINTER_VALUE_IN_PARENT;
    nodes_[0].parent = TREEPOINTER_VALUE_IN_PARENT;
    nodes_[0].child_index = parent->nodes_[position_in_parent].child_index;
  }

  void swap(TreeStorage& o) {
    nodes_.swap(o.nodes_);
    std::swap(parent_, o.parent_);
    std::swap(position_in_parent_, o.position_in_parent_);
    std::swap(first_free_node_, o.first_free_node_);
  }

  std::string DebugString(const StringSet* ss = nullptr, bool tree_indentation = false,
      int highlighted_position = -1, int last_node = std::numeric_limits<int>::max(), int start_node = 0) const {
    std::string s;
    if (tree_indentation) s.append("\n");

    DebugStringTraverse(&s, 0, 32, 0, ss, tree_indentation, highlighted_position, last_node, start_node);
    return s;
  }

  // Transforms the TreeStorage into a JavaScript program.
  // Fails if the TreeStorage is not a valid program or because an old AST format is used.
  // highlighted_position specifies the position of subtree root which will be highlighted in green color
  std::string DebugStringAsSource(const StringSet* ss, int highlighted_position) const;

  std::string DebugStringAsSource(const StringSet* ss) const {
    return DebugStringAsSource(ss, -1);
  }

  // Like DebugJSString except it returns window_size lines around the highlighted region
  std::string DebugStringAsSourceWindow(const StringSet* ss, int highlighted_position, int window_size) const;

  std::string NodeToString(const StringSet* ss, int node) const ;

  void Canonicalize();
  void GetSubtreesOfMaxSize(int max_size, std::vector<int>* subtrees);
  void GetTreeSizesAtNodes(std::vector<int>* tree_sizes);

  void CheckConsistency() const;

  bool CanSubstituteNode(int node_id, const TreeSubstitution& subst) const {
    const TreeNode& node = nodes_[node_id];
    if (subst.data.empty() || subst.data[0].right_sib >= 0) {
      // 0 or more than 1 elements are the root level of the substitution
      // requires that the substitution does not replace the root node or a node that already
      // has another one on the right.
      return node_id != 0 && node.right_sib == -1;
    }
    return true;
  }

  TreeStorage GetSubtreeForCompletion(int position, bool is_for_node_type) const;

  void SubstituteNode(int node_id, const TreeSubstitution& subst);

  bool CanSubstituteSingleNode(int node_id, const TreeSubstitution::Node& subst_node) const {
    const TreeNode& node = nodes_[node_id];
    if (subst_node.right_sib >= 0) {
      // 0 or more than 1 elements are the root level of the substitution
      // requires that the substitution does not replace the root node or a node that already
      // has another one on the right.
      return node_id != 0 && node.right_sib == -1;
    }
    return true;
  }

  void SubstituteSingleNode(int node_id, const TreeSubstitution::Node& node);

  bool CanSubstituteNodeType(int node_id, int type) {
    const TreeNode& node = nodes_[node_id];
    if (type == -1) return node_id != 0 && node.right_sib == -1 && node.first_child == -1;
    return true;
  }

  void SubstituteNodeType(int node_id, int type);

  void RemoveNode(int node_id) {
    RemoveNodeChildren(node_id);
    if (node_id == 0) return;  // Cannot remove the root.
    TreeNode& node = nodes_[node_id];
    CHECK_LT(node.right_sib, 0);
    if (node.parent >= 0) {
      if (nodes_[node.parent].first_child == node_id)
        nodes_[node.parent].first_child = -1;
      nodes_[node.parent].last_child = node.left_sib;
    }
    if (node.left_sib >= 0) {
      nodes_[node.left_sib].right_sib = -1;
    }
    DeallocateNode(node_id);
  }

  void RemoveNodeChildren(int start_node_id);

  void Parse(const Json::Value& v, StringSet* ss);

  void InlineIntoParent(TreeStorage* parent);

  unsigned NumAllocatedNodes() const { return nodes_.size(); }

  const TreeNode& node(int node_id) const { return nodes_[node_id]; }
  TreeNode& node(int node_id) { return nodes_[node_id]; }

  int NumNodeChildren(int position) const;

  bool operator==(const TreeStorage& other) const {
    return parent_ == other.parent_ && position_in_parent_ == other.position_in_parent_ &&
        first_free_node_ == other.first_free_node_ && nodes_ == other.nodes_;
  }

  size_t GetHash() const {
    return FingerprintMem(static_cast<const void*>(nodes_.data()), nodes_.size() * sizeof(TreeNode));
  }

  // Constructs a TreeStorage that includes the subtree from the given node.
  TreeStorage SubtreeFromNodeAsTree(int node) const;

  const TreeStorage* parent() const { return parent_; }
  int position_in_parent() const { return position_in_parent_; }

  // F(int node_id)
  template<class F>
  void ForEachSubnodeOfNode(int start_node, const F& f) const {
    int current_node = start_node;
    for (;;) {
      f(current_node);
      if (nodes_[current_node].first_child >= 0) {
        current_node = nodes_[current_node].first_child;
      } else {
        // No children - leave the node.
        for (;;) {
          if (current_node == start_node) {
            return;
          }
          // Leaving the node is first to the right, if not possible up until
          // a right sibling is present (or end of tree).
          int right_sib = nodes_[current_node].right_sib;
          if (right_sib >= 0) {
            current_node = right_sib;
            break;
          }
          current_node = nodes_[current_node].parent;
          DCHECK_GE(current_node, 0);
        }
      }
    }
  }

  // F(int node_id) -> bool
  // Only if the callback returns true for a node, then the children will be traversed.
  template<class F>
  void ForEachSubnodeOfNodeReturningTrue(int start_node, const F& f) const {
    int current_node = start_node;
    for (;;) {
      if (f(current_node) && nodes_[current_node].first_child >= 0) {
        current_node = nodes_[current_node].first_child;
      } else {
        // No children - leave the node.
        for (;;) {
          if (current_node == start_node) {
            return;
          }
          // Leaving the node is first to the right, if not possible up until
          // a right sibling is present (or end of tree).
          int right_sib = nodes_[current_node].right_sib;
          if (right_sib >= 0) {
            current_node = right_sib;
            break;
          }
          current_node = nodes_[current_node].parent;
          DCHECK_GE(current_node, 0);
        }
      }
    }
  }

  int GetLabelAtPosition(int position, bool for_type) const {
    const TreeNode& n = node(position);
    int label = for_type ? n.Type() : n.Value();
    if (for_type) {
      label = EncodeTypeLabel(TreeSubstitutionOnlyLabel({label, n.first_child != -1, n.right_sib != -1}));
    }
    return label;
  }

  void SubstituteNodeWithTree(int node_id, const TreeStorage& other);

private:
  void AddFirstNode() {
    TreeNode n;
    n.type = TreeNode::UNKNOWN_LABEL;
    n.value = -1;
    n.parent = -1;
    n.left_sib = -1;
    n.right_sib = -1;
    n.first_child = -1;
    n.last_child = -1;
    n.child_index = 0;
    nodes_.push_back(n);
  }

  int AddLastNode(const TreeNode& added_node) {
    if (added_node.type == TreeNode::EMPTY_NODE_LABEL) return -1;
    unsigned node_id = AllocateNode(added_node);
    const TreeNode& node = nodes_[node_id];
    if (node.left_sib >= 0) nodes_[node.left_sib].right_sib = node_id;
    DCHECK_GE(node.parent, 0);
    if (nodes_[node.parent].first_child < 0) {
      nodes_[node.parent].first_child = node_id;
    }
    nodes_[node.parent].last_child = node_id;
    DCHECK_LT(node.right_sib, 0);
    return node_id;
  }

  unsigned AllocateNode(const TreeNode& data) {
    if (first_free_node_ != -1) {
      unsigned result = first_free_node_;
      first_free_node_ = nodes_[result].type;
      DCHECK_EQ(nodes_[result].parent, TREEPOINTER_DEALLOCATED);
      nodes_[result] = data;
      return result;
    }
    nodes_.push_back(data);
    return nodes_.size() - 1;
  }

  void DeallocateNode(unsigned node_id) {
    if (node_id == nodes_.size() - 1) {
      nodes_.pop_back();
      return;
    }
    nodes_[node_id].parent = TREEPOINTER_DEALLOCATED;
    nodes_[node_id].type = first_free_node_;
    first_free_node_ = node_id;
  }

  void PrettyPrintTraverse(std::string* s, ConstLocalTreeTraversal& t, const StringSet* ss, int depth, int highlighted_position, bool is_highlighting) const;
  void PrettyPrintTraverseJava(std::string* s, ConstLocalTreeTraversal& t, const StringSet* ss, int depth, int highlighted_position, bool is_highlighting) const;
  void PrettyPrintTraverseJS(std::string* s, ConstLocalTreeTraversal& t, const StringSet* ss, int depth, int highlighted_position, bool is_highlighting) const;
  void DebugStringTraverse(std::string* s, int node, int max_depth, int depth, const StringSet* ss,
      bool tree_indentation, int highlighted_position, int last_node = -1, int start_node = 0) const;
  int CheckNodeConsistencyRecursive(int node_id, int max_depth) const;

  std::vector<TreeNode> nodes_;
  const TreeStorage* parent_;
  int position_in_parent_;
  int first_free_node_;

  friend class FullTreeTraversal;
  friend class LocalTreeTraversal;
  friend class ConstLocalTreeTraversal;
  friend class LocalEpsTreeTraversal;
  friend class ConstLocalEpsTreeTraversal;
  friend class SlicedTreeTraversal;
};

namespace std {
template <> struct hash<TreeStorage> {
  size_t operator()(const TreeStorage& x) const {
    return x.GetHash();
  }
};
}

// A traversal class going over the entire tree.
class FullTreeTraversal {
public:
  explicit FullTreeTraversal(const TreeStorage* storage, int position)
    : storage_(storage), position_(position) {
  }

  bool operator==(const FullTreeTraversal& o) const {
    return storage_ == o.storage_ && position_ == o.position_;
  }

  TreeIterator<const TreeNode, FullTreeTraversal> begin() {
    return TreeIterator<const TreeNode, FullTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS);
  }

  TreeIterator<const TreeNode, FullTreeTraversal> end() {
    return TreeIterator<const TreeNode, FullTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS, true);
  }

  const TreeNode& node() const {
    return storage_->nodes_[position_];
  }

  int position() const {
    return position_;
  }

  const TreeStorage* tree_storage() const {
    return storage_;
  }

  bool left() {
    int left_sib = node().left_sib;
    if (left_sib == TREEPOINTER_VALUE_IN_PARENT) {
      left_sib = move_to_parent_storage().left_sib;
    }
    if (left_sib < 0) return false;
    position_ = left_sib;
    return true;
  }

  bool right() {
    int right_sib = node().right_sib;
    if (right_sib == TREEPOINTER_VALUE_IN_PARENT) {
      right_sib = move_to_parent_storage().right_sib;
    }
    if (right_sib < 0) return false;
    position_ = right_sib;
    return true;
  }

  bool up() {
    int parent = node().parent;
    if (parent == TREEPOINTER_VALUE_IN_PARENT) {
      parent = move_to_parent_storage().parent;
    }
    if (parent < 0) return false;
    position_ = parent;
    return true;
  }

  bool down_first_child() {
    int first_child = node().first_child;
    if (first_child < 0) return false;
    position_ = first_child;
    return true;
  }

  bool down_last_child() {
    int last_child = node().last_child;
    if (last_child < 0) return false;
    position_ = last_child;
    return true;
  }

private:
  const TreeNode& move_to_parent_storage() {
    position_ = storage_->position_in_parent_;
    storage_ = storage_->parent_;
    return storage_->nodes_[position_];
  }

  const TreeStorage* storage_;
  int position_;
};

// Iterates over the tree only stored in a given local storage.
class LocalTreeTraversal {
public:
  explicit LocalTreeTraversal(TreeStorage* storage, int position)
    : storage_(storage), position_(position) {
  }

  bool operator==(const LocalTreeTraversal& o) const {
    return storage_ == o.storage_ && position_ == o.position_;
  }

  TreeIterator<TreeNode, LocalTreeTraversal> begin() {
    return TreeIterator<TreeNode, LocalTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS);
  }

  TreeIterator<TreeNode, LocalTreeTraversal> end() {
    return TreeIterator<TreeNode, LocalTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS, true);
  }

  TreeIterator<TreeNode, LocalTreeTraversal> begin(TreeIteratorMode mode) {
    return TreeIterator<TreeNode, LocalTreeTraversal>(*this, mode);
  }

  TreeIterator<TreeNode, LocalTreeTraversal> end(TreeIteratorMode mode) {
    return TreeIterator<TreeNode, LocalTreeTraversal>(*this, mode, true);
  }

  TreeNode& node() {
    return storage_->nodes_[position_];
  }

  int position() const {
    return position_;
  }

  TreeStorage* tree_storage() {
    return storage_;
  }

  bool left() {
    int left_sib = node().left_sib;
    if (left_sib < 0) return false;
    position_ = left_sib;
    return true;
  }

  bool right() {
    int right_sib = node().right_sib;
    if (right_sib < 0) return false;
    position_ = right_sib;
    return true;
  }

  bool up() {
    int parent = node().parent;
    if (parent < 0) return false;
    position_ = parent;
    return true;
  }

  bool down_first_child() {
    int first_child = node().first_child;
    if (first_child < 0) return false;
    position_ = first_child;
    return true;
  }

  bool down_last_child() {
    int last_child = node().last_child;
    if (last_child < 0) return false;
    position_ = last_child;
    return true;
  }

private:
  TreeStorage* storage_;
  int position_;
};

// Iterates over the tree only stored in a given local storage.
class ConstLocalTreeTraversal {
public:
  explicit ConstLocalTreeTraversal(const TreeStorage* storage, int position)
    : storage_(storage), position_(position) {
  }

  bool operator==(const ConstLocalTreeTraversal& o) const {
    return storage_ == o.storage_ && position_ == o.position_;
  }

  TreeIterator<const TreeNode, ConstLocalTreeTraversal> begin() const {
    return TreeIterator<const TreeNode, ConstLocalTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS);
  }

  TreeIterator<const TreeNode, ConstLocalTreeTraversal> end() const {
    return TreeIterator<const TreeNode, ConstLocalTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS, true);
  }

  TreeIterator<const TreeNode, ConstLocalTreeTraversal> begin(TreeIteratorMode mode) const {
    return TreeIterator<const TreeNode, ConstLocalTreeTraversal>(*this, mode);
  }

  TreeIterator<const TreeNode, ConstLocalTreeTraversal> end(TreeIteratorMode mode) const {
    return TreeIterator<const TreeNode, ConstLocalTreeTraversal>(*this, mode, true);
  }

  const TreeNode& node() const {
    return storage_->nodes_[position_];
  }

  int position() const {
    return position_;
  }

  const TreeStorage* tree_storage() const {
    return storage_;
  }

  bool left() {
    int left_sib = node().left_sib;
    if (left_sib < 0) return false;
    position_ = left_sib;
    return true;
  }

  bool right() {
    int right_sib = node().right_sib;
    if (right_sib < 0) return false;
    position_ = right_sib;
    return true;
  }

  bool up() {
    int parent = node().parent;
    if (parent < 0) return false;
    position_ = parent;
    return true;
  }

  bool down_first_child() {
    int first_child = node().first_child;
    if (first_child < 0) return false;
    position_ = first_child;
    return true;
  }

  bool down_last_child() {
    int last_child = node().last_child;
    if (last_child < 0) return false;
    position_ = last_child;
    return true;
  }

private:
  const TreeStorage* storage_;
  int position_;
};

// A traversal that visits also non-existing (eps) nodes, enabling writing to them.
class LocalEpsTreeTraversal {
public:
  explicit LocalEpsTreeTraversal(TreeStorage* storage, int position)
    : storage_(storage), position_(position) {
  }

  bool operator==(const LocalEpsTreeTraversal& o) const {
    return storage_ == o.storage_ && position_ == o.position_ && (position_ >= 0 || eps_node_.parent == o.eps_node_.parent);
  }

  TreeIterator<TreeNode, LocalEpsTreeTraversal> begin() {
    return TreeIterator<TreeNode, LocalEpsTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS);
  }

  TreeIterator<TreeNode, LocalEpsTreeTraversal> end() {
    return TreeIterator<TreeNode, LocalEpsTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS, true);
  }

  TreeNode& node() {
    return position_ < 0 ? eps_node_ : storage_->nodes_[position_];
  }

  void write_node() {
    if (position_ < 0) position_ = storage_->AddLastNode(eps_node_);
  }

  int position() const {
    return position_;
  }

  TreeStorage* tree_storage() {
    return storage_;
  }

  bool left() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    if (position_ < 0) position_ = storage_->AddLastNode(eps_node_);
    int left_sib = storage_->nodes_[position_].left_sib;
    if (left_sib < 0) return false;
    position_ = left_sib;
    return true;
  }

  bool right() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    if (position_ < 0) position_ = storage_->AddLastNode(eps_node_);
    if (position_ < 0) return false;
    int right_sib = node().right_sib;
    if (right_sib < 0) {
      if (position_ == 0)  return false;  // Nothing on the right of the root.
      allocate_eps_right_sibling();
    }
    position_ = right_sib;
    return true;
  }

  bool up() {
    if (position_ < 0) position_ = storage_->AddLastNode(eps_node_);
    int parent = node().parent;
    if (parent < 0) return false;
    position_ = parent;
    return true;
  }

  bool down_first_child() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    if (position_ < 0) position_ = storage_->AddLastNode(eps_node_);
    int first_child = node().first_child;
    if (first_child < 0) allocate_eps_child();
    position_ = first_child;
    return true;
  }

  bool down_last_child() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    if (position_ < 0) position_ = storage_->AddLastNode(eps_node_);
    int last_child = node().last_child;
    if (last_child < 0) allocate_eps_child();
    position_ = last_child;
    return true;
  }

private:
  void allocate_eps_child() {
    eps_node_.type = TreeNode::EMPTY_NODE_LABEL;
    eps_node_.value = -1;
    eps_node_.parent = position_;
    eps_node_.left_sib = -1;
    eps_node_.right_sib = -1;
    eps_node_.first_child = -1;
    eps_node_.last_child = -1;
    eps_node_.child_index = 0;
  }

  void allocate_eps_right_sibling() {
    DCHECK_GE(position_, 0);
    eps_node_.type = TreeNode::EMPTY_NODE_LABEL;
    eps_node_.value = -1;
    eps_node_.parent = node().parent;
    eps_node_.left_sib = position_;
    eps_node_.right_sib = -1;
    eps_node_.first_child = -1;
    eps_node_.last_child = -1;
    eps_node_.child_index = node().child_index + 1;
  }

  TreeStorage* storage_;
  int position_;
  TreeNode eps_node_;
};


// A traversal that visits also non-existing (eps) nodes, enabling copying one tree to another.
class ConstLocalEpsTreeTraversal {
public:
  explicit ConstLocalEpsTreeTraversal(const TreeStorage* storage, int position)
    : storage_(storage), position_(position) {
  }

  bool operator==(const ConstLocalEpsTreeTraversal& o) const {
    return storage_ == o.storage_ && position_ == o.position_ && (position_ >= 0 || eps_node_.parent == o.eps_node_.parent);
  }

  TreeIterator<const TreeNode, ConstLocalEpsTreeTraversal> begin() {
    return TreeIterator<const TreeNode, ConstLocalEpsTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS);
  }

  TreeIterator<const TreeNode, ConstLocalEpsTreeTraversal> end() {
    return TreeIterator<const TreeNode, ConstLocalEpsTreeTraversal>(*this, TreeIteratorMode::PRE_ORDER_FORWARD_DFS, true);
  }

  const TreeNode& node() const {
    return position_ < 0 ? eps_node_ : storage_->nodes_[position_];
  }

  int position() const {
    return position_;
  }

  const TreeStorage* tree_storage() {
    return storage_;
  }

  bool left() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    int left_sib = storage_->nodes_[position_].left_sib;
    if (left_sib < 0) return false;
    position_ = left_sib;
    return true;
  }

  bool right() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    if (position_ < 0) return false;
    int right_sib = node().right_sib;
    if (right_sib < 0) {
      if (position_ == 0)  return false;  // Nothing on the right of the root.
      allocate_eps_right_sibling();
    }
    position_ = right_sib;
    return true;
  }

  bool up() {
    int parent = node().parent;
    if (parent < 0) return false;
    position_ = parent;
    return true;
  }

  bool down_first_child() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    int first_child = node().first_child;
    if (first_child < 0) allocate_eps_child();
    position_ = first_child;
    return true;
  }

  bool down_last_child() {
    if (node().type == TreeNode::EMPTY_NODE_LABEL) return false;
    int last_child = node().last_child;
    if (last_child < 0) allocate_eps_child();
    position_ = last_child;
    return true;
  }

private:
  void allocate_eps_child() {
    eps_node_.type = TreeNode::EMPTY_NODE_LABEL;
    eps_node_.value = -1;
    eps_node_.parent = position_;
    eps_node_.left_sib = -1;
    eps_node_.right_sib = -1;
    eps_node_.first_child = -1;
    eps_node_.last_child = -1;
    eps_node_.child_index = 0;
  }

  void allocate_eps_right_sibling() {
    DCHECK_GE(position_, 0);
    eps_node_.type = TreeNode::EMPTY_NODE_LABEL;
    eps_node_.value = -1;
    eps_node_.parent = node().parent;
    eps_node_.left_sib = position_;
    eps_node_.right_sib = -1;
    eps_node_.first_child = -1;
    eps_node_.last_child = -1;
    eps_node_.child_index = node().child_index + 1;
  }

  const TreeStorage* storage_;
  int position_;
  TreeNode eps_node_;
};

int TreeSize(ConstLocalTreeTraversal t);

void CompareTrees(ConstLocalTreeTraversal t1, ConstLocalTreeTraversal t2, TreeCompareInfo* info, bool only_types = false, int max_depth = std::numeric_limits<int>::max());
void CompareTrees(ConstLocalTreeTraversal t1, ConstLocalTreeTraversal t2, int* num_equalities, int* num_diffs);

void ParseTreesInFileWithParallelJSONParse(
    StringSet* ss,
    const char* filename,
    int start_offset,
    int num_records,
    bool show_progress,
    std::vector<TreeStorage>* trees);

#endif /* SYNTREE_TREE_H_ */
