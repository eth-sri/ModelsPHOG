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

#include "base/strutil.h"

void TGenProgram::LoadFromStringOrDie(TCondLanguage* lang, const std::string& str) {
  Clear();
  std::vector<std::string> lines;
  SplitStringUsing(str, '\n', &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = TrimLeadingAndTrailingSpaces(lines[i]);
    if (line.empty()) continue;
    if (strncmp(line.c_str(), "switch", 6) == 0) {
      BranchCondProgram p;
      p.ParseAsProgramLineOrDie(lang, line);
      AddProgram(p);
    } else {
      SimpleCondProgram p;
      p.ParseFromStringOrDie(lang, line);
      AddProgram(p);
    }
  }
}

std::string TGenProgram::SaveToString(const TCondLanguage* lang) const {
  std::string result;
  for (size_t i = 0; i < size(); ++i) {
    result += SaveProgramAtPosToString(i, lang);
    result += "\n";
  }
  return result;
}

std::string TGenProgram::SaveProgramAtPosToString(int pos, const TCondLanguage* lang) const {
  switch (program_type(pos)) {
  case ProgramType::BRANCHED_PROGRAM:
    return branched_prog(pos).ToStringAsProgramLine(lang);
  case ProgramType::SIMPLE_PROGRAM:
    return simple_prog(pos).ToString(lang);
  }
  return "";
}

int TGenProgram::FindProgram(const BranchCondProgram& prog) const {
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].type == ProgramType::BRANCHED_PROGRAM && branched_progs_[entries_[i].program_internal_index] == prog) {
      return i;
    }
  }
  return -1;
}

int TGenProgram::FindProgram(const SimpleCondProgram& prog) const {
  for (size_t i = 0; i < entries_.size(); i++) {
    if (entries_[i].type == ProgramType::SIMPLE_PROGRAM && simple_progs_[entries_[i].program_internal_index] == prog) {
      return i;
    }
  }
  return -1;
}

size_t TGenProgram::AddProgramNoDuplicates(const SimpleCondProgram& prog) {
  // Check for duplicates
  int pos = FindProgram(prog);
  if (pos != -1) {
    return pos;
  }
  return AddProgram(prog);
}

size_t TGenProgram::AddProgramNoDuplicates(const BranchCondProgram& prog) {
  // Check for duplicates
  int pos = FindProgram(prog);
  if (pos != -1) {
    return pos;
  }
  return AddProgram(prog);
}

size_t TGenProgram::AddProgram(const BranchCondProgram& prog) {
  InternalEntry e;
  e.type = ProgramType::BRANCHED_PROGRAM;
  e.program_internal_index = branched_progs_.size();
  entries_.push_back(e);
  branched_progs_.push_back(prog);
  return entries_.size() - 1;
}

size_t TGenProgram::AddProgram(const SimpleCondProgram& prog) {
  InternalEntry e;
  e.type = ProgramType::SIMPLE_PROGRAM;
  e.program_internal_index = simple_progs_.size();
  entries_.push_back(e);
  simple_progs_.push_back(prog);
  return entries_.size() - 1;
}

size_t TGenProgram::GetProgramRecursiveSize(int pos) const {
  if (program_type(pos) == ProgramType::BRANCHED_PROGRAM) {
    const BranchCondProgram& program = branched_prog(pos);

    int size = program.cond.program.size();
    std::set<int> programs_set;
    program.GetReferencedPrograms(&programs_set);
    for (int prog : programs_set) {
      size += GetProgramRecursiveSize(prog);
    }
    return size;
  } else {
    CHECK(program_type(pos) == ProgramType::SIMPLE_PROGRAM);
    return simple_prog(pos).size();
  }
}


void TGenProgram::Clear() {
  entries_.clear();
  branched_progs_.clear();
  simple_progs_.clear();
}


namespace TGen {
void LoadTGen(TCondLanguage* lang, TGenProgram* prog, std::string file_name) {
  std::string lines;
  LOG(INFO) << "Loading TGen program from " << file_name;
  ReadFileToStringOrDie(file_name.c_str(), &lines);
  prog->LoadFromStringOrDie(lang, lines);
  // LOG(INFO) << "Loaded program:\n" << prog->SaveToString(lang);
}

void SaveTGen(TCondLanguage* lang, const TGenProgram& prog, std::string file_name) {
  std::ofstream f;
  f.open(file_name);
  if (f.is_open()) {
    f << prog.SaveToString(lang);
  } else {
    LOG(FATAL) << "Cannot open file '" << file_name << "'!";
  }
  f.close();
}
} // namespace
