/**
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <dirent.h>
#include <gflags/gflags.h>
#include <sys/time.h>
#include <algorithm>
#include <fstream>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "inc/utils.h"
#include "include/api/context.h"
#include "include/api/model.h"
#include "include/api/serialization.h"
#include "include/api/types.h"
#include "include/dataset/execute.h"
#include "include/dataset/vision.h"

using mindspore::Context;
using mindspore::GraphCell;
using mindspore::kSuccess;
using mindspore::Model;
using mindspore::ModelType;
using mindspore::MSTensor;
using mindspore::Serialization;
using mindspore::Status;
using mindspore::dataset::Execute;

DEFINE_string(mindir_path, "", "mindir path");
DEFINE_string(input_path, ".", "input path");
DEFINE_int32(device_id, 0, "device id");

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (RealPath(FLAGS_mindir_path).empty()) {
    std::cout << "Invalid mindir" << std::endl;
    return 1;
  }

  auto context = std::make_shared<Context>();
  auto ascend310 = std::make_shared<mindspore::Ascend310DeviceInfo>();
  ascend310->SetDeviceID(FLAGS_device_id);
  context->MutableDeviceInfo().push_back(ascend310);
  mindspore::Graph graph;
  Serialization::Load(FLAGS_mindir_path, ModelType::kMindIR, &graph);

  Model model;
  Status ret = model.Build(GraphCell(graph), context);
  if (ret != kSuccess) {
    std::cout << "ERROR: Build failed." << std::endl;
    return 1;
  }
  std::vector<MSTensor> model_inputs = model.GetInputs();
  if (model_inputs.empty()) {
    std::cout << "Invalid model, inputs is empty." << std::endl;
    return 1;
  }
  std::map<double, double> costTime_map;
  // get content file path
  auto content_files = GetAllFiles(FLAGS_input_path + "/content/");
  if (content_files.empty()) {
    std::cout << "ERROR: content data empty." << std::endl;
    return 1;
  }
  size_t content_size = content_files.size();
  // get style file path
  auto style_files = GetAllFiles(FLAGS_input_path + "/style/");

  if (style_files.empty()) {
    std::cout << "ERROR: style data empty." << std::endl;
    return 1;
  }
  size_t style_size = style_files.size();

  for (size_t i = 0; i < content_size; ++i) {
    for (size_t j = 0; j < style_size; ++j) {
      struct timeval start = {0};
      struct timeval end = {0};
      double startTimeMs;
      double endTimeMs;
      std::vector<MSTensor> inputs;
      std::vector<MSTensor> outputs;
      std::cout << "Start predict content files:" << content_files[i]
                << " with style file:" << style_files[j] << std::endl;

      auto input0 = ReadFileToTensor(content_files[i]);
      auto input1 = ReadFileToTensor(style_files[j]);

      inputs.emplace_back(model_inputs[0].Name(), model_inputs[0].DataType(),
                          model_inputs[0].Shape(), input0.Data().get(),
                          input0.DataSize());

      inputs.emplace_back(model_inputs[1].Name(), model_inputs[1].DataType(),
                          model_inputs[1].Shape(), input1.Data().get(),
                          input1.DataSize());

      gettimeofday(&start, nullptr);
      ret = model.Predict(inputs, &outputs);
      gettimeofday(&end, nullptr);
      if (ret != kSuccess) {
        std::cout << "Predict " << style_files[j] << " failed." << std::endl;
        return 1;
      }
      startTimeMs = (1.0 * start.tv_sec * 1000000 + start.tv_usec) / 1000;
      endTimeMs = (1.0 * end.tv_sec * 1000000 + end.tv_usec) / 1000;
      costTime_map.insert(std::pair<double, double>(startTimeMs, endTimeMs));
      std::cout << content_files[i] + "_" + style_files[j] << std::endl;
      WriteResult(content_files[i], style_files[j], outputs);
    }
  }
  double average = 0.0;
  int inferCount = 0;

  for (auto iter = costTime_map.begin(); iter != costTime_map.end(); iter++) {
    double diff = iter->second - iter->first;
    average += diff;
    inferCount++;
  }
  average = average / inferCount;
  std::stringstream timeCost;
  timeCost << "NN inference cost average time: " << average
           << " ms of infer_count " << inferCount << std::endl;
  std::cout << "NN inference cost average time: " << average
            << "ms of infer_count " << inferCount << std::endl;
  std::string fileName =
      "./time_Result" + std::string("/test_perform_static.txt");
  std::ofstream fileStream(fileName.c_str(), std::ios::trunc);
  fileStream << timeCost.str();
  fileStream.close();
  costTime_map.clear();
  return 0;
}
