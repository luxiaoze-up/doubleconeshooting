#!/usr/bin/env python3
"""检查当前 operationMode 状态"""
import os
os.environ['TANGO_HOST'] = 'localhost:10000'

import tango
d = tango.DeviceProxy("sys/vacuum/2")
print(f"operationMode: {d.operationMode}")
print(f"systemState: {d.systemState}")
print(f"simulatorMode: {d.simulatorMode}")

