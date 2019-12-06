#ifndef __HWC1_DEVICE_H__
#define __HWC1_DEVICE_H__

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <hardware/hwcomposer.h>
#include <system/graphics.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include "Hwc1Display.h"
#include "IRemoteDevice.h"
#include "RemoteDisplayMgr.h"
#include "display_protocol.h"

class SocketServer;

class Hwc1Device : public hwc_composer_device_1_t, public IRemoteDevice {
 public:
  Hwc1Device();
  virtual ~Hwc1Device();

  int init(const struct hw_module_t* module);

  // IRemoteDevice
  int addRemoteDisplay(RemoteDisplay * rd) override;
  int removeRemoteDisplay(RemoteDisplay* rd) override;
  int getMaxRemoteDisplayCount() override;
  int getRemoteDisplayCount() override;

 private:
  void workerThreadProc();
  void removePendingDisplays();

  // APIs implementation
  int prepare(size_t numDisplays, hwc_display_contents_1_t** displays);
  int set(size_t numDisplays, hwc_display_contents_1_t** displays);
  int eventControl(int disp, int event, int enabled);
  int blank(int disp, int blank);
  int query(int what, int* value);
  void registerProcs(hwc_procs_t const* procs);
  void dump(char* buff, int len);
  int getDisplayConfigs(int disp, uint32_t* configs, size_t* numConfigs);
  int getDisplayAttributes(int disp,
                           uint32_t config,
                           const uint32_t* attributes,
                           int32_t* values);

  template <typename T, typename HookType, HookType func, typename... Args>
  static T DeviceHook(hwc_composer_device_1_t* dev, Args... args) {
    auto ctx = (Hwc1Device*)dev;
    return static_cast<T>(((*ctx).*func)(std::forward<Args>(args)...));
  }
  static int hook_close(struct hw_device_t* dev) {
    if (dev) {
      delete dev;
    }
    return 0;
  }

 private:
  static const int kMaxDisplays = HWC_NUM_PHYSICAL_DISPLAY_TYPES;
  const hwc_procs_t* mCbProcs = nullptr;

  std::unique_ptr<Hwc1Display> mDummyDisplay;
  std::unique_ptr<Hwc1Display> mRemoteDisplays[kMaxDisplays];
  std::mutex mDisplayMutex;

  std::unique_ptr<RemoteDisplayMgr> mRemoteDisplayMgr;
};

#endif  // __HWC1_DEVICE_H__
