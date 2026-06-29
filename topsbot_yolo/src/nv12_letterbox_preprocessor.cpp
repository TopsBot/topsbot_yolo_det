// Copyright 2026 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_yolo/nv12_letterbox_preprocessor.hpp"

#include <algorithm>
#include <cstring>

#include "ta_cv_api_ext_c.h"

namespace topsbot_yolo
{

void Nv12LetterboxPreprocessor::configure(const PreprocessConfig & cfg)
{
  cfg_ = cfg;
  model_width_ = cfg.input_width;
  model_height_ = cfg.input_height;
}

bool Nv12LetterboxPreprocessor::process(
  DmabufAllocator & alloc,
  const TaImage & src_nv12,
  TaImage & letterbox_nv12,
  PreprocessParams & params)
{
  if (!src_nv12.mapped() || src_nv12.width() <= 0 || src_nv12.height() <= 0) {
    return false;
  }

  params.src_width = src_nv12.width();
  params.src_height = src_nv12.height();
  params.src_size = cv::Size(params.src_width, params.src_height);

  int resize_w = model_width_;
  int resize_h = model_height_;
  if (cfg_.letterbox) {
    params.ratio = std::min(
      static_cast<float>(model_height_) / static_cast<float>(params.src_height),
      static_cast<float>(model_width_) / static_cast<float>(params.src_width));
    resize_w = static_cast<int>(params.src_width * params.ratio);
    resize_h = static_cast<int>(params.src_height * params.ratio);
    params.top = (model_height_ - resize_h) / 2;
    params.bottom = model_height_ - resize_h - params.top;
    params.left = (model_width_ - resize_w) / 2;
    params.right = model_width_ - resize_w - params.left;
  } else {
    params.ratio = 1.f;
    params.top = params.bottom = params.left = params.right = 0;
  }
  params.resize_width = resize_w;
  params.resize_height = resize_h;

  TaImage resized;
  ta_cv_resize_t resize{};
  resize.in_width = params.src_width;
  resize.in_height = params.src_height;
  resize.out_width = resize_w;
  resize.out_height = resize_h;
  resize.start_x = 0;
  resize.start_y = 0;

  ta_cv_resize_image_t resize_attr{};
  resize_attr.resize_img_attr = &resize;
  resize_attr.interpolation = 1;

  resized = TaImage::create_nv12(alloc, resize_w, resize_h);
  if (resized.fd() < 0) {
    return false;
  }

  auto src_image = src_nv12.native();
  auto dst_image = resized.native();
  if (ta_cv_image_resize(&resize_attr, src_image, dst_image) != TACV_SUCCESS) {
    return false;
  }

  if (!cfg_.letterbox ||
    (params.top == 0 && params.bottom == 0 && params.left == 0 && params.right == 0))
  {
    letterbox_nv12 = std::move(resized);
    return true;
  }

  letterbox_nv12 = TaImage::create_nv12(alloc, model_width_, model_height_);
  if (letterbox_nv12.fd() < 0) {
    return false;
  }

  if (!letterbox_fill_cpu(resized, letterbox_nv12, params)) {
    letterbox_nv12.reset();
    return false;
  }
  return true;
}

bool Nv12LetterboxPreprocessor::letterbox_fill_cpu(
  const TaImage & resized,
  TaImage & letterbox,
  const PreprocessParams & params)
{
  if (!resized.mapped() || !letterbox.mapped()) {
    return false;
  }

  const int R = cfg_.letterbox_color[0];
  const int G = cfg_.letterbox_color[1];
  const int B = cfg_.letterbox_color[2];
  const uint8_t Y = static_cast<uint8_t>(0.299f * R + 0.587f * G + 0.114f * B);
  const uint8_t U = static_cast<uint8_t>(-0.169f * R - 0.331f * G + 0.5f * B + 128);
  const uint8_t V = static_cast<uint8_t>(0.5f * R - 0.419f * G - 0.081f * B + 128);

  auto * out = static_cast<uint8_t *>(letterbox.mapped());
  auto * out_y = out;
  auto * out_uv = out + model_width_ * model_height_;

  std::memset(out_y, Y, static_cast<size_t>(model_width_) * static_cast<size_t>(model_height_));
  for (int i = 0; i < model_width_ * model_height_ / 2; i += 2) {
    out_uv[i] = U;
    out_uv[i + 1] = V;
  }

  const auto * src = static_cast<const uint8_t *>(resized.mapped());
  const auto * src_y = src;
  const auto * src_uv = src + params.resize_width * params.resize_height;
  const int resize_w = params.resize_width;
  const int resize_h = params.resize_height;

  for (int y = 0; y < resize_h; ++y) {
    const int dst_y = params.top + y;
    if (dst_y < 0 || dst_y >= model_height_) {
      return false;
    }
    std::memcpy(
      out_y + dst_y * model_width_ + params.left,
      src_y + y * resize_w,
      static_cast<size_t>(resize_w));
  }

  for (int y = 0; y < resize_h / 2; ++y) {
    const int dst_y = params.top / 2 + y;
    if (dst_y < 0 || dst_y >= model_height_ / 2) {
      return false;
    }
    std::memcpy(
      out_uv + dst_y * model_width_ + params.left,
      src_uv + y * resize_w,
      static_cast<size_t>(resize_w));
  }

  return true;
}

}  // namespace topsbot_yolo
