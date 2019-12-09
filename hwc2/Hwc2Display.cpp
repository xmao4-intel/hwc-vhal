//#define LOG_NDEBUG 0

#include <errno.h>
#include <inttypes.h>

#include <cutils/properties.h>
#include <utils/Log.h>

#include "Hwc2Display.h"
#include "LocalDisplay.h"
#include "RemoteDisplay.h"

using namespace HWC2;

//#define DEBUG_LAYER
#ifdef DEBUG_LAYER
#define LAYER_TRACE(...) ALOGD(__VA_ARGS__)
#else
#define LAYER_TRACE(...)
#endif

Hwc2Display::Hwc2Display(hwc2_display_t id) {
  ALOGD("%s", __func__);
  mDisplayID = id;

  int w = 0, h = 0;

  char value[PROPERTY_VALUE_MAX];
  if (property_get("sys.display.size", value, nullptr)) {
    sscanf(value, "%dx%d", &w, &h);
    ALOGD("Display %" PRIu64 " default size <%d %d> from property settings", id,
          w, h);
  } else if (getResFromFb(w, h) == 0) {
    ALOGD("Display %" PRIu64 " default size <%d %d> from fb device", id, w, h);
  } else if (getResFromDebugFs(w, h) == 0) {
    ALOGD("Display %" PRIu64 " default size <%d %d> from debug fs", id, w, h);
  }

  if (w & h) {
    mWidth = w;
    mHeight = h;
  }
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
  mWidth = mRemoteDisplay->width();
  mHeight = mRemoteDisplay->height();
  mFramerate = mRemoteDisplay->fps();
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

  // dump();
  if (mRemoteDisplay) {
    updateRotation();

    if (mMode == 0 || mMode == 2) {
      if (mFbTarget) {
        mRemoteDisplay->displayBuffer(mFbTarget);
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

Error Hwc2Display::setVsyncEnabled(int32_t enabled) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);
  return Error::None;
}

Error Hwc2Display::validate(uint32_t* numTypes, uint32_t* numRequests) {
  ALOGV("Hwc2Display(%" PRIu64 ")::%s", mDisplayID, __func__);

  *numTypes = 0;
  *numRequests = 0;
  for (auto& l : mLayers) {
    Hwc2Layer& layer = l.second;
    switch (layer.type()) {
      case Composition::Device:
        layer.setValidatedType(Composition::Client);
        ++*numTypes;
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
    if (tr)
      break;
  }
  if (tr != mTransform) {
    int rot = (tr == 0) ? 0 : (tr == 4) ? 1 : (tr == 3) ? 2 : 3;

  ALOGD("Hwc2Display(%" PRIu64 ")::%s, setRotation to %d, tr=%d", mDisplayID, __func__, rot, tr);

    mRemoteDisplay->setRotation(rot);
    mTransform = tr;
  }

  return 0;
}

void Hwc2Display::dump() {
  ALOGD("-----Dump of Display(%" PRIu64 "):remote=%p, mode=%d-----", mDisplayID,
        mRemoteDisplay, mMode);
  for (auto& l : mLayers) {
    l.second.dump();
  }
}