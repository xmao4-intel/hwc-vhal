//#define LOG_NDEBUG 0
#include <cutils/log.h>
#include <cutils/properties.h>

#include <ui/GraphicBufferMapper.h>

//#include "BufferMapper.h"
#include "DirectInput.h"
#include "Keymap.h"
#include "VncDisplay.h"

class FastBufferRead : public RenderTask {
public:
    FastBufferRead(buffer_handle_t b, void* data) {
      mHandle = b;
      mOut = data;
    }
    int run() override {
      BufferTexture bufTex(mHandle, true);
      uint32_t w = bufTex.getWidth();
      uint32_t h = bufTex.getHeight();
      int32_t f = bufTex.getFormat();
      if (f != HAL_PIXEL_FORMAT_RGBA_8888) {
          ALOGD("FastBufferRead: Skip to dump format %d", f);
          return 0;
      }
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, mOut);
      //CHECK();
      return 0;
    }
private:
  buffer_handle_t mHandle;
  void* mOut = nullptr;
};

int VncDisplay::sClientCount = 0;
std::vector<VncDisplayObserver*> VncDisplay::sObservers;

VncDisplay::VncDisplay(int port, int w, int h)
    : mPort(port), mWidth(w), mHeight(h) {
  ALOGV("%s", __func__);
  char value[PROPERTY_VALUE_MAX];
  if (property_get("vendor.egl.set", value, nullptr) > 0) {
    if (strncmp(value, "mesa", 4) == 0)
      mUseGLReadback = true;
  }
}

VncDisplay::~VncDisplay() {
  ALOGV("%s", __func__);
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
    x = x * 32767 / mWidth;
    y = y * 32767 / mHeight;

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
    //ALOGD("%s:%s", __func__, msg);
  }
  ctx->button_mask = mask;
}

void VncDisplay::handleKeyboard(ClientContext* ctx,
                                rfbBool down,
                                rfbKeySym key) {
  ALOGV("%s", __func__);

  (void)ctx;

  if (mInputReceiver && down) {
    mInputReceiver->onKeyCode(keySymToScanCode(key), keySymToMask(key));
    // ALOGD("keyboard event:down=%d key=%d\n", down, key);
  }
}

int VncDisplay::init() {
  ALOGV("%s", __func__);

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

int VncDisplay::postFb(buffer_handle_t fb) {
  ALOGV("%s", __func__);

  uint8_t* rgb = nullptr;
  int32_t stride = 0, bpp = 0;
  
  if (sClientCount > 0) {
    if (mUseGLReadback) {
      if (mRenderThread == nullptr) {
        mRenderThread = std::make_unique<RenderThread>();
        mRenderThread->init();
      }
      auto fbr = new FastBufferRead(fb, mFramebuffer);
      mRenderThread->runTask(fbr);
    } else {
      auto& mapper = android::GraphicBufferMapper::get();
      android::Rect bounds(0, 0, mWidth, mHeight);
      mapper.lock(fb, GRALLOC_USAGE_SW_READ_OFTEN, bounds, (void**)&rgb, &bpp, &stride);
      ALOGD("locked addr=%p bpp=%d stride=%d", rgb, bpp, stride); 
      if (rgb) {
        for (uint32_t i = 0; i < mHeight; i++) {
          memcpy(mFramebuffer + i * mWidth * 4, rgb + i * stride, mWidth * 4);
        }
      } else {
        ALOGE("Failed to lock front buffer\n");
      }
      mapper.unlock(fb);
    }
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
