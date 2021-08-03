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

Author:
Xue Yifei (yifei.xue@intel.com)
Mao Marc (marc.mao@intel.com)
Date: 2021.06.09

*/
#ifndef __REMOTE_DISPLAY_H__
#define __REMOTE_DISPLAY_H__

#include <hardware/hwcomposer2.h>

#include <vector>

#include "IRemoteDevice.h"
#include "display_protocol.h"

class RemoteDisplay {
 public:
  RemoteDisplay(int fd);
  virtual ~RemoteDisplay();

  int width() const { return mWidth; }
  int height() const { return mHeight; }
  int fps() const { return mFramerate; }
  int xdpi() const { return mXDpi; }
  int ydpi() const { return mYDpi; }
  uint32_t flags() const { return mDisplayFlags.value; }
  bool primaryHotplug() const { return mDisplayFlags.primaryHotplug; }

  uint64_t getDisplayId() const { return mDisplayId; }
  void setDisplayId(uint64_t id) { mDisplayId = id; }
  int setDisplayStatusListener(DisplayStatusListener* listener) {
    mStatusListener = listener;
    return 0;
  }

  int setDisplayEventListener(DisplayEventListener* listener) {
    mEventListener = listener;
    return 0;
  }

  // requests sent to remote
  int getConfigs();
  int createBuffer(buffer_handle_t buffer);
  int removeBuffer(buffer_handle_t buffer);
  int displayBuffer(buffer_handle_t buffer);
  int setRotation(int rotation);
  int createLayer(uint64_t id);
  int removeLayer(uint64_t id);
  int updateLayers(std::vector<layer_info_t>& layerInfo);
  int presentLayers(std::vector<layer_buffer_info_t>& layerBuffer);

  // events from remote
  int onDisplayEvent();

 private:
  int _send(const void* buf, size_t n);
  int _recv(void* buf, size_t n);
  int _sendFds(int* pfd, size_t fdlen);
  int onDisplayInfoAck(const display_event_t& ev);
  int onDisplayBufferAck(const display_event_t& ev);
  int onPresentLayersAck(const display_event_t& ev);

 private:
  bool mDisconnected = false;
  uint64_t mDisplayId = 0;
  int mSocketFd = -1;
  DisplayStatusListener* mStatusListener = nullptr;
  DisplayEventListener* mEventListener = nullptr;

  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mFramerate;
  uint32_t mXDpi;
  uint32_t mYDpi;

  display_flags mDisplayFlags = {.value = 0};
};

#endif  // __REMOTE_DISPLAY_H__
