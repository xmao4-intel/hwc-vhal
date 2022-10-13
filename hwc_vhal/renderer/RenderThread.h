#ifndef _RENDER_THREAD_H_
#define _RENDER_THREAD_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <list>

class RenderTask {
public:
    virtual ~RenderTask() {}
    void runIt();
    void wait();

private:
    virtual int run() = 0;
    std::mutex mMutex;
    std::condition_variable mDone;
};

class ReleaseTask : public RenderTask {
public:
    ReleaseTask(RenderTask* rt) : mRenderTask(rt) {}
private:
    int run() override {
        if (mRenderTask)
            delete mRenderTask;
        return 0;
    }
    RenderTask* mRenderTask = nullptr;
};

class RenderThread {
public:
    void init();
    void deinit();
    void join();
    void runTaskAsync(RenderTask* t);
    void runTask(RenderTask* t);

private:
    int initRenderContext();
    void renderThreadProc();

private:
    std::unique_ptr<std::thread> mThread;
    std::list<RenderTask*> tasks;
    std::mutex mQueueMutex;
    std::condition_variable mNewTask;

    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLContext mContext = EGL_NO_CONTEXT;
    bool mContextReady = false;;
};

#endif
