#!/usr/bin/env python3
"""Compatibility entry point for the pinned shared-module check."""
from pathlib import Path
import subprocess
import sys
module = Path(__file__).resolve().parents[1] / 'ThirdParty/fasegroove-common'
command = [sys.executable, str(module / 'VerifyModule.py')]
if len(sys.argv) > 1:
    command += ['--canonical', sys.argv[1]]
sys.exit(subprocess.call(command))
