#!/usr/bin/env python3
import os
import sys
import stat
import re

import sys, os, os.path, platform
import time, subprocess
from subprocess import *

try:
    import simplejson as json
except ImportError:
    import json

def shell(command, cwd):
    p = subprocess.Popen(command, shell=True, cwd=cwd, stdout=subprocess.PIPE, close_fds=True)
    return p.stdout.read().strip().decode('utf-8')

SOURCE_DIR = sys.argv[1]
GCC_VERSION = sys.argv[2]

if not GCC_VERSION:
    GCC_VERSION = shell('g++ --version | head -n1 | awk \'{print $3}\'', SOURCE_DIR)

build_info = {}
build_info['ODPS_BUILD_TIME'] = shell('date "+%Y/%m/%d %T%z"', SOURCE_DIR)
build_info['ODPS_BUILD_GCC_VERSION'] = GCC_VERSION
git_revision = shell("git rev-parse HEAD 2>/dev/null", SOURCE_DIR)
if not git_revision:
    git_revision = '<Unknown>'
build_info['ODPS_GIT_REVISION'] = git_revision
git_branch  = shell("git symbolic-ref HEAD --short 2>/dev/null", SOURCE_DIR)
if not git_branch:
    git_branch = "<Unknown>"
build_info["ODPS_GIT_BRANCH"] = git_branch
git_url = shell("git ls-remote --get-url origin 2>/dev/null", SOURCE_DIR)
if not git_url:
    git_url = '<Unknown>'
build_info['ODPS_GIT_URL'] = git_url
buildinfo = json.dumps(build_info, indent=4, sort_keys=True)

f = open(SOURCE_DIR + '/build_sdk_version', 'w')
f.write(buildinfo)
f.close()
