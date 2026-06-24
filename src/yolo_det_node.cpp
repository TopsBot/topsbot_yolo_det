// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo_det/yolo_det_node.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>

#include <opencv2/imgcodecs.hpp>

#include "topsbot_yolo/image_source.hpp"
#include "topsbot_yolo/yolo_session.hpp"
#include "topsbot_yolo/coco_names.hpp"

namespace topsbot_yolo_det
{

namespace
{
std::string encoding_to_string(const std::array<uint8_t, 12> & raw)
{
  const char * cstr = reinterpret_cast<const char *>(raw.data());
  return std::string(cstr, strnlen(cstr, raw.size()));
}

std::string expand_user_path(const std::string & path)
{
  if (path.empty() || path[0] != '~') {
    return path;
  }
  const char * home = std::getenv("HOME");
  if (home == nullptr || home[0] == '\0') {
    return path;
  }
  if (path.size() == 1 || path[1] == '/') {
    return std::string(home) + path.substr(1);
  }
  return path;
}
}  // namespace

YoloDetNode::YoloDetNode(const rclcpp::NodeOptions & options)
: Node(
    "yolo_det_node",
    rclcpp::NodeOptions(options).automatically_declare_parameters_from_overrides(true))
{
  schedule_initialize();
}

void YoloDetNode::schedule_initialize()
{
  init_timer_ = create_wall_timer(
    std::chrono::milliseconds(1),
    [this]() {
      if (init_timer_) {
        init_timer_->cancel();
      }
      initialize();
    });
}

namespace
{
template<typename T>
T get_param(rclcpp::Node & node, const std::string & name, const T & default_val)
{
  if (!node.has_parameter(name)) {
    node.declare_parameter(name, default_val);
  }
  return node.get_parameter(name).get_value<T>();
}
}  // namespace

topsbot_yolo::YoloDetectorConfig YoloDetNode::load_config()
{
  topsbot_yolo::YoloDetectorConfig cfg;
  cfg.model_type = get_param(*this, "model_type", std::string("yolo11"));
  cfg.model_path = expand_user_path(get_param(*this, "model_path", std::string("")));
  cfg.device_id = get_param(*this, "device_id", 0);
  cfg.enable_profile = get_param(*this, "enable_profile", true);

  cfg.preprocess.letterbox = get_param(*this, "preprocess.letterbox", true);
  cfg.preprocess.auto_input_size = get_param(*this, "preprocess.auto_input_size", true);
  cfg.preprocess.input_width = get_param(*this, "preprocess.input_width", 640);
  cfg.preprocess.input_height = get_param(*this, "preprocess.input_height", 640);
  const auto color = get_param(
    *this, "preprocess.letterbox_color", std::vector<int64_t>{114, 114, 114});
  if (color.size() >= 3) {
    cfg.preprocess.letterbox_color[0] = static_cast<int>(color[0]);
    cfg.preprocess.letterbox_color[1] = static_cast<int>(color[1]);
    cfg.preprocess.letterbox_color[2] = static_cast<int>(color[2]);
  }

  cfg.postprocess.confidence_threshold = get_param(
    *this, "postprocess.confidence_threshold", 0.25);
  cfg.postprocess.nms_threshold = get_param(*this, "postprocess.nms_threshold", 0.45);
  cfg.postprocess.max_detections = get_param(*this, "postprocess.max_detections", 300);
  cfg.postprocess.class_names = get_param(
    *this, "postprocess.class_names", std::vector<std::string>{});

  input_mode_ = get_param(*this, "input_mode", std::string("tbmem"));
  image_topic_ = get_param(*this, "image_topic", std::string("/tbmem_img"));
  viz_topic_ = get_param(*this, "viz_topic", std::string("/yolo_viz"));
  detections_topic_ = get_param(*this, "detections_topic", std::string("/detections"));
  frame_id_ = get_param(*this, "frame_id", std::string("camera"));
  frame_skip_ = get_param(*this, "frame_skip", 0);
  publish_detections_ = get_param(*this, "publish_detections", false);
  tb_img_profile_ = get_param(*this, "tb_img_profile", std::string("480p"));
  image_file_ = get_param(*this, "image_file", std::string(""));
  output_image_file_ = get_param(*this, "output_image_file", std::string(""));
  enable_profile_ = cfg.enable_profile;
  return cfg;
}

void YoloDetNode::initialize()
{
  if (initialized_) {
    return;
  }

  if (!allocator_.get()) {
    RCLCPP_ERROR(get_logger(), "failed to create dmabuf heap allocator");
    return;
  }

  cfg_ = load_config();
  if (cfg_.model_path.empty()) {
    RCLCPP_ERROR(get_logger(), "model_path is required");
    return;
  }

  if (!session_.init(cfg_)) {
    RCLCPP_ERROR(get_logger(), "failed to init detector for model: %s", cfg_.model_path.c_str());
    return;
  }

  if (input_mode_ == "file") {
    run_file_mode_once();
    return;
  }

  viz_pub_ = create_publisher<sensor_msgs::msg::Image>(viz_topic_, 10);
  if (publish_detections_) {
    det_pub_ = create_publisher<tb_det_msgs::msg::TbPerceptionTargets>(detections_topic_, 10);
  }

  setup_subscriptions();

  RCLCPP_INFO(
    get_logger(),
    "yolo_det ready: model_type=%s model=%s input=%s topic=%s viz=%s",
    cfg_.model_type.c_str(), cfg_.model_path.c_str(), input_mode_.c_str(),
    image_topic_.c_str(), viz_topic_.c_str());
  initialized_ = true;
}

void YoloDetNode::run_file_mode_once()
{
  if (image_file_.empty() || output_image_file_.empty()) {
    RCLCPP_ERROR(
      get_logger(),
      "input_mode=file requires image_file and output_image_file");
    return;
  }
  if (!std::filesystem::exists(image_file_)) {
    RCLCPP_ERROR(get_logger(), "image_file not found: %s", image_file_.c_str());
    return;
  }

  auto nv12 = topsbot_yolo::ImageSource::from_file(allocator_, image_file_);
  if (!nv12) {
    RCLCPP_ERROR(get_logger(), "failed to decode: %s", image_file_.c_str());
    return;
  }

  std::vector<topsbot_yolo::Detection> detections;
  topsbot_yolo::InferenceTiming timing;
  cv::Mat viz_bgr;
  if (!session_.process(allocator_, nv12->image, detections, timing, viz_bgr)) {
    RCLCPP_ERROR(get_logger(), "file mode detection failed");
    return;
  }

  std::filesystem::create_directories(
    std::filesystem::path(output_image_file_).parent_path());
  if (!cv::imwrite(output_image_file_, viz_bgr)) {
    RCLCPP_ERROR(get_logger(), "failed to write: %s", output_image_file_.c_str());
    return;
  }

  RCLCPP_INFO(
    get_logger(),
    "file mode: %s -> %s, dets=%zu, pre=%.2f infer=%.2f post=%.2f viz=%.2f ms",
    image_file_.c_str(), output_image_file_.c_str(), detections.size(),
    timing.preprocess_ms, timing.infer_ms, timing.postprocess_ms, timing.viz_ms);

  initialized_ = true;
  rclcpp::shutdown();
}

void YoloDetNode::setup_subscriptions()
{
  const auto qos = rclcpp::SensorDataQoS();

  if (input_mode_ == "sensor_image") {
    image_sub_ = create_subscription<sensor_msgs::msg::Image>(
      image_topic_, qos,
      std::bind(&YoloDetNode::on_sensor_image, this, std::placeholders::_1));
    return;
  }

  if (tb_img_profile_ == "540p") {
    tb_sub_540_ = create_subscription<tb_img_msgs::msg::TbMsg540P>(
      image_topic_, qos,
      std::bind(&YoloDetNode::on_tb_img_540, this, std::placeholders::_1));
  } else if (tb_img_profile_ == "1080p") {
    tb_sub_1080_ = create_subscription<tb_img_msgs::msg::TbMsg1080P>(
      image_topic_, qos,
      std::bind(&YoloDetNode::on_tb_img_1080, this, std::placeholders::_1));
  } else {
    tb_sub_480_ = create_subscription<tb_img_msgs::msg::TbMsg480P>(
      image_topic_, qos,
      std::bind(&YoloDetNode::on_tb_img_480, this, std::placeholders::_1));
  }
}

void YoloDetNode::on_tb_img_480(const tb_img_msgs::msg::TbMsg480P::ConstSharedPtr & msg)
{
  process_frame(
    msg->data.data(), msg->data_size, msg->width, msg->height, msg->encoding, msg->time_stamp);
}

void YoloDetNode::on_tb_img_540(const tb_img_msgs::msg::TbMsg540P::ConstSharedPtr & msg)
{
  process_frame(
    msg->data.data(), msg->data_size, msg->width, msg->height, msg->encoding, msg->time_stamp);
}

void YoloDetNode::on_tb_img_1080(const tb_img_msgs::msg::TbMsg1080P::ConstSharedPtr & msg)
{
  process_frame(
    msg->data.data(), msg->data_size, msg->width, msg->height, msg->encoding, msg->time_stamp);
}

void YoloDetNode::on_sensor_image(const sensor_msgs::msg::Image::ConstSharedPtr & msg)
{
  auto nv12 = topsbot_yolo::ImageSource::from_sensor_image(allocator_, *msg);
  if (!nv12) {
    RCLCPP_ERROR(get_logger(), "unsupported sensor_msgs encoding: %s", msg->encoding.c_str());
    return;
  }
  process_nv12_image(nv12->image, msg->header.stamp);
}

bool YoloDetNode::process_nv12_image(
  const topsbot_yolo::TaImage & src_nv12,
  const builtin_interfaces::msg::Time & stamp)
{
  if (!initialized_) {
    return false;
  }

  ++frame_counter_;
  if (frame_skip_ > 0 && (frame_counter_ % (frame_skip_ + 1)) != 0) {
    return true;
  }

  std::vector<topsbot_yolo::Detection> detections;
  topsbot_yolo::InferenceTiming timing;
  cv::Mat viz_bgr;
  if (!session_.process(allocator_, src_nv12, detections, timing, viz_bgr)) {
    RCLCPP_ERROR(get_logger(), "detect failed");
    return false;
  }

  publish_viz(viz_bgr, stamp);
  if (publish_detections_) {
    publish_detections(detections, timing, stamp);
  }

  if (enable_profile_) {
    const float total = timing.preprocess_ms + timing.infer_ms + timing.postprocess_ms + timing.viz_ms;
    RCLCPP_INFO(
      get_logger(),
      "frame #%d pre=%.2f infer=%.2f post=%.2f viz=%.2f total=%.2f ms dets=%zu",
      frame_counter_, timing.preprocess_ms, timing.infer_ms, timing.postprocess_ms,
      timing.viz_ms, total, detections.size());
  }
  return true;
}

bool YoloDetNode::process_frame(
  const uint8_t * data, const uint32_t data_size, const uint32_t width, const uint32_t height,
  const std::array<uint8_t, 12> & encoding, const builtin_interfaces::msg::Time & stamp)
{
  if (!initialized_) {
    return false;
  }

  if (!encoding.empty()) {
    const std::string enc = encoding_to_string(encoding);
    if (enc != "nv12" && enc != "yuv420") {
      RCLCPP_ERROR(get_logger(), "tb_img encoding '%s' not supported", enc.c_str());
      return false;
    }
  }

  std::vector<uint8_t> bytes(data, data + data_size);
  auto nv12 = topsbot_yolo::ImageSource::from_nv12_bytes(
    allocator_, bytes, static_cast<int>(width), static_cast<int>(height));
  if (!nv12) {
    RCLCPP_ERROR(get_logger(), "failed to upload NV12 to dmabuf");
    return false;
  }

  return process_nv12_image(nv12->image, stamp);
}

void YoloDetNode::publish_viz(const cv::Mat & bgr, const builtin_interfaces::msg::Time & stamp)
{
  sensor_msgs::msg::Image msg;
  msg.header.stamp = stamp;
  msg.header.frame_id = frame_id_;
  msg.height = static_cast<uint32_t>(bgr.rows);
  msg.width = static_cast<uint32_t>(bgr.cols);
  msg.encoding = "bgr8";
  msg.is_bigendian = 0;
  msg.step = static_cast<uint32_t>(bgr.cols * 3);
  msg.data.assign(bgr.data, bgr.data + bgr.total() * bgr.elemSize());
  viz_pub_->publish(msg);
}

void YoloDetNode::publish_detections(
  const std::vector<topsbot_yolo::Detection> & detections,
  const topsbot_yolo::InferenceTiming & timing,
  const builtin_interfaces::msg::Time & stamp)
{
  if (!det_pub_) {
    return;
  }

  tb_det_msgs::msg::TbPerceptionTargets msg;
  msg.header.stamp = stamp;
  msg.header.frame_id = frame_id_;
  msg.fps = 0;

  auto make_perf = [](const std::string & type, float ms) {
      tb_det_msgs::msg::TbPerf perf;
      perf.type = type;
      perf.time_ms_duration = static_cast<double>(ms);
      return perf;
    };

  msg.perfs.push_back(make_perf("yolo_preprocess", timing.preprocess_ms));
  msg.perfs.push_back(make_perf("yolo_infer", timing.infer_ms));
  msg.perfs.push_back(make_perf("yolo_postprocess", timing.postprocess_ms));
  msg.perfs.push_back(make_perf("yolo_viz", timing.viz_ms));

  for (const auto & det : detections) {
    tb_det_msgs::msg::TbTarget target;
    target.type = topsbot_yolo::class_name(det.class_id, cfg_.postprocess.class_names);
    tb_det_msgs::msg::TbRoi roi;
    roi.type = target.type;
    roi.rect.x_offset = static_cast<uint32_t>(det.x);
    roi.rect.y_offset = static_cast<uint32_t>(det.y);
    roi.rect.width = static_cast<uint32_t>(det.width);
    roi.rect.height = static_cast<uint32_t>(det.height);
    roi.confidence = det.score;
    target.rois.push_back(roi);
    msg.targets.push_back(target);
  }

  det_pub_->publish(msg);
}

}  // namespace topsbot_yolo_det
