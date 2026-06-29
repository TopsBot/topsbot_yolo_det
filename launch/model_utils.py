# Copyright (c) 2026 TOPSBOT contributors.
#
# Licensed under the Apache License, Version 2.0 (the "License");

import os
import subprocess

from launch.substitutions import LaunchConfiguration
from launch.utilities import perform_substitutions
from launch_ros.substitutions import FindPackageShare


def ensure_models(context) -> None:
    """Block until required .nb models exist (download when enabled)."""
    auto = LaunchConfiguration('auto_download_models').perform(context)
    models_dir = os.path.expanduser(LaunchConfiguration('models_dir').perform(context))
    share = perform_substitutions(context, [FindPackageShare('topsbot_yolo_det')])
    script = os.path.join(share, 'scripts', 'ensure_models.sh')
    manifest = os.path.join(share, 'config', 'models_manifest.yaml')

    cmd = [
        script,
        '--manifest', manifest,
        '--models-dir', models_dir,
    ]
    if auto.lower() not in ('true', '1', 'yes'):
        cmd.append('--no-download')

    proc = subprocess.run(cmd, check=False)
    if proc.returncode != 0:
        raise RuntimeError(
            'YOLO model files are missing or download failed. '
            f'Expected under {models_dir}. '
            'Run download_models.sh manually or launch with auto_download_models:=true.')
