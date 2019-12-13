#ifndef __VNC_DISPLAY__H
#define __VNC_DISPLAY__H

#include <vector>

#include <hardware/gralloc1.h>
#include <hardware/hwcomposer2.h>
#include <system/graphics.h>

#include <inttypes.h>
#include <rfb/rfb.h>

#include "input_interface.h"

class VncDisplayObserver {
 public:
  virtual ~VncDisplayObserver() {}
  virtual int onHookClient(rfbClientPtr client) = 0;
  virtual int onUnhookClient(rfbClientPtr client) = 0;
};

class VncDisplay {
 public:
  struct ClientContext {
    int button_mask;
    int cursor_x;
    int cursor_y;
  };

 public:
  VncDisplay(int port, int w, int h);
  ~VncDisplay();

  int init();
  int postFb(buffer_handle_t fb);
  int fillColor(int color);

 public:
  static int addVncDisplayObserver(VncDisplayObserver* observer);
  static int removeVncDisplayObserver(VncDisplayObserver* observer);

 private:
  static int sClientCount;
  static std::vector<VncDisplayObserver*> sObservers;
  static enum rfbNewClientAction hookClient(rfbClientPtr client);
  static void unhookClient(rfbClientPtr client);
  static void hookMouse(int mask, int x, int y, rfbClientPtr client);
  static void hookKeyboard(rfbBool down, rfbKeySym key, rfbClientPtr client);

  void handleMouse(ClientContext* ctx, int mask, int x, int y);
  void handleKeyboard(ClientContext* ctx, rfbBool down, rfbKeySym key);

  int getGrallocDevice();
  int lockBuffer(buffer_handle_t b, uint8_t** data, uint32_t* s);
  int unlockBuffer(buffer_handle_t b);

 private:
  rfbScreenInfoPtr mScreen = nullptr;
  gralloc1_device_t* mGralloc = nullptr;
  GRALLOC1_PFN_LOCK pfnLock = nullptr;
  GRALLOC1_PFN_UNLOCK pfnUnlock = nullptr;
  GRALLOC1_PFN_GET_DIMENSIONS pfnGetDimensions = nullptr;
  GRALLOC1_PFN_GET_FORMAT pfnGetFormat = nullptr;
  GRALLOC1_PFN_GET_STRIDE pfnGetStride = nullptr;

  int mPort = 9000;
  uint32_t mWidth = 720;
  uint32_t mHeight = 1280;
  uint32_t mBytesPerPixel = 4;
  uint8_t* mFramebuffer = nullptr;

  IInputReceiver* mInputReceiver = nullptr;
};
#endif