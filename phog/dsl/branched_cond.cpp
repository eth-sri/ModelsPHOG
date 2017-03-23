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
#include "base/strutil.h"

void BranchCond::ParseFromStringOrDie(const TCondLanguage* language, const std::string& str) {
  if (str == "type")
    *this = BranchCond(TYPE_COND);
  else if (str == "parent_type")
    *this = BranchCond(PARENT_TYPE_COND);
  else if (str == "type_parent_type")
    *this = BranchCond(TYPE_AND_PARENT_TYPE_COND);
  else
    program = language->ParseStringToProgramOrDie(str);
}

std::string BranchCond::ToString(const TCondLanguage* language) const {
  return language->ProgramToString(program);
}

void BranchCondProgram::ParseAsSimpleFilterOrDie(TCondLanguage* language, const std::string& str) {
  size_t eqpos = str.find("==");
  CHECK_NE(eqpos, std::string::npos) << "Invalid filter " << str;
  cond.ParseFromStringOrDie(language, TrimLeadingAndTrailingSpaces(str.substr(0, eqpos)));
  std::vector<std::string> values;
  SplitStringUsing(str.substr(eqpos + 2), '|', &values);
  per_case_p.clear();
  for (size_t value_id = 0; value_id < values.size(); ++value_id) {
    std::vector<std::string> cmds;
    SplitStringUsing(TrimLeadingAndTrailingSpaces(values[value_id]), ' ', &cmds);

    std::vector<int> cmd_ids(cmds.size(), -1);
    for (size_t i = 0; i < cmds.size(); ++i) {
      CHECK(!cmds[i].empty()) << "Invalid filter " << str;
      if (cmds[i][0] == '-') {
        CHECK(ParseInt32(cmds[i], &cmd_ids[i])) << "Invalid number in filter " << str;
      } else {
        cmd_ids[i] = language->ss()->addString(UnEscapeStrSeparators(cmds[i]).c_str());
      }
    }
    per_case_p[cmd_ids] = 1;
  }
  p_default = 0;
}

void BranchCondProgram::ParseAsProgramLineOrDie(TCondLanguage* language, const std::string& str) {
  CHECK(strncmp(str.c_str(), "switch ", 7) == 0) << "Not a switch " << str;
  size_t colon = str.find(":");
  CHECK_NE(colon, std::string::npos) << "No : in " << str;
  cond.ParseFromStringOrDie(language, TrimLeadingAndTrailingSpaces(str.substr(7, colon - 7)));
  per_case_p.clear();

  std::vector<std::string> cases;
  SplitStringUsing(str.substr(colon + 1), ';', &cases);
  for (size_t case_id = 0; case_id < cases.size(); ++case_id) {
    std::string curr_case = TrimLeadingAndTrailingSpaces(cases[case_id]);
    int label = -1;
    if (sscanf(curr_case.c_str(), "else goto %d", &label) == 1) {
      p_default = label;
    } else {
      CHECK(strncmp(curr_case.c_str(), "on ", 3) == 0) << "Not on in " << curr_case;
      size_t q1 = curr_case.find('\"');
      CHECK_NE(q1, std::string::npos) << " no opening quote " << curr_case;
      size_t q2 = curr_case.find('\"', q1 + 1);
      CHECK_NE(q1, std::string::npos) << " no closing quote " << curr_case;
      CHECK_EQ(sscanf(curr_case.c_str() + q2 + 1, " goto %d", &label), 1) << "No goto in " << curr_case;

      std::vector<std::string> values;
      SplitStringUsing(curr_case.substr(q1 + 1, q2 - q1 - 1), '|', &values);
      for (size_t value_id = 0; value_id < values.size(); ++value_id) {
        std::vector<std::string> cmds;
        SplitStringUsing(TrimLeadingAndTrailingSpaces(values[value_id]), ' ', &cmds);

        std::vector<int> cmd_ids(cmds.size(), -1);
        for (size_t i = 0; i < cmds.size(); ++i) {
//          CHECK(!cmds[i].empty()) << "Invalid filter " << str;
          if (cmds[i].empty()) {
            // Allow empty values
            cmd_ids.clear();
          } else if (cmds[i][0] == '-') {
            CHECK(ParseInt32(cmds[i], &cmd_ids[i])) << "Invalid number in filter " << str;
          } else {
            cmd_ids[i] = language->ss()->addString(UnEscapeStrSeparators(cmds[i]).c_str());
          }
        }
        per_case_p[cmd_ids] = label;
      }
    }
  }
}

std::string BranchCondProgram::ToStringAsProgramLine(const TCondLanguage* language) const {
  std::string result;
  result += "switch ";
  result += cond.ToString(language);
  result += ":";
  std::set<int> programs_set;
  GetReferencedPrograms(&programs_set);
  programs_set.erase(p_default);
  std::vector<int> programs(programs_set.begin(), programs_set.end());
  programs.push_back(p_default);

  for (int p : programs) {
    if (p == p_default) {
      StringAppendF(&result, " else goto %d", p);
    } else {
      result += " on \"";
      bool first_cond = true;
      for (auto it = per_case_p.begin(); it != per_case_p.end(); ++it) {
        if (it->second == p) {
          if (!first_cond)
            result += "|";
          first_cond = false;
          result += BranchCondProgram::CaseToString(it->first, language->ss());
        }
      }
      StringAppendF(&result, "\" goto %d;", p);
    }
  }
  return result;
}

std::string BranchCondProgram::BranchToString(const StringSet* ss, int branch_id) const {
  std::set<int> programs_set;
  GetReferencedPrograms(&programs_set);
  CHECK(programs_set.find(branch_id) != programs_set.end());

  std::string result;
  if (branch_id == p_default) {
    StringAppendF(&result, " else goto %d", branch_id);
  } else {
    result += " on \"";
    bool first_cond = true;
    for (auto it = per_case_p.begin(); it != per_case_p.end(); ++it) {
      if (it->second == branch_id) {
        if (!first_cond)
          result += "|";
        first_cond = false;
        result += BranchCondProgram::CaseToString(it->first, ss);
      }
    }
    StringAppendF(&result, "\" goto %d;", branch_id);
  }
  return result;
}

void BranchCondProgram::GetReferencedPrograms(std::set<int>* programs) const {
  programs->clear();
  for (auto it = per_case_p.begin(); it != per_case_p.end(); ++it) {
    programs->insert(it->second);
  }
  programs->insert(p_default);
}

