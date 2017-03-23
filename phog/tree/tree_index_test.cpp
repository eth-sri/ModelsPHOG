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

#include "external/gtest/googletest/include/gtest/gtest.h"
#include "glog/logging.h"

#include "base/stringprintf.h"

#include "tree.h"

TEST(TreeIndex, TestIterator) {
  TreeStorage t;
  TreeSubstitution s(
      // TYPE,VALUE,FIRST_CHILD,RIGHT_SIB
      {{100, -1,  1, -1},  // Node 0
       {101, -1, -1,  2},  // Node 1
       {105, -1, -1,  3},
       {106, -1, -1,  4},
       {101, -1, -1,  5},
       {105, -1, -1, -1}});  // Node 5
  CHECK(t.CanSubstituteNode(0, s));
  t.SubstituteNode(0, s);

  ActorFinderByNodeType afnt;
  ActorIndex index(&afnt, &t);
  index.Build();

  {
    ActorSymbolIterator it(105, SlicedTreeTraversal(&t, 5), &index);
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(2, it.GetItem().position());
    EXPECT_FALSE(it.MoveLeft());
  }
  {
    ActorSymbolIterator it(101, SlicedTreeTraversal(&t, 4), &index);
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(1, it.GetItem().position());
    EXPECT_FALSE(it.MoveLeft());
  }
  {
    ActorSymbolIterator it(106, SlicedTreeTraversal(&t, 3), &index);
    EXPECT_FALSE(it.MoveLeft());
  }
}

TEST(TreeIndex, TestIteratorSubtree) {
  TreeStorage t;
  TreeSubstitution s(
      // TYPE,VALUE,FIRST_CHILD,RIGHT_SIB
      {{100, -1,  1, -1},  // Node 0
       {101, -1, -1,  2},  // Node 1
       {105, -1, -1,  3},
       {106, -1, -1,  4},
       {101, -1, -1,  5},
       {105, -1, -1, -1}});  // Node 5
  CHECK(t.CanSubstituteNode(0, s));
  t.SubstituteNode(0, s);

  ActorFinderByNodeType afnt;
  ActorIndex index(&afnt, &t);
  index.Build();


  TreeStorage subtree(&t, 3);
  TreeSubstitution subs(
      // TYPE,VALUE,FIRST_CHILD,RIGHT_SIB
      {{106, -1,  1, -1},  // Node 0
       {105, -1, -1,  2},
       {101, -1, -1,  3},
       {105, -1, -1,  4},
       {105, -1, -1, -1}});  // Node 4
  CHECK(subtree.CanSubstituteNode(0, subs));
  subtree.SubstituteNode(0, subs);

  {
    ActorSymbolIterator it(105, SlicedTreeTraversal(&t, 5), &index);
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(2, it.GetItem().position());
    EXPECT_FALSE(it.MoveLeft());
  }
  {
    ActorSymbolIterator it(105, SlicedTreeTraversal(&subtree, 3), &index);
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(&subtree, it.GetItem().tree_storage());
    EXPECT_EQ(1, it.GetItem().position());
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(&t, it.GetItem().tree_storage());
    EXPECT_EQ(2, it.GetItem().position());
    EXPECT_FALSE(it.MoveLeft());
  }
  {
    ActorSymbolIterator it(105, SlicedTreeTraversal(&subtree, 4), &index);
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(&subtree, it.GetItem().tree_storage());
    EXPECT_EQ(3, it.GetItem().position());
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(&subtree, it.GetItem().tree_storage());
    EXPECT_EQ(1, it.GetItem().position());
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(&t, it.GetItem().tree_storage());
    EXPECT_EQ(2, it.GetItem().position());
    EXPECT_FALSE(it.MoveLeft());
  }
  {
    subtree.SubstituteSingleNode(4, TreeSubstitution::Node({109, -1, -1, -1}));
    ActorSymbolIterator it(109, SlicedTreeTraversal(&subtree, 4), &index);
    EXPECT_FALSE(it.MoveLeft());
  }
  {
    ActorSymbolIterator it(101, SlicedTreeTraversal(&subtree, 2), &index);
    EXPECT_TRUE(it.MoveLeft());
    EXPECT_EQ(&t, it.GetItem().tree_storage());
    EXPECT_EQ(1, it.GetItem().position());
    EXPECT_FALSE(it.MoveLeft());
  }
}

int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

