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

//#define LOG_NDEBUG 0

#include <errno.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "Hwc1Device.h"
#include "Hwc1Display.h"

Hwc1Device::Hwc1Device() {
  /* initialize the procs */
  common.tag = HARDWARE_DEVICE_TAG;
  common.version = HWC_DEVICE_API_VERSION_1_3;
  common.close = Hwc1Device::hook_close;

  hwc_composer_device_1_t::prepare =
      DeviceHook<int, decltype(&Hwc1Device::prepare), &Hwc1Device::prepare,
                 size_t, hwc_display_contents_1_t**>;
  hwc_composer_device_1_t::set =
      DeviceHook<int, decltype(&Hwc1Device::set), &Hwc1Device::set, size_t,
                 hwc_display_contents_1_t**>;
  hwc_composer_device_1_t::eventControl =
      DeviceHook<int, decltype(&Hwc1Device::eventControl),
                 &Hwc1Device::eventControl, int, int, int>;
  hwc_composer_device_1_t::blank = DeviceHook<int, decltype(&Hwc1Device::blank),
                                              &Hwc1Device::blank, int, int>;
  hwc_composer_device_1_t::query = DeviceHook<int, decltype(&Hwc1Device::query),
                                              &Hwc1Device::query, int, int*>;
  hwc_composer_device_1_t::registerProcs =
      DeviceHook<void, decltype(&Hwc1Device::registerProcs),
                 &Hwc1Device::registerProcs, hwc_procs_t const*>;
  hwc_composer_device_1_t::dump = DeviceHook<void, decltype(&Hwc1Device::dump),
                                             &Hwc1Device::dump, char*, int>;
  hwc_composer_device_1_t::getDisplayConfigs =
      DeviceHook<int, decltype(&Hwc1Device::getDisplayConfigs),
                 &Hwc1Device::getDisplayConfigs, int, uint32_t*, size_t*>;
  hwc_composer_device_1_t::getDisplayAttributes =
      DeviceHook<int, decltype(&Hwc1Device::getDisplayAttributes),
                 &Hwc1Device::getDisplayAttributes, int, uint32_t,
                 const uint32_t*, int32_t*>;
}
Hwc1Device::~Hwc1Device() {}

int Hwc1Device::init(const struct hw_module_t* module) {
  ALOGV("%s", __func__);

  mRemoteDisplayMgr = std::unique_ptr<RemoteDisplayMgr>(new RemoteDisplayMgr());
  if (!mRemoteDisplayMgr) {
    ALOGE("Failed to create remote display manager, out of memory");
    return -1;
  }
  mRemoteDisplayMgr->init(this);

  if (mRemoteDisplayMgr->connectToRemote() < 0) {
    mDummyDisplay = std::unique_ptr<Hwc1Display>(new Hwc1Display(0));
    if (!mDummyDisplay) {
      ALOGE("Failed to create dummy display, out of memory");
      return -1;
    }
  }

  return 0;
}

int Hwc1Device::addRemoteDisplay(RemoteDisplay* rd) {
  if (!rd)
    return -1;

  int id = 0;

  {  // lock scope
    std::unique_lock<std::mutex> lk(mDisplayMutex);

    for (id = 0; id < kMaxDisplays; id++) {
      if (!mRemoteDisplays[id]) {
        break;
      }
    }
    if (id >= kMaxDisplays) {
      ALOGE("Too manny remote displays added");
      return -1;
    }

    ALOGD("%s: fd=%d", __func__, id);

    rd->setDisplayId(id);
    mRemoteDisplays[id] = std::unique_ptr<Hwc1Display>(new Hwc1Display(id));
    mRemoteDisplays[id]->init(rd);
  }
  if (mCbProcs && HWC_DISPLAY_EXTERNAL == id) {
    ALOGD("Send hotplug in  external display message to SurfaceFlinger");
    mCbProcs->hotplug(mCbProcs, HWC_DISPLAY_EXTERNAL, true);
  }
  return 0;
}

int Hwc1Device::removeRemoteDisplay(RemoteDisplay* rd) {
  if (!rd)
    return -1;

  int id = static_cast<int>(rd->getDisplayId());

  {  // lock scope
    std::unique_lock<std::mutex> lk(mDisplayMutex);

    if (id >= kMaxDisplays) {
      ALOGE("Display to be removed does not exist!");
      return -1;
    }

    mRemoteDisplays[id] = nullptr;
  }
  if (mCbProcs && HWC_DISPLAY_EXTERNAL == id) {
    ALOGD("Send hotplug out external display message to SurfaceFlinger");
    mCbProcs->hotplug(mCbProcs, HWC_DISPLAY_EXTERNAL, false);
  }

  return 0;
}
int Hwc1Device::getMaxRemoteDisplayCount() {
  return kMaxDisplays;
}
int Hwc1Device::getRemoteDisplayCount() {
  int count = 0;
  std::unique_lock<std::mutex> lk(mDisplayMutex);

  for (int i = 0; i < kMaxDisplays; i++) {
    if (mRemoteDisplays[i]) {
      count++;
    }
  }
  return count;
}

int Hwc1Device::prepare(size_t numDisplays,
                        hwc_display_contents_1_t** displays) {
  if (!displays)
    return 0;

  ALOGV("Hwc1Device::prepare: numDisplays=%zu", numDisplays);
  for (size_t i = 0; i < numDisplays; i++) {
    ALOGV("Hwc1Device::prepare: displays[%zu]=%p", i, displays[i]);
  }

  for (size_t i = 0; i < numDisplays; i++) {
    if (displays[i]) {
      if (!mRemoteDisplays[i]) {
        mDummyDisplay->prepare(displays[i]);
      } else {
        mRemoteDisplays[i]->prepare(displays[i]);
      }
    }
  }
  return 0;
}

int Hwc1Device::set(size_t numDisplays, hwc_display_contents_1_t** displays) {
  if (!displays)
    return 0;

  ALOGV("Hwc1Device::set: numDisplays=%zu", numDisplays);

  for (size_t i = 0; i < numDisplays; i++) {
    if (displays[i]) {
      if (!mRemoteDisplays[i]) {
        mDummyDisplay->set(displays[i]);
      } else {
        mRemoteDisplays[i]->set(displays[i]);
      }
    }
  }
  return 0;
}

int Hwc1Device::eventControl(int disp, int event, int enabled) {
  ALOGV("Hwc1Device::eventControl: disp=%d event=%d enabled=%d", disp, event,
        enabled);

  if (!mRemoteDisplays[disp]) {
    mDummyDisplay->eventControl(event, enabled);
  } else {
    mRemoteDisplays[disp]->eventControl(event, enabled);
  }
  return 0;
}
int Hwc1Device::blank(int disp, int blank) {
  ALOGV("Hwc1Device::blank: disp=%d blank=%d", disp, blank);

  if (!mRemoteDisplays[disp]) {
    mDummyDisplay->blank(blank);
  } else {
    mRemoteDisplays[disp]->blank(blank);
  }
  return 0;
}
int Hwc1Device::query(int what, int* value) {
  ALOGV("Hwc1Device::query: what=%d", what);

  int ret = 0;
  if (value) {
    switch (what) {
      case HWC_BACKGROUND_LAYER_SUPPORTED:
        *value = 0;
        break;
      case HWC_VSYNC_PERIOD:
        *value = 1000 * 1000 * 1000 / 60;
        break;
      case HWC_DISPLAY_TYPES_SUPPORTED:
        *value = HWC_DISPLAY_PRIMARY_BIT;
        break;
      default:
        ret = -1;
        break;
    }
  }
  return ret;
}
void Hwc1Device::registerProcs(hwc_procs_t const* procs) {
  ALOGV("Hwc1Device::registerProcs");

  mCbProcs = procs;
}
void Hwc1Device::dump(char* buff, int len) {}

int Hwc1Device::getDisplayConfigs(int disp,
                                  uint32_t* configs,
                                  size_t* numConfigs) {
  ALOGV("Hwc1Device::getDisplayConfigs:disp=%d", disp);

  if (!mRemoteDisplays[disp]) {
    mDummyDisplay->getConfigs(configs, numConfigs);
  } else {
    mRemoteDisplays[disp]->getConfigs(configs, numConfigs);
  }
  return 0;
}
int Hwc1Device::getDisplayAttributes(int disp,
                                     uint32_t config,
                                     const uint32_t* attributes,
                                     int32_t* values) {
  ALOGV("Hwc1Device::getDisplayAttributes:disp=%d,config=%d", disp, config);

  if (!mRemoteDisplays[disp]) {
    mDummyDisplay->getAttributes(config, attributes, values);
  } else {
    mRemoteDisplays[disp]->getAttributes(config, attributes, values);
  }
  return 0;
}

static int hwc1_vhal_device_open(const struct hw_module_t* module,
                                 const char* name,
                                 struct hw_device_t** device) {
  int ret = 0;

  if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
    signal(SIGPIPE, SIG_IGN);

    auto ctx = new Hwc1Device();
    if (ctx == nullptr) {
      ALOGE("%s : %d : Failed to alloc hwc context, out of memory!", __func__,
            __LINE__);
      return -ENOMEM;
    }
    ctx->init(module);

    ctx->common.module = const_cast<hw_module_t*>(module);
    *device = &(ctx->common);
  } else {
    ret = -1;
  }

  return ret;
}

static struct hw_module_methods_t hwc1_vhal_module_methods = {
    .open = hwc1_vhal_device_open};

hwc_module_t HAL_MODULE_INFO_SYM = {.common = {
                                        .tag = HARDWARE_MODULE_TAG,
                                        .version_major = 1,
                                        .version_minor = 3,
                                        .id = HWC_HARDWARE_MODULE_ID,
                                        .name = "HWComposer vHAL",
                                        .author = "AOSP Team",
                                        .methods = &hwc1_vhal_module_methods,
                                        .dso = nullptr,
                                        .reserved = {0},
                                    }};
