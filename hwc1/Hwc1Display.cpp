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

//#define LOG_NDEBUG 0
#include <cutils/log.h>

#include "Hwc1Display.h"

Hwc1Display::Hwc1Display(int id) : mDisplayID(id) {}
Hwc1Display::~Hwc1Display() {}

int Hwc1Display::init(RemoteDisplay* rd) {
  if (!rd)
    return -1;

  ALOGV("Hwc2Display(%d)::%s", mDisplayID, __func__);

  mRemoteDisplay = rd;
  mWidth = mRemoteDisplay->width();
  mHeight = mRemoteDisplay->height();
  mFramerate = mRemoteDisplay->fps();
  mXDpi = mRemoteDisplay->xdpi();
  mYDpi = mRemoteDisplay->ydpi();
  return 0;
}

int Hwc1Display::getConfigs(uint32_t* configs, size_t* numConfigs) {
  ALOGV("Hwc1Display::getConfigs");

  if (numConfigs) {
    *numConfigs = 1;
  }
  if (configs) {
    *configs = 1;
  }
  return 0;
}
int Hwc1Display::getAttributes(uint32_t config,
                               const uint32_t* attributes,
                               int32_t* values) {
  int ret = 0;
  uint32_t attrib = 0;
  int32_t rvalue = 0;

  ALOGV("Hwc1Display::getAttributes:config=%d", config);

  while (*attributes != HWC_DISPLAY_NO_ATTRIBUTE) {
    if (attributes) {
      attrib = *attributes;
    }
    switch (attrib) {
      case HWC_DISPLAY_VSYNC_PERIOD:
        rvalue = 1000 * 1000 * 1000 / mFramerate;
        break;
      case HWC_DISPLAY_WIDTH:
        rvalue = mWidth;
        break;
      case HWC_DISPLAY_HEIGHT:
        rvalue = mHeight;
        break;
      case HWC_DISPLAY_DPI_X:
        rvalue = mXDpi * 1000;
        break;
      case HWC_DISPLAY_DPI_Y:
        rvalue = mYDpi * 1000;
        break;
      case HWC_DISPLAY_COLOR_TRANSFORM:
        rvalue = 0;
        break;
      default:
        ALOGI("%s:%d : unknown attribute %d", __func__, __LINE__, attrib);
        rvalue = 0;
        break;
    }
    if (values) {
      *values = rvalue;
    }
    attributes++;
    values++;
  }
  return ret;
}

int Hwc1Display::prepare(hwc_display_contents_1_t* disp) {
  if (!disp)
    return -1;

  // ALOGD("Hwc1Display::prepare:flags=%d, numHwLayers=%zu", disp->flags,
  //      disp->numHwLayers);

  if (disp->flags & HWC_GEOMETRY_CHANGED) {
    if (mEnableSingleLayerOpt) {
      bool singleLayer =
          (disp->numHwLayers == 2) && (disp->hwLayers[0].transform == 0);

      if (singleLayer && !mSingleLayer) {
        mSingleLayer = true;
        ALOGD("Hwc1Display::prepare: enter single layer mode");
      } else if (mSingleLayer && !singleLayer) {
        ALOGD("Hwc1Display::prepare: exit single layer mode");
        exitSingleLayer();
      }
      mSingleLayer = singleLayer;
    }

    for (size_t i = 0; i < disp->numHwLayers; i++) {
      hwc_layer_1_t* l = disp->hwLayers + i;

      switch (l->compositionType) {
        case HWC_FRAMEBUFFER:
          break;
        case HWC_FRAMEBUFFER_TARGET:
          break;
        case HWC_OVERLAY:
        case HWC_BACKGROUND:
        default:
          l->compositionType = HWC_FRAMEBUFFER;
          break;
      }
    }
  }

  if (mEnableSingleLayerOpt && mSingleLayer) {
    ALOGD("Hwc1Display::prepare singleLayer %p %d", disp->hwLayers[0].handle,
          disp->hwLayers[0].compositionType);

    disp->hwLayers[0].compositionType = HWC_OVERLAY;
  }

  return 0;
}

int Hwc1Display::set(hwc_display_contents_1_t* disp) {
  if (!disp)
    return -1;

  // ALOGD("Hwc1Display::set:numHwLayers=%zu", disp->numHwLayers);

  for (size_t i = 0; i < disp->numHwLayers; i++) {
    hwc_layer_1_t* l = &disp->hwLayers[i];
    if (l->acquireFenceFd >= 0) {
      // sync_wait(l->acquireFenceFd, -1);
      close(l->acquireFenceFd);
      l->acquireFenceFd = -1;
      l->releaseFenceFd = -1;
    }
  }
  disp->retireFenceFd = -1;

  if (mCompositionMode == COMPOSITION_MODE::COMPOSITION_FBT &&
      disp->numHwLayers < 2) {
    return 0;
  }

  // Fullscreen opt
  if (mEnableSingleLayerOpt) {
    if (mSingleLayer && mRemoteDisplay) {
      ALOGD("Hwc1Display::setSingleLayerBuffer %p %d", disp->hwLayers[0].handle,
            disp->hwLayers[0].compositionType);

      setSingleLayerBuffer(disp->hwLayers[0].handle);
      return 0;
    }
  }

  buffer_handle_t buffer = disp->hwLayers[disp->numHwLayers - 1].handle;

  if (mRemoteDisplay && buffer) {
    bool isNew = true;

    for (auto fbt : mFbtBuffers) {
      if (buffer == fbt) {
        isNew = false;
      }
    }
    if (isNew) {
      mFbtBuffers.push_back(buffer);
      mRemoteDisplay->createBuffer(buffer);
    }
    mRemoteDisplay->displayBuffer(buffer);
  }
  return 0;
}

void Hwc1Display::dumpLayer(size_t idx, hwc_layer_1_t const* l) {
  ALOGI(
      "layers[%zu] : type=%d, hint=%x, flags=%08x, handle=%p, "
      "transform=%x, "
      "blend=%x",
      idx, l->compositionType, l->hints, l->flags, l->handle, l->transform,
      l->blending);

  ALOGI("\t\t\t src_crop={%f,%f,%f,%f}, dst_frame={%d,%d,%d,%d}",
        l->sourceCropf.left, l->sourceCropf.top, l->sourceCropf.right,
        l->sourceCropf.bottom, l->displayFrame.left, l->displayFrame.top,
        l->displayFrame.right, l->displayFrame.bottom);

  ALOGI("\t\t\t visual_region_num_rects=%zu", l->visibleRegionScreen.numRects);
  for (size_t i = 0; i < l->visibleRegionScreen.numRects; i++) {
    const hwc_rect_t* rect = l->visibleRegionScreen.rects + i;
    ALOGI("\t\t\t\t\t rect[%zu]={%d, %d, %d, %d}", i, rect->left, rect->top,
          rect->right, rect->bottom);
  }

  ALOGI("\t\t\t acquireFenceFd=%d, releaseFenceFd=%d, planeAlpha=%d",
        l->acquireFenceFd, l->releaseFenceFd, l->planeAlpha);
}

int Hwc1Display::setSingleLayerBuffer(buffer_handle_t b) {
  ALOGV("Hwc1Display::setSingleLayerBuffer");

  bool isNew = true;

  for (auto ab : mAppBuffers) {
    if (ab == b) {
      isNew = false;
    }
  }
  if (isNew) {
    mAppBuffers.push_back(b);
    mRemoteDisplay->createBuffer(b);
  }
  mRemoteDisplay->displayBuffer(b);

  return 0;
}

int Hwc1Display::exitSingleLayer() {
  ALOGV("Hwc1Display::exitSingleLayer");

  if (mRemoteDisplay) {
    for (auto b : mAppBuffers) {
      mRemoteDisplay->removeBuffer(b);
    }
    mAppBuffers.clear();
  }
  return 0;
}
