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

#ifndef SYNTREE_TCOND_LANGUAGE_H_
#define SYNTREE_TCOND_LANGUAGE_H_

#include <random>
#include <set>
#include <string>
#include <vector>

#include "glog/logging.h"

#include "base/stringprintf.h"
#include "base/stringset.h"
#include "phog/tree/tree_index.h"

#include "gflags/gflags.h"

// Describes the base TCond language. Specifics may build on top of this basic language.
class TCondLanguage {
public:
  explicit TCondLanguage(StringSet* ss)
        : ss_(ss) {
  }

  typedef SequenceHashFeature Feature;
  typedef std::mt19937 RandomGen;

  enum class OpCmd {
    WRITE_TYPE = 0,
    WRITE_VALUE,
    WRITE_POS,

    UP,
    LEFT,
    RIGHT,
    DOWN_FIRST,
    DOWN_LAST,
    PREV_DFS,
    PREV_LEAF,
    NEXT_LEAF,

    PREV_NODE_VALUE,
    PREV_NODE_TYPE,
    PREV_NODE_CONTEXT,

    LAST_OP_CMD
  };

  static std::string OpCmdStr(OpCmd cmd) {
    switch (cmd) {
    case OpCmd::WRITE_TYPE: return "WRITE_TYPE";
    case OpCmd::WRITE_VALUE: return "WRITE_VALUE";
    case OpCmd::WRITE_POS: return "WRITE_POS";
    case OpCmd::UP: return "UP";
    case OpCmd::LEFT: return "LEFT";
    case OpCmd::RIGHT: return "RIGHT";
    case OpCmd::DOWN_FIRST: return "DOWN_FIRST";
    case OpCmd::DOWN_LAST: return "DOWN_LAST";
    case OpCmd::PREV_DFS: return "PREV_DFS";
    case OpCmd::PREV_LEAF: return "PREV_LEAF";
    case OpCmd::NEXT_LEAF: return "NEXT_LEAF";
    case OpCmd::PREV_NODE_VALUE: return "PREV_NODE_VALUE";
    case OpCmd::PREV_NODE_TYPE: return "PREV_NODE_TYPE";
    case OpCmd::PREV_NODE_CONTEXT: return "PREV_NODE_CONTEXT";

    case OpCmd::LAST_OP_CMD: return "ERROR";
    }
    return "?";
  }

  struct Op {
    Op() : cmd(OpCmd::WRITE_TYPE), extra_data(-1) {}
    Op(OpCmd cmd) : cmd(cmd), extra_data(-1) {}
    Op(OpCmd cmd, int type) : cmd(cmd), extra_data(type) {}

    OpCmd cmd;
    int extra_data;

    inline bool operator==(const Op& o) const {
      return cmd == o.cmd && extra_data == o.extra_data;
    }
    inline bool operator!=(const Op& o) const {
      return !(*this == o);
    }

    inline bool operator<(const Op& o) const {
      if (cmd == o.cmd) return extra_data < o.extra_data;
      return cmd < o.cmd;
    }
  };

  typedef std::vector<Op> Program;

  std::string ProgramToString(const Program& p) const;
  Program ParseStringToProgramOrDie(const std::string& s) const;

  // ExecutionForTree is created for a tree that remains constant and the nodes in the tree are given
  // in canonical order. This is, nodes are numbered in depth-first search left-to-right order.
  //
  // This class indexes the nodes of the tree so that PREV_NODE_TYPE, PREV_NODE_VALUE, etc can be
  // executed fast.
  class ExecutionForTree {
  public:
    ExecutionForTree(const StringSet* ss, const TreeStorage* tree) : ss_(ss), tree_(tree),
          index_by_node_type_(&af_by_nt_, tree), index_by_node_value_(&af_by_nv_, tree), index_by_node_context_(&af_by_nc_, tree) {
      index_by_node_type_.Build();
      index_by_node_value_.Build();
      index_by_node_context_.Build();
    }

    // Returns true if each program operation could be performed, false otherwise. Updates the given traversal t.
    //
    // F(int op_added)
    // bool C(int called_function, SlicedTreeTraversal* t)
    template<class F>
    bool GetConditionedFeaturesForPosition(const Program& p, SlicedTreeTraversal* t, std::string* debug_info, const F& feature_callback) const {
      for (Op op : p) {
        switch (op.cmd) {
          case OpCmd::WRITE_TYPE:
          {
            int type = t->node().Type();
            if (debug_info != nullptr) {
              StringAppendF(debug_info, "[WRITE_TYPE - %s] ", type >= 0 ? ss_->getString(type) : std::to_string(type).c_str());
            }
            feature_callback(type);
            break;
          }
          case OpCmd::WRITE_VALUE:
          {
            int value = t->node().Value();
            if (debug_info != nullptr) {
              StringAppendF(debug_info, "[WRITE_VALUE - %s] ", value >= 0 ? ss_->getString(value) : std::to_string(value).c_str());
            }
            feature_callback(value);
            break;
          }
          case OpCmd::WRITE_POS:
          {
            if (debug_info != nullptr) {
              StringAppendF(debug_info, "[WRITE_POS - %d] ", t->node().child_index);
            }
            // Use negative value such that BranchCondProgram interprets it as number
            feature_callback(-1000 - t->node().child_index);
            break;
          }
          case OpCmd::UP:
            t->up();
            break;
          case OpCmd::LEFT:
            t->left();
            break;
          case OpCmd::RIGHT:
            t->right();
            break;
          case OpCmd::DOWN_FIRST:
          {
            t->down_first_child();
            break;
          }
          case OpCmd::DOWN_LAST:
          {
            t->down_last_child();
            break;
          }
          case OpCmd::PREV_LEAF:
            for (;;) {
              if (t->left()) {
                while (t->down_last_child()) {}
                break;
              } else {
                if (!t->up()) break;
              }
            }
            break;
          case OpCmd::NEXT_LEAF:
          {
            for (;;) {
              if (t->right()) {
                while (t->down_first_child()) {
                }
                break;
              } else {
                if (!t->up()) break;
              }
            }
            break;
          }
          case OpCmd::PREV_DFS:
            if (t->left()) {
              while (t->down_last_child()) {}
            } else {
              t->up();
            }
            break;
          case OpCmd::PREV_NODE_VALUE:
          {
            if (af_by_nv_.GetNodeActorSymbol(*t) != -1) {
              ActorSymbolIterator it(
                  af_by_nv_.GetNodeActorSymbol(*t),
                  *t,
                  &index_by_node_value_);
              if (it.MoveLeft()) {
                *t = it.GetItem();
              }
            }
            break;
          }
          case OpCmd::PREV_NODE_TYPE:
          {
            ActorSymbolIterator it(
                af_by_nt_.GetNodeActorSymbol(*t),
                *t,
                &index_by_node_type_);
            if (it.MoveLeft()) {
              *t = it.GetItem();
            }
            break;
          }
          case OpCmd::PREV_NODE_CONTEXT:
          {
            ActorSymbolIterator it(
                af_by_nc_.GetNodeActorSymbol(*t),
                *t,
                &index_by_node_context_);
            if (it.MoveLeft()) {
              *t = it.GetItem();
            }
            break;
          }
          case OpCmd::LAST_OP_CMD:
            break;
        }
      }

      if (debug_info != nullptr) {
        debug_info->append("\n");
      }

      return true;
    }

    const StringSet* ss() const { return ss_; }
    const TreeStorage* tree() const { return tree_; }

  private:
    const StringSet* ss_;
    const TreeStorage* tree_;
    ActorFinderByNodeType af_by_nt_;
    ActorIndex index_by_node_type_;

    ActorFinderByNodeValue af_by_nv_;
    ActorIndex index_by_node_value_;

    ActorFinderByNodeContext af_by_nc_;
    ActorIndex index_by_node_context_;
  };

  StringSet* ss() { return ss_; }
  const StringSet* ss() const { return ss_; }

private:
  StringSet* ss_;
};

// Hash for TCondLanguage::Program.
namespace std {
template <> struct hash<TCondLanguage::Program> {
  size_t operator()(const TCondLanguage::Program& x) const {
    unsigned result = 0;
    for (size_t i = 0; i < x.size(); ++i) {
      result = FingerprintCat(result, FingerprintCat(static_cast<int>(x[i].cmd), x[i].extra_data));
    }
    return result;
  }
};
}

// Used in PerFeatureValueCounter
namespace std {
template <> struct hash<std::pair<TCondLanguage::Feature, TreeSubstitutionOnlyLabel> > {
  size_t operator()(const std::pair<TCondLanguage::Feature, TreeSubstitutionOnlyLabel>& x) const {
    return FingerprintCat(std::hash<TCondLanguage::Feature>()(x.first), std::hash<TreeSubstitutionOnlyLabel>()(x.second));
  }
};

}

#endif /* SYNTREE_TCOND_LANGUAGE_H_ */
