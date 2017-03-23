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

#include "branched_cond.h"

#include "glog/logging.h"

#include "external/gtest/googletest/include/gtest/gtest.h"

TEST(BranchCondProgramTest, ParseFilter1) {
  StringSet ss;
  TCondLanguage lang(&ss);

  BranchCondProgram p;
  p.ParseAsSimpleFilterOrDie(&lang, "type == Property");

  // Check that we parsed it correctly.
  EXPECT_EQ(0, p.p_default);
  EXPECT_EQ(1u, p.per_case_p.size());
  int prop = ss.findString("Property");
  EXPECT_NE(prop, -1);
  EXPECT_EQ(1, p.per_case_p[std::vector<int>({prop})]);

  // Check how it serializes.
  EXPECT_EQ(
      "switch WRITE_TYPE: on \"Property\" goto 1; else goto 0",
      p.ToStringAsProgramLine(&lang));

  p.ParseAsSimpleFilterOrDie(&lang, "type == Expression|If");
  // Check that we parsed it correctly.
  EXPECT_EQ(0, p.p_default);
  EXPECT_EQ(2u, p.per_case_p.size());
  int expr = ss.findString("Expression");
  EXPECT_NE(expr, -1);
  int ifv = ss.findString("If");
  EXPECT_NE(ifv, -1);
  EXPECT_EQ(1, p.per_case_p[std::vector<int>({expr})]);
  EXPECT_EQ(1, p.per_case_p[std::vector<int>({ifv})]);

  // Check how it serializes.
  EXPECT_EQ(
      "switch WRITE_TYPE: on \"Expression|If\" goto 1; else goto 0",
      p.ToStringAsProgramLine(&lang));

  // Serializing and parsing produces the same result.
  BranchCondProgram p1;
  p1.ParseAsProgramLineOrDie(&lang, p.ToStringAsProgramLine(&lang));
  EXPECT_TRUE(p == p1) << p1.ToStringAsProgramLine(&lang);

  // This modifies p, thus we do it only in the end.
  EXPECT_EQ(0, p.per_case_p[std::vector<int>({prop})]);
}

TEST(BranchCondProgramTest, ParseFilter2) {
  StringSet ss;
  TCondLanguage lang(&ss);

  BranchCondProgram p;
  p.ParseAsSimpleFilterOrDie(&lang, "type_parent_type == Property Expression");

  // Check that we parsed it correctly.
  EXPECT_EQ(0, p.p_default);
  EXPECT_EQ(1u, p.per_case_p.size());
  int prop = ss.findString("Property");
  EXPECT_NE(prop, -1);
  int expr = ss.findString("Expression");
  EXPECT_NE(expr, -1);
  EXPECT_EQ(1, p.per_case_p[std::vector<int>({prop,expr})]);

  p.ParseAsSimpleFilterOrDie(&lang, "type_parent_type == Expression Expression | If\\sExpr If");
  // Check that we parsed it correctly.
  EXPECT_EQ(0, p.p_default);
  EXPECT_EQ(2u, p.per_case_p.size());
  int ife = ss.findString("If Expr");
  EXPECT_NE(ife, -1);
  int ifv = ss.findString("If");
  EXPECT_NE(ifv, -1);
  EXPECT_EQ(1, p.per_case_p[std::vector<int>({expr, expr})]);
  EXPECT_EQ(1, p.per_case_p[std::vector<int>({ife, ifv})]);

  // Check how it serializes.
  EXPECT_EQ(
      "switch WRITE_TYPE UP WRITE_TYPE: on \"Expression Expression|If\\sExpr If\" goto 1; else goto 0",
      p.ToStringAsProgramLine(&lang));

  // Serializing and parsing produces the same result.
  BranchCondProgram p1;
  p1.ParseAsProgramLineOrDie(&lang, p.ToStringAsProgramLine(&lang));
  EXPECT_TRUE(p == p1) << p1.ToStringAsProgramLine(&lang);

  // This modifies p, thus we do it only in the end.
  EXPECT_EQ(0, p.per_case_p[std::vector<int>({prop, expr})]);
}

TEST(BranchCondProgramTest, ParseWrite) {
  StringSet ss;
  TCondLanguage lang(&ss);

  BranchCondProgram p;
  p.ParseAsProgramLineOrDie(&lang, "switch WRITE_TYPE: on \"Expression\" goto 2; on \"Loop\" goto 3; else goto 0");

  EXPECT_EQ(
      "switch WRITE_TYPE: on \"Expression\" goto 2; on \"Loop\" goto 3; else goto 0",
      p.ToStringAsProgramLine(&lang));
}

TEST(BranchCondProgramTest, NonEqualBranchSize) {
  StringSet ss;
  TCondLanguage lang(&ss);

  BranchCondProgram p;
  p.ParseAsProgramLineOrDie(&lang, "switch WRITE_TYPE RIGHT WRITE_TYPE: on \"\" goto 1; on \"Expression\" goto 2; on \"Loop -1\" goto 3; else goto 0");
  EXPECT_EQ(p.ToString(&lang), "switch WRITE_TYPE RIGHT WRITE_TYPE: on \"\" goto 1; on \"Expression\" goto 2; on \"Loop -1\" goto 3; else goto 0");
}

int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
