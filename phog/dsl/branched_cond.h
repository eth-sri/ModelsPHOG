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

#ifndef SYNTREE_BRANCHED_COND_H_
#define SYNTREE_BRANCHED_COND_H_

#include "base/strutil.h"
#include "tcond_language.h"

// Provides conditioning programs with branching.
//
// The programs are of the type:
//    if (prog(TREEPOS) == value) ...

// Defines a prog in a conditioning.
class BranchCond {
public:
  BranchCond() {}
  enum PredefinedProgram {
    TYPE_COND,
    PARENT_TYPE_COND,
    TYPE_AND_PARENT_TYPE_COND
  };
  BranchCond(PredefinedProgram p) {
    if (p == TYPE_COND) {
      program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::WRITE_TYPE));
    } else if (p == PARENT_TYPE_COND) {
      program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::UP));
      program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::WRITE_TYPE));
    } else if (p == TYPE_AND_PARENT_TYPE_COND) {
      program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::WRITE_TYPE));
      program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::UP));
      program.push_back(TCondLanguage::Op(TCondLanguage::OpCmd::WRITE_TYPE));
    }
  }

  TCondLanguage::Program program;

  void ParseFromStringOrDie(const TCondLanguage* language, const std::string& str);
  std::string ToString(const TCondLanguage* language) const;
};

// Defines a set of rules:
// switch (prog(TREEPOS)) {
// case A: P1
// case B: P2
// case C: P1
//  ...
// default:
//   P_DEFAULT
// }
class BranchCondProgram {
public:
  BranchCond cond;
  std::map<std::vector<int>, int> per_case_p;  // id of the program referenced when if conditions match.
  int p_default;  // id of the default program (if neither if condition matches).

  static std::string CaseToString(const std::vector<int> values, const StringSet* ss) {
    std::string result;
    for (size_t i = 0; i < values.size(); ++i) {
      if (i != 0) result += " ";
      int curr_value = values[i];
      if (curr_value < 0)
        StringAppendF(&result, "%d", curr_value);
      else
        result += EscapeStrSeparators(ss->getString(curr_value));
    }
    return result;
  }

  std::string TargetToString(int target, const StringSet* ss) const {
    for (const auto& it : per_case_p) {
      if (it.second == target) {
        return BranchCondProgram::CaseToString(it.first, ss);
      }
    }
    return "";
  }

  void ParseAsSimpleFilterOrDie(TCondLanguage* language, const std::string& str);
  void ParseAsProgramLineOrDie(TCondLanguage* language, const std::string& str);
  std::string ToStringAsProgramLine(const TCondLanguage* language) const;
  std::string ToString(const TCondLanguage* language) const {
    return ToStringAsProgramLine(language);
  }
  std::string BranchToString(const StringSet* ss, int branch_id) const;

  bool operator==(const BranchCondProgram& o) const {
    return cond.program == o.cond.program && per_case_p == o.per_case_p && p_default == o.p_default;
  }
  bool operator!=(const BranchCondProgram& o) const { return !(*this == o); }
  bool operator<(const BranchCondProgram& o) const {
    if (cond.program != o.cond.program) return cond.program < o.cond.program;
    if (p_default != o.p_default) return p_default < o.p_default;
    return per_case_p < o.per_case_p;
  }
  size_t size() const { return cond.program.size() + per_case_p.size(); }

  // Returns the list of all program ids referenced by the branch instruction (non-recursively), except for the
  // default program.
  void GetReferencedPrograms(std::set<int>* programs) const;
};

#endif /* SYNTREE_BRANCHED_COND_H_ */
