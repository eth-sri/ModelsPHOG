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


#include <bitset>
#include <iomanip>
#include <memory>
#include <mutex>
#include <thread>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "json/json.h"

#include "base/readerutil.h"
#include "base/stringset.h"
#include "base/treeprinter.h"

#include "phog/tree/tree.h"
#include "phog/dsl/tcond_language.h"
#include "phog/dsl/tgen_program.h"
#include "phog/model/model.h"

DEFINE_int32(num_training_asts, 100000, "Maximun number of training ASTs to load.");
DEFINE_int32(num_eval_asts, 50000, "Maximun number of evaluation ASTs to load.");
DEFINE_string(training_data, "", "A file with the training data.");
DEFINE_string(evaluation_data, "", "A file with the training data.");
DEFINE_string(tgen_program, "", "A file with a TGen program.");
DEFINE_bool(is_for_node_type, false, "Whether the predictions are for node type (if false it is for node value).");

void Eval() {
  StringSet ss;
  TCondLanguage lang(&ss);
  TGenProgram tgen_program;
  TGen::LoadTGen(&lang, &tgen_program, FLAGS_tgen_program);

  std::vector<TreeStorage> trees, eval_trees;
  LOG(INFO) << "Loading training data...";
  ParseTreesInFileWithParallelJSONParse(
      &ss, FLAGS_training_data.c_str(), 0, FLAGS_num_training_asts, true, &trees);
  LOG(INFO) << "Training data with " << trees.size() << " trees loaded.";

  LOG(INFO) << "Loading evaluation data...";
  ParseTreesInFileWithParallelJSONParse(
      &ss, FLAGS_evaluation_data.c_str(), 0, FLAGS_num_eval_asts, true, &eval_trees);
  LOG(INFO) << "Evaluation data with " << eval_trees.size() << " trees loaded.";

  LOG(INFO) << "Training...";
  TGenModel model(tgen_program, FLAGS_is_for_node_type);
  for (size_t tree_id = 0; tree_id < trees.size(); ++tree_id) {
    const TreeStorage& tree = trees[tree_id];
    TCondLanguage::ExecutionForTree exec(&ss, &tree);
    for (unsigned node_id = 0; node_id < tree.NumAllocatedNodes(); ++node_id) {
      model.GenerativeTrainOneSample(model.start_program_id(), exec, FullTreeTraversal(&tree, node_id));
      LOG_EVERY_N(INFO, FLAGS_num_training_asts * 100)
          << "Training... (logged every " << FLAGS_num_training_asts * 100 << " samples).";
    }
  }
  model.GenerativeEndTraining();
  LOG(INFO) << "Training done.";

  std::vector<Metric> metrics{ Metric::ERROR_RATE };  // , Metric::ENTROPY, Metric::CONFIDENCE50 };
  std::vector<std::string> metric_names{ "error rate", "entropy", "confidence >50%" };

  for (size_t metric_id = 0; metric_id < metrics.size(); ++metric_id) {
    TGenModelEvaluationMetricComputation metric(metrics[metric_id]);
    LOG(INFO) << "Evaluating " << metric_names[metric_id] << "...";
    for (size_t tree_id = 0; tree_id < eval_trees.size(); ++tree_id) {
      const TreeStorage& tree = eval_trees[tree_id];
      TCondLanguage::ExecutionForTree exec(&ss, &tree);
      for (unsigned node_id = 0; node_id < tree.NumAllocatedNodes(); ++node_id) {
        metric.AddSample(&model, exec, node_id);
      }
    }
    LOG(INFO) << "Evaluation " << metric_names[metric_id] << " done.";
    printf("%s = %.4f\n", metric_names[metric_id].c_str(), metric.GetComputedValue());
  }

  LOG(INFO) << "Done.";
}


int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  CHECK(!FLAGS_training_data.empty()) << "--training_data is a required parameter.";
  CHECK(!FLAGS_evaluation_data.empty()) << "--evaluation_data is a required parameter.";
  CHECK(!FLAGS_tgen_program.empty()) << "--tgen_program is a required parameter.";
  Eval();
  return 0;
}
