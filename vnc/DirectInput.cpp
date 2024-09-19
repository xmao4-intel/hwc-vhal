#define LOG_NDEBUG 0

#define CLOGV ALOGV
#define CLOGD ALOGD
#define CLOGE ALOGE
#include <cutils/log.h>

#include "DirectInput.h"

DirectInputReceiver::DirectInputReceiver(int id) {
  CLOGV("%s", __func__);

  CreateTouchDevice(id);
}

DirectInputReceiver::~DirectInputReceiver() {
  CLOGV("%s", __func__);

  if (mFd >= 0) {
    close(mFd);
  }
}

bool DirectInputReceiver::CreateTouchDevice(int id) {
  (void)id;
  CLOGV("%s", __func__);

  // Todo: use socket to support multiple input for each display
  char path[256];
  const char* dir = getenv(kEnvWorkDir);

  if (access(kDevName, F_OK) == 0) {
    snprintf(path, 256, "%s", kDevName);
  } else if (dir && (strlen(dir) + strlen(kDevName) < 256)) {
    snprintf(path, 256, "%s/%s", dir, kDevName);
  } else {
    snprintf(path, 256, "%s", "/data/local/tmp/input-pipe");
  }

  mFd = open(path, O_RDWR | O_NONBLOCK, 0);
  if (mFd < 0) {
    CLOGE("Failed to open pipe for read:%s", strerror(errno));
    return false;
  }
  return true;
}

bool DirectInputReceiver::SendEvent(uint16_t type,
                                    uint16_t code,
                                    int32_t value) {
  struct input_event ev;
  timespec ts;

  if (mFd < 0) {
    CreateTouchDevice(0);
    if (mFd < 0)
      return false;
  }

  memset(&ev, 0, sizeof(struct input_event));
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ev.time.tv_sec = ts.tv_sec;
  ev.time.tv_usec = ts.tv_nsec / 1000;
  ev.type = type;
  ev.code = code;
  ev.value = value;

  if (write(mFd, &ev, sizeof(struct input_event)) < 0) {
    perror("Failed to send event\n");
    return false;
  }
  return true;
}

bool DirectInputReceiver::SendDown(int32_t slot,
                                   int32_t x,
                                   int32_t y,
                                   int32_t pressure) {
  CLOGV("%s", __func__);

  if ((uint32_t)slot >= kMaxSlot) {
    return false;
  }
  if (mContacts[slot].enabled || mEnabledSlots >= kMaxSlot || slot == 8) {
    SendReset();
  }
  mContacts[slot].enabled = true;
  mContacts[slot].trackingId = mTrackingId++;
  mEnabledSlots++;

  SendEvent(EV_ABS, ABS_MT_SLOT, slot);
  SendEvent(EV_ABS, ABS_MT_TRACKING_ID, mContacts[slot].trackingId);

  if (mEnabledSlots == 1) {
    SendEvent(EV_KEY, BTN_TOUCH, 1);
  }
  SendEvent(EV_ABS, ABS_MT_TOUCH_MAJOR, 0x00000004);
  SendEvent(EV_ABS, ABS_MT_WIDTH_MAJOR, 0x00000006);
  SendEvent(EV_ABS, ABS_MT_PRESSURE, pressure);
  SendEvent(EV_ABS, ABS_MT_POSITION_X, x);
  SendEvent(EV_ABS, ABS_MT_POSITION_Y, y);

  return true;
}

bool DirectInputReceiver::SendUp(int32_t slot, int32_t x, int32_t y) {
  (void)x;
  (void)y;
  CLOGV("%s", __func__);

  if (mEnabledSlots == 0 || (uint32_t)slot >= kMaxSlot ||
      !mContacts[slot].enabled) {
    return false;
  }

  mContacts[slot].enabled = false;
  mEnabledSlots--;

  SendEvent(EV_ABS, ABS_MT_SLOT, slot);
  SendEvent(EV_ABS, ABS_MT_TRACKING_ID, -1);
  if (mEnabledSlots == 0) {
    SendEvent(EV_KEY, BTN_TOUCH, 0);
  }
  return true;
}

bool DirectInputReceiver::SendMove(int32_t slot,
                                   int32_t x,
                                   int32_t y,
                                   int32_t pressure) {
  CLOGV("%s", __func__);

  if ((uint32_t)slot >= kMaxSlot || !mContacts[slot].enabled) {
    return false;
  }

  SendEvent(EV_ABS, ABS_MT_SLOT, slot);
  SendEvent(EV_ABS, ABS_MT_TOUCH_MAJOR, 0x00000004);
  SendEvent(EV_ABS, ABS_MT_WIDTH_MAJOR, 0x00000006);
  SendEvent(EV_ABS, ABS_MT_PRESSURE, pressure);
  SendEvent(EV_ABS, ABS_MT_POSITION_X, x);
  SendEvent(EV_ABS, ABS_MT_POSITION_Y, y);

  return true;
}

bool DirectInputReceiver::SendCommit() {
  CLOGV("%s", __func__);

  SendEvent(EV_SYN, SYN_REPORT, 0);
  return true;
}

bool DirectInputReceiver::SendReset() {
  CLOGV("%s", __func__);

  bool report = false;
  for (uint32_t slot = 0; slot < kMaxSlot; slot++) {
    if (mContacts[slot].enabled) {
      mContacts[slot].enabled = false;
      report = true;
    }
  }
  if (report) {
    SendEvent(EV_SYN, SYN_REPORT, 0);
  }
  return true;
}

void DirectInputReceiver::SendWait(uint32_t ms) {
  CLOGV("%s", __func__);

  usleep(ms * 1000);
}

bool DirectInputReceiver::ProcessOneCommand(const std::string& cmd) {
  CLOGV("%s", __func__);

  char type = 0;
  int32_t slot = 0;
  int32_t x = 0;
  int32_t y = 0;
  int32_t pressure = 0;
  int32_t ms = 0;

  // CLOGD("%s:%d %s", __func__, __LINE__, cmd.c_str());

  switch (cmd[0]) {
    case 'c':  // commit
      SendCommit();
      break;
    case 'r':  // reset
      SendReset();
      break;
    case 'd':  // down
      sscanf(cmd.c_str(), "%c %d %d %d %d", &type, &slot, &x, &y, &pressure);
      SendDown(slot, x, y, pressure);
      break;
    case 'u':  // up
      sscanf(cmd.c_str(), "%c %d %d %d", &type, &slot, &x, &y);
      SendUp(slot, x, y);
      break;
    case 'm':  // move
      sscanf(cmd.c_str(), "%c %d %d %d %d", &type, &slot, &x, &y, &pressure);
      SendMove(slot, x, y, pressure);
      break;
    case 'w':  // wait ms
      sscanf(cmd.c_str(), "%c %d", &type, &ms);
      SendWait(ms);
      break;
    default:
      break;
  }
  return true;
}

int DirectInputReceiver::getTouchInfo(TouchInfo* info) {
  CLOGV("%s", __func__);

  if (!info) {
    return -EINVAL;
  }

  info->max_contacts = kMaxSlot;
  info->max_pressure = kMaxPressure;
  info->max_x = kMaxPositionX;
  info->max_y = kMaxPositionY;
  info->pid = getpid();
  info->version = 1;

  return 0;
}
int DirectInputReceiver::onInputMessage(const std::string& msg) {
  CLOGV("%s", __func__);

  size_t begin = 0;
  size_t end = 0;

  // CLOGD("%s:%d %s", __func__, __LINE__, msg.c_str());

  while (true) {
    end = msg.find("\n", begin);
    if (end == std::string::npos)
      break;

    std::string cmd = msg.substr(begin, end);
    ProcessOneCommand(cmd);
    begin = end + 1;
    if (msg[begin] == '\r')
      begin++;
  }
  return 0;
}

int DirectInputReceiver::onKeyCode(uint16_t scanCode, uint32_t mask) {
  CLOGV("%s", __func__);

  if (mask & KEY_STATE_MASK::Shift) {
    SendEvent(EV_KEY, KEY_LEFTSHIFT, 1);
    SendCommit();
  }
  if (mask & KEY_STATE_MASK::Control) {
    SendEvent(EV_KEY, KEY_LEFTCTRL, 1);
    SendCommit();
  }
  if (mask & KEY_STATE_MASK::Mod1) {
    SendEvent(EV_KEY, KEY_LEFTALT, 1);
    SendCommit();
  }

  SendEvent(EV_KEY, scanCode, 1);
  SendCommit();
  SendEvent(EV_KEY, scanCode, 0);
  SendCommit();

  if (mask & KEY_STATE_MASK::Shift) {
    SendEvent(EV_KEY, KEY_LEFTSHIFT, 0);
    SendCommit();
  }
  if (mask & KEY_STATE_MASK::Control) {
    SendEvent(EV_KEY, KEY_LEFTCTRL, 0);
    SendCommit();
  }
  if (mask & KEY_STATE_MASK::Mod1) {
    SendEvent(EV_KEY, KEY_LEFTALT, 0);
    SendCommit();
  }
  return 0;
}

int DirectInputReceiver::onKeyChar(char ch) {
  (void)ch;
  CLOGV("%s", __func__);

  return 0;
}

int DirectInputReceiver::onText(const char* msg) {
  (void)msg;
  CLOGV("%s", __func__);

  return 0;
}
