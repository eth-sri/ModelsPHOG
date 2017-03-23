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

#include "tree_index.h"

ActorFinder::~ActorFinder() {
}

ActorFinderByNodeType::~ActorFinderByNodeType() {
}

ActorFinderByNodeValue::~ActorFinderByNodeValue() {
}

ActorFinderByNodeContext::~ActorFinderByNodeContext() {
}

void ActorIndex::Build() {
  symbol_predecessors_.assign(tree_->NumAllocatedNodes(), SymbolSequencePredecessor());

  TreeSlice slice(nullptr);
  tree_->ForEachSubnodeOfNode(0, [this, &slice](int node_id){
    int symbol = actor_finder_->GetNodeActorSymbol(SlicedTreeTraversal(tree_, node_id, &slice));
    if (symbol >= 0) {
      std::vector<int>& symbol_nodes = symbol_sequences_[symbol].nodes;
      symbol_predecessors_[node_id].symbol = symbol;
      if (!symbol_nodes.empty()) {
        symbol_predecessors_[node_id].pred_position = symbol_nodes.back();
      }
      symbol_nodes.push_back(node_id);
    }
  });
}

