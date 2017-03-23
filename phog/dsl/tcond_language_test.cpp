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

#include "tcond_language.h"

#include "external/gtest/googletest/include/gtest/gtest.h"
#include "glog/logging.h"

TEST(TCondLanguageTest, SlicedExecutionForTree) {
  StringSet ss;
  TreeStorage tree;
  {
    TreeSubstitution s(
        {
          {ss.addString("Root"), -1, 1, -1},  // 0
          {ss.addString("MemberExpression"), -1, 2, 3},  // 1
          {ss.addString("Identifier"), ss.addString("foo"), -1, -1},  // 2
          {ss.addString("Property"), ss.addString("bar"), -1, -1},  // 3
        });
    tree.SubstituteNode(0, s);
  }

  std::string callback_result;
  auto callback = [&callback_result,&ss](int v) {
    if (!callback_result.empty()) callback_result += " ";
    if (v < 0)
      StringAppendF(&callback_result, "%d", v);
    else
      callback_result += ss.getString(v);
  };

  TCondLanguage lang(&ss);
  TCondLanguage::ExecutionForTree exec(&ss, &tree);

  {
    TreeSlice slice(&tree, 3);
    SlicedTreeTraversal tt(&tree, 3, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("PREV_LEAF WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("foo", callback_result);
  }

  {
    TreeSlice slice(&tree, 3);
    SlicedTreeTraversal tt(&tree, 3, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("PREV_LEAF PREV_NODE_CONTEXT WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("foo", callback_result);
  }

  {
    TreeSlice slice(&tree, 3);
    SlicedTreeTraversal tt(&tree, 3, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("PREV_LEAF PREV_NODE_CONTEXT NEXT_LEAF WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("-1", callback_result);
  }
}

TEST(TCondLanguageTest, SimpleProgramExecution) {
  StringSet ss;
  TreeStorage tree;
  {
    TreeSubstitution s(
        {
          {ss.addString("Root"), -1, 1, -1},  // 0
          {ss.addString("VarDecls"), -1, 2, 3},  // 1
          {ss.addString("Var"), ss.addString("v1"), -1, -1},  // 2
          {ss.addString("PlusExpr"), -1, 4, -1},  // 3
          {ss.addString("Var"), ss.addString("v1"), -1, 5},  // 4
          {ss.addString("Var"), ss.addString("v2"), -1, -1}  // 5
        });
    tree.SubstituteNode(0, s);
  }

  std::string callback_result;
  auto callback = [&callback_result,&ss](int v) {
    if (!callback_result.empty()) callback_result += " ";
    if (v < 0)
      StringAppendF(&callback_result, "%d", v);
    else
      callback_result += ss.getString(v);
  };


  TCondLanguage lang(&ss);
  TCondLanguage::ExecutionForTree exec(&ss, &tree);

  {
    TreeSlice slice(&tree, 5);
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("WRITE_TYPE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("-1", callback_result);
  }
  {
    TreeSlice slice(&tree, 5);
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("-1", callback_result);
  }
  {
    TreeSlice slice(&tree, 5, true);
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("WRITE_TYPE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("Var", callback_result);
  }
  {
    TreeSlice slice(&tree, 5, true);
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("-1", callback_result);
  }

  {
    TreeSlice slice(&tree, 5, false);  // Cannot condition on TYPE in the first node, PREV_NODE_TYPE does not move.
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("PREV_NODE_TYPE WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("-1", callback_result);
  }
  {
    TreeSlice slice(&tree, 5, true);  // Can condition on type. PREV_NODE_TYPE moves to the previous type.
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("PREV_NODE_TYPE WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("v1", callback_result);
  }

  {
    TreeSlice slice(&tree, 5, false);  // Cannot condition on TYPE in the first node.
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("UP WRITE_TYPE WRITE_VALUE UP WRITE_TYPE UP WRITE_TYPE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("PlusExpr -1 Root Root", callback_result);
  }

  {
    TreeSlice slice(&tree, 5, false);  // Cannot condition on TYPE.
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("LEFT WRITE_TYPE LEFT WRITE_TYPE UP WRITE_TYPE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("Var Var PlusExpr", callback_result);
  }

  {
    TreeSlice slice(&tree, 5, false);  // Cannot condition on TYPE.
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("PREV_LEAF WRITE_TYPE PREV_LEAF WRITE_TYPE UP WRITE_TYPE RIGHT WRITE_TYPE LEFT WRITE_TYPE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("Var Var VarDecls PlusExpr VarDecls", callback_result);
  }

  {
    TreeSlice slice(&tree, 5, false);  // Cannot condition on TYPE.
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("UP WRITE_TYPE DOWN_FIRST WRITE_TYPE WRITE_VALUE RIGHT WRITE_VALUE LEFT WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("PlusExpr Var v1 -1 v1", callback_result);
  }

  {
    TreeSlice slice(&tree, 5, false);  // Cannot condition on TYPE.
    SlicedTreeTraversal tt(&tree, 5, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("UP WRITE_TYPE DOWN_LAST WRITE_TYPE WRITE_VALUE RIGHT WRITE_VALUE LEFT WRITE_VALUE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("PlusExpr PlusExpr -1 -1 -1", callback_result);
  }

  {
    TreeSlice slice(&tree, 1, false);  // Cannot condition on TYPE.
    SlicedTreeTraversal tt(&tree, 1, &slice);
    TCondLanguage::Program p = lang.ParseStringToProgramOrDie("PREV_DFS WRITE_TYPE PREV_DFS WRITE_TYPE PREV_DFS WRITE_TYPE");

    callback_result.clear();
    exec.GetConditionedFeaturesForPosition(p, &tt, nullptr, callback);
    EXPECT_EQ("Root Root Root", callback_result);
  }
}

int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
