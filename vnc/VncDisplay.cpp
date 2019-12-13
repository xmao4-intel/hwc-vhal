//#define LOG_NDEBUG 0

#include <utils/Log.h>

#include "DirectInput.h"
#include "Keymap.h"
#include "VncDisplay.h"

int VncDisplay::sClientCount = 0;
std::vector<VncDisplayObserver*> VncDisplay::sObservers;

VncDisplay::VncDisplay(int port, int w, int h)
    : mPort(port), mWidth(w), mHeight(h) {
  ALOGV("%s", __func__);
}

VncDisplay::~VncDisplay() {
  ALOGV("%s", __func__);
  if (mGralloc) {
    gralloc1_close(mGralloc);
  }
}

int VncDisplay::addVncDisplayObserver(VncDisplayObserver* observer) {
  if (observer) {
    sObservers.push_back(observer);
  }
  return 0;
}

int VncDisplay::removeVncDisplayObserver(VncDisplayObserver* observer) {
  if (observer) {
    for (auto it = sObservers.begin(); it != sObservers.end();) {
      if (*it == observer) {
        it = sObservers.erase(it);
      } else {
        it++;
      }
    }
  }
  return 0;
}

enum rfbNewClientAction VncDisplay::hookClient(rfbClientPtr client) {
  ALOGV("%s", __func__);

  client->clientData = (void*)calloc(sizeof(VncDisplay::ClientContext), 1);
  client->clientGoneHook = VncDisplay::unhookClient;

  for (auto observer : sObservers) {
    observer->onHookClient(client);
  }
  sClientCount++;
  return RFB_CLIENT_ACCEPT;
}

void VncDisplay::unhookClient(rfbClientPtr client) {
  ALOGV("%s", __func__);

  free(client->clientData);
  client->clientData = nullptr;

  for (auto observer : sObservers) {
    observer->onUnhookClient(client);
  }
  sClientCount--;
}

void VncDisplay::hookMouse(int mask, int x, int y, rfbClientPtr client) {
  ALOGV("%s", __func__);

  auto ctx = (ClientContext*)client->clientData;
  auto sctx = (VncDisplay*)client->screen->screenData;
  sctx->handleMouse(ctx, mask, x, y);
}

void VncDisplay::hookKeyboard(rfbBool down,
                              rfbKeySym key,
                              rfbClientPtr client) {
  ALOGV("%s", __func__);

  auto ctx = (ClientContext*)client->clientData;
  auto sctx = (VncDisplay*)client->screen->screenData;
  sctx->handleKeyboard(ctx, down, key);
}

void VncDisplay::handleMouse(ClientContext* ctx, int mask, int x, int y) {
  ALOGV("%s", __func__);

  if (mInputReceiver && x > 0 && x < (int)mWidth && y > 0 && y < (int)mHeight) {
    char msg[32];

    // remap x, y to touch space
    x = x * 720 / mWidth;
    y = y * 1280 / mHeight;

    if (mask && ctx->button_mask == 0) {  // down
      snprintf(msg, 32, "d 0 %d %d 50 \nc\n", x, y);
      mInputReceiver->onInputMessage(msg);
    } else if (mask == 0 && ctx->button_mask) {  // up
      snprintf(msg, 32, "u 0\nc\n");
      mInputReceiver->onInputMessage(msg);
    } else if (mask && ctx->button_mask) {  // move
      snprintf(msg, 32, "m 0 %d %d 50 \nc\n", x, y);
      mInputReceiver->onInputMessage(msg);
    } else {  // hover
      // do nothing
      snprintf(msg, 32, "hover %d %d\n", x, y);
    }
    // ALOGD("%s:%s", __func__, msg);
  }
  ctx->button_mask = mask;
}

void VncDisplay::handleKeyboard(ClientContext* ctx,
                                rfbBool down,
                                rfbKeySym key) {
  ALOGV("%s", __func__);

  if (mInputReceiver && down) {
    mInputReceiver->onKeyCode(keySymToScanCode(key), keySymToMask(key));
    //ALOGD("keyboard event:down=%d key=%d\n", down, key);
  }
}

int VncDisplay::init() {
  ALOGV("%s", __func__);

  if (getGrallocDevice() < 0) {
    return -1;
  }

  mInputReceiver = new DirectInputReceiver(0);

  mFramebuffer = (uint8_t*)malloc(mWidth * mHeight * mBytesPerPixel);
  if (!mFramebuffer) {
    ALOGE("Failed to allocate framebuffer\n");
    return false;
  }

  int argc = 1;
  char* argv = nullptr;

  mScreen = rfbGetScreen(&argc, &argv, mWidth, mHeight, 8, 3, mBytesPerPixel);
  if (!mScreen) {
    ALOGE("Failed to get screen info\n");
    return false;
  }

  mScreen->desktopName = "HWC Display";
  mScreen->port = mPort;
  mScreen->frameBuffer = (char*)mFramebuffer;
  mScreen->screenData = this;
  mScreen->alwaysShared = TRUE;
  mScreen->ptrAddEvent = VncDisplay::hookMouse;
  mScreen->kbdAddEvent = VncDisplay::hookKeyboard;
  mScreen->newClientHook = VncDisplay::hookClient;
  rfbInitServer(mScreen);

  ALOGD("Start VNC server at port %d, run event loop in seperate thread",
        mPort);
  rfbRunEventLoop(mScreen, -1, TRUE);

  return 0;
}

int VncDisplay::getGrallocDevice() {
  ALOGV("%s", __func__);

  const hw_module_t* mod = nullptr;

  if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &mod) != 0) {
    ALOGE("Failed to load gralloc module");
    return -1;
  }

  if (gralloc1_open(mod, &mGralloc) != 0) {
    ALOGE("Failed to open gralloc1 device");
    return -1;
  }

  if (mGralloc) {
    pfnLock = (GRALLOC1_PFN_LOCK)(
        mGralloc->getFunction(mGralloc, GRALLOC1_FUNCTION_LOCK));
    pfnUnlock = (GRALLOC1_PFN_UNLOCK)(
        mGralloc->getFunction(mGralloc, GRALLOC1_FUNCTION_UNLOCK));
    pfnGetDimensions = (GRALLOC1_PFN_GET_DIMENSIONS)(
        mGralloc->getFunction(mGralloc, GRALLOC1_FUNCTION_UNLOCK));
    pfnGetFormat = (GRALLOC1_PFN_GET_FORMAT)(
        mGralloc->getFunction(mGralloc, GRALLOC1_FUNCTION_GET_FORMAT));
    pfnGetStride = (GRALLOC1_PFN_GET_STRIDE)(
        mGralloc->getFunction(mGralloc, GRALLOC1_FUNCTION_GET_STRIDE));
  }
  return 0;
}

int VncDisplay::lockBuffer(buffer_handle_t b, uint8_t** data, uint32_t* s) {
  ALOGV("%s", __func__);

  if (!b || !pfnLock || !pfnGetStride) {
    return -1;
  }

  gralloc1_rect_t rect = {0, 0, (int32_t)mWidth, (int32_t)mHeight};
  int fenceFd = -1;
  if (pfnLock(mGralloc, b, 0x0, 0x3, &rect, (void**)data, fenceFd) != 0) {
    ALOGE("Failed to lock buffer %p", b);
    return -1;
  }
  if (pfnGetStride(mGralloc, b, s) != 0) {
    ALOGE("Failed to get buffer %p stride", b);
    return -1;
  }
  // ALOGD("lock buffer return addr %p stride=%d", *data, *s);
  return 0;
}

int VncDisplay::unlockBuffer(buffer_handle_t b) {
  ALOGV("%s", __func__);

  if (!b || !pfnUnlock) {
    return -1;
  }

  int releaseFenceFd = -1;

  if (pfnUnlock(mGralloc, b, &releaseFenceFd) != 0) {
    ALOGE("Failed to unlock buffer %p", b);
    return -1;
  }
  if (releaseFenceFd >= 0) {
    close(releaseFenceFd);
  }
  return 0;
}

int VncDisplay::postFb(buffer_handle_t fb) {
  ALOGV("%s", __func__);

  uint8_t* rgb = nullptr;
  uint32_t stride = 0;

  if (sClientCount > 0) {
    lockBuffer(fb, &rgb, &stride);
    if (rgb) {
      for (uint32_t i = 0; i < mHeight; i++) {
        memcpy(mFramebuffer + i * mWidth * 4, rgb + i * stride * 4, mWidth * 4);
      }
    } else {
      ALOGE("Failed to lock front buffer\n");
    }
    unlockBuffer(fb);

    rfbMarkRectAsModified(mScreen, 0, 0, mWidth, mHeight);
  }
  return 0;
}

int VncDisplay::fillColor(int color) {
  ALOGV("%s", __func__);

  if (sClientCount > 0) {
    int* fb32 = (int*)mFramebuffer;
    for (uint32_t i = 0; i < mHeight; i++) {
      for (uint32_t j = 0; j < mWidth; j++) {
        fb32[i * mWidth + j] = color;
      }
    }
    rfbMarkRectAsModified(mScreen, 0, 0, mWidth, mHeight);
  }
  return 0;
}
