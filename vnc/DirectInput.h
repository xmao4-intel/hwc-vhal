#ifndef __DIRECT_INPUT_RECEIVER_H__
#define __DIRECT_INPUT_RECEIVER_H__

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/input.h>
//#include <linux/uinput.h>

#include "input_interface.h"

class DirectInputReceiver : public IInputReceiver {
 public:
  DirectInputReceiver(int id);
  virtual ~DirectInputReceiver();

  // IInputReceiver
  int getTouchInfo(TouchInfo* info) override;
  int onInputMessage(const std::string& msg) override;
  int onKeyCode(uint16_t scanCode, uint32_t mask) override;
  int onKeyChar(char ch) override;
  int onText(const char* msg) override;

 protected:
  // Process one mini-touch command
  bool ProcessOneCommand(const std::string& cmd);

  uint32_t GetMaxPositionX() { return kMaxPositionX - 1; }
  uint32_t GetMaxPositionY() { return kMaxPositionY - 1; }

  bool CreateTouchDevice(int id);
  bool SendEvent(uint16_t type, uint16_t code, int32_t value);
  bool SendDown(int32_t slot, int32_t x, int32_t y, int32_t pressure);
  bool SendUp(int32_t slot, int32_t x, int32_t y);
  bool SendMove(int32_t slot, int32_t x, int32_t y, int32_t pressure);
  bool SendCommit();
  bool SendReset();
  void SendWait(uint32_t ms);

 private:
  //const char* kEnvWorkDir = "AIC_WORK_DIR";
  //const char* kDevName = "/ipc/input-pipe";
  static const uint32_t kMaxSlot = 9;
  static const uint32_t kMaxMajor = 15;
  static const uint32_t kMaxPositionX = 720;
  static const uint32_t kMaxPositionY = 1280;
  static const uint32_t kMaxPressure = 255;
  static const uint32_t kMaxTrackingId = 65535;

  struct Contact {
    bool enabled = false;
    int32_t trackingId = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t pressure = 0;
  };

  int mFd = -1;
  Contact mContacts[kMaxSlot];
  int32_t mTrackingId = 0;
  uint32_t mEnabledSlots = 0;
};
#endif  //__DIRECT_INPUT_RECEIVER_H__
