//#define LOG_NDEBUG 0

#include <errno.h>
#include <inttypes.h>

#include <cutils/properties.h>
#include <cutils/log.h>

#include "Hwc2Display.h"
#include "LocalDisplay.h"
#include "RemoteDisplay.h"

#include "Hwc2Device.h"
#ifdef ENABLE_LAYER_DUMP
#include "BufferDumper.h"
#endif

#include "BufferMapper.h"
using namespace HWC2;

//#define DEBUG_LAYER
#ifdef DEBUG_LAYER
#define LAYER_TRACE(...) ALOGD(__VA_ARGS__)
#else
#define LAYER_TRACE(...)
#endif

Hwc2Display::Hwc2Display(hwc2_display_t id, Hwc2Device& device)
  : mDevice(device),
    mVsyncThread(*this) {
  ALOGD("%s", __func__);
  mDisplayID = id;

  mShowCurrentFrame = false;
  int w = 0, h = 0;
  char * p_w_env = NULL;
  char * p_h_env = NULL;
  char value[PROPERTY_VALUE_MAX];
  p_w_env = getenv("K8S_ENV_DISPLAY_RESOLUTION_X");
  p_h_env = getenv("K8S_ENV_DISPLAY_RESOLUTION_Y");
  if (p_w_env != NULL && p_h_env!= NULL) {
    w = atoi(p_w_env);
    h = atoi(p_h_env);
    ALOGD("Display %" PRIu64 " default size <%d %d> from environment setting", id,
          w, h);
  } else if (property_get("sys.display.size", value, nullptr)) {
    sscanf(value, "%dx%d", &w, &h);
    ALOGD("Display %" PRIu64 " default size <%d %d> from property settings", id,
          w, h);
  } else if (getResFromFb(w, h) == 0) {
    ALOGD("Display %" PRIu64 " default size <%d %d> from fb device", id, w, h);
  } else if (getResFromDebugFs(w, h) == 0) {
    ALOGD("Display %" PRIu64 " default size <%d %d> from debug fs", id, w, h);
  }

  char * p_fps_env = NULL;
  p_fps_env = getenv("K8S_ENV_DISPLAY_FPS");
  if(p_fps_env != NULL) {
    mFramerate = atoi(p_fps_env);
  }

  property_get("ro.hwc_vhal.bypass", value, "false");
  if (0 == strcmp("true", value)) {
    mFullscreenOpt = true;
  }

  if (w && h) {
    mWidth = w;
    mHeight = h;
  }

#ifdef ENABLE_HWC_VNC
  // if (property_get("sys.display.vnc", value, nullptr) && (atoi(value) > 0)) {
  int port = 9000 + (int)id;
  mVncDisplay = new VncDisplay(port, mWidth, mHeight);
  if (mVncDisplay && mVncDisplay->init() < 0) {
    delete mVncDisplay;
    mVncDisplay = nullptr;
  }
  // }
#endif
  mVsyncThread.run("", -19 /* ANDROID_PRIORITY_URGENT_AUDIO */);
  BufferMapper::getMapper().addCallback(onGrallocCallbackHook, this);

}

Hwc2Display::~Hwc2Display() {
  ALOGD("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
  if (mFbAcquireFenceFd >= 0) {
    close(mFbAcquireFenceFd);
    mFbAcquireFenceFd = -1;
  }
  if (mOutputBufferFenceFd >= 0) {
    close(mOutputBufferFenceFd);
    mOutputBufferFenceFd = -1;
  }
}

int Hwc2Display::attach(RemoteDisplay* rd) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  if (!rd)
    return -1;

  mRemoteDisplay = rd;
  mPort = (mDisplayID != kPrimayDisplay) ?  mRemoteDisplay->port() : 0;
  mWidth = mRemoteDisplay->width();
  mHeight = mRemoteDisplay->height();
  //mFramerate = mRemoteDisplay->fps();
  mXDpi = mRemoteDisplay->xdpi();
  mYDpi = mRemoteDisplay->ydpi();

  display_flags flags;
  flags.value = mRemoteDisplay->flags();
  mVersion = flags.version;
  mMode = flags.mode;

  ALOGD("Hwc2Display(%" PRIu64
        ")::%s w=%d,h=%d,fps=%d, xdpi=%d,ydpi=%d, protocal "
        "version=%d, mode=%d",
        mDisplayID, __func__, mWidth, mHeight, mFramerate, mXDpi, mYDpi,
        mVersion, mMode);
  return 0;
}

int Hwc2Display::detach(RemoteDisplay* rd) {
  if (rd == mRemoteDisplay) {
    mFbtBuffers.clear();
    mFullScreenBuffers.clear();
    mTransform = 0;
    mRemoteDisplay = nullptr;
  }
  return 0;
}

int Hwc2Display::onBufferDisplayed(const buffer_info_t& info) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  return 0;
}
int Hwc2Display::onPresented(std::vector<layer_buffer_info_t>& layerBuffer,
                             int& fence) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  display_flags flags;
  flags.value = mRemoteDisplay->flags();
  // mMode = flags.mode;

  return 0;
}

Error Hwc2Display::vsync(int64_t timestamp) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
  return Error::None;
}
Error Hwc2Display::refresh() {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
  return Error::None;
}
Error Hwc2Display::hotplug(bool in) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
  return Error::None;
}

Error Hwc2Display::acceptChanges() {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  for (auto& it : mLayers)
    it.second.acceptTypeChange();

  return Error::None;
}

Error Hwc2Display::createLayer(hwc2_layer_t* layer) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  LAYER_TRACE("Hwc2Display(%" PRIu64 ")::%s mode=%d layerId=%" PRIx64,
              mDisplayID, __func__, mMode, mLayerIndex);

  if (mRemoteDisplay && mMode > 0) {
    mRemoteDisplay->createLayer(mLayerIndex);
  }
  mLayers.emplace(mLayerIndex, mLayerIndex);
  mLayers.at(mLayerIndex).setRemoteDisplay(mRemoteDisplay);
  *layer = mLayerIndex;
  mLayerIndex++;
  return Error::None;
}

Error Hwc2Display::destroyLayer(hwc2_layer_t layer) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  LAYER_TRACE("Hwc2Display(%" PRIu64 ")::%s mode=%d layerId=%" PRIx64,
              mDisplayID, __func__, mMode, mLayerIndex);

  if (mRemoteDisplay && mMode > 0) {
    mRemoteDisplay->removeLayer(layer);
  }

  if (mRemoteDisplay && mMode == 0 && mFullScreenMode) {
    const std::set<buffer_handle_t> &buffers = mLayers.at(layer).buffers();
    for ( auto buffer : buffers) {
      mRemoteDisplay->removeBuffer(buffer);
      mFullScreenBuffers.erase((int64_t)buffer);
    }
  }

  mLayers.erase(layer);
  return Error::None;
}

Error Hwc2Display::getActiveConfig(hwc2_config_t* config) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *config = mConfig;
  return Error::None;
}

Error Hwc2Display::getChangedCompositionTypes(uint32_t* numElements,
                                              hwc2_layer_t* layers,
                                              int32_t* types) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  uint32_t numChanges = 0;
  for (auto& l : mLayers) {
    if (l.second.typeChanged()) {
      if (layers && types && numChanges < *numElements) {
        layers[numChanges] = l.first;
        types[numChanges] = static_cast<int32_t>(l.second.validatedType());
      }
      numChanges++;
    }
  }
  if (!layers && !types) {
    *numElements = numChanges;
  }
  return Error::None;
}

Error Hwc2Display::getClientTargetSupport(uint32_t width,
                                          uint32_t height,
                                          int32_t format,
                                          int32_t dataspace) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  if (width != (uint32_t)mWidth || height != (uint32_t)mHeight) {
    return Error::Unsupported;
  }

  if ((format == HAL_PIXEL_FORMAT_RGBA_8888 ||
       format == HAL_PIXEL_FORMAT_RGBX_8888 ||
       format == HAL_PIXEL_FORMAT_BGRA_8888) &&
      (dataspace == HAL_DATASPACE_UNKNOWN ||
       dataspace == HAL_DATASPACE_STANDARD_UNSPECIFIED)) {
    return HWC2::Error::None;
  }

  return Error::Unsupported;
}

Error Hwc2Display::getColorModes(uint32_t* numModes, int32_t* modes) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *numModes = 1;
  if (modes) {
    *modes = HAL_COLOR_MODE_NATIVE;
  }
  return Error::None;
}

Error Hwc2Display::getAttribute(hwc2_config_t config,
                                int32_t attribute,
                                int32_t* value) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s:config=%d,attribute=%d", mDisplayID,
        __func__, config, attribute);

  if (config != mConfig || !value) {
    return Error::BadConfig;
  }

  auto attr = static_cast<Attribute>(attribute);
  switch (attr) {
    case Attribute::Width:
      *value = mWidth;
      break;
    case Attribute::Height:
      *value = mHeight;
      break;
    case Attribute::VsyncPeriod:
      *value = 1000 * 1000 * 1000 / mFramerate;
      break;
    case Attribute::DpiX:
      *value = mXDpi * 1000;
      break;
    case Attribute::DpiY:
      *value = mYDpi * 1000;
      break;
    default:
      *value = -1;
      return Error::BadConfig;
  }
  return Error::None;
}

Error Hwc2Display::getConfigs(uint32_t* num_configs, hwc2_config_t* configs) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  for (auto buffer : mFullScreenBuffers) {
    mRemoteDisplay->removeBuffer(buffer.second);
  }
  mFullScreenBuffers.clear();

  for (auto buffer : mFbtBuffers) {
    mRemoteDisplay->removeBuffer(buffer);
  }
  mFbtBuffers.clear();

  *num_configs = 1;
  if (configs) {
    configs[0] = mConfig;
  }
  return Error::None;
}

Error Hwc2Display::getName(uint32_t* size, char* name) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *size = strlen(mName) + 1;
  if (name) {
    strncpy(name, mName, *size);
  }
  return Error::None;
}

Error Hwc2Display::getRequests(int32_t* displayRequests,
                               uint32_t* numElements,
                               hwc2_layer_t* layers,
                               int32_t* layerRequests) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *numElements = 0;
  return Error::None;
}

Error Hwc2Display::getType(int32_t* type) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *type = HWC2_DISPLAY_TYPE_PHYSICAL;
  return Error::None;
}

Error Hwc2Display::getDozeSupport(int32_t* support) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *support = 0;
  return Error::None;
}

Error Hwc2Display::getHdrCapabilities(uint32_t* numTypes,
                                      int32_t* /*types*/,
                                      float* /*max_luminance*/,
                                      float* /*max_average_luminance*/,
                                      float* /*min_luminance*/) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *numTypes = 0;
  return Error::None;
}

Error Hwc2Display::getReleaseFences(uint32_t* numElements,
                                    hwc2_layer_t* layers,
                                    int32_t* fences) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  if (!layers || !fences) {
    *numElements = mLayers.size();
    return Error::None;
  }

  uint32_t numLayers = 0;
  for (auto& l : mLayers) {
    if (numLayers < *numElements) {
      layers[numLayers] = l.first;
      fences[numLayers] = l.second.releaseFence();
      numLayers++;
    }
  }
  *numElements = numLayers;

  return Error::None;
}

Error Hwc2Display::present(int32_t* retireFence) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  buffer_handle_t target = nullptr;
  if (mRemoteDisplay) {
    if (mMode == 0 || mMode == 2) {
      if (!mFullScreenMode) {
        target = mFbTarget;
      } else {
        bool isNew = true;
	uint32_t zOrder = 0;
        for (auto& l : mLayers) {
          Hwc2Layer& layer = l.second;
	  if ( layer.zOrder() <= zOrder) {
            zOrder = layer.zOrder();
	    target = layer.buffer();
	  }
        }

        for (auto appTarget : mFullScreenBuffers) {
          if (appTarget.second == target) {
	    isNew = false;
          }
        }
        if (isNew && target != nullptr) {
          mFullScreenBuffers.insert(std::make_pair(((int64_t)target), target));
          mRemoteDisplay->createBuffer(target);
        }
      }
      if (target) {
        if (mShowCurrentFrame)
          mRemoteDisplay->displayBuffer(target);
#if 0
        updateRotation();
#ifdef VIDEO_STREAMING_OPT
        std::vector<layer_info_t> layerInfos;
        for (auto& layer : mLayers) {
          if (layer.second.changed()) {
            layerInfos.push_back(layer.second.info());
          }
        }
        if (layerInfos.size()) {
          mRemoteDisplay->updateLayers(layerInfos);
        }
        for (auto& layer : mLayers) {
          layer.second.setUnchanged();
        }
#endif
#endif
      }
    }
    if (mMode > 0) {
      bool forceUpdateAll = false;
      std::vector<layer_info_t> layerInfos;
      for (auto& layer : mLayers) {
        if (forceUpdateAll || layer.second.changed()) {
          layerInfos.push_back(layer.second.info());
        }
      }
      if (layerInfos.size()) {
        mRemoteDisplay->updateLayers(layerInfos);
      }

      std::vector<layer_buffer_info_t> layerBuffers;
      for (auto& layer : mLayers) {
        if (layer.second.bufferChanged()) {
          layerBuffers.push_back(layer.second.layerBuffer());
        }
      }
      if (layerBuffers.size()) {
        mRemoteDisplay->presentLayers(layerBuffers);
      }
      for (auto& layer : mLayers) {
        layer.second.setUnchanged();
      }
    }
  }

#ifdef ENABLE_HWC_VNC
  if (mVncDisplay && mFbTarget) {
    mVncDisplay->postFb(mFbTarget);
  }
#endif

#ifdef ENABLE_LAYER_DUMP
  dump();

  if (mFrameToDump == 0) {
    char value[PROPERTY_VALUE_MAX];
    if (property_get("hwc_vhal.frame_to_dump", value, nullptr)) {
      mFrameToDump = atoi(value);
      property_set("hwc_vhal.frame_to_dump", "0");
    }
  }
  if (mFrameToDump > 0) {
    auto& dumper = BufferDumper::getBufferDumper();

    ALOGD("Dump fb=%p for %d", mFbTarget, mFrameNum);
    dumper.dumpBuffer(mFbTarget, mFrameNum);
    mFrameToDump--;
  }
#endif

  mFrameNum++;
  char* p_env_dump = NULL;
  p_env_dump = getenv("ENV_DUMP_GFX_FPS");
  if ((p_env_dump != NULL) && strcmp(p_env_dump, "true") == 0 && (mFrameNum % mFramerate == 0)) {
    int64_t currentNS = systemTime(SYSTEM_TIME_MONOTONIC);
    int64_t lastNS = mLastPresentTime;
    float deltaMS = (currentNS - lastNS) / 1000000.0;
    float fps = 1000.0 * mFramerate / deltaMS;
    ALOGI("total mFrameNum = %d, fps = %.2f\n", mFrameNum, fps);
    mLastPresentTime = currentNS;
  }

  mShowCurrentFrame = false;
  *retireFence = -1;
  return Error::None;
}

Error Hwc2Display::setActiveConfig(hwc2_config_t config) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  mConfig = config;
  return Error::None;
}

Error Hwc2Display::setClientTarget(buffer_handle_t target,
                                   int32_t acquireFence,
                                   int32_t dataspace,
                                   hwc_region_t damage) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
  mFbTarget = target;
  if (mFbAcquireFenceFd >= 0) {
    close(mFbAcquireFenceFd);
  }
  mFbAcquireFenceFd = acquireFence;

  if (mRemoteDisplay) {
    bool isNew = true;

    for (auto fbt : mFbtBuffers) {
      if (fbt == mFbTarget) {
        isNew = false;
      }
    }
    if (isNew) {
      mFbtBuffers.push_back(mFbTarget);
      mRemoteDisplay->createBuffer(mFbTarget);
    }
  }
  return Error::None;
}

Error Hwc2Display::setColorMode(int32_t mode) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  mColorMode = mode;
  return Error::None;
}

Error Hwc2Display::setColorTransform(const float* matrix, int32_t hint) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  return Error::None;
}

Error Hwc2Display::setOutputBuffer(buffer_handle_t buffer,
                                   int32_t releaseFence) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  mOutputBuffer = buffer;
  if (mOutputBufferFenceFd >= 0) {
    close(mOutputBufferFenceFd);
  }
  mOutputBufferFenceFd = releaseFence;

  return Error::None;
}

Error Hwc2Display::setPowerMode(int32_t mode) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  return Error::None;
}

static bool isValid(Vsync enable) {
  switch (enable) {
    case Vsync::Enable: // Fall-through
    case Vsync::Disable: return true;
    case Vsync::Invalid: return false;
  }
  return false;
}

Error Hwc2Display::setVsyncEnabled(int32_t enabled) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s %d", mDisplayID, __func__, enabled);
  Vsync enable = static_cast<Vsync>(enabled);
  if (!isValid(enable)) {
    return Error::BadParameter;
  }
  if (enable == mVsyncEnabled) {
    return Error::None;
  }

  mVsyncEnabled = enable;
  return Error::None;
}

bool Hwc2Display::VsyncThread::threadLoop() {
  struct timespec rt;
  if (clock_gettime(CLOCK_MONOTONIC, &rt) == -1) {
    ALOGE("%s: error in vsync thread clock_gettime: %s",
          __FUNCTION__, strerror(errno));
    return true;
  }
  const int logInterval = 60;
  int64_t lastLogged = rt.tv_sec;
  int sent = 0;
  int lastSent = 0;

  struct timespec wait_time;
  wait_time.tv_sec = 0;
  wait_time.tv_nsec = 1000 * 1000 * 1000 / mDisplay.mFramerate;
  const int64_t kOneRefreshNs = 1000 * 1000 * 1000 / mDisplay.mFramerate;
  const int64_t kOneSecondNs = 1000ULL * 1000ULL * 1000ULL;
  int64_t lastTimeNs = -1;
  int64_t phasedWaitNs = 0;
  int64_t currentNs = 0;

  while (true) {
    clock_gettime(CLOCK_MONOTONIC, &rt);
    currentNs = rt.tv_nsec + rt.tv_sec * kOneSecondNs;

    if (lastTimeNs < 0) {
      phasedWaitNs = currentNs + kOneRefreshNs;
    } else {
      phasedWaitNs = kOneRefreshNs *
        (( currentNs - lastTimeNs) / kOneRefreshNs + 1) + lastTimeNs;
    }

    wait_time.tv_sec = phasedWaitNs / kOneSecondNs;
    wait_time.tv_nsec = phasedWaitNs - wait_time.tv_sec * kOneSecondNs;

    int ret;
    do {
      ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wait_time, NULL);
    } while (ret == -1 && errno == EINTR);

    lastTimeNs = phasedWaitNs;

    if (mDisplay.mVsyncEnabled != Vsync::Enable) {
      continue;
    }

    auto callbacks = mDisplay.mDevice.getCallbacks();
    const auto& callbackInfo = callbacks[(int32_t)Callback::Vsync];
    auto vsync = reinterpret_cast<HWC2_PFN_VSYNC>(callbackInfo.pointer);
    if (vsync) {
      vsync(callbackInfo.data, mDisplay.mDisplayID, lastTimeNs);
    }

    if (rt.tv_sec - lastLogged >= logInterval) {
      ALOGV("sent %d syncs in %ld", sent - lastSent, (long)(rt.tv_sec - lastLogged));
      lastLogged = rt.tv_sec;
      lastSent = sent;
    }
    ++sent;
  }
  return false;
}

bool Hwc2Display::checkFullScreenMode() {
  bool hasAppUi = true;
  bool oneLandscapeLayer = false;
  uint32_t surfaceViewCount = 0;

  for (auto& l : mLayers) {
    Hwc2Layer& layer = l.second;
    if(NULL != strstr(layer.name(),"SurfaceView"))
      surfaceViewCount++;

    //if only have one landscape layer, consider it fullscreen mode
    if (layer.buffer() != nullptr && 1 == mLayers.size() && layer.info().transform == 0)
      oneLandscapeLayer = true;
  }

  //if all layers are SurfaceView layer and layer count <=2, consider it fullscreen mode(for tencent special case)
  if (mLayers.size() == surfaceViewCount && surfaceViewCount <= 2)
    hasAppUi = false;

  int32_t format = -1;
  if (1 == mLayers.size()) {
    for (auto& l : mLayers) {
        Hwc2Layer& layer = l.second;
        BufferMapper::getMapper().getBufferFormat(layer.buffer(), format);
    }
  }
  //ALOGI("buf format(%d)\n", format);
  return (oneLandscapeLayer || !hasAppUi) && (format == HAL_PIXEL_FORMAT_RGBA_8888 || format == HAL_PIXEL_FORMAT_RGBX_8888);
}

void Hwc2Display::exitFullScreenMode() {
  ALOGD("Exit fullscreen mode");

  if (mRemoteDisplay) {
    for (auto buffer : mFullScreenBuffers) {
      mRemoteDisplay->removeBuffer(buffer.second);
    }
    mFullScreenBuffers.clear();
  }
}

void Hwc2Display::onGrallocCallback(int event, const buffer_handle_t buffer) {
  switch (event) {
    case GRALLOC_EVENT_ALLOCATE:
      //ALOGD("%s: buffer %p allocated", __func__, buffer);
      break;
    case GRALLOC_EVENT_RETAIN:
      //ALOGD("%s: buffer %p retained", __func__, buffer);
      break;
    case GRALLOC_EVENT_RELEASE:
      //ALOGD("%s: buffer %p released", __func__, buffer);
      // todo: move handle to main thread to avoid race condition
      for (auto it : mFullScreenBuffers) {
        if (it.second == buffer) {
          ALOGD("%s: fullscreen buffer %p released", __func__, buffer);
          exitFullScreenMode();
          break;
        }
      }
      break;
    default:
      ALOGD("%s: unknown gralloc event with buffer %p", __func__, buffer);
      break;
  }
}

Error Hwc2Display::validate(uint32_t* numTypes, uint32_t* numRequests) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  // check if there's any layer occupying the 1/2 display, if not, wo shouldn't show this frame
  for (auto& l : mLayers) {
    Hwc2Layer& layer = l.second;
    if (((layer.info().dstFrame.right - layer.info().dstFrame.left) > mWidth/2) && ((layer.info().dstFrame.bottom - layer.info().dstFrame.top) > mHeight/2)) {
      mShowCurrentFrame = true;
      break;
    }
  }
  if (false == mShowCurrentFrame) {
    ALOGW("there's no any layer occupying the 1/2 display !!!\n");
  }

  *numTypes = 0;
  *numRequests = 0;

if (mFullscreenOpt) {
  bool tmp = checkFullScreenMode();
  if(tmp != mFullScreenMode) {
    mFullScreenMode = tmp;
    if (mFullScreenMode == false)
      exitFullScreenMode();
    else {
      ALOGD("Enter fullscreen mode");
    }
  }

  // notify streamer to clear old buffers when layer resizes/reallocate new buffers
  if ((tmp == true) && (1 == mLayers.size())) {
    for (auto& layer : mLayers) {
      if (layer.second.changed() == true) {
        ALOGI("Layer has geometry change, so clear the fullscreen buffer.\n");
        exitFullScreenMode();
      }
    } // end of for
    for (auto& layer : mLayers) {
      layer.second.setUnchanged();
    }
  }
}

  for (auto& l : mLayers) {
    Hwc2Layer& layer = l.second;
    switch (layer.type()) {
      case Composition::Device:
	if (!mFullScreenMode) {
          layer.setValidatedType(Composition::Client);
          ++*numTypes;
	} else {
          layer.setValidatedType(Composition::Device);
	}
        break;
      case Composition::SolidColor:
      case Composition::Cursor:
      case Composition::Sideband:
        layer.setValidatedType(Composition::Client);
        ++*numTypes;
        break;
      default:
        layer.setValidatedType(layer.type());
        break;
    }
  }

  // dump();
  return *numTypes > 0 ? Error::HasChanges : Error::None;
}

int Hwc2Display::updateRotation() {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  if (!mRemoteDisplay)
    return 0;

  uint32_t tr = 0;
  for (auto& layer : mLayers) {
    tr = layer.second.info().transform;
    if (tr == mTransform)
      break;
  }
  if (tr != mTransform) {
    int rot = (tr == 0) ? 0 : (tr == 4) ? 1 : (tr == 3) ? 2 : 3;

    ALOGD("Hwc2Display(%" PRIu64 ")::%s, setRotation to %d, tr=%d", mDisplayID,
          __func__, rot, tr);

    mRemoteDisplay->setRotation(rot);
    mTransform = tr;

#ifdef ENABLE_LAYER_DUMP
    if (mDebugRotationTransition) {
      mFrameToDump = 10;
    }
#endif
  }

  return 0;
}
Error Hwc2Display::getCapabilities(uint32_t* outNumCapabilities, uint32_t* /*outCapabilities*/) {
    ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
    *outNumCapabilities = 0;

    return Error::None;
}

Error Hwc2Display::setBrightness(float brightness) {
    ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

    return Error::None;
}

Error Hwc2Display::getIdentificationData(uint8_t* outPort, uint32_t* outDataSize, uint8_t* /*outData*/) {
    ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
    *outPort = mPort;
    *outDataSize = 0;
    return Error::None;
}

void Hwc2Display::dump() {
  ALOGD("-----Dump of Display(%" PRIu64 "): frame=%d remote=%p, mode=%d-----",
        mDisplayID, mFrameNum, mRemoteDisplay, mMode);
  for (auto& l : mLayers) {
    l.second.dump();
  }
}
