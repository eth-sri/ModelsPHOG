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

#ifndef PHOG_MODEL_MODEL_H_
#define PHOG_MODEL_MODEL_H_

#include "phog/dsl/tgen_program.h"

//////////////////////////////////////////////////////////////////////
// Execution of TGen programs.
//////////////////////////////////////////////////////////////////////

// The following methods executes a "curr" program that may possibly call into a program in "all".
template <class Callback>
bool ExecuteContextProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const SimpleCondProgram* curr, const TGenProgram* all, const Callback& cb);
template <class Callback>
bool ExecuteContextProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const BranchCondProgram* curr, const TGenProgram* all, const Callback& cb);

// Executes one program from "all" given by an id.
template <class Callback>
bool ExecuteContextProgramByIdInAll(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    int program_id_in_all, const TGenProgram* all, const Callback& cb) {
  switch (all->program_type(program_id_in_all)) {
  case TGenProgram::ProgramType::BRANCHED_PROGRAM:
    return ExecuteContextProgram(exec, traversal, debug_info, &all->branched_prog(program_id_in_all), all, cb);
  case TGenProgram::ProgramType::SIMPLE_PROGRAM:
    return ExecuteContextProgram(exec, traversal, debug_info, &all->simple_prog(program_id_in_all), all, cb);
  }
  return false;
}

template <class Callback>
bool ExecuteContextProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const SimpleCondProgram* curr, const TGenProgram* all, const Callback& cb) {
  return exec->GetConditionedFeaturesForPosition(
      curr->context_program, traversal, debug_info, cb);
}

struct BranchContextAccumulator {
  BranchContextAccumulator(std::vector<int>* bc) : branch_context(bc) {}
  std::vector<int>* branch_context;

  bool operator()(int v) const {
    branch_context->push_back(v);
    return true;
  }
};

template <class Callback>
bool ExecuteContextProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const BranchCondProgram* curr, const TGenProgram* all, const Callback& cb) {
  std::vector<int> branch_context;
  BranchContextAccumulator acc(&branch_context);
  SlicedTreeTraversal branch_t = *traversal;
  exec->GetConditionedFeaturesForPosition(
      curr->cond.program, &branch_t, debug_info, acc);
  auto it = curr->per_case_p.find(branch_context);
  int called_p = (it == curr->per_case_p.end()) ? curr->p_default : it->second;
  return ExecuteContextProgramByIdInAll(exec, traversal, debug_info, called_p, all, cb);
}

// TEq program

// The following methods executes a "curr" program that may possibly call into a program in "all".
template <class Callback>
bool ExecuteEqProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const SimpleCondProgram* curr, const TGenProgram* all, const Callback& cb);
template <class Callback>
bool ExecuteEqProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const BranchCondProgram* curr, const TGenProgram* all, const Callback& cb);

// Executes one program from "all" given by an id.
template <class Callback>
bool ExecuteEqProgramByIdInAll(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    int program_id_in_all, const TGenProgram* all, const Callback& cb) {
  switch (all->program_type(program_id_in_all)) {
  case TGenProgram::ProgramType::BRANCHED_PROGRAM:
    return ExecuteEqProgram(exec, traversal, debug_info, &all->branched_prog(program_id_in_all), all, cb);
  case TGenProgram::ProgramType::SIMPLE_PROGRAM:
    return ExecuteEqProgram(exec, traversal, debug_info, &all->simple_prog(program_id_in_all), all, cb);
  }
  return false;
}

template <class Callback>
bool ExecuteEqProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const SimpleCondProgram* curr, const TGenProgram* all, const Callback& cb) {
  return exec->GetConditionedFeaturesForPosition(
      curr->eq_program, traversal, debug_info, cb);
}

template <class Callback>
bool ExecuteEqProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal* traversal, std::string* debug_info,
    const BranchCondProgram* curr, const TGenProgram* all, const Callback& cb) {
  std::vector<int> branch_context;
  BranchContextAccumulator acc(&branch_context);
  exec->GetConditionedFeaturesForPosition(
      curr->cond.program, traversal, debug_info, acc);
  auto it = curr->per_case_p.find(branch_context);
  int called_p = (it == curr->per_case_p.end()) ? curr->p_default : it->second;
  return ExecuteEqProgramByIdInAll(exec, traversal, debug_info, called_p, all, cb);
}

// Utility function to return the context from running a straight-line TCond program.
//
// The parameter all is the list of call targets that all may point to.
bool ComputeContextFromTCondProgram(
    const TCondLanguage::ExecutionForTree* exec,
    SlicedTreeTraversal traversal,
    const TCondLanguage::Program& cond,
    const TGenProgram* all,
    std::vector<int>* context);


class TGenModel {
public:
  typedef TCondLanguage::Feature Feature;

  explicit TGenModel(const TGenProgram& program, bool is_for_node_type);
  TGenModel(TGenModel&& o) = delete;
  TGenModel(const TGenModel& o) = delete;
  ~TGenModel();

  // Adds one sample (of training data) to the model.
  void GenerativeTrainOneSample(
      int program_id,
      const TCondLanguage::ExecutionForTree& exec,
      FullTreeTraversal sample);

  // Must be called after all calls of GenerativeTrainOneSample are done.
  void GenerativeEndTraining();


  // Gets the probability of the label at the position given by the iterator "sample".
  double GetLabelLogProb(
      int program_id,
      const TCondLanguage::ExecutionForTree& exec,
      FullTreeTraversal sample,
      const TreeSlice* slice) const;

  // Returns (an approximation) if the given label at the FullTreeTraversal is correct.
  bool IsLabelBestPrediction(
      int program_id,
      const TCondLanguage::ExecutionForTree& exec,
      FullTreeTraversal sample,
      const TreeSlice* slice) const;

  // Returns the log-probability of the label for which the model has highest confidence to be the best label.
  std::pair<double, int> GetBestLabelLogProb(
      int program_id,
      const TCondLanguage::ExecutionForTree& exec,
      FullTreeTraversal sample,
      const TreeSlice* slice) const;

  int GetLabelAtPosition(
      int program_id,
      const TCondLanguage::ExecutionForTree& exec,
      FullTreeTraversal sample,
      const TreeSlice* slice,
      bool use_teq = true) const;

  bool is_for_node_type() const { return is_for_node_type_; }

  int start_program_id() const { return program_.size() - 1; }
private:
  int GetSubmodelBranch(
      int program_id,
      const TCondLanguage::ExecutionForTree& exec,
      FullTreeTraversal sample,
      const TreeSlice* slice) const;

  double GetLabelLogProbInner(
      int program_id,
      const TCondLanguage::ExecutionForTree& exec,
      SlicedTreeTraversal sample,
      int label) const;

  const TGenProgram program_;
  bool is_for_node_type_;
  std::vector<PerFeatureValueCounter<Feature, int> > counts_;
};



enum class Metric {
  DEFAULT,  // Uses the --default_tgen_eval_metric flag.
  ENTROPY,
  ERROR_RATE,
  CONFIDENCE50  // At least 50% confidence for correct prediction.
};


class TGenModelEvaluationMetricComputation {
public:
  TGenModelEvaluationMetricComputation(Metric metric);
  void AddSample(
      const TGenModel* model,
      const TCondLanguage::ExecutionForTree& exec,
      int position_in_tree);
  double GetComputedValue() const;

private:
  Metric metric_;
  double value_;
  int num_samples_;
};


#endif /* PHOG_MODEL_MODEL_H_ */
