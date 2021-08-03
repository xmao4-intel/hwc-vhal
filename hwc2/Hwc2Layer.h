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
Date: 2021.06.09

*/
#ifndef __HWC2_LAYER_H__
#define __HWC2_LAYER_H__

#include <hardware/hwcomposer2.h>
#include "RemoteDisplay.h"
#include "display_protocol.h"

#include <set>

class Hwc2Layer {
 public:
  Hwc2Layer(hwc2_layer_t idx);
  ~Hwc2Layer();

  void setRemoteDisplay(RemoteDisplay* disp) { mRemoteDisplay = disp; }
  HWC2::Composition type() const { return mType; }
  void setValidatedType(HWC2::Composition t) { mValidatedType = t; }
  HWC2::Composition validatedType() const { return mValidatedType; }
  bool typeChanged() const { return mValidatedType != mType; }
  void acceptTypeChange() { mType = mValidatedType; }

  int releaseFence() const { return mReleaseFence; }
  bool changed() const { return mInfo.changed; }
  layer_info_t& info() { return mInfo; }
  bool bufferChanged() const { return mLayerBuffer.changed; }
  layer_buffer_info_t& layerBuffer() { return mLayerBuffer; }
  void setUnchanged() {
    mInfo.changed = false;
    mLayerBuffer.changed = false;
  }
  void dump();

  // Layer hooks
  HWC2::Error setCursorPosition(int32_t x, int32_t y);
  HWC2::Error setBlendMode(int32_t mode);
  HWC2::Error setBuffer(buffer_handle_t buffer, int32_t acquireFence);
  HWC2::Error setColor(hwc_color_t color);
  HWC2::Error setCompositionType(int32_t type);
  HWC2::Error setDataspace(int32_t dataspace);
  HWC2::Error setDisplayFrame(hwc_rect_t frame);
  HWC2::Error setPlaneAlpha(float alpha);
  HWC2::Error setSidebandStream(const native_handle_t* stream);
  HWC2::Error setSourceCrop(hwc_frect_t crop);
  HWC2::Error setSurfaceDamage(hwc_region_t damage);
  HWC2::Error setTransform(int32_t transform);
  HWC2::Error setVisibleRegion(hwc_region_t visible);
  HWC2::Error setZOrder(uint32_t z);
#ifdef SUPPORT_LAYER_TASK_INFO
  HWC2::Error setTaskInfo(uint32_t stackId,
                          uint32_t taskId,
                          uint32_t userId,
                          uint32_t index);
#endif

 private:
  hwc2_layer_t mLayerID = 0;
  HWC2::Composition mType = HWC2::Composition::Invalid;
  HWC2::Composition mValidatedType = HWC2::Composition::Invalid;
  int mReleaseFence = -1;

  std::set<buffer_handle_t> mBuffers;
  buffer_handle_t mBuffer = nullptr;
  int mAcquireFence = -1;

  int32_t mDataspace = 0;
  hwc_rect_t mDstFrame;
  hwc_frect_t mSrcCrop;
  hwc_region_t mDamage;
  hwc_region_t mVisibleRegion;

  hwc_color_t mColor = {.r = 0, .g = 0, .b = 0, .a = 0};
  float mAlpha = 0.0f;
  int32_t mTransform = 0;
  uint32_t mZOrder = 0;

  uint32_t mStackId = 0;
  uint32_t mTaskId = 0;
  uint32_t mUserId = 0;
  uint32_t mIndex = 0;

  RemoteDisplay* mRemoteDisplay = nullptr;
  layer_info_t mInfo;
  layer_buffer_info_t mLayerBuffer;
};

#endif  // __HWC2_LAYER_H__
