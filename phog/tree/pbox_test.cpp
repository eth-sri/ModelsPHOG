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

#include "glog/logging.h"
#include "json/json.h"

#include "base/stringset.h"
#include "pbox.h"

#include "external/gtest/googletest/include/gtest/gtest.h"

TEST(PBoxTest, ContinuationTest) {
  FLAGS_smoothing_type = KneserNey;
  PerFeatureValueCounter<SequenceHashFeature, int> counts_;

  {
    SequenceHashFeature f;
    f.PushBack(1);
    f.PushBack(2);
    counts_.AddValue(f, 10, 3);
  }

  {
    SequenceHashFeature f;
    f.PushBack(1);
    f.PushBack(3);
    counts_.AddValue(f, 10, 2);
  }

  {
    SequenceHashFeature f;
    f.PushBack(1);
    f.PushBack(2);
    counts_.AddValue(f, 11, 1);
  }

  {
    SequenceHashFeature f;
    f.PushBack(1);
    f.PushBack(3);
    counts_.AddValue(f, 11, 1);
  }

  {
    SequenceHashFeature f;
    f.PushBack(1);
    f.PushBack(4);
    counts_.AddValue(f, 11, 1);
  }

  {
    SequenceHashFeature f;
    counts_.AddValue(f, 11, 1);
    counts_.AddValue(f, 10, 2);
  }

  counts_.EndAdding();

  {
    SequenceHashFeature f;
    f.PushBack(1);
    f.PushBack(4);
    EXPECT_EQ(5, counts_.GetTotalPrefixCount(f));
    EXPECT_EQ(2, counts_.GetValuePrefixCount(f, 10));
    EXPECT_EQ(3, counts_.GetValuePrefixCount(f, 11));
  }

  {
    SequenceHashFeature f;
    f.PushBack(1);
    EXPECT_EQ(0, counts_.GetTotalPrefixCount(f));
    EXPECT_EQ(0, counts_.GetValuePrefixCount(f, 10));
    EXPECT_EQ(0, counts_.GetValuePrefixCount(f, 11));
  }

  {
    SequenceHashFeature f;
    EXPECT_EQ(2, counts_.GetTotalPrefixCount(f));
    EXPECT_EQ(1, counts_.GetValuePrefixCount(f, 10));
    EXPECT_EQ(1, counts_.GetValuePrefixCount(f, 11));
  }
}

int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

