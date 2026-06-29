// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "topsbot_yolo_det/yolo_det_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<topsbot_yolo_det::YoloDetNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
