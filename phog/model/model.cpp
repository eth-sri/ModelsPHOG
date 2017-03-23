/*
   Copyright 2017 Veselin Raychev

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


#include "model.h"

#include "glog/logging.h"

DEFINE_bool(enable_teq, true, "Enable using TEq programs");
DEFINE_int32(beam_size, 4, "Number of best labels to try at each model order.");

const int TEQ_LABEL_INDEX_START = -10;
const int TEQ_MAX_LABEL_INDEX = 10;


DEFINE_string(default_tgen_eval_metric, "entropy",
    "The metric used by default in most of the code: entropy, errorrate or confidence50");

Metric ResolveDefaultTGenEvalMetric(Metric metric) {
  if (metric != Metric::DEFAULT)
    return metric;  // Keep it as original.
  if (FLAGS_default_tgen_eval_metric == "entropy") {
    return Metric::ENTROPY;
  } else if (FLAGS_default_tgen_eval_metric == "errorrate") {
    return Metric::ERROR_RATE;
  } else if (FLAGS_default_tgen_eval_metric == "confidence50") {
    return Metric::CONFIDENCE50;
  } else {
    LOG(FATAL) << "Unknown --default_tgen_eval_metric=\""
        << FLAGS_default_tgen_eval_metric << "\". Must be logperp or errorrate.";
  }
  return Metric::ENTROPY;
}


TGenModelEvaluationMetricComputation::TGenModelEvaluationMetricComputation(Metric metric)
    : metric_(ResolveDefaultTGenEvalMetric(metric)), value_(0), num_samples_(0) {
}

void TGenModelEvaluationMetricComputation::AddSample(
    const TGenModel* model,
    const TCondLanguage::ExecutionForTree& exec,
    int position_in_tree) {
  FullTreeTraversal sample(exec.tree(), position_in_tree);
  TreeSlice slice(exec.tree(), position_in_tree, !model->is_for_node_type());

  ++num_samples_;
  switch (metric_) {
  case Metric::ENTROPY:
    value_ -= model->GetLabelLogProb(model->start_program_id(),exec, sample, &slice);
    break;

  case Metric::ERROR_RATE:
    if (!model->IsLabelBestPrediction(model->start_program_id(),exec, sample, &slice)) {
      value_ = value_ + 1;  // Count errors.
    }
    break;

  case Metric::CONFIDENCE50:
    if (model->GetLabelLogProb(model->start_program_id(), exec, sample, &slice) <= -1) {
      value_ = value_ + 1;  // Log_2 (prob) of <= -1 (i.e. probability of <= 50%) is considered an error.
    }
    break;

  case Metric::DEFAULT:
    LOG(FATAL) << "Unresolved evaluation metric.";
  }
}

double TGenModelEvaluationMetricComputation::GetComputedValue() const {
  switch (metric_) {
  case Metric::ENTROPY:
  case Metric::ERROR_RATE:
  case Metric::CONFIDENCE50:
    return value_ / num_samples_;
  case Metric::DEFAULT:
    LOG(FATAL) << "Unresolved evaluation metric.";
  }
  return 0;
}


TGenModel::TGenModel(const TGenProgram& program, bool is_for_node_type)
    : program_(program), is_for_node_type_(is_for_node_type), counts_(program.size()) {
}

TGenModel::~TGenModel() {
}

void TGenModel::GenerativeTrainOneSample(
    int program_id,
    const TCondLanguage::ExecutionForTree& exec,
    FullTreeTraversal sample) {

  TreeSlice slice(sample.tree_storage(), sample.position(), !is_for_node_type_);

  size_t call_length = 0;
  while (program_.program_type(program_id) == TGenProgram::ProgramType::BRANCHED_PROGRAM) {
    program_id = GetSubmodelBranch(program_id, exec, sample, &slice);
    ++call_length;
    CHECK_LE(call_length, program_.size());
  }

  int label = GetLabelAtPosition(program_id, exec, sample, &slice);

  Feature f;
  // Record unconditioned feature:
  counts_[program_id].AddValue(f, label, 1);
  // Use conditioned features:
  SlicedTreeTraversal traversal(sample.tree_storage(), sample.position(), &slice);
  ExecuteContextProgramByIdInAll(
      &exec,
      &traversal, nullptr,
      program_id, &program_,
      [this, label, program_id, &f](int op_added)->bool {
    f.PushBack(op_added);
    counts_[program_id].AddValue(f, label, 1);
    return true;
  });
}

int TGenModel::GetLabelAtPosition(
    int program_id,
    const TCondLanguage::ExecutionForTree& exec,
    FullTreeTraversal sample,
    const TreeSlice* slice,
    bool use_teq) const {
  if (FLAGS_enable_teq && use_teq) {
    size_t call_length = 0;
    while (program_.program_type(program_id) == TGenProgram::ProgramType::BRANCHED_PROGRAM) {
      program_id = GetSubmodelBranch(program_id, exec, sample, slice);
      ++call_length;
      CHECK_LE(call_length, program_.size());
    }
  }

  const TreeNode& node = sample.node();
  int label = is_for_node_type_ ? node.Type() : node.Value();

  if (FLAGS_enable_teq && use_teq) {
    int op_count = 0;
    SlicedTreeTraversal traversal(sample.tree_storage(), sample.position(), slice);
    ExecuteEqProgramByIdInAll(
        &exec,
        &traversal, nullptr,
        program_id, &program_,
        [&label, &op_count](int op)->bool{
      if (label >= 0 && op == label && op_count < TEQ_MAX_LABEL_INDEX) {
        label = TEQ_LABEL_INDEX_START - op_count;
      }
      ++op_count;
      return true;
    });
  }
  if (is_for_node_type_) {
    label = EncodeTypeLabel(TreeSubstitutionOnlyLabel({label, node.first_child != -1, node.right_sib != -1}));
  }
  return label;
}


void TGenModel::GenerativeEndTraining() {
  for (size_t i = 0; i < counts_.size(); ++i) {
    counts_[i].EndAdding();
  }
}


int TGenModel::GetSubmodelBranch(
    int program_id,
    const TCondLanguage::ExecutionForTree& exec,
    FullTreeTraversal sample,
    const TreeSlice* slice) const {
  const BranchCondProgram& program = program_.branched_prog(program_id);

  thread_local std::vector<int> branch_context;
  branch_context.clear();
  BranchContextAccumulator branch_context_acc(&branch_context);
  SlicedTreeTraversal traversal(sample.tree_storage(), sample.position(), slice);
  exec.GetConditionedFeaturesForPosition(
      program.cond.program, &traversal, nullptr, branch_context_acc);
  auto it = program.per_case_p.find(branch_context);
  return (it == program.per_case_p.end()) ? program.p_default : it->second;
}

double TGenModel::GetLabelLogProb(
    int program_id,
    const TCondLanguage::ExecutionForTree& exec,
    FullTreeTraversal sample,
    const TreeSlice* slice) const {
  size_t call_length = 0;
  while (program_.program_type(program_id) == TGenProgram::ProgramType::BRANCHED_PROGRAM) {
    program_id = GetSubmodelBranch(program_id, exec, sample, slice);
    ++call_length;
    CHECK_LE(call_length, program_.size());
  }

  int best_label = GetLabelAtPosition(program_id, exec, sample, slice);
  return GetLabelLogProbInner(
      program_id, exec, SlicedTreeTraversal(sample.tree_storage(), sample.position(), slice), best_label);
}

double TGenModel::GetLabelLogProbInner(
    int program_id,
    const TCondLanguage::ExecutionForTree& exec,
    SlicedTreeTraversal sample,
    int label) const {
  Feature f;
  Smoothing wb;

  // Unconditional feature is handled separately:
  const auto* uncond_stats = counts_[program_id].GetFeatureStatsOrNull(f);
  if (uncond_stats != nullptr) {
    wb.SetUnconditionedProb(counts_[program_id].GetCount(f, label),
        uncond_stats->UniqueLabels(),
        uncond_stats->TotalCount(),
        counts_[program_id].GetValuePrefixCount(f, label),
        counts_[program_id].GetTotalPrefixCount(f));
  }
  SlicedTreeTraversal traversal = sample;
  ExecuteContextProgramByIdInAll(
      &exec,
      &traversal, nullptr,
      program_id, &program_,
      [&wb, &label, this, &f, program_id](int op_added) {
    f.PushBack(op_added);
    const auto* stats = counts_[program_id].GetFeatureStatsOrNull(f);
    if (stats != nullptr) {
      wb.AddForwardBackoff(
          counts_[program_id].GetCount(f, label),
          stats->UniqueLabels(),
          stats->TotalCount(),
          stats->GetCounts(),
          counts_[program_id].GetValuePrefixCount(f, label),
          counts_[program_id].GetTotalPrefixCount(f),
          counts_[program_id].GetKneserNeyDelta(f));
    }
  });

  return wb.GetLogProb();
}

std::pair<double, int> TGenModel::GetBestLabelLogProb(
    int program_id,
    const TCondLanguage::ExecutionForTree& exec, FullTreeTraversal sample, const TreeSlice* slice) const {
  size_t call_length = 0;
  while (program_.program_type(program_id) == TGenProgram::ProgramType::BRANCHED_PROGRAM) {
    program_id = GetSubmodelBranch(program_id, exec, sample, slice);
    ++call_length;
    CHECK_LE(call_length, program_.size());
  }

  Feature f;
  const auto& uncond_items = counts_[program_id].LabelsSortedByProbability(f);
  if (uncond_items.empty()) return std::make_pair(0.0, -1);
  int best_label = *(uncond_items[0].second);
  double best_score = GetLabelLogProbInner(
      program_id, exec, SlicedTreeTraversal(sample.tree_storage(), sample.position(), slice), best_label);

  for (size_t i = 1; static_cast<int>(i) < FLAGS_beam_size && i < uncond_items.size(); i++) {
    int label = *(uncond_items[i].second);
    if (label != best_label) {
      double score = GetLabelLogProbInner(
          program_id, exec, SlicedTreeTraversal(sample.tree_storage(), sample.position(), slice), label);
      if (score > best_score) {
        best_score = score;
        best_label = label;
      }
    }
  }

  SlicedTreeTraversal traversal(sample.tree_storage(), sample.position(), slice);
  ExecuteContextProgramByIdInAll(
      &exec,
      &traversal, nullptr,
      program_id, &program_,
      [&f, this, &best_label, &best_score, &sample, &exec, slice, program_id](int op_added) {
    f.PushBack(op_added);
    const auto& items = counts_[program_id].LabelsSortedByProbability(f);
    for (size_t i = 0; static_cast<int>(i) < FLAGS_beam_size && i < items.size(); i++) {
      int label = *(items[i].second);
      if (label != best_label) {
        double score = GetLabelLogProbInner(
            program_id, exec, SlicedTreeTraversal(sample.tree_storage(), sample.position(), slice), label);
        if (score > best_score) {
          best_score = score;
          best_label = label;
        }
      }
    }
  });

  return std::make_pair(best_score, best_label);
}


bool TGenModel::IsLabelBestPrediction(
    int program_id,
    const TCondLanguage::ExecutionForTree& exec,
    FullTreeTraversal sample,
    const TreeSlice* slice) const {
  size_t call_length = 0;
  while (program_.program_type(program_id) == TGenProgram::ProgramType::BRANCHED_PROGRAM) {
    program_id = GetSubmodelBranch(program_id, exec, sample, slice);
    ++call_length;
    CHECK_LE(call_length, program_.size());
  }

  return GetBestLabelLogProb(program_id, exec, sample, slice).second == GetLabelAtPosition(program_id, exec, sample, slice);
}


