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

def shell(command):
    p = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, close_fds=True)
    return str(p.stdout.read().strip().decode('utf-8'))

SOURCE_DIR = sys.argv[1]

sdk_git_branch = shell("git symbolic-ref HEAD --short 2>/dev/null")
if not sdk_git_branch:
    sdk_git_branch = '<Unknown>'
sdk_git_revision = shell("git rev-parse HEAD 2>/dev/null")
if not sdk_git_revision:
    sdk_git_revision = '<Unknown>'

HEADER_FILE = SOURCE_DIR + '/sdk_version.h'

if os.access(HEADER_FILE, os.R_OK):
    with open(HEADER_FILE) as fr:
        frcontent = fr.read()
        if (sdk_git_revision in frcontent) and (sdk_git_branch in frcontent):
            exit() # good, up to date


f = open(SOURCE_DIR + '/sdk_version.h', 'w')
f.write('#ifndef COMMON_SDK_VERSION_H\n')
f.write('#define COMMON_SDK_VERSION_H\n')
f.write('#define SDK_GIT_REVISION "%s"\n' % sdk_git_revision)
f.write('#define SDK_GIT_BRANCH "%s"\n' % sdk_git_branch)
f.write('#endif\n')
f.close()
