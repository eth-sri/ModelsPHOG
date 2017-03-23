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

#include "tree.h"
#include "tree_slice.h"

#include <string>
#include <cctype>
#include <algorithm>
#include <unordered_set>

#include "gtest/gtest.h"
#include "glog/logging.h"
#include "json/json.h"

#include "base/stringprintf.h"

std::string LocalTTDebugString(TreeStorage* s) {
  std::string result;
  LocalTreeTraversal t(s, 0);
  for (auto it = t.begin(); it != t.end(); ++it) {
    StringAppendF(&result, "%d ", it->Type());
  }
  return result;
}

std::string LocalTTPostDebugString(TreeStorage* s) {
  std::string result;
  LocalTreeTraversal t(s, 0);
  for (auto it = t.begin(TreeIteratorMode::POST_ORDER_FORWARD_DFS); it != t.end(TreeIteratorMode::POST_ORDER_FORWARD_DFS); ++it) {
    StringAppendF(&result, "%d ", it->Type());
  }
  return result;
}

std::string WritableTTDebugString(TreeStorage* s) {
  std::string result;
  LocalEpsTreeTraversal t(s, 0);
  for (auto it = t.begin(); it != t.end(); ++it) {
    StringAppendF(&result, "%d ", it->Type());
  }
  return result;
}

TEST(TreeTest, WritingTree) {
  TreeStorage storage;
  LocalEpsTreeTraversal t(&storage, 0);
  auto it = t.begin();
  ASSERT_TRUE(it != t.end());
  it->SetType(0);
  ++it;
  ASSERT_TRUE(it != t.end());
  it->SetType(TreeNode::EMPTY_NODE_LABEL);
  ++it;
  ASSERT_TRUE(it == t.end());
  EXPECT_EQ("[0]", storage.DebugString());
  EXPECT_EQ(1, TreeSize(ConstLocalTreeTraversal(&storage, 0)));
  storage.CheckConsistency();

  it = t.begin();
  ASSERT_TRUE(it != t.end());
  EXPECT_EQ(0, it->Type());
  ++it;
  ASSERT_TRUE(it != t.end());
  EXPECT_EQ(TreeNode::EMPTY_NODE_LABEL, it->Value());
  it->SetType(1);
  ++it;
  ASSERT_TRUE(it != t.end());
  EXPECT_EQ(TreeNode::EMPTY_NODE_LABEL, it->Value());
  ++it;
  EXPECT_EQ("[0 [1]]", storage.DebugString());
  EXPECT_EQ(2, TreeSize(ConstLocalTreeTraversal(&storage, 0)));
  storage.CheckConsistency();

  ASSERT_TRUE(it != t.end());
  it->SetType(2);
  ++it;
  EXPECT_EQ("[0 [1] [2]]", storage.DebugString());
  EXPECT_EQ(3, TreeSize(ConstLocalTreeTraversal(&storage, 0)));
  storage.CheckConsistency();

  ASSERT_TRUE(it != t.end());
  it->SetType(21);
  ++it;
  EXPECT_EQ("[0 [1] [2 [21]]]", storage.DebugString());
  EXPECT_EQ(4, TreeSize(ConstLocalTreeTraversal(&storage, 0)));
  storage.CheckConsistency();

  ++it;
  ASSERT_TRUE(it != t.end());

  it->SetType(22);
  ++it;
  EXPECT_EQ("[0 [1] [2 [21] [22]]]", storage.DebugString());
  EXPECT_EQ(5, TreeSize(ConstLocalTreeTraversal(&storage, 0)));
  storage.CheckConsistency();
  ASSERT_TRUE(it != t.end());

  it->SetType(221);

  ++it;
  ASSERT_TRUE(it != t.end());
  ++it;
  ASSERT_TRUE(it != t.end());


  ++it;
  ASSERT_TRUE(it != t.end());
  ++it;
  ASSERT_TRUE(it != t.end());
  it->SetType(3);
  ++it;
  EXPECT_EQ("[0 [1] [2 [21] [22 [221]]] [3]]", storage.DebugString());
  EXPECT_EQ(7, TreeSize(ConstLocalTreeTraversal(&storage, 0)));
  storage.CheckConsistency();
  ASSERT_TRUE(it != t.end());

  ++it;
  ASSERT_TRUE(it != t.end());
  ++it;
  ASSERT_TRUE(it == t.end());
  EXPECT_EQ("[0 [1] [2 [21] [22 [221]]] [3]]", storage.DebugString());
  storage.CheckConsistency();

  EXPECT_EQ("0 1 2 21 22 221 3 ", LocalTTDebugString(&storage));
  EXPECT_EQ("1 21 221 22 2 3 0 ", LocalTTPostDebugString(&storage));
  EXPECT_EQ("0 1 -1 2 21 -1 22 221 -1 -1 -1 3 -1 -1 ", WritableTTDebugString(&storage));

  storage.RemoveNodeChildren(2);  // We happen to know that node [2] will be allocated at position 2.
  storage.CheckConsistency();
  EXPECT_EQ("[0 [1] [2] [3]]", storage.DebugString());

  storage.RemoveNodeChildren(1);  // 1 has no children.
  storage.CheckConsistency();
  EXPECT_EQ("[0 [1] [2] [3]]", storage.DebugString());

  LocalTreeTraversal tt(&storage, 0);
  EXPECT_TRUE(tt.down_first_child());
  EXPECT_FALSE(tt.down_first_child());
  EXPECT_EQ(tt.node().Type(), 1);

  TreeSubstitution sub1({{5,-1,1,-1}, {51,-1,3,2}, {52,-1,-1,-1}, {511,-1,-2,-2}});
  ASSERT_TRUE(storage.CanSubstituteNode(tt.position(), sub1));
  storage.SubstituteNode(tt.position(), sub1);
  storage.CheckConsistency();
  EXPECT_EQ("[0 [5 [51 [511 [-2:-2]] [-2:-2]] [52]] [2] [3]]", storage.DebugString());

  storage.RemoveNodeChildren(tt.position());
  storage.CheckConsistency();
  EXPECT_EQ("[0 [5] [2] [3]]", storage.DebugString());
  storage.SubstituteNode(tt.position(), sub1);
  storage.CheckConsistency();
  EXPECT_EQ("[0 [5 [51 [511 [-2:-2]] [-2:-2]] [52]] [2] [3]]", storage.DebugString());
  EXPECT_EQ(9, TreeSize(ConstLocalTreeTraversal(&storage, 0)));

  {
    std::string r;
    storage.ForEachSubnodeOfNode(0, [&r, &storage](int node){
      StringAppendF(&r, "%d ", storage.node(node).Type());
    });
    EXPECT_EQ("0 5 51 511 -1 -1 52 2 3 ", r);
  }
  {
    LocalTreeTraversal tt(&storage, 0);
    EXPECT_TRUE(tt.down_first_child());
    std::string r;
    storage.ForEachSubnodeOfNode(tt.position(), [&r, &storage](int node){
      StringAppendF(&r, "%d ", storage.node(node).Type());
    });
    EXPECT_EQ("5 51 511 -1 -1 52 ", r);
  }
  {
    LocalTreeTraversal tt(&storage, 0);
    EXPECT_TRUE(tt.down_last_child());
    std::string r;
    storage.ForEachSubnodeOfNode(tt.position(), [&r, &storage](int node){
      StringAppendF(&r, "%d ", storage.node(node).Type());
    });
    EXPECT_EQ("3 ", r);
  }

  {
    std::string r;
    storage.ForEachSubnodeOfNodeReturningTrue(0, [&r, &storage](int node) -> bool {
      int type = storage.node(node).Type();
      StringAppendF(&r, "%d ", type);
      return type != 51 && type != 2;
    });
    EXPECT_EQ("0 5 51 52 2 3 ", r);
  }
  {
    LocalTreeTraversal tt(&storage, 0);
    EXPECT_TRUE(tt.down_first_child());
    std::string r;
    storage.ForEachSubnodeOfNodeReturningTrue(tt.position(), [&r, &storage](int node){
      int type = storage.node(node).Type();
      StringAppendF(&r, "%d ", type);
      return type != 51 && type != 2;
    });
    EXPECT_EQ("5 51 52 ", r);
  }
}

TEST(TreeTest, FullTraversal) {
  TreeStorage root_tree;
  root_tree.SubstituteNode(0, TreeSubstitution({{7,-1,1,-1}, {1,-1,-1,2}, {-2,-1,-1,3}, {4,-1,-1,-1}}));
  EXPECT_EQ("[7 [1] [-2] [4]]", root_tree.DebugString());

  TreeStorage second_tree(&root_tree, 2);
  second_tree.SubstituteNode(0, TreeSubstitution({{5,-1,1,-1}, {6,-1,-1,2}, {10,-1,-1,-1}}));
  EXPECT_EQ("[5 [6] [10]]", second_tree.DebugString());

  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(6, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.down_last_child());
    EXPECT_EQ(10, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.left());
    EXPECT_EQ(1, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.right());
    EXPECT_EQ(4, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.right());
    EXPECT_FALSE(t.right());
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
  }

  EXPECT_EQ("[7 [1] [-2] [4]]", root_tree.DebugString());
  second_tree.InlineIntoParent(&root_tree);
  EXPECT_EQ("[7 [1] [5 [6] [10]] [4]]", root_tree.DebugString());
  root_tree.CheckConsistency();

  // second_tree stays non-modified.
  EXPECT_EQ("[5 [6] [10]]", second_tree.DebugString());
  second_tree.CheckConsistency();
}

// A test where at the inline point of the second_tree into the root_tree, there is no right sibling.
TEST(TreeTest, FullTraversal1) {
  TreeStorage root_tree;
  root_tree.SubstituteNode(0, TreeSubstitution({{7,-1,1,-1}, {1,-1,-1,2}, {-2,-1,-1,-1}}));
  EXPECT_EQ("[7 [1] [-2]]", root_tree.DebugString());

  TreeStorage second_tree(&root_tree, 2);
  second_tree.SubstituteNode(0, TreeSubstitution({{5,-1,1,-1}, {6,-1,-1,2}, {10,-1,-1,-1}}));
  EXPECT_EQ("[5 [6] [10]]", second_tree.DebugString());

  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.left());
    EXPECT_EQ(1, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_FALSE(t.right());  // No right sibling.
    EXPECT_EQ(5, t.node().Type());  // Check that the traversal is unmoved.
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(6, t.node().Type());
    EXPECT_TRUE(t.up());
    EXPECT_TRUE(t.left());
    EXPECT_EQ(1, t.node().Type());
  }

  second_tree.InlineIntoParent(&root_tree);
  EXPECT_EQ("[7 [1] [5 [6] [10]]]", root_tree.DebugString());
  root_tree.CheckConsistency();

  {
    ConstLocalTreeTraversal t(&root_tree, 0);
    ASSERT_TRUE(t.down_first_child());
    TreeStorage ssub = root_tree.SubtreeFromNodeAsTree(t.position());

    EXPECT_EQ("[1]", ssub.DebugString());
    ssub.CheckConsistency();
  }
  {
    ConstLocalTreeTraversal t(&root_tree, 0);
    ASSERT_TRUE(t.down_last_child());
    TreeStorage ssub = root_tree.SubtreeFromNodeAsTree(t.position());
    EXPECT_EQ("[5 [6] [10]]", ssub.DebugString());
    ssub.CheckConsistency();
  }
}

// A test where at the inline point of the second_tree into the root_tree, there is no left sibling.
TEST(TreeTest, FullTraversal2) {
  TreeStorage root_tree;
  root_tree.SubstituteNode(0, TreeSubstitution({{7,-1,1,-1}, {-2,-1,-1,2}, {4,-1,-1,-1}}));
  EXPECT_EQ("[7 [-2] [4]]", root_tree.DebugString());

  TreeStorage second_tree(&root_tree, 1);
  second_tree.SubstituteNode(0, TreeSubstitution({{5,-1,1,-1}, {6,-1,-1,2}, {10,-1,-1,-1}}));
  EXPECT_EQ("[5 [6] [10]]", second_tree.DebugString());

  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_FALSE(t.left());  // No left sibling.
    EXPECT_EQ(5, t.node().Type());  // Check that the traversal is unmoved.
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(6, t.node().Type());
    EXPECT_TRUE(t.up());
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
  }
  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.right());
    EXPECT_EQ(4, t.node().Type());
  }

  second_tree.InlineIntoParent(&root_tree);
  EXPECT_EQ("[7 [5 [6] [10]] [4]]", root_tree.DebugString());
  root_tree.CheckConsistency();
}

TEST(TreeTest, FullTraversalSlice) {
  TreeStorage root_tree;
  root_tree.SubstituteNode(0, TreeSubstitution({{7,-1,1,-1}, {3,-1,-1,2}, {4,-1,-1,-1}}));
  EXPECT_EQ("[7 [3] [4]]", root_tree.DebugString());

  TreeStorage second_tree(&root_tree, 1);
  second_tree.SubstituteNode(0, TreeSubstitution({{5,-1,1,-1}, {6,-1,-1,2}, {10,-1,-1,-1}}));
  EXPECT_EQ("[5 [6] [10]]", second_tree.DebugString());

  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(3, t.node().Type());
    EXPECT_TRUE(t.right());
    EXPECT_EQ(4, t.node().Type());
  }

  {
    TreeSlice slice(&root_tree, second_tree.position_in_parent());
    SlicedTreeTraversal t(&second_tree, 0, &slice);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
    EXPECT_TRUE(t.down_first_child());
    // should traverse back to the subtree
    EXPECT_EQ(5, t.node().Type());
    EXPECT_EQ(0, t.node().child_index);
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(6, t.node().Type());
  }

  {
    TreeSlice slice(&root_tree, second_tree.position_in_parent());
    SlicedTreeTraversal t(&second_tree, 0, &slice);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
    // Should slice the right sibling of predicted subtree
    EXPECT_FALSE(t.down_last_child());
  }

}

TEST(TreeTest, FullTraversalSlice3) {
  TreeStorage root_tree;
  root_tree.SubstituteNode(0, TreeSubstitution({{5,-1,1,-1}, {51,-1,2,4}, {511,-1,-1,3},  {512,-1,-1,-1}, {52,-1,-1,-1}}));
  LOG(INFO) << root_tree.DebugString();
  EXPECT_EQ("[5 [51 [511] [512]] [52]]", root_tree.DebugString());

  root_tree.Canonicalize();
  LOG(INFO) << root_tree.DebugString();

  {
    TreeSlice slice(&root_tree, 1, true);
    SlicedTreeTraversal t(&root_tree, 0, &slice);
    EXPECT_EQ(5, t.node().Type());
    EXPECT_TRUE(t.down_first_child());
    // The read should be allowed as it is the begin node and we set allow_reading_type_for_begin_node = true
    EXPECT_EQ(51, t.node().Type());
    // We should not be allowed to go to the right node
    EXPECT_FALSE(t.right());
    EXPECT_EQ(51, t.node().Type());
    EXPECT_FALSE(t.right());
  }

  {
    TreeSlice slice(&root_tree, 2, true);
    SlicedTreeTraversal t(&root_tree, 0, &slice);
    LOG(INFO) << "pos: " << t.position();
    EXPECT_EQ(5, t.node().Type());
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(51, t.node().Type());
    // We should not be allowed to go to the right node
    EXPECT_FALSE(t.right());
    EXPECT_EQ(51, t.node().Type());
  }
}

TEST(TreeTest, FullTraversalSlice2) {
  TreeStorage root_tree;
  root_tree.SubstituteNode(0, TreeSubstitution({{7,-1,1,-1}, {3,-1,-1,2}, {4,-1,-1,-1}}));
  EXPECT_EQ("[7 [3] [4]]", root_tree.DebugString());

  TreeStorage second_tree(&root_tree, 2);
  second_tree.SubstituteNode(0, TreeSubstitution({{5,-1,1,-1}, {6,-1,-1,2}, {10,-1,-1,-1}}));
  EXPECT_EQ("[5 [6] [10]]", second_tree.DebugString());

  {
    FullTreeTraversal t(&second_tree, 0);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(3, t.node().Type());
    EXPECT_TRUE(t.right());
    EXPECT_EQ(4, t.node().Type());
  }

  {
    TreeSlice slice(&root_tree, second_tree.position_in_parent());
    SlicedTreeTraversal t(&second_tree, 0, &slice);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(7, t.node().Type());
    EXPECT_TRUE(t.down_first_child());
    EXPECT_EQ(3, t.node().Type());
    EXPECT_TRUE(t.right());
    EXPECT_EQ(-1, t.node().Type());
  }

}

TEST(TreeTest, FullTraversalSlice4) {

  {
  StringSet ss;
  TreeSubstitution s(
      {
    {ss.addString("Root"), -1, 1, -1},  // 0
    {ss.addString("VarDecls"), -1, 2, 3},  // 1
    {ss.addString("Var"), ss.addString("v1"), -1, -1},  // 2
    {ss.addString("PlusExpr"), -1, 4, -1},  // 3
    {ss.addString("Var"), ss.addString("v1"), -1, 5},  // 4
    {ss.addString("Var"), ss.addString("v2"), -1, -1}  // 5
      });
  TreeStorage tree;
  tree.SubstituteNode(0, s);

  int completion_pos = 4;
  TreeSlice slice(&tree, completion_pos, false);

  {
//    UP DOWN_LAST WRITE_TYPE
    SlicedTreeTraversal t(&tree, completion_pos, &slice);
    EXPECT_TRUE(t.up());
    EXPECT_EQ(3, t.position());
    EXPECT_EQ(ss.findString("PlusExpr"), t.node().Type());
    EXPECT_FALSE(t.down_last_child());
  }
  }

  {
    StringSet ss;
    TreeSubstitution s(
        {
      {ss.addString("Root"), -1, 1, -1},  // 0
      {ss.addString("VarDecls"), -1, 2, 3},  // 1
      {ss.addString("Var"), ss.addString("v1"), -1, -1},  // 2
      {ss.addString("PlusExpr"), -1, 4, -1},  // 3
      {ss.addString("Var"), ss.addString("v1"), -1, -1},  // 4
        });
    TreeStorage tree;
    tree.SubstituteNode(0, s);

    int completion_pos = 4;
    TreeSlice slice(&tree, completion_pos, false);

    {
      //    UP DOWN_LAST WRITE_TYPE
      SlicedTreeTraversal t(&tree, completion_pos, &slice);
      EXPECT_TRUE(t.up());
      EXPECT_EQ(3, t.position());
      EXPECT_EQ(ss.findString("PlusExpr"), t.node().Type());
      EXPECT_FALSE(t.down_last_child());
      EXPECT_EQ(3, t.position());
    }
  }

  {
    StringSet ss;
    TreeSubstitution s(
        {
      {ss.addString("Root"), -1, 1, -1},  // 0
      {ss.addString("VarDecls"), -1, 2, 3},  // 1
      {ss.addString("Var"), ss.addString("v1"), -1, -1},  // 2
      {ss.addString("PlusExpr"), -1, 4, -1},  // 3
      {ss.addString("Var"), ss.addString("v1"), -1, -1},  // 4
        });
    TreeStorage tree;
    tree.SubstituteNode(0, s);

    int completion_pos = 4;
    TreeSlice slice(&tree, completion_pos, false);

    {
      //    UP DOWN_LAST WRITE_TYPE
      SlicedTreeTraversal t(&tree, completion_pos, &slice);
      EXPECT_TRUE(t.up());
      EXPECT_EQ(3, t.position());
      EXPECT_EQ(ss.findString("PlusExpr"), t.node().Type());
      EXPECT_TRUE(t.down_first_child());
      EXPECT_EQ(4, t.position());
    }
  }

}

// JSON Parsing

void PrepareTestProgram(TreeStorage* tree, StringSet* ss, const char* program_json) {
  Json::Reader jsonreader;
  Json::Value v;
  CHECK(jsonreader.parse(program_json, v, false)) << "Could not parse JSON";
  tree->Parse(v, ss);
}

void PrepareTestProgram(TreeStorage* tree, StringSet* ss) {
  const char* program_json =
        "[  { \"id\":0, \"type\":\"Program\", \"children\":[1,4,36] },  { \"id\":1, \"type\":\"VariableDeclaration\", \"children\":[2,3] },  { \"id\":2, \"type\":\"VariableDeclarator\", \"value\":\"map\" },  { \"id\":3, \"type\":\"VariableDeclarator\", \"value\":\"q\" },  { \"id\":4, \"type\":\"FunctionDeclaration\", \"children\":[5] },  { \"id\":5, \"type\":\"BlockStatement\", \"children\":[6,21], \"scope\":[\"mapOptions\"] },  { \"id\":6, \"type\":\"VariableDeclaration\", \"children\":[7] },  { \"id\":7, \"type\":\"VariableDeclarator\", \"value\":\"mapOptions\", \"children\":[8] },  { \"id\":8, \"type\":\"ObjectExpression\", \"children\":[9,11] },  { \"id\":9, \"type\":\"Property\", \"value\":\"zoom\", \"children\":[10] },  { \"id\":10, \"type\":\"Literal\", \"value\":8 },  { \"id\":11, \"type\":\"Property\", \"value\":\"center\", \"children\":[12] },  { \"id\":12, \"type\":\"NewExpression\", \"children\":[13,18,20] },  { \"id\":13, \"type\":\"MemberExpression\", \"children\":[14,17] },  { \"id\":14, \"type\":\"MemberExpression\", \"children\":[15,16] },  { \"id\":15, \"type\":\"Identifier\", \"value\":\"google\" },  { \"id\":16, \"type\":\"Property\", \"value\":\"maps\" },  { \"id\":17, \"type\":\"Property\", \"value\":\"LatLng\" },  { \"id\":18, \"type\":\"UnaryExpression\", \"value\":\"-\", \"children\":[19] },  { \"id\":19, \"type\":\"Literal\", \"value\":34.397 },  { \"id\":20, \"type\":\"Literal\", \"value\":150.644 },  { \"id\":21, \"type\":\"ExpressionStatement\", \"children\":[22] },  { \"id\":22, \"type\":\"AssignmentExpression\", \"children\":[23,24] },  { \"id\":23, \"type\":\"Identifier\", \"value\":\"map\" },  { \"id\":24, \"type\":\"NewExpression\", \"children\":[25,30,35] },  { \"id\":25, \"type\":\"MemberExpression\", \"children\":[26,29] },  { \"id\":26, \"type\":\"MemberExpression\", \"children\":[27,28] },  { \"id\":27, \"type\":\"Identifier\", \"value\":\"google\" },  { \"id\":28, \"type\":\"Property\", \"value\":\"maps\" },  { \"id\":29, \"type\":\"Property\", \"value\":\"Map\" },  { \"id\":30, \"type\":\"CallExpression\", \"children\":[31,34] },  { \"id\":31, \"type\":\"MemberExpression\", \"children\":[32,33] },  { \"id\":32, \"type\":\"Identifier\", \"value\":\"document\" },  { \"id\":33, \"type\":\"Property\", \"value\":\"getElementById\" },  { \"id\":34, \"type\":\"Literal\", \"value\":\"map-canvas\" },  { \"id\":35, \"type\":\"Identifier\", \"value\":\"mapOptions\" },  { \"id\":36, \"type\":\"ExpressionStatement\", \"children\":[37] },  { \"id\":37, \"type\":\"CallExpression\", \"children\":[38,45,46,47] },  { \"id\":38, \"type\":\"MemberExpression\", \"children\":[39,44] },  { \"id\":39, \"type\":\"MemberExpression\", \"children\":[40,43] },  { \"id\":40, \"type\":\"MemberExpression\", \"children\":[41,42] },  { \"id\":41, \"type\":\"Identifier\", \"value\":\"google\" },  { \"id\":42, \"type\":\"Property\", \"value\":\"maps\" },  { \"id\":43, \"type\":\"Property\", \"value\":\"event\" },  { \"id\":44, \"type\":\"Property\", \"value\":\"addDomListener\" },  { \"id\":45, \"type\":\"Identifier\", \"value\":\"window\" },  { \"id\":46, \"type\":\"Literal\", \"value\":\"load\" },  { \"id\":47, \"type\":\"Identifier\", \"value\":\"initialize\" }, 0] ";

  PrepareTestProgram(tree, ss, program_json);
}

TEST(TreeTest, ParsingAndCopying) {
  StringSet ss;
  TreeStorage storage;
  PrepareTestProgram(&storage, &ss);
  storage.CheckConsistency();

  int count = 0;
  for (auto it = FullTreeTraversal(&storage, 0).begin(); !it.at_end(); ++it) {
    ++count;
  }
  EXPECT_EQ(48, count);

  TreeStorage storage2;
  storage2.CheckConsistency();
  auto write_it = LocalEpsTreeTraversal(&storage2, 0).begin();
  for (auto it = LocalEpsTreeTraversal(&storage, 0).begin(); !it.at_end(); ++it) {
    ASSERT_FALSE(write_it.at_end());
    write_it->SetType(it->Type());
    write_it->SetValue(it->Value());
    ++write_it;
  }
  EXPECT_TRUE(write_it.at_end());

  EXPECT_EQ(storage.DebugString(), storage2.DebugString());
}

TEST(TreeTest, CompareTrees) {
  TreeStorage s1;
  s1.SubstituteNode(0, TreeSubstitution({{1,2,-1,-1}}));

  TreeStorage s2;
  s2.SubstituteNode(0, TreeSubstitution({{1,-1,-1,-1}}));

  TreeStorage s3;
  s3.SubstituteNode(0, TreeSubstitution({{2,-1,-1,-1}}));

  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s1, 0), ConstLocalTreeTraversal(&s1, 0), &eq, &diff);
    EXPECT_EQ(2, eq);
    EXPECT_EQ(0, diff);
  }
  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s1, 0), ConstLocalTreeTraversal(&s2, 0), &eq, &diff);
    EXPECT_EQ(1, eq);
    EXPECT_EQ(1, diff);
  }
  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s1, 0), ConstLocalTreeTraversal(&s3, 0), &eq, &diff);
    EXPECT_EQ(0, eq);
    EXPECT_EQ(2, diff);
  }
  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s2, 0), ConstLocalTreeTraversal(&s3, 0), &eq, &diff);
    EXPECT_EQ(1, eq);
    EXPECT_EQ(1, diff);
  }

  TreeStorage s11;
  s11.SubstituteNode(0, TreeSubstitution({{1,2,1,-1}, {1,2,-1,-1}}));
  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s1, 0), ConstLocalTreeTraversal(&s11, 0), &eq, &diff);
    EXPECT_EQ(2, eq);
    EXPECT_EQ(2, diff);
    CompareTrees(ConstLocalTreeTraversal(&s11, 0), ConstLocalTreeTraversal(&s1, 0), &eq, &diff);
    EXPECT_EQ(2, eq);
    EXPECT_EQ(2, diff);
  }
  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s2, 0), ConstLocalTreeTraversal(&s11, 0), &eq, &diff);
    EXPECT_EQ(1, eq);
    EXPECT_EQ(3, diff);
    CompareTrees(ConstLocalTreeTraversal(&s11, 0), ConstLocalTreeTraversal(&s2, 0), &eq, &diff);
    EXPECT_EQ(1, eq);
    EXPECT_EQ(3, diff);
  }

  TreeStorage s21;
  s21.SubstituteNode(0, TreeSubstitution({{1,2,1,-1}, {1,2,-1,2}, {3,4,-1,-1}}));
  TreeStorage s22;
  s22.SubstituteNode(0, TreeSubstitution({{1,2,1,-1}, {0,0,-1,2}, {3,4,-1,-1}}));
  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s21, 0), ConstLocalTreeTraversal(&s22, 0), &eq, &diff);
    EXPECT_EQ(4, eq);
    EXPECT_EQ(2, diff);
  }
  {
    int eq, diff;
    CompareTrees(ConstLocalTreeTraversal(&s1, 0), ConstLocalTreeTraversal(&s22, 0), &eq, &diff);
    EXPECT_EQ(2, eq);
    EXPECT_EQ(4, diff);
    CompareTrees(ConstLocalTreeTraversal(&s22, 0), ConstLocalTreeTraversal(&s1, 0), &eq, &diff);
    EXPECT_EQ(2, eq);
    EXPECT_EQ(4, diff);
  }
}

TEST(TreeTest, TreeHashing) {
  TreeStorage s21;
  s21.SubstituteNode(0, TreeSubstitution({{1,2,1,-1}, {1,2,-1,2}, {3,4,-1,-1}}));
  TreeStorage s22;
  s22.SubstituteNode(0, TreeSubstitution({{1,2,1,-1}, {0,0,-1,2}, {3,4,-1,-1}}));
  TreeStorage s23;
  s23.SubstituteNode(0, TreeSubstitution({{1,2,1,-1}, {1,2,-1,2}, {3,4,-1,-1}}));

  EXPECT_FALSE(s21 == s22);
  EXPECT_TRUE(s21 == s23);
  EXPECT_TRUE(std::hash<TreeStorage>()(s21) == std::hash<TreeStorage>()(s23));
  EXPECT_FALSE(s22 == s23);

  std::unordered_set<TreeStorage> tree_set;
  EXPECT_TRUE(tree_set.insert(s21).second);
  EXPECT_TRUE(tree_set.insert(s22).second);
  EXPECT_FALSE(tree_set.insert(s23).second);
}

TEST(TreeTest, TreeSubstitution) {
  TreeStorage t1;
  EXPECT_FALSE(t1.CanSubstituteNodeType(0, -1));
  EXPECT_TRUE(t1.CanSubstituteNodeType(0, 5));
  t1.SubstituteNodeType(0, 5);
  EXPECT_EQ("[5:-2 [-2:-2]]", t1.DebugString());

  TreeStorage t2 = t1;
  EXPECT_FALSE(t2.CanSubstituteNodeType(0, -1));
  EXPECT_TRUE(t2.CanSubstituteNodeType(1, -1));
  t2.SubstituteNodeType(1, -1);
  EXPECT_EQ("[5:-2]", t2.DebugString());

  EXPECT_TRUE(t1.CanSubstituteNodeType(1, 1));
  t1.SubstituteNodeType(1, 1);
  EXPECT_EQ("[5:-2 [1:-2 [-2:-2]] [-2:-2]]", t1.DebugString());

  {
    ConstLocalTreeTraversal t(&t1, 0);
    EXPECT_TRUE(t.down_first_child());
    EXPECT_TRUE(t.down_first_child());
    EXPECT_FALSE(t.down_first_child());
    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), 5));
    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), -1));
    t1.SubstituteNodeType(t.position(), -1);
    EXPECT_EQ("[5:-2 [1:-2] [-2:-2]]", t1.DebugString());
  }
  {
    ConstLocalTreeTraversal t(&t1, 0);
    EXPECT_TRUE(t.down_first_child());
    EXPECT_FALSE(t.down_first_child());
    EXPECT_TRUE(t.right());
    EXPECT_FALSE(t.right());
    EXPECT_FALSE(t.down_first_child());
    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), 5));
    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), -1));
    t1.SubstituteNodeType(t.position(), 7);
    EXPECT_EQ("[5:-2 [1:-2] [7:-2 [-2:-2]] [-2:-2]]", t1.DebugString());
  }
  {
    ConstLocalTreeTraversal t(&t1, 0);
    EXPECT_TRUE(t.down_first_child());
    EXPECT_FALSE(t.down_first_child());
    EXPECT_TRUE(t.right());
    EXPECT_TRUE(t.down_first_child());
    EXPECT_FALSE(t.down_first_child());

    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), 5));
    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), -1));
    t1.SubstituteNodeType(t.position(), -1);
    EXPECT_EQ("[5:-2 [1:-2] [7:-2] [-2:-2]]", t1.DebugString());
  }
  {
    ConstLocalTreeTraversal t(&t1, 0);
    EXPECT_TRUE(t.down_first_child());
    EXPECT_FALSE(t.down_first_child());
    EXPECT_TRUE(t.right());
    EXPECT_TRUE(t.right());
    EXPECT_FALSE(t.right());
    EXPECT_FALSE(t.down_first_child());

    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), 5));
    EXPECT_TRUE(t1.CanSubstituteNodeType(t.position(), -1));
    t1.SubstituteNodeType(t.position(), -1);
    EXPECT_EQ("[5:-2 [1:-2] [7:-2]]", t1.DebugString());
  }
}

void normalize_code(std::string* code){
  code->erase(std::remove_if(code->begin(), code->end(), ::isspace), code->end());
  code->erase(std::remove_if(code->begin(), code->end(), [](char c) {return c == ';'; }), code->end());
}

TEST(TreeTestJSLabels, TreeToJavascript) {
  StringSet ss;
  TreeStorage storage;
  const char* program_json =
      "[ { \"id\":0, \"type\":\"Program\", \"children\":[1,4] }, { \"id\":1, \"type\":\"VariableDeclaration\", \"children\":[2,3] }, { \"id\":2, \"type\":\"VariableDeclarator\", \"value\":\"i\" }, { \"id\":3, \"type\":\"VariableDeclarator\", \"value\":\"j\" }, { \"id\":4, \"type\":\"LabeledStatement\", \"value\":\"loop1\", \"children\":[5] }, { \"id\":5, \"type\":\"ForStatement\", \"children\":[6,9,12,14] }, { \"id\":6, \"type\":\"AssignmentExpression\", \"children\":[7,8] }, { \"id\":7, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":8, \"type\":\"LiteralNumber\", \"value\":\"0\" }, { \"id\":9, \"type\":\"BinaryExpression\", \"value\":\"<\", \"children\":[10,11] }, { \"id\":10, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":11, \"type\":\"LiteralNumber\", \"value\":\"3\" }, { \"id\":12, \"type\":\"UpdateExpression\", \"value\":\"?++\", \"children\":[13] }, { \"id\":13, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":14, \"type\":\"BlockStatement\", \"children\":[15,54] }, { \"id\":15, \"type\":\"LabeledStatement\", \"value\":\"loop2\", \"children\":[16] }, { \"id\":16, \"type\":\"ForStatement\", \"children\":[17,20,23,25] }, { \"id\":17, \"type\":\"AssignmentExpression\", \"children\":[18,19] }, { \"id\":18, \"type\":\"Identifier\", \"value\":\"j\" }, { \"id\":19, \"type\":\"LiteralNumber\", \"value\":\"0\" }, { \"id\":20, \"type\":\"BinaryExpression\", \"value\":\"<\", \"children\":[21,22] }, { \"id\":21, \"type\":\"Identifier\", \"value\":\"j\" }, { \"id\":22, \"type\":\"LiteralNumber\", \"value\":\"3\" }, { \"id\":23, \"type\":\"UpdateExpression\", \"value\":\"?++\", \"children\":[24] }, { \"id\":24, \"type\":\"Identifier\", \"value\":\"j\" }, { \"id\":25, \"type\":\"BlockStatement\", \"children\":[26,42] }, { \"id\":26, \"type\":\"IfStatement\", \"children\":[27,34,36] }, { \"id\":27, \"type\":\"LogicalExpression\", \"value\":\"&&\", \"children\":[28,31] }, { \"id\":28, \"type\":\"BinaryExpression\", \"value\":\"==\", \"children\":[29,30] }, { \"id\":29, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":30, \"type\":\"LiteralNumber\", \"value\":\"1\" }, { \"id\":31, \"type\":\"BinaryExpression\", \"value\":\"==\", \"children\":[32,33] }, { \"id\":32, \"type\":\"Identifier\", \"value\":\"j\" }, { \"id\":33, \"type\":\"LiteralNumber\", \"value\":\"1\" }, { \"id\":34, \"type\":\"BlockStatement\", \"children\":[35] }, { \"id\":35, \"type\":\"ContinueStatement\", \"value\":\"loop1\" }, { \"id\":36, \"type\":\"IfStatement\", \"children\":[37,40] }, { \"id\":37, \"type\":\"BinaryExpression\", \"value\":\">\", \"children\":[38,39] }, { \"id\":38, \"type\":\"Identifier\", \"value\":\"j\" }, { \"id\":39, \"type\":\"LiteralNumber\", \"value\":\"2\" }, { \"id\":40, \"type\":\"BlockStatement\", \"children\":[41] }, { \"id\":41, \"type\":\"BreakStatement\", \"value\":\"loop2\" }, { \"id\":42, \"type\":\"ExpressionStatement\", \"children\":[43] }, { \"id\":43, \"type\":\"CallExpression\", \"children\":[44,47] }, { \"id\":44, \"type\":\"MemberExpression\", \"children\":[45,46] }, { \"id\":45, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":46, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":47, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[48,53] }, { \"id\":48, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[49,52] }, { \"id\":49, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[50,51] }, { \"id\":50, \"type\":\"LiteralString\", \"value\":\"i = \" }, { \"id\":51, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":52, \"type\":\"LiteralString\", \"value\":\", j = \" }, { \"id\":53, \"type\":\"Identifier\", \"value\":\"j\" }, { \"id\":54, \"type\":\"ContinueStatement\" }, 0]";

  PrepareTestProgram(&storage, &ss, program_json);

  std::string generated_code = storage.DebugStringAsSource(&ss);
  std::string original_code =
      "var i, j;"
      ""
      "loop1:"
      "for (i = 0; i < 3; i++) {      "
      "   loop2:"
      "   for (j = 0; j < 3; j++) {   "
      "      if (i == 1 && j == 1) {"
      "        continue loop1;"
      "      } else if (j > 2) {"
      "       break loop2;"
      "      }"
      "      console.log(\"i = \" + i + \", j = \" + j);"
      "   }"
      "   continue;"
      "}";

  normalize_code(&original_code);
  normalize_code(&generated_code);

  EXPECT_EQ(original_code, generated_code);
}

TEST(TreeTestJSSwitch, TreeToJavascript) {
  StringSet ss;
  TreeStorage storage;
  const char* program_json =
      "[ { \"id\":0, \"type\":\"Program\", \"children\":[1] }, { \"id\":1, \"type\":\"SwitchStatement\", \"children\":[2,3,13,16,26] }, { \"id\":2, \"type\":\"Identifier\", \"value\":\"expr\" }, { \"id\":3, \"type\":\"SwitchCase\", \"children\":[4,5] }, { \"id\":4, \"type\":\"LiteralString\", \"value\":\"Oranges\" }, { \"id\":5, \"type\":\"BlockStatement\", \"children\":[6,12] }, { \"id\":6, \"type\":\"ExpressionStatement\", \"children\":[7] }, { \"id\":7, \"type\":\"CallExpression\", \"children\":[8,11] }, { \"id\":8, \"type\":\"MemberExpression\", \"children\":[9,10] }, { \"id\":9, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":10, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":11, \"type\":\"LiteralString\", \"value\":\"Oranges are $0.59 a pound.\" }, { \"id\":12, \"type\":\"BreakStatement\" }, { \"id\":13, \"type\":\"SwitchCase\", \"children\":[14,15] }, { \"id\":14, \"type\":\"LiteralString\", \"value\":\"Mangoes\" }, { \"id\":15, \"type\":\"BlockStatement\" }, { \"id\":16, \"type\":\"SwitchCase\", \"children\":[17,18] }, { \"id\":17, \"type\":\"LiteralString\", \"value\":\"Papayas\" }, { \"id\":18, \"type\":\"BlockStatement\", \"children\":[19,25] }, { \"id\":19, \"type\":\"ExpressionStatement\", \"children\":[20] }, { \"id\":20, \"type\":\"CallExpression\", \"children\":[21,24] }, { \"id\":21, \"type\":\"MemberExpression\", \"children\":[22,23] }, { \"id\":22, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":23, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":24, \"type\":\"LiteralString\", \"value\":\"Mangoes and papayas are $2.79 a pound.\" }, { \"id\":25, \"type\":\"BreakStatement\" }, { \"id\":26, \"type\":\"SwitchCase\", \"children\":[27,28] }, { \"id\":27, \"type\":\"EmptyStatement\" }, { \"id\":28, \"type\":\"BlockStatement\", \"children\":[29] }, { \"id\":29, \"type\":\"ExpressionStatement\", \"children\":[30] }, { \"id\":30, \"type\":\"CallExpression\", \"children\":[31,34] }, { \"id\":31, \"type\":\"MemberExpression\", \"children\":[32,33] }, { \"id\":32, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":33, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":34, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[35,38] }, { \"id\":35, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[36,37] }, { \"id\":36, \"type\":\"LiteralString\", \"value\":\"Sorry, we are out of \" }, { \"id\":37, \"type\":\"Identifier\", \"value\":\"expr\" }, { \"id\":38, \"type\":\"LiteralString\", \"value\":\".\" }, 0]";

  PrepareTestProgram(&storage, &ss, program_json);

  std::string generated_code = storage.DebugStringAsSource(&ss);
  std::string original_code =
      "switch (expr) {"
      "  case \"Oranges\":"
      "    console.log(\"Oranges are $0.59 a pound.\");"
      "    break;  "
      "  case \"Mangoes\":"
      "  case \"Papayas\":"
      "    console.log(\"Mangoes and papayas are $2.79 a pound.\");"
      "    break;"
      "  default:"
      "    console.log(\"Sorry, we are out of \" + expr + \".\");"
      "}";

  normalize_code(&original_code);
  normalize_code(&generated_code);

  EXPECT_EQ(original_code, generated_code);
}

TEST(TreeTestJSTryCatch, TreeToJavascript) {
  StringSet ss;
  TreeStorage storage;
  const char* program_json =
      "[ { \"id\":0, \"type\":\"Program\", \"children\":[1,26,40] }, { \"id\":1, \"type\":\"TryStatement\", \"children\":[2,7,19] }, { \"id\":2, \"type\":\"BlockStatement\", \"children\":[3] }, { \"id\":3, \"type\":\"ThrowStatement\", \"children\":[4] }, { \"id\":4, \"type\":\"NewExpression\", \"children\":[5,6] }, { \"id\":5, \"type\":\"Identifier\", \"value\":\"Error\" }, { \"id\":6, \"type\":\"LiteralString\", \"value\":\"oops\" }, { \"id\":7, \"type\":\"CatchClause\", \"children\":[8,9] }, { \"id\":8, \"type\":\"Identifier\", \"value\":\"ex\" }, { \"id\":9, \"type\":\"BlockStatement\", \"children\":[10] }, { \"id\":10, \"type\":\"ExpressionStatement\", \"children\":[11] }, { \"id\":11, \"type\":\"CallExpression\", \"children\":[12,15,16] }, { \"id\":12, \"type\":\"MemberExpression\", \"children\":[13,14] }, { \"id\":13, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":14, \"type\":\"Property\", \"value\":\"error\" }, { \"id\":15, \"type\":\"LiteralString\", \"value\":\"inner\" }, { \"id\":16, \"type\":\"MemberExpression\", \"children\":[17,18] }, { \"id\":17, \"type\":\"Identifier\", \"value\":\"ex\" }, { \"id\":18, \"type\":\"Property\", \"value\":\"message\" }, { \"id\":19, \"type\":\"BlockStatement\", \"children\":[20] }, { \"id\":20, \"type\":\"ExpressionStatement\", \"children\":[21] }, { \"id\":21, \"type\":\"CallExpression\", \"children\":[22,25] }, { \"id\":22, \"type\":\"MemberExpression\", \"children\":[23,24] }, { \"id\":23, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":24, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":25, \"type\":\"LiteralString\", \"value\":\"finally\" }, { \"id\":26, \"type\":\"TryStatement\", \"children\":[27,32,33] }, { \"id\":27, \"type\":\"BlockStatement\", \"children\":[28] }, { \"id\":28, \"type\":\"ThrowStatement\", \"children\":[29] }, { \"id\":29, \"type\":\"NewExpression\", \"children\":[30,31] }, { \"id\":30, \"type\":\"Identifier\", \"value\":\"Error\" }, { \"id\":31, \"type\":\"LiteralString\", \"value\":\"no catch\" }, { \"id\":32, \"type\":\"EmptyStatement\" }, { \"id\":33, \"type\":\"BlockStatement\", \"children\":[34] }, { \"id\":34, \"type\":\"ExpressionStatement\", \"children\":[35] }, { \"id\":35, \"type\":\"CallExpression\", \"children\":[36,39] }, { \"id\":36, \"type\":\"MemberExpression\", \"children\":[37,38] }, { \"id\":37, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":38, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":39, \"type\":\"LiteralString\", \"value\":\"finally\" }, { \"id\":40, \"type\":\"TryStatement\", \"children\":[41,46,58] }, { \"id\":41, \"type\":\"BlockStatement\", \"children\":[42] }, { \"id\":42, \"type\":\"ThrowStatement\", \"children\":[43] }, { \"id\":43, \"type\":\"NewExpression\", \"children\":[44,45] }, { \"id\":44, \"type\":\"Identifier\", \"value\":\"Error\" }, { \"id\":45, \"type\":\"LiteralString\", \"value\":\"no finally\" }, { \"id\":46, \"type\":\"CatchClause\", \"children\":[47,48] }, { \"id\":47, \"type\":\"Identifier\", \"value\":\"ex\" }, { \"id\":48, \"type\":\"BlockStatement\", \"children\":[49] }, { \"id\":49, \"type\":\"ExpressionStatement\", \"children\":[50] }, { \"id\":50, \"type\":\"CallExpression\", \"children\":[51,54,55] }, { \"id\":51, \"type\":\"MemberExpression\", \"children\":[52,53] }, { \"id\":52, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":53, \"type\":\"Property\", \"value\":\"error\" }, { \"id\":54, \"type\":\"LiteralString\", \"value\":\"inner\" }, { \"id\":55, \"type\":\"MemberExpression\", \"children\":[56,57] }, { \"id\":56, \"type\":\"Identifier\", \"value\":\"ex\" }, { \"id\":57, \"type\":\"Property\", \"value\":\"message\" }, { \"id\":58, \"type\":\"EmptyStatement\" }, 0]";

  PrepareTestProgram(&storage, &ss, program_json);

  std::string generated_code = storage.DebugStringAsSource(&ss);
  std::string original_code =
      "try {"
      "    throw new Error(\"oops\");"
      "} catch (ex) {"
      "    console.error(\"inner\", ex.message);"
      "} finally {"
      "    console.log(\"finally\");"
      "}"
      ""
      "try {"
      "    throw new Error(\"no catch\");"
      "} finally {"
      "    console.log(\"finally\");"
      "} "
      ""
      "try {"
      "    throw new Error(\"no finally\");"
      "} catch (ex) {"
      "    console.error(\"inner\", ex.message);"
      "} ";

  normalize_code(&original_code);
  normalize_code(&generated_code);

  EXPECT_EQ(original_code, generated_code);
}

TEST(TreeTestWithExpandedLiteral, TreeToJavascript) {
  StringSet ss;
  TreeStorage storage;
  const char* program_json =
      "[ { \"id\":0, \"type\":\"Program\", \"children\":[1,16,30,38,54,57,73,78,83,96,101,117,125,136,144,156,166,178,188,191] }, { \"id\":1, \"type\":\"VariableDeclaration\", \"children\":[2] }, { \"id\":2, \"type\":\"VariableDeclarator\", \"value\":\"x\", \"children\":[3] }, { \"id\":3, \"type\":\"FunctionExpression\", \"children\":[4,5,6,7] }, { \"id\":4, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":5, \"type\":\"Identifier\", \"value\":\"a\" }, { \"id\":6, \"type\":\"Identifier\", \"value\":\"b\" }, { \"id\":7, \"type\":\"BlockStatement\", \"children\":[8] }, { \"id\":8, \"type\":\"ExpressionStatement\", \"children\":[9] }, { \"id\":9, \"type\":\"CallExpression\", \"children\":[10,13] }, { \"id\":10, \"type\":\"MemberExpression\", \"children\":[11,12] }, { \"id\":11, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":12, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":13, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[14,15] }, { \"id\":14, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":15, \"type\":\"Identifier\", \"value\":\"a\" }, { \"id\":16, \"type\":\"FunctionDeclaration\", \"children\":[17,18,19,20,21] }, { \"id\":17, \"type\":\"Identifier\", \"value\":\"x\" }, { \"id\":18, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":19, \"type\":\"Identifier\", \"value\":\"a\" }, { \"id\":20, \"type\":\"Identifier\", \"value\":\"b\" }, { \"id\":21, \"type\":\"BlockStatement\", \"children\":[22] }, { \"id\":22, \"type\":\"ExpressionStatement\", \"children\":[23] }, { \"id\":23, \"type\":\"CallExpression\", \"children\":[24,27] }, { \"id\":24, \"type\":\"MemberExpression\", \"children\":[25,26] }, { \"id\":25, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":26, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":27, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[28,29] }, { \"id\":28, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":29, \"type\":\"Identifier\", \"value\":\"b\" }, { \"id\":30, \"type\":\"ExpressionStatement\", \"children\":[31] }, { \"id\":31, \"type\":\"CallExpression\", \"children\":[32,33,34,35] }, { \"id\":32, \"type\":\"Identifier\", \"value\":\"x\" }, { \"id\":33, \"type\":\"LiteralRegExp\", \"value\":\"/^f/\" }, { \"id\":34, \"type\":\"LiteralString\", \"value\":\"g\" }, { \"id\":35, \"type\":\"FunctionExpression\", \"children\":[36,37] }, { \"id\":36, \"type\":\"Identifier\", \"value\":\"c\" }, { \"id\":37, \"type\":\"BlockStatement\" }, { \"id\":38, \"type\":\"ForStatement\", \"children\":[39,42,45,47] }, { \"id\":39, \"type\":\"VariableDeclaration\", \"children\":[40] }, { \"id\":40, \"type\":\"VariableDeclarator\", \"value\":\"i\", \"children\":[41] }, { \"id\":41, \"type\":\"LiteralNumber\", \"value\":\"0\" }, { \"id\":42, \"type\":\"BinaryExpression\", \"value\":\"<\", \"children\":[43,44] }, { \"id\":43, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":44, \"type\":\"LiteralNumber\", \"value\":\"10\" }, { \"id\":45, \"type\":\"UpdateExpression\", \"value\":\"++?\", \"children\":[46] }, { \"id\":46, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":47, \"type\":\"BlockStatement\", \"children\":[48] }, { \"id\":48, \"type\":\"ExpressionStatement\", \"children\":[49] }, { \"id\":49, \"type\":\"CallExpression\", \"children\":[50,53] }, { \"id\":50, \"type\":\"MemberExpression\", \"children\":[51,52] }, { \"id\":51, \"type\":\"Identifier\", \"value\":\"log\" }, { \"id\":52, \"type\":\"Property\", \"value\":\"console\" }, { \"id\":53, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":54, \"type\":\"VariableDeclaration\", \"children\":[55] }, { \"id\":55, \"type\":\"VariableDeclarator\", \"value\":\"i\", \"children\":[56] }, { \"id\":56, \"type\":\"LiteralNumber\", \"value\":\"0\" }, { \"id\":57, \"type\":\"ForStatement\", \"children\":[58,59,62,63] }, { \"id\":58, \"type\":\"EmptyStatement\" }, { \"id\":59, \"type\":\"BinaryExpression\", \"value\":\"<\", \"children\":[60,61] }, { \"id\":60, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":61, \"type\":\"LiteralNumber\", \"value\":\"10\" }, { \"id\":62, \"type\":\"EmptyStatement\" }, { \"id\":63, \"type\":\"BlockStatement\", \"children\":[64,70] }, { \"id\":64, \"type\":\"ExpressionStatement\", \"children\":[65] }, { \"id\":65, \"type\":\"CallExpression\", \"children\":[66,69] }, { \"id\":66, \"type\":\"MemberExpression\", \"children\":[67,68] }, { \"id\":67, \"type\":\"Identifier\", \"value\":\"log\" }, { \"id\":68, \"type\":\"Property\", \"value\":\"console\" }, { \"id\":69, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":70, \"type\":\"ExpressionStatement\", \"children\":[71] }, { \"id\":71, \"type\":\"UpdateExpression\", \"value\":\"++?\", \"children\":[72] }, { \"id\":72, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":73, \"type\":\"VariableDeclaration\", \"children\":[74] }, { \"id\":74, \"type\":\"VariableDeclarator\", \"value\":\"jasmine\", \"children\":[75] }, { \"id\":75, \"type\":\"CallExpression\", \"children\":[76,77] }, { \"id\":76, \"type\":\"Identifier\", \"value\":\"require\" }, { \"id\":77, \"type\":\"LiteralString\", \"value\":\"jasmine-node\" }, { \"id\":78, \"type\":\"VariableDeclaration\", \"children\":[79] }, { \"id\":79, \"type\":\"VariableDeclarator\", \"value\":\"sys\", \"children\":[80] }, { \"id\":80, \"type\":\"CallExpression\", \"children\":[81,82] }, { \"id\":81, \"type\":\"Identifier\", \"value\":\"require\" }, { \"id\":82, \"type\":\"LiteralString\", \"value\":\"sys\" }, { \"id\":83, \"type\":\"ForInStatement\", \"children\":[84,86,87] }, { \"id\":84, \"type\":\"VariableDeclaration\", \"children\":[85] }, { \"id\":85, \"type\":\"VariableDeclarator\", \"value\":\"key\" }, { \"id\":86, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":87, \"type\":\"BlockStatement\", \"children\":[88] }, { \"id\":88, \"type\":\"ExpressionStatement\", \"children\":[89] }, { \"id\":89, \"type\":\"AssignmentExpression\", \"children\":[90,93] }, { \"id\":90, \"type\":\"ArrayAccess\", \"children\":[91,92] }, { \"id\":91, \"type\":\"Identifier\", \"value\":\"global\" }, { \"id\":92, \"type\":\"Property\", \"value\":\"key\" }, { \"id\":93, \"type\":\"ArrayAccess\", \"children\":[94,95] }, { \"id\":94, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":95, \"type\":\"Property\", \"value\":\"key\" }, { \"id\":96, \"type\":\"IfStatement\", \"children\":[97,100] }, { \"id\":97, \"type\":\"BinaryExpression\", \"value\":\"==\", \"children\":[98,99] }, { \"id\":98, \"type\":\"Identifier\", \"value\":\"sys\" }, { \"id\":99, \"type\":\"LiteralBoolean\", \"value\":\"true\" }, { \"id\":100, \"type\":\"BlockStatement\" }, { \"id\":101, \"type\":\"IfStatement\", \"children\":[102,109,110] }, { \"id\":102, \"type\":\"LogicalExpression\", \"value\":\"&&\", \"children\":[103,106] }, { \"id\":103, \"type\":\"BinaryExpression\", \"value\":\"!=\", \"children\":[104,105] }, { \"id\":104, \"type\":\"Identifier\", \"value\":\"sys\" }, { \"id\":105, \"type\":\"LiteralNumber\", \"value\":\"0\" }, { \"id\":106, \"type\":\"BinaryExpression\", \"value\":\">\", \"children\":[107,108] }, { \"id\":107, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":108, \"type\":\"LiteralNumber\", \"value\":\"10\" }, { \"id\":109, \"type\":\"BlockStatement\" }, { \"id\":110, \"type\":\"BlockStatement\", \"children\":[111] }, { \"id\":111, \"type\":\"ExpressionStatement\", \"children\":[112] }, { \"id\":112, \"type\":\"CallExpression\", \"children\":[113,116] }, { \"id\":113, \"type\":\"MemberExpression\", \"children\":[114,115] }, { \"id\":114, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":115, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":116, \"type\":\"LiteralString\", \"value\":\"hello\" }, { \"id\":117, \"type\":\"ExpressionStatement\", \"children\":[118] }, { \"id\":118, \"type\":\"AssignmentExpression\", \"children\":[119,120] }, { \"id\":119, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":120, \"type\":\"MemberExpression\", \"children\":[121,124] }, { \"id\":121, \"type\":\"MemberExpression\", \"children\":[122,123] }, { \"id\":122, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":123, \"type\":\"Property\", \"value\":\"walk\" }, { \"id\":124, \"type\":\"Property\", \"value\":\"root\" }, { \"id\":125, \"type\":\"WhileStatement\", \"children\":[126,129] }, { \"id\":126, \"type\":\"BinaryExpression\", \"value\":\"!=\", \"children\":[127,128] }, { \"id\":127, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":128, \"type\":\"LiteralNull\", \"value\":\"null\" }, { \"id\":129, \"type\":\"BlockStatement\", \"children\":[130] }, { \"id\":130, \"type\":\"ExpressionStatement\", \"children\":[131] }, { \"id\":131, \"type\":\"AssignmentExpression\", \"children\":[132,133] }, { \"id\":132, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":133, \"type\":\"MemberExpression\", \"children\":[134,135] }, { \"id\":134, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":135, \"type\":\"Property\", \"value\":\"parent\" }, { \"id\":136, \"type\":\"ExpressionStatement\", \"children\":[137] }, { \"id\":137, \"type\":\"AssignmentExpression\", \"children\":[138,139] }, { \"id\":138, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":139, \"type\":\"MemberExpression\", \"children\":[140,143] }, { \"id\":140, \"type\":\"MemberExpression\", \"children\":[141,142] }, { \"id\":141, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":142, \"type\":\"Property\", \"value\":\"walk\" }, { \"id\":143, \"type\":\"Property\", \"value\":\"root\" }, { \"id\":144, \"type\":\"DoWhileStatement\", \"children\":[145,148] }, { \"id\":145, \"type\":\"BinaryExpression\", \"value\":\"!=\", \"children\":[146,147] }, { \"id\":146, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":147, \"type\":\"Identifier\", \"value\":\"undefined\" }, { \"id\":148, \"type\":\"BlockStatement\", \"children\":[149,155] }, { \"id\":149, \"type\":\"ExpressionStatement\", \"children\":[150] }, { \"id\":150, \"type\":\"AssignmentExpression\", \"children\":[151,152] }, { \"id\":151, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":152, \"type\":\"MemberExpression\", \"children\":[153,154] }, { \"id\":153, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":154, \"type\":\"Property\", \"value\":\"parent\" }, { \"id\":155, \"type\":\"ContinueStatement\" }, { \"id\":156, \"type\":\"ExpressionStatement\", \"children\":[157] }, { \"id\":157, \"type\":\"CallExpression\", \"children\":[158,161] }, { \"id\":158, \"type\":\"MemberExpression\", \"children\":[159,160] }, { \"id\":159, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":160, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":161, \"type\":\"BinaryExpression\", \"value\":\"===\", \"children\":[162,165] }, { \"id\":162, \"type\":\"MemberExpression\", \"children\":[163,164] }, { \"id\":163, \"type\":\"ThisExpression\" }, { \"id\":164, \"type\":\"Property\", \"value\":\"document\" }, { \"id\":165, \"type\":\"Identifier\", \"value\":\"document\" }, { \"id\":166, \"type\":\"VariableDeclaration\", \"children\":[167] }, { \"id\":167, \"type\":\"VariableDeclarator\", \"value\":\"o\", \"children\":[168] }, { \"id\":168, \"type\":\"ObjectExpression\", \"children\":[169,171] }, { \"id\":169, \"type\":\"Property\", \"value\":\"prop\", \"children\":[170] }, { \"id\":170, \"type\":\"LiteralNumber\", \"value\":\"37\" }, { \"id\":171, \"type\":\"Property\", \"value\":\"f\", \"children\":[172] }, { \"id\":172, \"type\":\"FunctionExpression\", \"children\":[173] }, { \"id\":173, \"type\":\"BlockStatement\", \"children\":[174] }, { \"id\":174, \"type\":\"ReturnStatement\", \"children\":[175] }, { \"id\":175, \"type\":\"MemberExpression\", \"children\":[176,177] }, { \"id\":176, \"type\":\"ThisExpression\" }, { \"id\":177, \"type\":\"Property\", \"value\":\"prop\" }, { \"id\":178, \"type\":\"VariableDeclaration\", \"children\":[179] }, { \"id\":179, \"type\":\"VariableDeclarator\", \"value\":\"elvisLives\", \"children\":[180] }, { \"id\":180, \"type\":\"ConditionalExpression\", \"children\":[181,186,187] }, { \"id\":181, \"type\":\"BinaryExpression\", \"value\":\">\", \"children\":[182,185] }, { \"id\":182, \"type\":\"MemberExpression\", \"children\":[183,184] }, { \"id\":183, \"type\":\"Identifier\", \"value\":\"Math\" }, { \"id\":184, \"type\":\"Property\", \"value\":\"PI\" }, { \"id\":185, \"type\":\"LiteralNumber\", \"value\":\"4\" }, { \"id\":186, \"type\":\"LiteralString\", \"value\":\"Yep\" }, { \"id\":187, \"type\":\"LiteralString\", \"value\":\"Nope\" }, { \"id\":188, \"type\":\"VariableDeclaration\", \"children\":[189,190] }, { \"id\":189, \"type\":\"VariableDeclarator\", \"value\":\"index\" }, { \"id\":190, \"type\":\"VariableDeclarator\", \"value\":\"len\" }, { \"id\":191, \"type\":\"ForStatement\", \"children\":[192,201,204,206] }, { \"id\":192, \"type\":\"SequenceExpression\", \"children\":[193,196] }, { \"id\":193, \"type\":\"AssignmentExpression\", \"children\":[194,195] }, { \"id\":194, \"type\":\"Identifier\", \"value\":\"index\" }, { \"id\":195, \"type\":\"LiteralNumber\", \"value\":\"0\" }, { \"id\":196, \"type\":\"AssignmentExpression\", \"children\":[197,198] }, { \"id\":197, \"type\":\"Identifier\", \"value\":\"len\" }, { \"id\":198, \"type\":\"MemberExpression\", \"children\":[199,200] }, { \"id\":199, \"type\":\"Identifier\", \"value\":\"list\" }, { \"id\":200, \"type\":\"Property\", \"value\":\"length\" }, { \"id\":201, \"type\":\"BinaryExpression\", \"value\":\"<\", \"children\":[202,203] }, { \"id\":202, \"type\":\"Identifier\", \"value\":\"index\" }, { \"id\":203, \"type\":\"Identifier\", \"value\":\"len\" }, { \"id\":204, \"type\":\"UpdateExpression\", \"value\":\"++?\", \"children\":[205] }, { \"id\":205, \"type\":\"Identifier\", \"value\":\"index\" }, { \"id\":206, \"type\":\"BlockStatement\", \"children\":[207] }, { \"id\":207, \"type\":\"BreakStatement\" }, 0]";

  PrepareTestProgram(&storage, &ss, program_json);

  std::string generated_code = storage.DebugStringAsSource(&ss);
  std::string original_code =
      "var x = function (t,a,b) {"
      "          console.log(t + a);"
      "      };"
      ""
      "      function x(t,a,b){"
      "          console.log(t + b);"
      "      }"
      ""
      "      x(/^f/, \"g\", function(c){});"
      ""
      "      for (var i = 0; i < 10; ++i) {"
      "          log.console(i);"
      "      }"
      ""
      "      var i = 0;"
      "      for (; i < 10; ) {"
      "          log.console(i);"
      "          ++i;"
      "      }"
      ""
      "      var jasmine = require(\"jasmine-node\");"
      "      var sys = require(\"sys\");"
      ""
      "      for(var key in jasmine) {"
      "        global[key] = jasmine[key];"
      "      }"
      ""
      "      if (sys == true) {"
      ""
      "      }"
      ""
      "      if (sys != 0 && i > 10) {"
      ""
      "      } else {"
      "          console.log(\"hello\");"
      "      }"
      ""
      "      node = jasmine.walk.root;"
      "      while (node != null) {"
      "          node = node.parent;"
      "      }"
      ""
      "      node = jasmine.walk.root;"
      "      do {"
      "          node = node.parent;"
      "          continue;"
      "      } while (node != undefined);"
      ""
      "      console.log(this.document === document);"
      ""
      "      var o = {"
      "         prop: 37,"
      "         f: function() {"
      "             return this.prop;"
      "         }"
      "      };"
      ""
      "     var elvisLives = Math.PI > 4 ? \"Yep\" : \"Nope\";"
      ""
      "     var index, len;"
      "     for (index = 0, len = list.length; index < len; ++index) {"
      "         break;"
      "     }";

  normalize_code(&original_code);
  normalize_code(&generated_code);

  EXPECT_EQ(original_code, generated_code);
}

TEST(TreeTestWithSimpleLiteral, TreeToJavascript) {
  StringSet ss;
  TreeStorage storage;
  const char* program_json =
      "[ { \"id\":0, \"type\":\"Program\", \"children\":[1,16,30,38,54,57,73,78,83,96,105,121,129,140,148] }, { \"id\":1, \"type\":\"VariableDeclaration\", \"children\":[2] }, { \"id\":2, \"type\":\"VariableDeclarator\", \"value\":\"x\", \"children\":[3] }, { \"id\":3, \"type\":\"FunctionExpression\", \"children\":[4,5,6,7] }, { \"id\":4, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":5, \"type\":\"Identifier\", \"value\":\"a\" }, { \"id\":6, \"type\":\"Identifier\", \"value\":\"b\" }, { \"id\":7, \"type\":\"BlockStatement\", \"children\":[8] }, { \"id\":8, \"type\":\"ExpressionStatement\", \"children\":[9] }, { \"id\":9, \"type\":\"CallExpression\", \"children\":[10,13] }, { \"id\":10, \"type\":\"MemberExpression\", \"children\":[11,12] }, { \"id\":11, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":12, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":13, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[14,15] }, { \"id\":14, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":15, \"type\":\"Identifier\", \"value\":\"a\" }, { \"id\":16, \"type\":\"FunctionDeclaration\", \"children\":[17,18,19,20,21] }, { \"id\":17, \"type\":\"Identifier\", \"value\":\"x\" }, { \"id\":18, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":19, \"type\":\"Identifier\", \"value\":\"a\" }, { \"id\":20, \"type\":\"Identifier\", \"value\":\"b\" }, { \"id\":21, \"type\":\"BlockStatement\", \"children\":[22] }, { \"id\":22, \"type\":\"ExpressionStatement\", \"children\":[23] }, { \"id\":23, \"type\":\"CallExpression\", \"children\":[24,27] }, { \"id\":24, \"type\":\"MemberExpression\", \"children\":[25,26] }, { \"id\":25, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":26, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":27, \"type\":\"BinaryExpression\", \"value\":\"+\", \"children\":[28,29] }, { \"id\":28, \"type\":\"Identifier\", \"value\":\"t\" }, { \"id\":29, \"type\":\"Identifier\", \"value\":\"b\" }, { \"id\":30, \"type\":\"ExpressionStatement\", \"children\":[31] }, { \"id\":31, \"type\":\"CallExpression\", \"children\":[32,33,34,35] }, { \"id\":32, \"type\":\"Identifier\", \"value\":\"x\" }, { \"id\":33, \"type\":\"Literal\", \"value\":\"f\" }, { \"id\":34, \"type\":\"Literal\", \"value\":\"g\" }, { \"id\":35, \"type\":\"FunctionExpression\", \"children\":[36,37] }, { \"id\":36, \"type\":\"Identifier\", \"value\":\"c\" }, { \"id\":37, \"type\":\"BlockStatement\" }, { \"id\":38, \"type\":\"ForStatement\", \"children\":[39,42,45,47] }, { \"id\":39, \"type\":\"VariableDeclaration\", \"children\":[40] }, { \"id\":40, \"type\":\"VariableDeclarator\", \"value\":\"i\", \"children\":[41] }, { \"id\":41, \"type\":\"Literal\", \"value\":0 }, { \"id\":42, \"type\":\"BinaryExpression\", \"value\":\"<\", \"children\":[43,44] }, { \"id\":43, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":44, \"type\":\"Literal\", \"value\":10 }, { \"id\":45, \"type\":\"UpdateExpression\", \"value\":\"++?\", \"children\":[46] }, { \"id\":46, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":47, \"type\":\"BlockStatement\", \"children\":[48] }, { \"id\":48, \"type\":\"ExpressionStatement\", \"children\":[49] }, { \"id\":49, \"type\":\"CallExpression\", \"children\":[50,53] }, { \"id\":50, \"type\":\"MemberExpression\", \"children\":[51,52] }, { \"id\":51, \"type\":\"Identifier\", \"value\":\"log\" }, { \"id\":52, \"type\":\"Property\", \"value\":\"console\" }, { \"id\":53, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":54, \"type\":\"VariableDeclaration\", \"children\":[55] }, { \"id\":55, \"type\":\"VariableDeclarator\", \"value\":\"i\", \"children\":[56] }, { \"id\":56, \"type\":\"Literal\", \"value\":0 }, { \"id\":57, \"type\":\"ForStatement\", \"children\":[58,59,62,63] }, { \"id\":58, \"type\":\"EmptyStatement\" }, { \"id\":59, \"type\":\"BinaryExpression\", \"value\":\"<\", \"children\":[60,61] }, { \"id\":60, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":61, \"type\":\"Literal\", \"value\":10 }, { \"id\":62, \"type\":\"EmptyStatement\" }, { \"id\":63, \"type\":\"BlockStatement\", \"children\":[64,70] }, { \"id\":64, \"type\":\"ExpressionStatement\", \"children\":[65] }, { \"id\":65, \"type\":\"CallExpression\", \"children\":[66,69] }, { \"id\":66, \"type\":\"MemberExpression\", \"children\":[67,68] }, { \"id\":67, \"type\":\"Identifier\", \"value\":\"log\" }, { \"id\":68, \"type\":\"Property\", \"value\":\"console\" }, { \"id\":69, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":70, \"type\":\"ExpressionStatement\", \"children\":[71] }, { \"id\":71, \"type\":\"UpdateExpression\", \"value\":\"++?\", \"children\":[72] }, { \"id\":72, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":73, \"type\":\"VariableDeclaration\", \"children\":[74] }, { \"id\":74, \"type\":\"VariableDeclarator\", \"value\":\"jasmine\", \"children\":[75] }, { \"id\":75, \"type\":\"CallExpression\", \"children\":[76,77] }, { \"id\":76, \"type\":\"Identifier\", \"value\":\"require\" }, { \"id\":77, \"type\":\"Literal\", \"value\":\"jasmine-node\" }, { \"id\":78, \"type\":\"VariableDeclaration\", \"children\":[79] }, { \"id\":79, \"type\":\"VariableDeclarator\", \"value\":\"sys\", \"children\":[80] }, { \"id\":80, \"type\":\"CallExpression\", \"children\":[81,82] }, { \"id\":81, \"type\":\"Identifier\", \"value\":\"require\" }, { \"id\":82, \"type\":\"Literal\", \"value\":\"sys\" }, { \"id\":83, \"type\":\"ForInStatement\", \"children\":[84,86,87] }, { \"id\":84, \"type\":\"VariableDeclaration\", \"children\":[85] }, { \"id\":85, \"type\":\"VariableDeclarator\", \"value\":\"key\" }, { \"id\":86, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":87, \"type\":\"BlockStatement\", \"children\":[88] }, { \"id\":88, \"type\":\"ExpressionStatement\", \"children\":[89] }, { \"id\":89, \"type\":\"AssignmentExpression\", \"children\":[90,93] }, { \"id\":90, \"type\":\"ArrayAccess\", \"children\":[91,92] }, { \"id\":91, \"type\":\"Identifier\", \"value\":\"global\" }, { \"id\":92, \"type\":\"Property\", \"value\":\"key\" }, { \"id\":93, \"type\":\"ArrayAccess\", \"children\":[94,95] }, { \"id\":94, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":95, \"type\":\"Property\", \"value\":\"key\" }, { \"id\":96, \"type\":\"IfStatement\", \"children\":[97,104] }, { \"id\":97, \"type\":\"LogicalExpression\", \"value\":\"&&\", \"children\":[98,101] }, { \"id\":98, \"type\":\"BinaryExpression\", \"value\":\"!=\", \"children\":[99,100] }, { \"id\":99, \"type\":\"Identifier\", \"value\":\"sys\" }, { \"id\":100, \"type\":\"Literal\", \"value\":null }, { \"id\":101, \"type\":\"BinaryExpression\", \"value\":\">\", \"children\":[102,103] }, { \"id\":102, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":103, \"type\":\"Literal\", \"value\":10 }, { \"id\":104, \"type\":\"BlockStatement\" }, { \"id\":105, \"type\":\"IfStatement\", \"children\":[106,113,114] }, { \"id\":106, \"type\":\"LogicalExpression\", \"value\":\"&&\", \"children\":[107,110] }, { \"id\":107, \"type\":\"BinaryExpression\", \"value\":\"!=\", \"children\":[108,109] }, { \"id\":108, \"type\":\"Identifier\", \"value\":\"sys\" }, { \"id\":109, \"type\":\"Literal\", \"value\":null }, { \"id\":110, \"type\":\"BinaryExpression\", \"value\":\">\", \"children\":[111,112] }, { \"id\":111, \"type\":\"Identifier\", \"value\":\"i\" }, { \"id\":112, \"type\":\"Literal\", \"value\":10 }, { \"id\":113, \"type\":\"BlockStatement\" }, { \"id\":114, \"type\":\"BlockStatement\", \"children\":[115] }, { \"id\":115, \"type\":\"ExpressionStatement\", \"children\":[116] }, { \"id\":116, \"type\":\"CallExpression\", \"children\":[117,120] }, { \"id\":117, \"type\":\"MemberExpression\", \"children\":[118,119] }, { \"id\":118, \"type\":\"Identifier\", \"value\":\"console\" }, { \"id\":119, \"type\":\"Property\", \"value\":\"log\" }, { \"id\":120, \"type\":\"Literal\", \"value\":\"hello\" }, { \"id\":121, \"type\":\"ExpressionStatement\", \"children\":[122] }, { \"id\":122, \"type\":\"AssignmentExpression\", \"children\":[123,124] }, { \"id\":123, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":124, \"type\":\"MemberExpression\", \"children\":[125,128] }, { \"id\":125, \"type\":\"MemberExpression\", \"children\":[126,127] }, { \"id\":126, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":127, \"type\":\"Property\", \"value\":\"walk\" }, { \"id\":128, \"type\":\"Property\", \"value\":\"root\" }, { \"id\":129, \"type\":\"WhileStatement\", \"children\":[130,133] }, { \"id\":130, \"type\":\"BinaryExpression\", \"value\":\"!=\", \"children\":[131,132] }, { \"id\":131, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":132, \"type\":\"Literal\", \"value\":null }, { \"id\":133, \"type\":\"BlockStatement\", \"children\":[134] }, { \"id\":134, \"type\":\"ExpressionStatement\", \"children\":[135] }, { \"id\":135, \"type\":\"AssignmentExpression\", \"children\":[136,137] }, { \"id\":136, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":137, \"type\":\"MemberExpression\", \"children\":[138,139] }, { \"id\":138, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":139, \"type\":\"Property\", \"value\":\"parent\" }, { \"id\":140, \"type\":\"ExpressionStatement\", \"children\":[141] }, { \"id\":141, \"type\":\"AssignmentExpression\", \"children\":[142,143] }, { \"id\":142, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":143, \"type\":\"MemberExpression\", \"children\":[144,147] }, { \"id\":144, \"type\":\"MemberExpression\", \"children\":[145,146] }, { \"id\":145, \"type\":\"Identifier\", \"value\":\"jasmine\" }, { \"id\":146, \"type\":\"Property\", \"value\":\"walk\" }, { \"id\":147, \"type\":\"Property\", \"value\":\"root\" }, { \"id\":148, \"type\":\"DoWhileStatement\", \"children\":[149,152] }, { \"id\":149, \"type\":\"BinaryExpression\", \"value\":\"!=\", \"children\":[150,151] }, { \"id\":150, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":151, \"type\":\"Literal\", \"value\":null }, { \"id\":152, \"type\":\"BlockStatement\", \"children\":[153] }, { \"id\":153, \"type\":\"ExpressionStatement\", \"children\":[154] }, { \"id\":154, \"type\":\"AssignmentExpression\", \"children\":[155,156] }, { \"id\":155, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":156, \"type\":\"MemberExpression\", \"children\":[157,158] }, { \"id\":157, \"type\":\"Identifier\", \"value\":\"node\" }, { \"id\":158, \"type\":\"Property\", \"value\":\"parent\" }, 0]";

  PrepareTestProgram(&storage, &ss, program_json);

  std::string generated_code = storage.DebugStringAsSource(&ss);
  std::string original_code =
      "var x = function (t,a,b) {"
      "          console.log(t + a);"
      "      };"
      ""
      "      function x(t,a,b){"
      "          console.log(t + b);"
      "      }"
      ""
      "      x('f', 'g', function(c){});"
      ""
      "      for (var i = ?number; i < ?number; ++i) {"
      "          log.console(i);"
      "      }"
      ""
      "      var i = ?number;"
      "      for (; i < ?number; ) {"
      "          log.console(i);"
      "          ++i;"
      "      }"
      ""
      "      var jasmine = require('jasmine-node');"
      "      var sys = require('sys');"
      ""
      "      for(var key in jasmine) {"
      "        global[key] = jasmine[key];"
      "      }"
      ""
      "      if (sys != ?number && i > ?number) {"
      ""
      "      }"
      ""
      "      if (sys != ?number && i > ?number) {"
      ""
      "      } else {"
      "          console.log('hello');"
      "      }"
      ""
      "      node = jasmine.walk.root;"
      "      while (node != ?number) {"
      "          node = node.parent;"
      "      }"
      ""
      "      node = jasmine.walk.root;"
      "      do {"
      "          node = node.parent;"
      "      } while (node != ?number);";


  normalize_code(&original_code);
  normalize_code(&generated_code);

  EXPECT_EQ(original_code, generated_code);
}

int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

