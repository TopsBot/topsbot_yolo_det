// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/viz_renderer.hpp"

#include <opencv2/imgproc.hpp>

#include "topsbot_yolo/coco_names.hpp"

namespace topsbot_yolo
{

cv::Mat VizRenderer::nv12_to_bgr(const uint8_t * data, const int w, const int h)
{
  cv::Mat nv12(h * 3 / 2, w, CV_8UC1, const_cast<uint8_t *>(data));
  cv::Mat bgr;
  cv::cvtColor(nv12, bgr, cv::COLOR_YUV2BGR_NV12);
  return bgr;
}

cv::Mat VizRenderer::draw(
  const uint8_t * src_nv12, const int src_w, const int src_h,
  const std::vector<Detection> & detections) const
{
  cv::Mat bgr = nv12_to_bgr(src_nv12, src_w, src_h);
  for (const auto & d : detections) {
    const cv::Rect r(
      cvRound(d.x), cvRound(d.y), cvRound(d.width), cvRound(d.height));
    cv::rectangle(bgr, r, cv::Scalar(0, 255, 0), 2);
    const auto label = class_name(d.class_id, class_names_) + " " +
      std::to_string(static_cast<int>(d.score * 100.f)) + "%";
    cv::putText(
      bgr, label, cv::Point(r.x, std::max(0, r.y - 5)),
      cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
  }
  return bgr;
}

}  // namespace topsbot_yolo
