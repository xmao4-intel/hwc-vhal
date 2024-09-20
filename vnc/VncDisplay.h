#ifndef __VNC_DISPLAY__H
#define __VNC_DISPLAY__H

#include <vector>

#include <hardware/gralloc1.h>
#include <hardware/hwcomposer2.h>
#include <system/graphics.h>

#include <inttypes.h>
#include <rfb/rfb.h>

#include "input_interface.h"

#include "BufferTexture.h"
#include "RenderThread.h"

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

 private:
  rfbScreenInfoPtr mScreen = nullptr;

  int mPort = 9000;
  uint32_t mWidth = 1280;
  uint32_t mHeight = 720;
  uint32_t mBytesPerPixel = 4;
  uint8_t* mFramebuffer = nullptr;

  IInputReceiver* mInputReceiver = nullptr;

  bool mUseGLReadback = false;
  std::unique_ptr<RenderThread> mRenderThread;
};
#endif