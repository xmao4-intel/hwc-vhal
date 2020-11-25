#ifndef __HWC2_DEVICE_H__
#define __HWC2_DEVICE_H__

#include <map>
#include <unordered_map>
#include <vector>

#include <hardware/hwcomposer2.h>

#include "Hwc2Display.h"
#include "IRemoteDevice.h"
#include "RemoteDisplayMgr.h"

#ifdef ENABLE_HWC_VNC
#include "VncDisplay.h"
#endif

#ifdef ENABLE_HWC_VNC
class Hwc2Device : public hwc2_device_t,
                   public IRemoteDevice,
                   public VncDisplayObserver {
#else
class Hwc2Device : public hwc2_device_t, public IRemoteDevice {
#endif
 public:
  Hwc2Device();
  virtual ~Hwc2Device() {}

  HWC2::Error init();

  Hwc2Display* getDisplay(hwc2_display_t disp) {
    auto display = mDisplays.find(disp);
    if (display == mDisplays.end()) {
      return nullptr;
    }
    return &display->second;
  }

  HWC2::Error onHotplug(hwc2_display_t disp, uint32_t connected);
  HWC2::Error onRefresh(hwc2_display_t disp);

  // IRemoteDevice
  int addRemoteDisplay(RemoteDisplay* rd) override;
  int removeRemoteDisplay(RemoteDisplay* rd) override;
  int getMaxRemoteDisplayCount() override;
  int getRemoteDisplayCount() override;

 public:
  static Hwc2Device* toHwc2Device(hwc2_device_t* dev) {
    return static_cast<Hwc2Device*>(dev);
  }

  template <typename PFN, typename T>
  static hwc2_function_pointer_t asFP(T function) {
    static_assert(std::is_same<PFN, T>::value, "Incompatible fn pointer");
    return reinterpret_cast<hwc2_function_pointer_t>(function);
  }

  template <typename T, typename HookType, HookType func, typename... Args>
  static T DeviceHook(hwc2_device_t* dev, Args... args) {
    Hwc2Device* hwc = toHwc2Device(dev);
    return static_cast<T>(((*hwc).*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t DisplayHook(hwc2_device_t* dev,
                             hwc2_display_t disp,
                             Args... args) {
    Hwc2Device* hwc = toHwc2Device(dev);
    Hwc2Display* display = hwc->getDisplay(disp);
    if (!display) {
      return static_cast<int32_t>(HWC2::Error::BadDisplay);
    }
    return static_cast<int32_t>((display->*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t LayerHook(hwc2_device_t* dev,
                           hwc2_display_t disp,
                           hwc2_layer_t l,
                           Args... args) {
    Hwc2Device* hwc = toHwc2Device(dev);
    Hwc2Display* display = hwc->getDisplay(disp);
    if (!display) {
      return static_cast<int32_t>(HWC2::Error::BadDisplay);
    }
    Hwc2Layer& layer = display->getLayer(l);
    return static_cast<int32_t>((layer.*func)(std::forward<Args>(args)...));
  }

  // global hook
  static int openHook(const struct hw_module_t* module,
                      const char* name,
                      struct hw_device_t** dev);

  // hwc2_device_t hooks
  static int closeHook(hw_device_t* dev);
  static void getCapabilitiesHook(hwc2_device_t* dev,
                                  uint32_t* out_count,
                                  int32_t* out_capabilities);
  static hwc2_function_pointer_t getFunctionHook(struct hwc2_device* device,
                                                 int32_t descriptor);

  // Device functions
  HWC2::Error createVirtualDisplay(uint32_t width,
                                   uint32_t height,
                                   int32_t* format,
                                   hwc2_display_t* display);
  HWC2::Error destroyVirtualDisplay(hwc2_display_t display);
  void dump(uint32_t* size, char* buffer);
  uint32_t getMaxVirtualDisplayCount();
  HWC2::Error registerCallback(int32_t descriptor,
                               hwc2_callback_data_t data,
                               hwc2_function_pointer_t function);

#ifdef ENABLE_HWC_VNC
  int onHookClient(rfbClientPtr client) override {
    onRefresh(kPrimayDisplay);
    return 0;
  }
  int onUnhookClient(rfbClientPtr client) override { return 0; }
#endif

 private:
  static std::atomic<hwc2_display_t> sNextId;
  bool registed = false;
  bool server_mode = false;
  const int kMaxDisplayCount = 100;
  const hwc2_display_t kPrimayDisplay = 0;

  struct CallbackInfo {
    hwc2_callback_data_t data;
    hwc2_function_pointer_t pointer;
  };
  std::unordered_map<int32_t, CallbackInfo> mCallbacks;
  std::vector<std::pair<hwc2_display_t, bool>> mPendingHotplugs;

  std::map<hwc2_display_t, Hwc2Display> mDisplays;
  std::mutex mDisplayMutex;

  std::unique_ptr<RemoteDisplayMgr> mRemoteDisplayMgr;
};

#endif  // __HWC2_DEVICE_H__
