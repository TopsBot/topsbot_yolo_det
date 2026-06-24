# Copyright (c) 2026 TOPSBOT contributors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_name = LaunchConfiguration('config')
    models_dir = LaunchConfiguration('models_dir')
    auto_download_models = LaunchConfiguration('auto_download_models')

    usb_cam_launch = PathJoinSubstitution([
        FindPackageShare('topsbot_usb_cam'),
        'launch',
        'usb_cam.launch.py',
    ])

    yolo_launch = PathJoinSubstitution([
        FindPackageShare('topsbot_yolo_det'),
        'launch',
        'yolo_det.launch.py',
    ])

    return LaunchDescription([
        DeclareLaunchArgument(
            'config',
            default_value='yolo11_demo.yaml',
            description='YOLO model yaml (yolo11_demo.yaml or yolo26_demo.yaml)'),
        DeclareLaunchArgument(
            'models_dir',
            default_value='~/topsbot/models',
            description='Directory for YOLO .nb model files'),
        DeclareLaunchArgument(
            'auto_download_models',
            default_value='true',
            description='Download missing models before starting YOLO node'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(usb_cam_launch),
            launch_arguments={
                'preset': 'presets/mjpeg2nv12_zerocopy.yaml',
            }.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(yolo_launch),
            launch_arguments={
                'config': config_name,
                'models_dir': models_dir,
                'auto_download_models': auto_download_models,
            }.items(),
        ),
    ])
