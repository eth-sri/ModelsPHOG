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

#include "base/strutil.h"


std::string TCondLanguage::ProgramToString(const TCondLanguage::Program& p) const {
  std::string result;
  bool first = true;
  for (Op op : p) {
    if (!first) result.append(" ");
    first = false;

    result.append(OpCmdStr(op.cmd));
    if (op.extra_data != -1) {
      StringAppendF(&result, "@%d", op.extra_data);
    }
  }
  return result;
}

TCondLanguage::Program TCondLanguage::ParseStringToProgramOrDie(const std::string& s) const {
  std::vector<std::string> op_strs;
  if (!s.empty())
    SplitStringUsing(s, ' ', &op_strs);
  Program p;
  for (const std::string& op_str : op_strs) {
    std::vector<std::string> op_parts;
    SplitStringUsing(op_str, '@', &op_parts);
    for (OpCmd i = OpCmd::WRITE_TYPE; i <= OpCmd::LAST_OP_CMD; i = static_cast<OpCmd>(static_cast<int>(i) + 1)) {
      CHECK(i != OpCmd::LAST_OP_CMD) << "Invalid op " << op_str << " in \"" << s << "\"";
      if (OpCmdStr(i) == op_parts[0]) {
        Op op;
        op.cmd = i;
        if (op_parts.size() > 1) {
          op.extra_data = std::stoi(op_parts[1]);
        } else {
          op.extra_data = -1;
        }
        p.push_back(op);
        break;
      }
    }
  }

  return p;
}



