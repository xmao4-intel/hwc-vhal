//#define LOG_NDEBUG 0

#include <GLES3/gl3.h>
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

#include "RenderThread.h"
#include <cutils/log.h>

void RenderTask::runIt() {
    std::unique_lock<std::mutex> lck(mMutex);
    run();
    mDone.notify_all();
}

void RenderTask::wait() {
    std::unique_lock<std::mutex> lck(mMutex);
    mDone.wait(lck);
}

void RenderThread::init() {
    mThread = std::unique_ptr<std::thread>(
    new std::thread(&RenderThread::renderThreadProc, this));
}

void RenderThread::deinit() {
    eglDestroyContext(mDisplay, mContext);
    eglTerminate(mDisplay);
}

void RenderThread::join() {
    mThread->join();
}

void RenderThread::runTaskAsync(RenderTask* t) {
    if (!t) return;
    std::unique_lock<std::mutex> lck(mQueueMutex);
    tasks.push_back(t);
    mNewTask.notify_all();
}

void RenderThread::runTask(RenderTask* t) {
    if (!t) return;
    runTaskAsync(t);
    t->wait();
}

int RenderThread::initRenderContext() {
    ALOGV("%s", __func__);

    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (mDisplay == EGL_NO_DISPLAY) {
        ALOGE("eglGetDisplay returned EGL_NO_DISPLAY.");
        return -1;
    }

    EGLint majorVersion;
    EGLint minorVersion;
    EGLBoolean returnValue = eglInitialize(mDisplay, &majorVersion, &minorVersion);
    if (returnValue != EGL_TRUE) {
        ALOGE("eglInitialize failed");
        return -1;
    }

    EGLConfig cfg;
    EGLint count;
    EGLint s_configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_NONE };
    if (eglChooseConfig(mDisplay, s_configAttribs, &cfg, 1, &count) == EGL_FALSE) {
        ALOGE("eglChooseConfig failed");
        return -1;
    }

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    mContext = eglCreateContext(mDisplay, cfg, EGL_NO_CONTEXT, context_attribs);
    if (mContext == EGL_NO_CONTEXT) {
        ALOGE("eglCreateContext failed");
        return -1;
    }

    returnValue = eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext);
    if (returnValue != EGL_TRUE) {
        ALOGE("eglMakeCurrent failed returned %d", returnValue);
        return -1;
    }

    mContextReady = true;
    return 0;
}

void RenderThread::renderThreadProc() {
    ALOGV("renderThreadProc");

    if (initRenderContext() < 0) {
        ALOGE("Failed to initialize render context!");
        return;
    }
    while (1) {
        {
            std::unique_lock<std::mutex> lck(mQueueMutex);
            if (tasks.empty()) {
                mNewTask.wait(lck);
            }
        }
        auto t = tasks.front();
        tasks.pop_front();
        t->runIt();
    }
}
