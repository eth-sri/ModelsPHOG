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

#ifndef SYNTREE_TREE_INDEX_H_
#define SYNTREE_TREE_INDEX_H_

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pbox.h"
#include "tree.h"
#include "tree_slice.h"


// A base class that returns the actor and its action from a position in a tree.
class ActorFinder {
public:
  virtual ~ActorFinder();

  // Given an actor position, returns the action.
  virtual bool GetActionFromActor(SlicedTreeTraversal actor, SlicedTreeTraversal* action) const = 0;

  // Given a position, returns if it is an actor and the symbol of the actor.
  virtual int GetNodeActorSymbol(SlicedTreeTraversal t) const = 0;
};


// Actor Index
class ActorIndex {
public:
  struct Sequence {
    std::vector<int> nodes;
  };

  explicit ActorIndex(const ActorFinder* actor_finder, const TreeStorage* tree)
    : actor_finder_(actor_finder), tree_(tree) {
  }

  ActorIndex(const ActorIndex&) = default;
  ActorIndex(ActorIndex&&) = default;

  // Builds an ActorIndex for all nodes in the tree.
  void Build();

  const ActorFinder* actor_finder() const {
    return actor_finder_;
  }

  const Sequence* find_sequence(int symbol) const {
    auto it = symbol_sequences_.find(symbol);
    if (it == symbol_sequences_.end()) return nullptr;
    return &it->second;
  }

  struct SymbolSequencePredecessor {
    SymbolSequencePredecessor() : symbol(-1), pred_position(-1) {}
    int symbol;
    int pred_position;
  };

  // Takes a position and moves to the position to the left if the position on
  // the left is indexed. Returns true on success.
  bool get_symbol_pedecessor(const TreeStorage* tree, int symbol, int* position) const {
    if (tree == tree_) {
      CHECK_LT(*position, static_cast<int>(symbol_predecessors_.size()));
      if (symbol_predecessors_[*position].symbol == symbol) {
        *position = symbol_predecessors_[*position].pred_position;
        return true;
      }
    }
    return false;
  }

private:
  const ActorFinder* actor_finder_;
  const TreeStorage* tree_;
  std::unordered_map<int, Sequence> symbol_sequences_;
  std::vector<SymbolSequencePredecessor> symbol_predecessors_;
};

class ActorSymbolIterator {
public:
  ActorSymbolIterator(int symbol, SlicedTreeTraversal tree_pos, const ActorIndex* index)
    : symbol_(symbol), tree_pos_(tree_pos), index_(index) {
  }

  bool MoveLeft() {
    int position = tree_pos_.position();
    if (index_->get_symbol_pedecessor(tree_pos_.tree_storage(), symbol_, &position)) {
      if (position < 0) return false;
      tree_pos_ = SlicedTreeTraversal(tree_pos_.tree_storage(), position, tree_pos_.slice());
      return true;
    }

    if (tree_pos_.tree_storage()->parent() != nullptr) {
      // We are in a local tree that is not indexed -- iterate over all elements to find the symbol.

      ConstLocalTreeTraversal local_t(tree_pos_.tree_storage(), tree_pos_.position());
      for (;;) {
        if (local_t.left()) {
          while (local_t.down_last_child()) {}
        } else {
          if (!local_t.up()) {
            break;
          }
        }
        int symbol = index_->actor_finder()->GetNodeActorSymbol(SlicedTreeTraversal(local_t.tree_storage(), local_t.position(), nullptr));
        if (symbol == symbol_) {
          tree_pos_ = SlicedTreeTraversal(local_t.tree_storage(), local_t.position(), tree_pos_.slice());
          return true;
        }
      }

      // MoveLeft did not find the symbol in the local tree. Move to the parent.
      int parent_pos = tree_pos_.tree_storage()->position_in_parent();
      tree_pos_ = SlicedTreeTraversal(tree_pos_.tree_storage()->parent(), parent_pos, tree_pos_.slice());
    }

    // The tree is indexed. Lookup the index.
    const typename ActorIndex::Sequence* seq = index_->find_sequence(symbol_);
    if (seq == nullptr) return false;
    auto it = std::lower_bound(seq->nodes.begin(), seq->nodes.end(), tree_pos_.position());
    if (it == seq->nodes.begin()) return false;
    --it;
    tree_pos_ = SlicedTreeTraversal(tree_pos_.tree_storage(), *it, tree_pos_.slice());
    return true;
  }

  SlicedTreeTraversal GetItem() {
    return tree_pos_;
  }

private:
  int symbol_;
  SlicedTreeTraversal tree_pos_;
  const ActorIndex* index_;
};


// Simple ActorFinder. Each node type is an actor => the nodes are grouped by type.
class ActorFinderByNodeType : public ActorFinder {
public:
  ~ActorFinderByNodeType();

  virtual bool GetActionFromActor(SlicedTreeTraversal actor, SlicedTreeTraversal* action) const override {
    return false;
  }

  virtual int GetNodeActorSymbol(SlicedTreeTraversal t) const override {
    return t.node().Type();
  }
};

// Simple ActorFinder. Each node value is an actor => the nodes are grouped by value.
class ActorFinderByNodeValue : public ActorFinder {
public:
  ~ActorFinderByNodeValue();

  virtual bool GetActionFromActor(SlicedTreeTraversal actor, SlicedTreeTraversal* action) const override {
    return false;
  }

  virtual int GetNodeActorSymbol(SlicedTreeTraversal t) const override {
    return t.node().Value();
  }
};

// Simple ActorFinder. Each node value is an actor => the nodes are grouped by context (node type, node value + parent node type).
class ActorFinderByNodeContext : public ActorFinder {
public:
  ~ActorFinderByNodeContext();

  virtual bool GetActionFromActor(SlicedTreeTraversal actor, SlicedTreeTraversal* action) const override {
    return false;
  }

  virtual int GetNodeActorSymbol(SlicedTreeTraversal t) const override {
    SequenceHashFeature f;
    int context_size = 0;
    do {
      f.PushBack(t.node().Value());
      f.PushBack(t.node().Type());
      context_size++;
    } while (t.up() && context_size < 3);
    return f.hash_;
  }
};

#endif /* SYNTREE_TREE_INDEX_H_ */
