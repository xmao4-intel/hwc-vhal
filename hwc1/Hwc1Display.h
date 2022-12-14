#ifndef __HWC1_DISPLAY_H__
#define __HWC1_DISPLAY_H__

#include <vector>

#include <hardware/hwcomposer.h>

#include "IRemoteDevice.h"
#include "RemoteDisplay.h"

class Hwc1Display {
 public:
  Hwc1Display(int id);
  virtual ~Hwc1Display();

  int init(RemoteDisplay * rd);
  int getConfigs(uint32_t* configs, size_t* numConfigs);
  int getAttributes(uint32_t config,
                    const uint32_t* attributes,
                    int32_t* values);
  int blank(int blank) { return 0; }
  int eventControl(int event, int enabled) { return 0; }

  virtual int prepare(hwc_display_contents_1_t* disp);
  virtual int set(hwc_display_contents_1_t* disp);

 protected:
  void dumpLayer(size_t idx, hwc_layer_1_t const* l);
  int setSingleLayerBuffer(buffer_handle_t b);
  int exitSingleLayer();

 protected:
  enum COMPOSITION_MODE {
    COMPOSITION_OVERLAY = 0,
    COMPOSITION_FBT,
    COMPOSITION_SMART_FBT
  };
  int mCompositionMode = COMPOSITION_MODE::COMPOSITION_FBT;

  const int kPrimayDisplay = 0;

  int mDisplayID = 0;
  int mWidth = 720;
  int mHeight = 1280;
  int mFramerate = 60;
  int mXDpi = 240;
  int mYDpi = 240;

  // remote display
  RemoteDisplay* mRemoteDisplay = nullptr;
  std::vector<buffer_handle_t> mFbtBuffers;

  bool mEnableSingleLayerOpt = false;
  bool mSingleLayer = false;
  std::vector<buffer_handle_t> mAppBuffers;
};
#endif  //__HWC1_DISPLAY_H__