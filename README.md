# topsbot_yolo_det

[中文文档](README_cn.md)

# Overview

`topsbot_yolo_det` runs YOLO object detection on TOPSBOT boards: it subscribes to NV12 camera images, runs NPU inference, draws bounding boxes on the original image, and publishes the visualization topic `/yolo_viz` for preview or downstream use.

Typical pipeline:

```
USB camera → /tbmem_img (NV12) → YOLO detection → /yolo_viz (image with boxes)
```

Supports **YOLO11** and **YOLO26** models; switch between them via launch parameters.

# Bill of Materials

| # | Item | Notes |
| - | ---- | ----- |
| 1 | TOPSBOT MES20 board | Requires NPU (`ta_runtime`) and dmabuf support |
| 2 | USB camera | Use with `topsbot_usb_cam`; MJPEG 640×480 recommended |
| 3 | YOLO model files | `.nb` format, e.g. `yolo11s_nv12_float16.nb` |

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

### Option 1: Camera + YOLO one-shot demo (recommended)

Starts USB camera and YOLO detection together — ideal for first-time setup:

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
yolo_det ready: model_type=yolo11 model=... input=tbmem topic=/tbmem_img viz=/yolo_viz
frame #1 pre=... infer=... post=... viz=... total=... ms dets=...
```

### Option 2: YOLO node only

Use when another node already publishes `/tbmem_img`:

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py

# Switch to YOLO26
ros2 launch topsbot_yolo_det yolo_det.launch.py config:=yolo26_demo.yaml
```

### Option 3: Offline single-image test (no camera)

Validates model and bounding-box rendering without ROS topics:

```bash
ros2 run topsbot_yolo_det yolo_det_file_cli \
  /path/to/input.jpg /tmp/yolo_out.jpg \
  --model-path ~/topsbot/models/yolo11s_nv12_float16.nb \
  --model-type yolo11
```

Open `/tmp/yolo_out.jpg` to inspect detection boxes.

Optional flags: `--conf 0.25`, `--nms 0.45`, `--max-dets 300`. Use `--model-type yolo26` for YOLO26.

## View Results

### Live preview (recommended)

On the board or a PC on the same network (`ROS_DOMAIN_ID` must match):

```bash
ros2 run rqt_image_view rqt_image_view /yolo_viz
```

### Verify topics are publishing

```bash
ros2 topic hz /tbmem_img    # camera input (full demo)
ros2 topic hz /yolo_viz     # boxed output
ros2 topic echo /yolo_viz --once | head -20
```

**Expected**: `/yolo_viz` is `bgr8` image at the same resolution as the camera (typically 640×480).

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

### Publish structured detections

By default only boxed images are published; for `tb_det_msgs` detection messages:

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py \
  --ros-args -p publish_detections:=true

ros2 topic echo /detections --once
```

# Interface Reference

## Subscribed Topics

| Name | Message type | Description |
| ---- | ------------ | ----------- |
| `/tbmem_img` | `tb_img_msgs/msg/TbMsg480P` etc. | Default input, NV12 zero-copy (`input_mode:=tbmem`) |

## Published Topics

| Name | Message type | Description |
| ---- | ------------ | ----------- |
| `/yolo_viz` | `sensor_msgs/msg/Image` | `bgr8` image with boxes (**enabled by default**) |
| `/detections` | `tb_det_msgs/msg/TbPerceptionTargets` | Structured detections (disabled by default) |

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
| `viz_topic` | Visualization output | `/yolo_viz` |
| `frame_skip` | Frames to skip, 0 = none | `0` |
| `enable_profile` | Print per-stage timing | `true` |
| `publish_detections` | Publish `/detections` | `false` |

`input_mode` values:

| Value | Description |
| ----- | ----------- |
| `tbmem` | Subscribe to `/tbmem_img` (**default**, with `topsbot_usb_cam`) |
| `sensor_image` | Subscribe to `sensor_msgs/Image` (debug) |
| `file` | Read local JPG, write output and exit (see `config/yolo_file_demo.yaml`) |

Post-processing parameters (confidence, NMS, etc.) are in `config/yolo_demo_common.yaml`.

# FAQ

**1. Model load failure on startup?**

- Ensure `models_zip_url` in `config/models_manifest.yaml` is a valid share link
- Or run `download_models.sh` manually
- Verify `model_path` in `config/yolo11_demo.yaml` (or `yolo26_demo.yaml`) points to an existing `.nb` file

**2. No data on `/yolo_viz`?**

- Confirm the camera is publishing: `ros2 topic hz /tbmem_img`
- Full demo requires `topsbot_usb_cam`
- Camera must output **NV12** zero-copy; for RGB output, use `topsbot_usb_cam`'s `mjpeg2nv12_zerocopy` preset

**3. dmabuf / heap errors?**

The YOLO node must run on MES20 with NPU and dmabuf support; real-time detection cannot run on a PC.

**4. Cannot see `/yolo_viz` from PC?**

Ensure `ROS_DOMAIN_ID` matches on both sides and check with `ros2 topic list`.

**5. Low frame rate?**

- Increase `frame_skip`
- Check pre/infer/post/viz timings in logs when `enable_profile:=true`

**6. One-time offline validation without a camera?**

Use `yolo_det_file_cli` (see Option 3 above), or `input_mode:=file` with `config/yolo_file_demo.yaml`.

**7. USB camera integration?**

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py
```

The camera starts in NV12 zero-copy mode automatically; see `topsbot_usb_cam` documentation for details.

---

## License

Apache License 2.0 — see [LICENSE](./LICENSE) and [NOTICE](./NOTICE).

Contributions welcome; see [CONTRIBUTING.md](./CONTRIBUTING.md).
