/*
Copyright (C) 2021 Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions
and limitations under the License.


SPDX-License-Identifier: Apache-2.0

Author: Xue Yifei (yifei.xue@intel.com)
Date: 2021.06.09

*/

#ifndef __IREMOTE_DEVICE_H__
#define __IREMOTE_DEVICE_H__

#include <vector>
#include <display_protocol.h>

class RemoteDisplay;
struct IRemoteDevice {
  virtual ~IRemoteDevice() {}
  virtual int addRemoteDisplay(RemoteDisplay* rd) = 0;
  virtual int removeRemoteDisplay(RemoteDisplay* rd) = 0;
  virtual int getMaxRemoteDisplayCount() = 0;
  virtual int getRemoteDisplayCount() = 0;
};

struct DisplayStatusListener {
  virtual ~DisplayStatusListener(){};
  virtual int onConnect(int fd) = 0;
  virtual int onDisconnect(int fd) = 0;
};

struct DisplayEventListener {
  virtual ~DisplayEventListener(){};
  virtual int onBufferDisplayed(const buffer_info_t& info) = 0;
  virtual int onPresented(std::vector<layer_buffer_info_t>& layerBuffer, int& fence) = 0;
};

#endif  //__IREMOTE_DEVICE_H__
