// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_YOLO_DET__YOLO_DET_NODE_HPP_
#define TOPSBOT_YOLO_DET__YOLO_DET_NODE_HPP_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "tb_det_msgs/msg/tb_perception_targets.hpp"
#include "tb_img_msgs/msg/tb_msg1080_p.hpp"
#include "tb_img_msgs/msg/tb_msg480_p.hpp"
#include "tb_img_msgs/msg/tb_msg540_p.hpp"

#include "topsbot_yolo/dmabuf_allocator.hpp"
#include "topsbot_yolo/ta_image.hpp"
#include "topsbot_yolo/yolo_session.hpp"

namespace topsbot_yolo_det
{

class YoloDetNode : public rclcpp::Node
{
public:
  explicit YoloDetNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  void schedule_initialize();
  void initialize();
  void setup_subscriptions();
  void run_file_mode_once();

  void on_tb_img_480(const tb_img_msgs::msg::TbMsg480P::ConstSharedPtr & msg);
  void on_tb_img_540(const tb_img_msgs::msg::TbMsg540P::ConstSharedPtr & msg);
  void on_tb_img_1080(const tb_img_msgs::msg::TbMsg1080P::ConstSharedPtr & msg);
  void on_sensor_image(const sensor_msgs::msg::Image::ConstSharedPtr & msg);

  bool process_nv12_image(
    const topsbot_yolo::TaImage & src_nv12,
    const builtin_interfaces::msg::Time & stamp);

  bool process_frame(
    const uint8_t * data, uint32_t data_size, uint32_t width, uint32_t height,
    const std::array<uint8_t, 12> & encoding, const builtin_interfaces::msg::Time & stamp);

  void publish_viz(const cv::Mat & bgr, const builtin_interfaces::msg::Time & stamp);
  void publish_detections(
    const std::vector<topsbot_yolo::Detection> & detections,
    const topsbot_yolo::InferenceTiming & timing,
    const builtin_interfaces::msg::Time & stamp);

  topsbot_yolo::YoloDetectorConfig load_config();

  bool initialized_{false};
  int frame_counter_{0};
  int frame_skip_{0};
  bool enable_profile_{true};
  bool publish_detections_{false};
  std::string image_topic_{"/tbmem_img"};
  std::string viz_topic_{"/yolo_viz"};
  std::string detections_topic_{"/detections"};
  std::string frame_id_{"camera"};
  std::string input_mode_{"tbmem"};
  std::string tb_img_profile_{"480p"};
  std::string image_file_;
  std::string output_image_file_;

  topsbot_yolo::DmabufAllocator allocator_;
  topsbot_yolo::YoloSession session_;
  topsbot_yolo::YoloDetectorConfig cfg_;

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Subscription<tb_img_msgs::msg::TbMsg480P>::SharedPtr tb_sub_480_;
  rclcpp::Subscription<tb_img_msgs::msg::TbMsg540P>::SharedPtr tb_sub_540_;
  rclcpp::Subscription<tb_img_msgs::msg::TbMsg1080P>::SharedPtr tb_sub_1080_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr viz_pub_;
  rclcpp::Publisher<tb_det_msgs::msg::TbPerceptionTargets>::SharedPtr det_pub_;
  rclcpp::TimerBase::SharedPtr init_timer_;
};

}  // namespace topsbot_yolo_det

#endif  // TOPSBOT_YOLO_DET__YOLO_DET_NODE_HPP_
