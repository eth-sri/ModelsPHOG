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

#ifndef SYNTREE_PBOX_H_
#define SYNTREE_PBOX_H_

#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <iostream>

#include "base/sparsehash/dense_hash_map.h"
#include "base/stringprintf.h"
#include "tree.h"
#include "tree_slice.h"

#include "gflags/gflags.h"

DECLARE_int32(smoothing_type);
DECLARE_double(kneser_ney_d);

template<class V>
std::string DebugValue(const V* value, const StringSet* ss);

template<>
std::string DebugValue<int>(const int* value, const StringSet* ss);

/*******
 * Every Value type used in the ValueCounter should define empty_key and deleted_key in order to be used with google hash map
 */

template<typename _Tp>
    struct empty_key;

template<typename _Tp>
    struct deleted_key;

template <> struct empty_key<int> {
  int operator()() const {
    return -10;
  }
};

template <> struct deleted_key<int> {
  int operator()() const {
    return -11;
  }
};

template <> struct empty_key<TreeSubstitution::Node> {
  TreeSubstitution::Node operator()() const {
    return {-10, -10, -10, -10};
  }
};

template <> struct deleted_key<TreeSubstitution::Node> {
  TreeSubstitution::Node operator()() const {
    return {-11, -11, -11, -11};
  }
};

template <> struct empty_key<TreeSubstitutionOnlyLabel> {
  TreeSubstitutionOnlyLabel operator()() const {
    return {-10, false, false};
  }
};

template <> struct deleted_key<TreeSubstitutionOnlyLabel> {
  TreeSubstitutionOnlyLabel operator()() const {
    return {-11, false, false};
  }
};


// Simple class that counts the number of times something happens.
template<class X>
class ValueCounter {
public:
  ValueCounter() {}
  ValueCounter(const ValueCounter&) = delete;

  void AddValue(const X& value, int count) {
    CHECK(data_.sorted_by_prob_.empty()) << "AddValue after EndAdding";
    data_.values_[value].Add(count);
    data_.total_count_ += count;
  }

  void EndAdding() {
    CHECK(data_.sorted_by_prob_.empty()) << "EndAdding called twice.";
    data_.sorted_by_prob_.reserve(data_.values_.size());
    double z = (data_.total_count_ + 1.0 * (data_.values_.size() + 1));
    for (auto it = data_.values_.begin(); it != data_.values_.end(); ++it) {
      it->second.prob_ = log2((it->second.count_ + 1.0) / z);
      data_.sorted_by_prob_.emplace_back(it->second.prob_, &it->first);
    }
    data_.unmet_log_prob_ = log2(1 / z);
    std::sort(data_.sorted_by_prob_.begin(), data_.sorted_by_prob_.end(), std::greater<std::pair<double, const X*> >());
  }

  double EstimateLogProbability(const X& value) const {
    auto it = data_.values_.find(value);
    if (it == data_.values_.end()) {
      return data_.unmet_log_prob_;
    }
    return it->second.prob_;
//    return log2(GetMLProb(value));
  }

  double MaxLogProbability() const {
    return data_.sorted_by_prob_.empty() ? data_.unmet_log_prob_ : data_.sorted_by_prob_[0].first;
  }

  const std::vector<std::pair<double, const X*> >& sorted_by_prob() const {
    return data_.sorted_by_prob_;
  }

  double UnknownLabelLogProbability() const {
    return data_.unmet_log_prob_;
  }

  double GetMLProb(const X& value) const {
    double z = (data_.total_count_ + 1.0 * data_.values_.size() + 1);
    auto it = data_.values_.find(value);
    if (it == data_.values_.end()) {
      return 1 / z;
    }
    return (it->second.count_ + 1)/ z;
  }

  int GetTotalCount() const {
    return data_.total_count_;
  }

  int GetUniqueCount() const {
    return data_.values_.size();
  }

private:
  struct CounterValue {
    double prob_;
    int count_;

    void Add(int count){
      count_ += count;
    }
  };

  struct Data {
    google::dense_hash_map<X, CounterValue, std::hash<X> > values_;
    std::vector<std::pair<double, const X*> > sorted_by_prob_;
    double unmet_log_prob_;
    int total_count_;

    Data() : unmet_log_prob_(0.0), total_count_(0) {
      values_.set_empty_key(empty_key<X>()());
      values_.set_deleted_key(deleted_key<X>()());
    }
  };

  Data data_;
};


class KneserNeyDelta {
public:
  KneserNeyDelta() : deltas_estimated_(false) {
    clear();
  };

  void clear() {
    for (int i = 0; i < 5; i++) {
      deltas_[std::min(i,3)] = 0;
      counts_[i] = 0;
    }
    deltas_estimated_ = false;
  }

  double GetDelta(int count) const {
    CHECK(deltas_estimated_);
    return deltas_[std::min(count,3)];
  }

  void AddCount(int count) {
    CHECK(count > 0);
    counts_[std::min(count,4)]++;
  }

  void EndAdding() {
    deltas_estimated_ = true;
    LOG(INFO) << "n1: " << counts_[1] << ", n2: " << counts_[2] << ", n3: " << counts_[3] << ", n4: " << counts_[4];
    if (counts_[1] != 0 || counts_[2] != 0) {
      double Y = static_cast<double>(counts_[1]) / (counts_[1] + 2*counts_[2]);
      if (counts_[1] != 0) {
        deltas_[1] = 1 - 2*Y*(static_cast<double>(counts_[2]) / counts_[1]);
      }
      if (counts_[2] != 0) {
        deltas_[2] = 2 - 3*Y*(static_cast<double>(counts_[3]) / counts_[2]);
      }
      if (counts_[3] != 0) {
        deltas_[3] = 3 - 4*Y*(static_cast<double>(counts_[4]) / counts_[3]);
      }
    }
    for (int i = 0; i < 4; i++) {
      deltas_[i] = std::max(std::min(deltas_[i], 1.0), 0.0);
    }
    LOG(INFO) << "delta_1 = " << deltas_[1] << ", delta_2: " << deltas_[2] << ", delta_3: " << deltas_[3];
  }


private:
  bool deltas_estimated_;
  int counts_[5];
  double deltas_[4];
};

enum SmoothingTypes {
  WittenBell = 0,
  KneserNey,
  Laplace
};

class Smoothing {
public:
  Smoothing() : prob_(0), prob_tmp_(0) {}

  void SetUnconditionedProb(int count, int unique_count, int total_count, int prefix_count, int total_prefix_count) {
    prob_ = (count + 1.0) / (total_count + unique_count + 1.0);
    if (FLAGS_smoothing_type == KneserNey) {
      CHECK(prefix_count <= 1);
      prob_tmp_ = (prefix_count + 1.0) / (prefix_count + total_prefix_count + 1.0);
    }
  }

  void AddForwardBackoff(int count, int unique_count, int total_count, const std::vector<int>* counts, int prefix_count, int total_prefix_count,
      const KneserNeyDelta* delta) {
    switch (FLAGS_smoothing_type) {
    case WittenBell:
    {
      double p_ml = static_cast<double>(count) / total_count;
      double lambda = 1 - (unique_count / (double) (unique_count + total_count));
      CHECK(p_ml >= 0 && p_ml <= 1);

      prob_ = lambda * p_ml + (1 - lambda) * prob_;
      break;
    }
    case KneserNey:
    {
      if (FLAGS_kneser_ney_d != -1) {
        // Higher order feature, use counts
        {
          double lambda = static_cast<double>(unique_count * FLAGS_kneser_ney_d) / total_count;
          double p_ml = std::max(static_cast<double>(count) - FLAGS_kneser_ney_d, 0.0) / total_count;
          CHECK(p_ml >= 0 && p_ml <= 1);

          prob_ = p_ml + lambda * prob_tmp_;
        }

        // Lower order feature, use continuation
        {
          double lambda = static_cast<double>(prefix_count * FLAGS_kneser_ney_d) / total_prefix_count;
          prob_tmp_ = std::max(static_cast<double>(prefix_count) - FLAGS_kneser_ney_d, 0.0) / total_prefix_count + lambda * prob_tmp_;
        }
      } else {
        CHECK(delta != nullptr);

        // Higher order feature, use counts
        {
          double lambda = (delta->GetDelta(1) * counts->at(1) + delta->GetDelta(2) * counts->at(2) + delta->GetDelta(3) * counts->at(3)) / total_count;
          double p_ml = std::max(static_cast<double>(count) - delta->GetDelta(count), 0.0) / total_count;
          CHECK(p_ml >= 0 && p_ml <= 1);

          prob_ = p_ml + lambda * prob_tmp_;
          // Avoid asigning zero probability, not part of Kneser-Ney Smoothing
          if (prob_ == 0.0) {
            prob_ = (1.0 + count) / (1.0 + unique_count + total_count);
          }
        }

        // Lower order feature, use continuation
        {
          double lambda = (delta->GetDelta(1) * counts->at(1) + delta->GetDelta(2) * counts->at(2) + delta->GetDelta(3) * counts->at(3)) / total_prefix_count;
          prob_tmp_ = std::max(static_cast<double>(prefix_count) - delta->GetDelta(prefix_count), 0.0) / total_prefix_count + lambda * prob_tmp_;
        }
      }
      break;
    }
    case Laplace:
    {
      prob_ = static_cast<double>(count + 1) / (total_count + unique_count + 1);
      break;
    }
    default:
      LOG(FATAL) << "Unknown smoothing type " << FLAGS_smoothing_type;

    }
  }

  double GetLogProb() const {
    return log2(prob_);
  }

  double GetProb() const {
    return prob_;
  }

private:
  double prob_;

  // helper probability for kneser ney smoothing
  double prob_tmp_;
};




class SingleNodeSubstitutionLabel {
public:
  typedef TreeSubstitution::Node Label;

  Label GetLabelAtNode(const TreeStorage* t, int node_id) const {
    const TreeNode& node = t->node(node_id);
    return TreeSubstitution::Node({node.Type(), node.Value(), node.first_child == -1 ? -1 : -2, node.right_sib == -1 ? -1 : -2});
  }

  bool OutputLabelToTree(const TreeStorage* t, int node_id, const Label& l, double score,
      const std::function<void(TreeStorage&& tree, int position, double pbox_score)>& callback) const {
    if (!t->CanSubstituteSingleNode(node_id, l))
      return false;
    TreeStorage completion(*t);
    completion.SubstituteSingleNode(node_id, l);
    callback(std::move(completion), node_id, score);
    return true;
  }
};






/***************************************/

struct NumberFeature {
  explicit NumberFeature(int v) : value(v) { }
  NumberFeature(const NumberFeature& o) = default;
  bool operator==(const NumberFeature& o) const { return value == o.value; }

  static NumberFeature FullyBackoffed() {
    return NumberFeature(-1);
  }

  bool Backoff() {
    if (value == -1) return false;
    value = -1;
    return true;
  }

  std::string DebugString(const StringSet* ss) const {
    return StringPrintf("%d", value);
  }

  int value;
};

struct SequenceFeature {
  SequenceFeature() {}
  explicit SequenceFeature(std::initializer_list<int> init) : data(init) {}
  explicit SequenceFeature(std::vector<int>&& v) : data(v) {}
  SequenceFeature(const SequenceFeature& o) = default;
  SequenceFeature(SequenceFeature&& o) = default;

  bool operator==(const SequenceFeature& o) const { return data == o.data; }

  static SequenceFeature FullyBackoffed() {
    return SequenceFeature();
  }

  bool Backoff() {
    if (data.empty()) return false;
    data.pop_back();
    return true;
  }

  std::string DebugString(const StringSet* ss) const {
    std::string result = "[";
    for (size_t i = 0; i < data.size(); ++i) {
      if (i != 0) result.append(",");
      if (data[i] >= 0 && ss != nullptr)
        result.append(ss->getString(data[i]));
      else
        StringAppendF(&result, "%d", data[i]);
    }
    result.append("]");
    return result;
  }

  std::vector<int> data;
};

template<int Bound>
struct BoundedSequenceFeature {
  BoundedSequenceFeature() : len(0) { }
  BoundedSequenceFeature(const BoundedSequenceFeature<Bound>& o) = default;
  BoundedSequenceFeature(BoundedSequenceFeature<Bound>&& o) = default;

  bool operator==(const BoundedSequenceFeature<Bound>& o) const {
    return len == o.len && memcmp(data, o.data, sizeof(int) * len) == 0;
  }

  void PushBack(int value) {
    data[len++] = value;
    DCHECK_LE(len, Bound);
  }

  bool Backoff() {
    if (len == 0) return false;
    --len;
    return true;
  }

  static BoundedSequenceFeature<Bound> FullyBackoffed() {
    return BoundedSequenceFeature<Bound>();
  }

  std::string DebugString(const StringSet* ss) const {
    std::string result = "[";
    for (int i = 0; i < len; i++){
      if (i != 0) result.append(",");
      if (data[i] >= 0 && ss != nullptr)
        result.append(ss->getString(data[i]));
      else
        StringAppendF(&result, "%d", data[i]);
    }
    result.append("]");
    return result;
  }

  int len;
  int data[Bound];
};


class SequenceHashFeature {
public:
  SequenceHashFeature() : hash_(0), size_(0) {}

  static SequenceHashFeature EmptyFeature() {
    return SequenceHashFeature(-1, 0);
  }

  static SequenceHashFeature DeletedFeature() {
    return SequenceHashFeature(-2, 0);
  }

  bool operator==(const SequenceHashFeature& o) const {
     return hash_ == o.hash_;
   }

  void PushBack(int value) {
    hash_ = FingerprintCat(hash_, value);
    hash_ = std::abs(hash_);
    size_++;
  }

  int size() const {
    return size_;
  }

  void WriteToFileOrDie(FILE* f) const {
    CHECK_EQ(1, fwrite(&hash_, sizeof(int), 1, f));
  }

  void ReadFromFileOrDie(FILE* f) {
    CHECK_EQ(1, fread(&hash_, sizeof(int), 1, f));
  }

  int hash_;
  int size_;
private:
  SequenceHashFeature(int hash, int size) : hash_(hash), size_(size) {}
};

template <> struct empty_key<SequenceHashFeature> {
  SequenceHashFeature operator()() const {
    return SequenceHashFeature::EmptyFeature();
  }
};

template <> struct deleted_key<SequenceHashFeature> {
  SequenceHashFeature operator()() const {
    return SequenceHashFeature::DeletedFeature();
  }
};

namespace std {
template <> struct hash<SequenceHashFeature> {
  size_t operator()(const SequenceHashFeature& x) const {
    return x.hash_;
  }
};

template <> struct hash<std::pair<SequenceHashFeature, int> > {
  size_t operator()(const std::pair<SequenceHashFeature, int>& x) const {
    return FingerprintCat(std::hash<SequenceHashFeature>()(x.first), x.second);
  }
};
}

namespace std {
template <> struct hash<NumberFeature> {
  size_t operator()(const NumberFeature& x) const {
    return hash<int>()(x.value);
  }
};

template <> struct hash<SequenceFeature> {
  size_t operator()(const SequenceFeature& x) const {
    unsigned hash = 0;
    for (int d : x.data) {
      hash = FingerprintCat(hash, d);
    }
    return hash;
  }
};

template <int Bound> struct hash<BoundedSequenceFeature<Bound> > {
  size_t operator()(const BoundedSequenceFeature<Bound>& x) const {
    unsigned hash = 0;
    for (int i = 0; i < x.len; ++i) {
      hash = FingerprintCat(hash, x.data[i]);
    }
    return hash;
  }
};
}


template<class F, class V>
class PerFeatureValueCounter {
public:

  class ValueStats {
  public:
    ValueStats() : total_count_(0) {}

    void AddFeatureForValue(const V& value) {
      per_value_continuations_[value] += 1;
      total_count_++;
    }

    int GetValuePrefixCount(const V& value) const {
      const auto it = per_value_continuations_.find(value);
      if (it != per_value_continuations_.end()) {
        return it->second;
      }
      return 0;
    }

    int TotalPrefixCount() const {
      return total_count_;
    }

    std::unordered_map<V, int> per_value_continuations_;
  private:
    int total_count_;
  };

  class FeatureStats {
  public:
    FeatureStats() : total_count_(0), unique_count_(0), counts_(4,0) {}

    int TotalCount() const {
      return total_count_;
    }

    int UniqueLabels() const {
      return unique_count_;
    }

    const std::vector<int>* GetCounts() const {
      return &counts_;
    }

    std::string DebugString(const StringSet* ss = nullptr) const {
      std::string result;
      int i = 0;
      for (auto it = sorted_by_prob_.begin(); it != sorted_by_prob_.end(); it++){
        StringAppendF(&result, "\t%f -> %s\n", it->first, DebugValue(it->second, ss).c_str()); //it->second->DebugString(ss).c_str());
        if (i++ > 100) {
          result.append("\t...\n");
          break;
        }
      }

      return result;
    }

  private:
    friend class PerFeatureValueCounter<F, V>;

    int total_count_;
    int unique_count_;
    std::vector<std::pair<double, const V*> > sorted_by_prob_;
    std::vector<int> counts_;

    void AddValue(int count, const V* value) {
      total_count_ += count;
      unique_count_++;
      // We start by storing the counts and calculate the prob later
      // when we collected the total_count and unique_count
      sorted_by_prob_.push_back(std::pair<double, const V*>(count, value));
      counts_[std::min(count, 3)]++;
    }

    void CalculateProb() {
      for (auto it = sorted_by_prob_.begin(); it != sorted_by_prob_.end(); it++){
        it->first = GetMLProb(it->first);
      }
    }

    void SortValues() {
      std::sort(sorted_by_prob_.begin(), sorted_by_prob_.end(), std::greater<std::pair<double, const V*> >());
    }

    double GetMLProb(int count) const {
      return static_cast<double>(count) / static_cast<double>(total_count_);
    }

    double GetLaplaceSmoothedMLProb(int count) const {
      return (count + 1.0) / (total_count_ + unique_count_ + 1.0);
    }
  };

private:
  google::dense_hash_map<std::pair<F, V>, int, std::hash<std::pair<F, V> > > feature_value_counts_;
  std::unordered_map<F, FeatureStats> feature_stats_;
  std::unordered_map<int, ValueStats> value_stats_;
  std::unordered_map<int, KneserNeyDelta> deltas_;
  const std::vector<std::pair<double, const V*> > empty_vec_;

public:
  PerFeatureValueCounter() {
    feature_value_counts_.set_empty_key(std::pair<F, V>(empty_key<F>()(), empty_key<V>()()));
    feature_value_counts_.set_deleted_key(std::pair<F, V>(deleted_key<F>()(), deleted_key<V>()()));
  }

  void AddValue(const F& feature, const V& value, int count) {
    feature_value_counts_[std::pair<F, V>(feature, value)] += count;
  }

  void EndAdding() {
    feature_stats_.clear();
    value_stats_.clear();
    deltas_.clear();

    int max_feature_size = -1;
    for (auto it = feature_value_counts_.begin(); it != feature_value_counts_.end(); it++){
      feature_stats_[it->first.first].AddValue(it->second, &it->first.second);

      max_feature_size = std::max(max_feature_size, it->first.first.size());
      // Value stats and deltas are only used for continuation counts for Kneser-Ney smoothing
      if (FLAGS_smoothing_type == KneserNey) {
        value_stats_[it->first.first.size()].AddFeatureForValue(it->first.second);
        deltas_[it->first.first.size()].AddCount(it->second);
      }
    }

    if (FLAGS_smoothing_type == KneserNey) {
      //We want the higher order of deltas be estimated from value counts, the rest from prefix counts
      for (const auto it : value_stats_) {
        LOG(INFO) << "Estimates for order " << it.first;
        auto delta_it = deltas_.find(it.first);
        CHECK(delta_it != deltas_.end());
        if (it.first == max_feature_size) {
          delta_it->second.EndAdding();
          continue;
        }

        delta_it->second.clear();
        for (const auto prefix : it.second.per_value_continuations_) {
          delta_it->second.AddCount(prefix.second);
        }
        delta_it->second.EndAdding();
      }

    }

    for (auto it = feature_stats_.begin(); it != feature_stats_.end(); it++) {
      it->second.CalculateProb();
      it->second.SortValues();
    }
  }

  size_t Size() const {
    return feature_stats_.size();
  }

  // Reading out the data:
  unsigned NumFeatureValues() const {
    return feature_value_counts_.size();
  }

  // CB(F feature, V value, int count);
  template<class CB>
  void ForEachFeatureValue(const CB& f) const {
    for (auto it = feature_value_counts_.begin(); it != feature_value_counts_.end(); it++) {
      f(it->first.first, it->first.second, it->second);
    }
  }

  std::string DebugString(const StringSet* ss) const {
    std::string result;
    for (auto it = feature_stats_.begin(); it != feature_stats_.end(); it++) {
      StringAppendF(&result, "Feature: \n%s", it->second.DebugString(ss).c_str());
    }

    return result;
  }

  const FeatureStats* GetFeatureStatsOrNull(const F& feature) const {
    auto it = feature_stats_.find(feature);
    if (it == feature_stats_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  const std::vector<std::pair<double, const V*> >& LabelsSortedByProbability(const F& feature) const {
    auto it = feature_stats_.find(feature);
    if (it == feature_stats_.end()) {
      return empty_vec_;
    }
    return it->second.sorted_by_prob_;
  }

  int GetValuePrefixCount(const F& feature, const V& value) const {
    if (FLAGS_smoothing_type != KneserNey) {
      return 0;
    }
    const auto it = value_stats_.find(feature.size());
    if (it != value_stats_.end()) {
      return it->second.GetValuePrefixCount(value);
    }
    return 0;
  }

  int GetTotalPrefixCount(const F& feature) const {
    if (FLAGS_smoothing_type != KneserNey) {
      return 0;
    }
    const auto it = value_stats_.find(feature.size());
    if (it != value_stats_.end()) {
      return it->second.TotalPrefixCount();
    }
    return 0;
  }

  const KneserNeyDelta* GetKneserNeyDelta(const F& feature) const {
    if (FLAGS_smoothing_type != KneserNey) {
      return nullptr;
    }
    const auto it = deltas_.find(feature.size());
    CHECK(it != deltas_.end());
    return &it->second;
  }

  double GetMLProb(const F& feature, const V& value, const FeatureStats* feature_stats) const {
    return feature_stats->GetMLProb(GetCount(feature, value));
  }

  double GetLaplaceSmoothedMLProb(const F& feature, const V& value, const FeatureStats* feature_stats) const {
    return feature_stats->GetLaplaceSmoothedMLProb(GetCount(feature, value));
  }

  int GetCount(const F& feature, const V& value) const {
    auto it = feature_value_counts_.find(std::pair<F, V>(feature, value));
    if (it == feature_value_counts_.end()) {
      return 0;
    }
    return it->second;
  }
};

#endif /* SYNTREE_PBOX_H_ */
