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

#include "tgen_program.h"

#include "glog/logging.h"

#include "external/gtest/googletest/include/gtest/gtest.h"

TEST(TGenProgramTest, LoadSave) {
  std::string prog =
      "WRITE_TYPE LEFT WRITE_TYPE\n"
      "UP WRITE_TYPE\n"
      "switch WRITE_TYPE: on \"Property\" goto 1; else goto 0\n"
      "UP UP RIGHT WRITE_TYPE WRITE_VALUE\n"
      "switch UP WRITE_TYPE: on \"Expr\" goto 2; else goto 3\n"
      "UP UP WRITE_TYPE\n"
      "switch UP UP WRITE_TYPE: on \"Expr\" goto 4; else goto 5\n";

  StringSet ss;
  TCondLanguage lang(&ss);
  TGenProgram p;
  p.LoadFromStringOrDie(&lang, prog);
  EXPECT_EQ(prog, p.SaveToString(&lang));

  ASSERT_EQ(7u, p.size());
  EXPECT_TRUE(TGenProgram::ProgramType::SIMPLE_PROGRAM == p.program_type(0));
  EXPECT_TRUE(TGenProgram::ProgramType::SIMPLE_PROGRAM == p.program_type(1));
  EXPECT_TRUE(TGenProgram::ProgramType::BRANCHED_PROGRAM == p.program_type(2));
  EXPECT_TRUE(TGenProgram::ProgramType::SIMPLE_PROGRAM == p.program_type(3));
  EXPECT_TRUE(TGenProgram::ProgramType::BRANCHED_PROGRAM == p.program_type(4));
  EXPECT_TRUE(TGenProgram::ProgramType::SIMPLE_PROGRAM == p.program_type(5));
  EXPECT_TRUE(TGenProgram::ProgramType::BRANCHED_PROGRAM == p.program_type(6));
}

int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
