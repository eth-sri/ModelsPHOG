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
#include "base/strutil.h"


void SimpleCondProgram::ParseFromStringOrDie(const TCondLanguage* language, const std::string& str) {
  if (str == "empty") {
    eq_program.clear();
    context_program.clear();
    return;
  }
  size_t pos = str.find("=eq=");
  if (pos == std::string::npos) {
    eq_program = TCondLanguage::Program();
    context_program = language->ParseStringToProgramOrDie(str);
    return;
  }

  eq_program = language->ParseStringToProgramOrDie(TrimLeadingAndTrailingSpaces(str.substr(0, pos)));
  context_program = language->ParseStringToProgramOrDie(TrimLeadingAndTrailingSpaces(str.substr(pos + 4)));
}

std::string SimpleCondProgram::ToString(const TCondLanguage* language) const {
  if (eq_program.empty()) {
    if (context_program.empty())
      return "empty";
    return language->ProgramToString(context_program);
  }
  return language->ProgramToString(eq_program) + " =eq= " + language->ProgramToString(context_program);
}

