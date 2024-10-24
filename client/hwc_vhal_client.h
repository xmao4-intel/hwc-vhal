#ifndef HWC_VHAL_CLIENT_H
#define HWC_VHAL_CLIENT_H

#include <memory>
#include <map>

#include <hardware/gralloc1.h>
#include "display_protocol.h"

struct HwcVhalCallbacks {
    virtual ~HwcVhalCallbacks() {}
    virtual void onCreateBuffer(uint64_t id, buffer_handle_t h) = 0;
    virtual void onRemoveBuffer(uint64_t id, buffer_handle_t h) = 0;
    virtual void onDisplayBuffer(uint64_t id, buffer_handle_t h) = 0;
};

class HwcVhalClient {
public:
    HwcVhalClient(int w, int h, std::unique_ptr<HwcVhalCallbacks> cb);
    virtual ~HwcVhalClient() {}

    int init();
    int processEvents();

private:
    int recvFds(int* pfd, size_t fdlen);
    int updateDispConfig(const display_event_t& de);
    int createBuffer(const display_event_t& de);
    int removeBuffer(const display_event_t& de);
    int displayBuffer(const display_event_t& de);

private:
    const char* kHwcSock = "hwc-sock";
    int mFd = -1;
    int mWidth = 1;
    int mHeight = 1;
    std::unique_ptr<HwcVhalCallbacks> mCallbacks;
    std::map<uint64_t, buffer_handle_t>  mBuffers;
};

#endif
