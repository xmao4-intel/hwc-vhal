
#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <cutils/properties.h>

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <cstdlib>

#include "display_protocol.h"
#include "hwc_vhal_client.h"

#define ASSERT(a) if (!(a)) { ALOGE("assert %s failed  in %s:%d", #a, __FILE__, __LINE__); }

HwcVhalClient::HwcVhalClient(int w, int h, std::unique_ptr<HwcVhalCallbacks> cb) 
    : mWidth(w), mHeight(h), mCallbacks(std::move(cb)) {
}

int HwcVhalClient::init() {
    mFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mFd < 0) {
        ALOGE("Failed to create unix socket:%s",  strerror(errno));
        return -1;
    }

    const char* path = kHwcSock;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    bool abstract = true;
    if (abstract) {
        strncpy(&addr.sun_path[1], path, strlen(path));
        addr.sun_path[0] = 0;
    } else {
        strncpy(&addr.sun_path[0], path, strlen(path));
    }
    if (connect(mFd, (struct sockaddr*)&addr, sizeof(sa_family_t) + strlen(path) + 1) < 0) {
        ALOGE("Failed to connect %s:%s", path, strerror(errno));
        close(mFd);
        mFd = -1;
        return -1;
    }
    ALOGD("Connect to %s succesfully, socket fd=%d", path, mFd);
    ALOGD("this=%p fd=%d", this, this->mFd);
    return mFd;
}

int HwcVhalClient::recvFds(int* pfd, size_t fdlen)
{
    int ret = 0;
    int count = 0;
    int i = 0;
    struct msghdr msg;
    int rdata[4] = {0};
    struct iovec vec;
    char cmsgbuf[CMSG_SPACE(fdlen * sizeof(int))];
    struct cmsghdr* p_cmsg;
    int* p_fds;

    if (pfd == NULL) {
        return -1;
    }

    vec.iov_base = rdata;
    vec.iov_len = 16;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    msg.msg_flags = 0;

    p_fds = (int*)CMSG_DATA(CMSG_FIRSTHDR(&msg));
    count = recvmsg(mFd, &msg, MSG_WAITALL);
    if (count < 0) {
        ALOGE("Failed to recv mFd from remote\n");
        ret = -1;
    } else {
        p_cmsg = CMSG_FIRSTHDR(&msg);
        if (p_cmsg == NULL) {
            ALOGE("No msg hdr\n");
            ret = -1;
        } else {
            p_fds = (int*)CMSG_DATA(p_cmsg);
            for (i = 0; i < (int)fdlen; i++) {
                pfd[i] = p_fds[i];
            }
        }
    }
    return ret;
}

int HwcVhalClient::updateDispConfig(const display_event_t& de) {
    ASSERT(de.type == DD_EVENT_DISPINFO_REQ && de.size == sizeof(display_event_t));

    display_info_event_t ev{};

    ev.event.type = DD_EVENT_DISPINFO_ACK;
    ev.event.size = sizeof(ev);

    ev.info.flags = 1;
    ev.info.width = mWidth;
    ev.info.height = mHeight;
    ev.info.stride = ev.info.width;
    ev.info.format = 5;
    ev.info.xdpi = 240;
    ev.info.ydpi = 240;
    ev.info.fps = 60;
    ev.info.minSwapInterval = 1;
    ev.info.maxSwapInterval = 1;
    ev.info.numFramebuffers = 2;

    ssize_t len = 0;
    len = send(mFd, &ev, sizeof(ev), 0);
    if (len <= 0) {
        ALOGE("Failed to send: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int HwcVhalClient::createBuffer(const display_event_t& de) {
    ASSERT(de.type == DD_EVENT_CREATE_BUFFER && de.size > sizeof(buffer_info_event_t) + sizeof(native_handle_t));

    buffer_info_event_t ev{};
    ssize_t len = 0;
    void* handlePtr = nullptr;

    len = recv(mFd, &ev.info, sizeof(ev.info), 0);
    if (len <= 0) {
        ALOGE("Failed to read buffer info: %s\n", strerror(errno));
        return -1;
    }

    ssize_t handleSize = de.size - sizeof(buffer_info_event_t);
    handlePtr = malloc(handleSize);
    if (handlePtr == nullptr) {
        ALOGE("Failed to allocate local buffer handle: %s\n", strerror(errno));
        return -1;
    }

    len = recv(mFd, handlePtr, handleSize, 0);
    if (len <= 0) {
        ALOGE("Failed to read buffer info: %s\n", strerror(errno));
        free(handlePtr);
        return -1;
    }

    buffer_handle_t handle = static_cast<buffer_handle_t>(handlePtr);
    if (recvFds(const_cast<int*>(&handle->data[0]), handle->numFds) == -1) {
        free(handlePtr);
        return -1;
    }

    if (mBuffers.find(ev.info.bufferId) != mBuffers.end()) {
        auto ptr = const_cast<native_handle_t*>(mBuffers.at(ev.info.bufferId));
        free(ptr);
        mBuffers.erase(ev.info.bufferId);
    }
    mBuffers.insert({ev.info.bufferId, handle});

    if (mCallbacks) {
        mCallbacks->onCreateBuffer(ev.info.bufferId, handle);
    }
    return 0;
}

int HwcVhalClient::removeBuffer(const display_event_t& de) {
    ASSERT(de.type == DD_EVENT_REMOVE_BUFFER && de.size == sizeof(buffer_info_event_t));

    buffer_info_event_t ev{};
    ssize_t len = 0;

    len = recv(mFd, &ev.info, sizeof(ev.info), 0);
    if (len <= 0) {
        ALOGE("Failed to read buffer info: %s\n", strerror(errno));
        return -1;
    }

    buffer_handle_t handle = nullptr;
    if (mBuffers.find(ev.info.bufferId) != mBuffers.end()) {
        handle = mBuffers.at(ev.info.bufferId);
    }
    if (mCallbacks && handle) {
        mCallbacks->onRemoveBuffer(ev.info.bufferId, handle);
    }
    if (handle) {
        mBuffers.erase(ev.info.bufferId);
        auto ptr = const_cast<native_handle_t*>(handle);
        free(ptr);
    }
    return 0;
}

int HwcVhalClient::displayBuffer(const display_event_t& de) {
    ASSERT(de.type == DD_EVENT_DISPLAY_REQ && de.size == sizeof(buffer_info_event_t));

    buffer_info_event_t ev{};
    ssize_t len = 0;
    len = recv(mFd, &ev.info, sizeof(ev.info), 0);
    if (len <= 0) {
        ALOGE("Failed to read buffer info: %s\n", strerror(errno));
        return -1;
    }

    buffer_handle_t handle = nullptr;
    if (mBuffers.find(ev.info.bufferId) != mBuffers.end()) {
        handle = mBuffers.at(ev.info.bufferId);
    }
    if (mCallbacks && handle) {
        mCallbacks->onDisplayBuffer(ev.info.bufferId, handle);
    }

    ev.event.type = DD_EVENT_DISPLAY_ACK;
    ev.event.size = sizeof(ev);
    len = send(mFd, &ev, sizeof(ev), 0);
    if (len <= 0) {
        ALOGE("send() failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int HwcVhalClient::processEvents() {
    ALOGD("this=%p fd=%d", this, this->mFd);
    while (1) {
        display_event_t de{};
        ssize_t len = read(mFd, &de, sizeof(de));
        if (len <= 0) {
            ALOGE("Failed to read from %d, ret=%zd:%s", mFd, len, strerror(errno));
            break;
        }
        switch (de.type) {
            case DD_EVENT_DISPINFO_REQ:
                ALOGD("DD_EVENT_DISPINFO_REQ");
                updateDispConfig(de);
                break;
            case DD_EVENT_CREATE_BUFFER:
                ALOGD("DD_EVENT_CREATE_BUFFER");
                createBuffer(de);
                break;
            case DD_EVENT_REMOVE_BUFFER:
                ALOGD("DD_EVENT_REMOVE_BUFFER");
                removeBuffer(de);
                break;
            case DD_EVENT_DISPLAY_REQ:
                ALOGD("DD_EVENT_DISPLAY_REQ");
                displayBuffer(de);
                break;
            default:
                ALOGD("Unhandled event type=%d size=%d", de.type, de.size);
                uint8_t unused;
                for(int i = 0; i < de.size; i++)
                    read(mFd, &unused, 1);
                break;
        }
    }
    return 0;
}