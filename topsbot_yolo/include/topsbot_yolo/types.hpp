// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO__TYPES_HPP_
#define TOPSBOT_YOLO__TYPES_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace topsbot_yolo
{

struct Detection
{
  float x{0.f};
  float y{0.f};
  float width{0.f};
  float height{0.f};
  int32_t class_id{-1};
  float score{0.f};
};

struct InferenceTiming
{
  float preprocess_ms{0.f};
  float infer_ms{0.f};
  float postprocess_ms{0.f};
  float viz_ms{0.f};
};

struct PreprocessConfig
{
  bool letterbox{true};
  int letterbox_color[3]{114, 114, 114};
  bool auto_input_size{true};
  int input_width{640};
  int input_height{640};
};

struct PostprocessConfig
{
  float confidence_threshold{0.25f};
  float nms_threshold{0.45f};
  int max_detections{300};
  std::vector<std::string> class_names;
};

struct YoloDetectorConfig
{
  std::string model_type{"yolo11"};
  std::string model_path;
  int device_id{0};
  PreprocessConfig preprocess;
  PostprocessConfig postprocess;
  bool enable_profile{false};
};

}  // namespace topsbot_yolo

#endif  // TOPSBOT_YOLO__TYPES_HPP_
