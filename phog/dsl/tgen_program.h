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

#ifndef SYNTREE_TGEN_PROGRAM_H_
#define SYNTREE_TGEN_PROGRAM_H_

#include <string>
#include <vector>

#include "branched_cond.h"
#include "simple_cond.h"

#include "base/fileutil.h"
#include <fstream>

// A TGen program contains a sequence of SimpleCond or BranchedCond programs
// addressable by index.
class TGenProgram {
public:
  enum class ProgramType {
    SIMPLE_PROGRAM,
    BRANCHED_PROGRAM,
  };

  struct Entry {
    explicit Entry(const BranchCondProgram* bp) : type(ProgramType::BRANCHED_PROGRAM), branched_program(bp) {}
    explicit Entry(const SimpleCondProgram* sp) : type(ProgramType::SIMPLE_PROGRAM), simple_program(sp) {}

    ProgramType type;
    union {
      const BranchCondProgram* branched_program;
      const SimpleCondProgram* simple_program;
    };
  };

  void LoadFromStringOrDie(TCondLanguage* lang, const std::string& str);
  std::string SaveToString(const TCondLanguage* lang) const;
  std::string SaveProgramAtPosToString(int pos, const TCondLanguage* lang) const;

  // Adds a program and returns the index at which it was added.
  // The index is always equal to size(), and size() increases.
  size_t AddProgram(const BranchCondProgram& prog);
  size_t AddProgram(const SimpleCondProgram& prog);

  // Adds the program only if it's not already contained
  size_t AddProgramNoDuplicates(const SimpleCondProgram& prog);
  size_t AddProgramNoDuplicates(const BranchCondProgram& prog);

  int FindProgram(const BranchCondProgram& prog) const;
  int FindProgram(const SimpleCondProgram& prog) const;

  void Clear();

  size_t GetProgramRecursiveSize(int pos) const;

  size_t size() const {
    return entries_.size();
  }
  ProgramType program_type(int pos) const {
    CHECK((size_t) pos < entries_.size());
    return entries_[pos].type;
  }
  // Gets the branched program if program_type(pos) is ProgramType::BRANCHED_PROGRAM.
  BranchCondProgram* mutable_branched_prog(int pos) {
    CHECK((size_t) pos < entries_.size());
    CHECK(entries_[pos].type == ProgramType::BRANCHED_PROGRAM);
    return &branched_progs_[entries_[pos].program_internal_index];
  }
  const BranchCondProgram& branched_prog(int pos) const {
    CHECK((size_t) pos < entries_.size());
    CHECK(entries_[pos].type == ProgramType::BRANCHED_PROGRAM);
    return branched_progs_[entries_[pos].program_internal_index];
  }
  // Gets the simple program if program_type(pos) is ProgramType::SIMPLE_PROGRAM.
  SimpleCondProgram* mutable_simple_prog(int pos) {
    CHECK((size_t) pos < entries_.size());
    CHECK(entries_[pos].type == ProgramType::SIMPLE_PROGRAM);
    return &simple_progs_[entries_[pos].program_internal_index];
  }
  const SimpleCondProgram& simple_prog(int pos) const {
    CHECK((size_t) pos < entries_.size());
    CHECK(entries_[pos].type == ProgramType::SIMPLE_PROGRAM);
    return simple_progs_[entries_[pos].program_internal_index];
  }
  Entry program_at(int pos) const {
    CHECK((size_t) pos < entries_.size());
    if (program_type(pos) == ProgramType::SIMPLE_PROGRAM) {
      return Entry(&simple_progs_[entries_[pos].program_internal_index]);
    }
    return Entry(&branched_progs_[entries_[pos].program_internal_index]);
  }

private:
  // InernalEntry has index to the vectors instead of pointers, because growing
  // the vectors would invalidate the pointers.
  struct InternalEntry {
    ProgramType type;
    int program_internal_index;
  };

  std::vector<InternalEntry> entries_;
  std::vector<BranchCondProgram> branched_progs_;
  std::vector<SimpleCondProgram> simple_progs_;
};

namespace TGen {
void LoadTGen(TCondLanguage* lang, TGenProgram* prog, std::string file_name);
void SaveTGen(TCondLanguage* lang, const TGenProgram& prog, std::string file_name);
} // namespace


#endif /* SYNTREE_TGEN_PROGRAM_H_ */
