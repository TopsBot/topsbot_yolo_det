# topsbot_yolo_det

[中文文档](README_cn.md)

# Overview

`topsbot_yolo_det` runs YOLO object detection on TOPSBOT boards: it subscribes to NV12 camera images, runs NPU inference (letterbox + `ta_runtime`), and publishes structured detections on `/detections` (`tb_det_msgs/TbPerceptionTargets`). Bounding-box coordinates are mapped back to the original image space.

For live preview with overlays, use **`topsbot_webviz`** (browser Canvas); this node focuses on inference only.

Typical pipeline:

```
USB camera → /tbmem_img (NV12) → topsbot_yolo_det → /detections
                                              ↘
topsbot_webviz ← /tbmem_img + /detections → Web browser
```

Supports **YOLO11** and **YOLO26** models; switch between them via launch parameters.

# Bill of Materials

| # | Item | Notes |
| - | ---- | ----- |
| 1 | TOPSBOT MES20 board | Requires NPU (`ta_runtime`) and dmabuf support |
| 2 | USB camera | Use with `topsbot_usb_cam`; MJPEG 640×480 recommended |
| 3 | YOLO model files | `.nb` format, e.g. `yolo11s_nv12_float16.nb` |
| 4 | `topsbot_webviz` (optional) | Web preview with detection overlays |

# Usage

## Prepare Model Files

Model files (`.nb`) are **not distributed with the source repository**. On first run, `models.zip` is automatically downloaded from Aliyun Drive and extracted to `~/topsbot/models/` (no root required; the directory is created if missing).

| Model | Default path |
| ----- | ------------ |
| YOLO11 | `~/topsbot/models/yolo11s_nv12_float16.nb` |
| YOLO26 | `~/topsbot/models/yolo26s_nv12_float16.nb` |

`models.zip` contains both `.nb` files at its root (no subdirectories).

### Auto-download (default)

Before starting the YOLO node, launch checks whether models exist; if missing, it downloads and extracts from `models_zip_url` in `config/models_manifest.yaml`:

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py
```

For offline environments or when models are already in place, disable auto-download:

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py auto_download_models:=false
```

Custom model directory (must match `model_path` in yaml):

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py models_dir:=~/my_models
```

### Manual download

Maintainers should replace `models_zip_url` in `config/models_manifest.yaml` with a valid Aliyun Drive share link, then run:

```bash
# From source tree
./scripts/download_models.sh

# From installed package
$(ros2 pkg prefix topsbot_yolo_det)/share/topsbot_yolo_det/scripts/download_models.sh
```

Dependencies: `wget`, `curl`, `jq`, `unzip`, `python3-yaml`.

If paths differ, update `model_path` in `config/yolo11_demo.yaml` or `config/yolo26_demo.yaml`.

## Installation

On a target board (MES20 or other device with ROS 2 Humble):

```bash
source /opt/ros/humble/setup.bash
sudo apt update
sudo apt install -y ros-humble-topsbot-yolo-det ros-humble-topsbot-usb-cam
```

After installation:

```bash
source /opt/ros/humble/setup.bash
```

> DEB packages will be published to the apt repository; if not yet available, wait for release or contact maintainers.

### Build from source

(TBD)

## Run Detection

### Option 1: Camera + YOLO (inference only)

Starts USB camera and YOLO detection:

```bash
source /opt/ros/humble/setup.bash

# Default YOLO11
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py

# Switch to YOLO26
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py config:=yolo26_demo.yaml
```

On success, logs look like:

```text
Publishing loaned tb_img_msgs on 'tbmem_img'
yolo_det ready: model_type=yolo11 model=... input=tbmem topic=/tbmem_img detections=/detections publish_det=true
frame #1 pre=... infer=... post=... total=... ms dets=...
```

### Option 2: Camera + YOLO + Web preview (recommended)

Full demo with browser overlay (requires `topsbot_webviz`):

```bash
ros2 launch topsbot_webviz scenario.launch.py scenario:=nv12_yolo
# Browser: http://<board-ip>:8000/
```

### Option 3: YOLO node only

Use when another node already publishes `/tbmem_img`:

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py

# Switch to YOLO26
ros2 launch topsbot_yolo_det yolo_det.launch.py config:=yolo26_demo.yaml
```

### Option 4: Offline single-image test (no camera)

Validates model and bounding-box rendering without ROS topics; writes a boxed JPG:

```bash
ros2 run topsbot_yolo_det yolo_det_file_cli \
  /path/to/input.jpg /tmp/yolo_out.jpg \
  --model-path ~/topsbot/models/yolo11s_nv12_float16.nb \
  --model-type yolo11
```

Open `/tmp/yolo_out.jpg` to inspect detection boxes.

Optional flags: `--conf 0.25`, `--nms 0.45`, `--max-dets 300`. Use `--model-type yolo26` for YOLO26.

## View Results

### Web preview (recommended)

With `topsbot_webviz` scenario `nv12_yolo`:

```bash
ros2 launch topsbot_webviz scenario.launch.py scenario:=nv12_yolo
```

Open `http://<board-ip>:8000/` in a browser on the same network.

### Verify topics

```bash
ros2 topic hz /tbmem_img
ros2 topic hz /detections
ros2 topic echo /detections --once
```

**Expected**: `/detections` publishes `TbPerceptionTargets` at roughly the camera frame rate; `header.stamp` matches the corresponding `/tbmem_img` `time_stamp`.

## Model Switching and Tuning

### Switch YOLO11 / YOLO26

Change the launch `config` parameter only — no code changes:

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py config:=yolo26_demo.yaml
```

Ensure the corresponding `.nb` file exists at the path specified by `model_path`.

### Reduce load (frame skip)

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py \
  --ros-args -p frame_skip:=2
```

### Disable structured detections

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py \
  --ros-args -p publish_detections:=false
```

# Interface Reference

## Subscribed Topics

| Name | Message type | Description |
| ---- | ------------ | ----------- |
| `/tbmem_img` | `tb_img_msgs/msg/TbMsg480P` etc. | Default input, NV12 zero-copy (`input_mode:=tbmem`) |

## Published Topics

| Name | Message type | Description |
| ---- | ------------ | ----------- |
| `/detections` | `tb_det_msgs/msg/TbPerceptionTargets` | Structured detections in **original image coordinates** (**default on**) |

> **v0.2.0 breaking change**: `/yolo_viz` (`sensor_msgs/Image` bgr8 with drawn boxes) was removed. Use `topsbot_webviz` for preview, or `yolo_det_file_cli` / `input_mode:=file` for offline JPG output.

## Launch Parameters

| Parameter | Description | Default |
| --------- | ----------- | ------- |
| `config` | Model config under `config/` | `yolo11_demo.yaml` |
| `models_dir` | Directory for `.nb` models | `~/topsbot/models` |
| `auto_download_models` | Auto-download when missing | `true` |

Common `config` values:

| config | Model |
| ------ | ----- |
| `yolo11_demo.yaml` | YOLO11 (**default**) |
| `yolo26_demo.yaml` | YOLO26 |

## Node Parameters

| Parameter | Description | Default |
| --------- | ----------- | ------- |
| `model_type` | Model type | `yolo11` |
| `model_path` | Path to `.nb` model | See demo yaml |
| `input_mode` | Input source | `tbmem` |
| `image_topic` | Subscription topic | `/tbmem_img` |
| `detections_topic` | Detection output topic | `/detections` |
| `publish_detections` | Publish `/detections` | `true` |
| `frame_skip` | Frames to skip, 0 = none | `0` |
| `enable_profile` | Print pre/infer/post timing | `true` |

`input_mode` values:

| Value | Description |
| ----- | ----------- |
| `tbmem` | Subscribe to `/tbmem_img` (**default**, with `topsbot_usb_cam`) |
| `sensor_image` | Subscribe to `sensor_msgs/Image` (debug) |
| `file` | Read local JPG, write boxed output and exit (see `config/yolo_file_demo.yaml`) |

Post-processing parameters (confidence, NMS, etc.) are in `config/yolo_demo_common.yaml`.

# FAQ

**1. Model load failure on startup?**

- Ensure `models_zip_url` in `config/models_manifest.yaml` is a valid share link
- Or run `download_models.sh` manually
- Verify `model_path` in `config/yolo11_demo.yaml` (or `yolo26_demo.yaml`) points to an existing `.nb` file

**2. No data on `/detections`?**

- Confirm the camera is publishing: `ros2 topic hz /tbmem_img`
- Full demo requires `topsbot_usb_cam`
- Camera must output **NV12** zero-copy; use `topsbot_usb_cam`'s `mjpeg2nv12_zerocopy` preset
- Check `publish_detections:=true` (default since v0.2.0)

**3. dmabuf / heap errors?**

The YOLO node must run on MES20 with NPU and dmabuf support; real-time detection cannot run on a PC.

**4. How to preview detection boxes?**

Use `topsbot_webviz`:

```bash
ros2 launch topsbot_webviz scenario.launch.py scenario:=nv12_yolo
```

**5. Low frame rate?**

- Increase `frame_skip`
- Check pre/infer/post timings in logs when `enable_profile:=true`

**6. One-time offline validation without a camera?**

Use `yolo_det_file_cli` (see Option 4 above), or `input_mode:=file` with `config/yolo_file_demo.yaml`.

**7. USB camera integration?**

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py
```

The camera starts in NV12 zero-copy mode automatically; see `topsbot_usb_cam` documentation for details.

**8. Upgrading from v0.1.0?**

- `/yolo_viz` is removed; subscribe to `/detections` or use `topsbot_webviz`
- `publish_detections` now defaults to `true`
- `viz_topic` parameter removed

---

## License

Apache License 2.0 — see [LICENSE](./LICENSE) and [NOTICE](./NOTICE).

Contributions welcome; see [CONTRIBUTING.md](./CONTRIBUTING.md).
