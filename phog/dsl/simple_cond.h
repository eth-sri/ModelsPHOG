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

#ifndef SYNTREE_SIMPLE_COND_H_
#define SYNTREE_SIMPLE_COND_H_

#include "tcond_language.h"

#include <vector>

#include "base/base.h"

#include "phog/tree/pbox.h"
#include "phog/tree/tree.h"

// Class to describe simple tree conditioning without branches.
class SimpleCondProgram {
public:
  SimpleCondProgram() {}
  explicit SimpleCondProgram(const TCondLanguage::Program& eq, const TCondLanguage::Program& context) : eq_program(eq), context_program(context) {}
  explicit SimpleCondProgram(const TCondLanguage::Program& context) : context_program(context) {}

  // Program that generates equality labels.
  TCondLanguage::Program eq_program;
  // Program that generates conditioning context.
  TCondLanguage::Program context_program;

  void ParseFromStringOrDie(const TCondLanguage* language, const std::string& str);
  std::string ToString(const TCondLanguage* language) const;
  void Optimize(const TCondLanguage* language);
  bool IsValid(const TCondLanguage* language) const;
  bool operator==(const SimpleCondProgram& o) const { return eq_program == o.eq_program && context_program == o.context_program; }
  bool operator!=(const SimpleCondProgram& o) const { return !(*this == o); }
  bool operator<(const SimpleCondProgram& o) const {
    if (eq_program != o.eq_program) return eq_program < o.eq_program;
    return context_program < o.context_program;
  }
  size_t size() const { return eq_program.size() + context_program.size(); }
};


#endif /* SYNTREE_SIMPLE_COND_H_ */
