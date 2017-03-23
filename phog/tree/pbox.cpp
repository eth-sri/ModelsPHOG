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

#include "pbox.h"

DEFINE_int32(smoothing_type, 0, "Smoothing type to use. Either of:\n"
    "  --smoothing_type=0           WittenBell (default)\n"
    "  --smoothing_type=1           KneserNey\n"
    "  --smoothing_type=2           Laplace\n"
    );

DEFINE_double(kneser_ney_d, -1, "Delta used with KneserNey smoothing. Should be in the range <0,1>. If set to -1 (default) it is determined automatically.");

template<class V>
std::string DebugValue(const V* value, const StringSet* ss) {
  return value->DebugString(ss);
}

template<>
std::string DebugValue<int>(const int* value, const StringSet* ss) {
  if (ss == nullptr) {
    return StringPrintf("%d", *value);
  } else {
    return std::string(ss->getString(*value));
  }
}
