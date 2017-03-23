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

#ifndef SYNTREE_TREE_SLICE_H_
#define SYNTREE_TREE_SLICE_H_

#include "tree.h"

// Keeps track of nodes that should not be read in a tree even if they are physically there.
// The  TreeSlice is useful in learning/evaluation since we learn each node by only conditioning on
// part of the nodes that should be predicted earlier.
class TreeSlice {
public:
  TreeSlice(const TreeStorage* storage) : storage_(storage), begin_(-1), end_(-1), allow_reading_type_for_begin_node_(false) {}
  TreeSlice(const TreeStorage* storage, int begin) : storage_(storage), begin_(begin), end_(storage->NumAllocatedNodes()), allow_reading_type_for_begin_node_(false) {}
  TreeSlice(const TreeStorage* storage, int begin, bool allow_reading_type_for_begin_node) : storage_(storage), begin_(begin), end_(storage->NumAllocatedNodes()),
      allow_reading_type_for_begin_node_(allow_reading_type_for_begin_node) {}

  // Denotes that the node should NOT be conditioned on (as it is in the [removed] slice).
  bool IsNodeSliced(const TreeStorage* storage, int node_id) const {
    return storage == storage_ && node_id >= begin_ && node_id < end_;
  }

  // The first sliced (removed) node.
  int BeginNode() const {
    return begin_;
  }

  // One node past the last sliced (removed) node.
  int EndNode() const {
    return end_;
  }

  const TreeStorage* SlicedStorage() const {
    return storage_;
  }

  bool AllowReadingTypeForBeginNode() const {
    return allow_reading_type_for_begin_node_;
  }

private:
  const TreeStorage* storage_;
  const int begin_;
  const int end_;
  const bool allow_reading_type_for_begin_node_;
};

class SlicedTreeTraversal {
public:
  explicit SlicedTreeTraversal(const TreeStorage* storage, int position)
      : storage_(storage), position_(position), slice_(nullptr), last_subtree_position_(-1), last_subtree_(nullptr) {
    }
  explicit SlicedTreeTraversal(const TreeStorage* storage, int position, const TreeSlice* slice)
    : storage_(storage), position_(position), slice_(slice), last_subtree_position_(-1), last_subtree_(nullptr) {
  }

  bool operator==(const SlicedTreeTraversal& o) const {
    return storage_ == o.storage_ && position_ == o.position_ && slice_ == o.slice_;
  }

  const TreeNode node() const {
    if (slice_ != nullptr && slice_->IsNodeSliced(storage_, position_)) {
      if (position_ == slice_->BeginNode()) {
        TreeNode result(TreeNode::EMPTY_NODE);
        // child_index for the predicted node should be available
        result.child_index = storage_->nodes_[position_].child_index;
        // Additionally we want to be able to leave the node to unsliced nodes left and up
        result.left_sib = storage_->nodes_[position_].left_sib;
        result.parent = storage_->nodes_[position_].parent;
        if (slice_->AllowReadingTypeForBeginNode())
          result.SetType(storage_->nodes_[position_].Type());
        return result;
      }
      return TreeNode::EMPTY_NODE;
    }

    return storage_->nodes_[position_];
  }

  int position() const {
    return position_;
  }

  const TreeStorage* tree_storage() const {
    return storage_;
  }

  const TreeSlice* slice() const {
    return slice_;
  }

  bool left() {
    int left_sib = storage_->nodes_[position_].left_sib;
    if (left_sib == TREEPOINTER_VALUE_IN_PARENT && can_move_to_parent_storage()) {
      left_sib = move_to_parent_storage().left_sib;
    }
    if (left_sib < 0) return false;
    if (slice_ != nullptr &&
        slice_->IsNodeSliced(storage_, left_sib)) {
      CHECK(left_sib != slice_->BeginNode());
      return false;
    }
    position_ = left_sib;
    return true;
  }

  bool right() {
    int right_sib = node().right_sib;
    if (right_sib == TREEPOINTER_VALUE_IN_PARENT && can_move_to_parent_storage()) {
      right_sib = move_to_parent_storage().right_sib;
    }
    if (right_sib < 0) return false;
    // Unless the slices node is beginning we dont want to move there
    // This is important as otherwise it allows conditioning on presence of the right node
    if (slice_ != nullptr &&
        slice_->IsNodeSliced(storage_, right_sib) && !(right_sib == slice_->BeginNode())) {
      return false;
    }
    position_ = right_sib;
    return true;
  }

  bool up() {
    int parent = storage_->nodes_[position_].parent;
    if (parent == TREEPOINTER_VALUE_IN_PARENT && can_move_to_parent_storage()) {
      parent = move_to_parent_storage().parent;
    }
    if (parent < 0) return false;
    if (slice_ != nullptr &&
        slice_->IsNodeSliced(storage_, parent)) {
      CHECK(parent != slice_->BeginNode());
      return false;
    }
    position_ = parent;
    return true;
  }

  bool down_first_child() {
    int first_child = storage_->nodes_[position_].first_child;
    if (first_child < 0) return false;
    if (can_move_to_subtree_storage(first_child)) {
      move_to_subtree_storage();
      return true;
    }
    if (slice_ != nullptr &&
        slice_->IsNodeSliced(storage_, first_child) && !(first_child == slice_->BeginNode())) {
      return false;
    }
    position_ = first_child;
    return true;
  }

  bool down_last_child() {
    int last_child = storage_->nodes_[position_].last_child;
    if (last_child < 0) return false;
    if (can_move_to_subtree_storage(last_child)) {
      move_to_subtree_storage();
      return true;
    }
    if ((slice_ != nullptr &&
        slice_->IsNodeSliced(storage_, last_child)) || storage_->nodes_[last_child].HasNonTerminal()) {
      // We need to make sure that using this instruction we cannot distinguish whether
      // the node being predicted has right siblings. There are two cases:
      // 1: subtree completion -- the last child is always the predicted node
      // 2: original tree -- last child might be some other node which is sliced
      // We need to make sure that the behavior is consistent in both and therefore the move fails in both cases.
      return false;
    }
    position_ = last_child;
    return true;
  }

private:
  const TreeNode& move_to_parent_storage() {
    CHECK(position_ == 0);
    last_subtree_ = storage_;
    last_subtree_position_ = storage_->position_in_parent_;
    position_ = storage_->position_in_parent_;
    storage_ = storage_->parent_;
    return storage_->nodes_[position_];
  }

  void move_to_subtree_storage() {
    CHECK(last_subtree_ != nullptr);
    storage_ = last_subtree_;
    position_ = 0;
    last_subtree_ = nullptr;
    last_subtree_position_ = -1;
  }

  bool can_move_to_subtree_storage(int position) const {
    return position == last_subtree_position_ && last_subtree_ != nullptr;
  }

  bool can_move_to_parent_storage() const {
    return !(slice_ != nullptr &&
        slice_->IsNodeSliced(storage_->parent_, storage_->position_in_parent_) &&
        !(storage_->position_in_parent_ == slice_->BeginNode()));
  }

  const TreeStorage* storage_;
  int position_;
  const TreeSlice* slice_;

  // Supports traversing back to the last subtree.
  // Could be extended with a stack to support arbitrary depth. (currently makes the learning very slow)
  int last_subtree_position_;
  const TreeStorage* last_subtree_;
};


#endif /* SYNTREE_TREE_SLICE_H_ */
