# topsbot_yolo_det

[English](README.md)

# 功能介绍

`topsbot_yolo_det` 在 TOPSBOT 板端运行 YOLO 目标检测：订阅相机 NV12 图像，经 NPU 推理后在原图画框，并发布可视化话题 `/yolo_viz` 供预览或下游使用。

典型链路：

```
USB 相机 → /tbmem_img (NV12) → YOLO 检测 → /yolo_viz (带检测框的图像)
```

支持 **YOLO11**、**YOLO26** 两种模型，可在 launch 参数中切换。

# 物料清单

| 序号 | 名称 | 说明 |
| ---- | ---- | ---- |
| 1 | TOPSBOT MES20 板卡 | 需 NPU（`ta_runtime`）与 dmabuf 环境 |
| 2 | USB 摄像头 | 配合 `topsbot_usb_cam` 使用，建议 MJPEG 640×480 |
| 3 | YOLO 模型文件 | `.nb` 格式，如 `yolo11s_nv12_float16.nb` |

# 使用方法

## 准备模型文件

模型文件（`.nb`）**不随源码仓库分发**，首次运行时会自动从阿里云盘下载 `models.zip` 并解压到 `~/topsbot/models/`（无需 root 权限，目录不存在时会自动创建）。

| 模型 | 默认路径 |
| ---- | -------- |
| YOLO11 | `~/topsbot/models/yolo11s_nv12_float16.nb` |
| YOLO26 | `~/topsbot/models/yolo26s_nv12_float16.nb` |

`models.zip` 根目录包含上述两个 `.nb` 文件（无子目录）。

### 自动下载（默认）

launch 启动 YOLO 节点前会自动检查模型是否存在；缺失则从 `config/models_manifest.yaml` 中的 `models_zip_url` 下载并解压：

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py
```

离线环境或已手动放置模型时，可关闭自动下载：

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py auto_download_models:=false
```

自定义模型目录（需与 yaml 中 `model_path` 一致）：

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py models_dir:=~/my_models
```

### 手动下载

维护者需在 `config/models_manifest.yaml` 中将 `models_zip_url` 替换为实际阿里云盘分享链接后，可手动执行：

```bash
# 源码树
./scripts/download_models.sh

# 已安装包
$(ros2 pkg prefix topsbot_yolo_det)/share/topsbot_yolo_det/scripts/download_models.sh
```

依赖工具：`wget`、`curl`、`jq`、`unzip`、`python3-yaml`。

若路径不同，请修改 `config/yolo11_demo.yaml` 或 `config/yolo26_demo.yaml` 中的 `model_path`。

## 安装功能包

在目标板（MES20 等已安装 ROS 2 Humble 的设备）上操作：

```bash
source /opt/ros/humble/setup.bash
sudo apt update
sudo apt install -y ros-humble-topsbot-yolo-det ros-humble-topsbot-usb-cam
```

安装完成后：

```bash
source /opt/ros/humble/setup.bash
```

> DEB 包后续会上传至 apt 源；当前若源中尚未提供该包，请等待发布或联系维护者。

### 从源码编译

（待补充）

## 启动检测

### 方式一：相机 + YOLO 一键 demo（推荐）

USB 摄像头与 YOLO 检测同时启动，适合首次体验：

```bash
source /opt/ros/humble/setup.bash

# 默认 YOLO11
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py

# 切换 YOLO26
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py config:=yolo26_demo.yaml
```

启动成功时，日志类似：

```text
Publishing loaned tb_img_msgs on 'tbmem_img'
yolo_det ready: model_type=yolo11 model=... input=tbmem topic=/tbmem_img viz=/yolo_viz
frame #1 pre=... infer=... post=... viz=... total=... ms dets=...
```

### 方式二：仅启动 YOLO 节点

相机已由其他节点发布 `/tbmem_img` 时使用：

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py

# 切换 YOLO26
ros2 launch topsbot_yolo_det yolo_det.launch.py config:=yolo26_demo.yaml
```

### 方式三：离线单张图片测试（无需相机）

不依赖 ROS 话题，用本地 JPG 验证模型与画框效果：

```bash
ros2 run topsbot_yolo_det yolo_det_file_cli \
  /path/to/input.jpg /tmp/yolo_out.jpg \
  --model-path ~/topsbot/models/yolo11s_nv12_float16.nb \
  --model-type yolo11
```

完成后打开 `/tmp/yolo_out.jpg` 查看检测框。

可选参数：`--conf 0.25`、`--nms 0.45`、`--max-dets 300`。YOLO26 时将 `--model-type` 换为 `yolo26`。

## 查看效果

### 实时预览（推荐）

板端或同一网络的 PC 上（`ROS_DOMAIN_ID` 需一致）：

```bash
ros2 run rqt_image_view rqt_image_view /yolo_viz
```

### 确认话题有数据

```bash
ros2 topic hz /tbmem_img    # 相机输入（完整 demo 时）
ros2 topic hz /yolo_viz     # 画框输出
ros2 topic echo /yolo_viz --once | head -20
```

**预期**：`/yolo_viz` 为 `bgr8` 图像，分辨率与相机原图一致（典型 640×480）。

## 切换模型与常用调整

### 切换 YOLO11 / YOLO26

只需改 launch 的 `config` 参数，无需改代码：

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py config:=yolo26_demo.yaml
```

切换前确认对应 `.nb` 文件已放在 `model_path` 所指路径。

### 降低负载（跳帧）

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py \
  --ros-args -p frame_skip:=2
```

### 发布结构化检测结果

默认只发布画框图像；如需 `tb_det_msgs` 检测消息：

```bash
ros2 launch topsbot_yolo_det yolo_det.launch.py \
  --ros-args -p publish_detections:=true

ros2 topic echo /detections --once
```

# 接口说明

## 订阅话题

| 名称 | 消息类型 | 说明 |
| ---- | -------- | ---- |
| `/tbmem_img` | `tb_img_msgs/msg/TbMsg480P` 等 | 默认输入，NV12 零拷贝（`input_mode:=tbmem`） |

## 发布话题

| 名称 | 消息类型 | 说明 |
| ---- | -------- | ---- |
| `/yolo_viz` | `sensor_msgs/msg/Image` | 画框后的 `bgr8` 图像（**默认开启**） |
| `/detections` | `tb_det_msgs/msg/TbPerceptionTargets` | 结构化检测输出（默认关闭） |

## Launch 参数

| 参数名 | 说明 | 默认值 |
| ------ | ---- | ------ |
| `config` | 模型配置，位于 `config/` 下 | `yolo11_demo.yaml` |
| `models_dir` | `.nb` 模型安装目录 | `~/topsbot/models` |
| `auto_download_models` | 缺失时自动下载 | `true` |

常用 `config`：

| config | 模型 |
| ------ | ---- |
| `yolo11_demo.yaml` | YOLO11（**默认**） |
| `yolo26_demo.yaml` | YOLO26 |

## 节点参数

| 参数名 | 说明 | 默认值 |
| ------ | ---- | ------ |
| `model_type` | 模型类型 | `yolo11` |
| `model_path` | `.nb` 模型路径 | 见各 demo yaml |
| `input_mode` | 输入方式 | `tbmem` |
| `image_topic` | 订阅话题 | `/tbmem_img` |
| `viz_topic` | 画框输出话题 | `/yolo_viz` |
| `frame_skip` | 跳帧数，0 表示不跳 | `0` |
| `enable_profile` | 打印各阶段耗时 | `true` |
| `publish_detections` | 是否发布 `/detections` | `false` |

`input_mode` 可选值：

| 值 | 说明 |
| -- | ---- |
| `tbmem` | 订阅 `/tbmem_img`（**默认**，配合 `topsbot_usb_cam`） |
| `sensor_image` | 订阅 `sensor_msgs/Image`（调试） |
| `file` | 读取本地 JPG，写输出后退出（见 `config/yolo_file_demo.yaml`） |

后处理相关参数（置信度、NMS 等）见 `config/yolo_demo_common.yaml`。

# 常见问题

**1. 启动报模型加载失败？**

- 确认 `config/models_manifest.yaml` 中 `models_zip_url` 已替换为有效分享链接
- 或手动运行 `download_models.sh`
- 检查 `config/yolo11_demo.yaml`（或 `yolo26_demo.yaml`）里 `model_path` 是否指向真实存在的 `.nb` 文件

**2. `/yolo_viz` 没有数据？**

- 先确认相机在发图：`ros2 topic hz /tbmem_img`
- 完整 demo 需同时安装 `topsbot_usb_cam`
- 相机须为 **NV12** 零拷贝输出；若相机发 RGB，请改用 `topsbot_usb_cam` 的 `mjpeg2nv12_zerocopy` preset

**3. 报 dmabuf / heap 相关错误？**

YOLO 节点需在 MES20 板端运行，并具备 NPU 与 dmabuf 环境；PC 上无法直接跑实时检测链路。

**4. PC 上看不到 `/yolo_viz`？**

确认两端 `ROS_DOMAIN_ID` 一致，并用 `ros2 topic list` 检查话题是否可见。

**5. 帧率偏低怎么办？**

- 增大 `frame_skip` 跳帧
- 查看日志中 `enable_profile:=true` 打印的 pre/infer/post/viz 耗时，定位瓶颈

**6. 如何只做一次离线验证、不接相机？**

使用 `yolo_det_file_cli`（见上文「方式三」），或用 `input_mode:=file` 加载 `config/yolo_file_demo.yaml`。

**7. 如何与 USB 相机联调？**

```bash
ros2 launch topsbot_yolo_det cam_yolo_demo.launch.py
```

相机会自动以 NV12 零拷贝方式启动；详见 `topsbot_usb_cam` 文档。

---

## 许可

Apache License 2.0 — 见 [LICENSE](./LICENSE) 与 [NOTICE](./NOTICE)。

欢迎贡献，请参阅 [CONTRIBUTING.md](./CONTRIBUTING.md)。
