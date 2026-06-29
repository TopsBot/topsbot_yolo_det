# Changelog

All notable changes to this package are documented here.

## 0.2.0 (2026-06-29)

### Added

- `YoloSession::process_with_viz()` for offline file/CLI paths (NV12→BGR + draw boxes → JPG).

### Changed

- **Breaking**: online `yolo_det_node` no longer publishes `/yolo_viz`; primary output is `/detections` (`TbPerceptionTargets`, default on).
- Online pipeline uses `YoloSession::process()` only (no OpenCV color convert or draw per frame).
- `publish_detections` defaults to `true`; removed `viz_topic` parameter.
- README updated for `topsbot_webviz` integration.

## 0.1.0 (2026-06-21)

### Added

- YOLO11 / YOLO26 NV12 detection node with tbmem zero-copy input.
- `yolo_det_file_cli` for offline JPG validation.
- Launch files: `yolo_det.launch.py`, `cam_yolo_demo.launch.py`.
- Model auto-download via `scripts/download_models.sh` and `ensure_models.sh`.
- Community-oriented README with apt install and demo instructions.

### Changed

- Default model directory: `~/topsbot/models`.
- Release build defaults to `-O3` when `CMAKE_BUILD_TYPE` is unset.
- Demo launch aligned with `topsbot_usb_cam` preset API.
