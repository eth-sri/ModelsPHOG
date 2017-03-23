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

#include "simple_cond.h"

#include "glog/logging.h"

#include "external/gtest/googletest/include/gtest/gtest.h"

TEST(SimpleCondTest, SaveLoadTCond) {
  StringSet ss;
  TCondLanguage lang(&ss);

  std::string ptext = "UP WRITE_TYPE";
  SimpleCondProgram p;
  p.ParseFromStringOrDie(&lang, ptext);
  EXPECT_EQ(0u, p.eq_program.size());
  EXPECT_EQ(2u, p.context_program.size());

  EXPECT_EQ(ptext, p.ToString(&lang));
  p.eq_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::LEFT));
  p.eq_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::WRITE_VALUE));

  EXPECT_EQ("LEFT WRITE_VALUE =eq= UP WRITE_TYPE", p.ToString(&lang));
}

TEST(SimpleCondTest, SaveLoadTCondTEq) {
  StringSet ss;
  TCondLanguage lang(&ss);
  std::string ptext = "LEFT LEFT WRITE_VALUE =eq= UP WRITE_TYPE";

  SimpleCondProgram p;
  p.ParseFromStringOrDie(&lang, ptext);
  EXPECT_EQ(3u, p.eq_program.size());
  EXPECT_EQ(2u, p.context_program.size());

  EXPECT_EQ(ptext, p.ToString(&lang));
}

TEST(SimpleCondTest, SaveLoadEmptyCond) {
  StringSet ss;
  TCondLanguage lang(&ss);

  SimpleCondProgram p;
  EXPECT_EQ("empty", p.ToString(&lang));

  p.eq_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::LEFT));
  p.eq_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::WRITE_VALUE));

  std::string ps = p.ToString(&lang);
  EXPECT_EQ("LEFT WRITE_VALUE =eq= ", ps);

  SimpleCondProgram p1;
  p1.ParseFromStringOrDie(&lang, ps);
  EXPECT_TRUE(p1 == p) << p1.ToString(&lang) << " " << p.ToString(&lang);
}

TEST(SimpleCondTest, SaveLoadEmptyEq) {
  StringSet ss;
  TCondLanguage lang(&ss);

  SimpleCondProgram p;
  EXPECT_EQ("empty", p.ToString(&lang));

  SimpleCondProgram p_empty_parsed;
  p_empty_parsed.context_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::UP));
  p_empty_parsed.eq_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::UP));
  p_empty_parsed.ParseFromStringOrDie(&lang, "");
  EXPECT_TRUE(p_empty_parsed == p) << p_empty_parsed.ToString(&lang) << " " << p.ToString(&lang);

  p.context_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::LEFT));
  p.context_program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::WRITE_VALUE));

  std::string ps = p.ToString(&lang);
  EXPECT_EQ("LEFT WRITE_VALUE", ps);

  SimpleCondProgram p1;
  p1.ParseFromStringOrDie(&lang, ps);
  EXPECT_TRUE(p1 == p) << p1.ToString(&lang) << " " << p.ToString(&lang);
}


int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
