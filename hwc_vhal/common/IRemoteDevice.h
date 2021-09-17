#ifndef __IREMOTE_DEVICE_H__
#define __IREMOTE_DEVICE_H__

#include <vector>
#include <display_protocol.h>

class RemoteDisplay;
struct IRemoteDevice {
  virtual ~IRemoteDevice() {}
  virtual int addRemoteDisplay(RemoteDisplay* rd) = 0;
  virtual int removeRemoteDisplay(RemoteDisplay* rd) = 0;
  virtual int getMaxRemoteDisplayCount() = 0;
  virtual int getRemoteDisplayCount() = 0;
};

struct DisplayStatusListener {
  virtual ~DisplayStatusListener(){};
  virtual int onConnect(int fd) = 0;
  virtual int onDisconnect(int fd) = 0;
};

struct DisplayEventListener {
  virtual ~DisplayEventListener(){};
  virtual int onBufferDisplayed(const buffer_info_t& info) = 0;
  virtual int onPresented(std::vector<layer_buffer_info_t>& layerBuffer, int& fence) = 0;
};

#endif  //__IREMOTE_DEVICE_H__