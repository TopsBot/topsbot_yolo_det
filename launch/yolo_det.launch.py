import os
import sys

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

_launch_dir = os.path.dirname(os.path.abspath(__file__))
if _launch_dir not in sys.path:
    sys.path.insert(0, _launch_dir)
from model_utils import ensure_models  # noqa: E402


def _launch_setup(context, *args, **kwargs):
    ensure_models(context)

    config_name = LaunchConfiguration('config').perform(context)
    pkg_share = FindPackageShare('topsbot_yolo_det')

    return [Node(
        package='topsbot_yolo_det',
        executable='yolo_det_node',
        name='yolo_det_node',
        output='screen',
        parameters=[
            PathJoinSubstitution([pkg_share, 'config', 'yolo_demo_common.yaml']),
            PathJoinSubstitution([pkg_share, 'config', config_name]),
        ],
    )]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'config',
            default_value='yolo11_demo.yaml',
            description='Model config under topsbot_yolo_det/config/'),
        DeclareLaunchArgument(
            'models_dir',
            default_value='~/topsbot/models',
            description='Directory for YOLO .nb model files'),
        DeclareLaunchArgument(
            'auto_download_models',
            default_value='true',
            description='Download missing models from models_manifest.yaml before start'),
        OpaqueFunction(function=_launch_setup),
    ])
