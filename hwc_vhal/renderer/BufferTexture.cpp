#include "BufferTexture.h"
#include "BufferMapper.h"
#include <cutils/log.h>

//#define DEBUG_GL

#ifdef DEBUG_GL
#define CHECK() \
{\
    GLenum err = glGetError(); \
    if (err != GL_NO_ERROR) \
    {\
        ALOGD("%d:glGetError returns %d", __LINE__, err); \
    }\
}
#else
#define CHECK()
#endif

BufferTexture::BufferTexture(buffer_handle_t b, bool fbo) {
    if (EGL_NO_DISPLAY == mDisplay) {
        mDisplay = eglGetCurrentDisplay();
    }
    if (EGL_NO_DISPLAY == mDisplay) {
        ALOGE("BufferTexture: EGL is not initialized");
        return;
    }

    uint32_t stride = 0;
    uint64_t usage = GraphicBuffer::USAGE_HW_TEXTURE;
    GLenum target = fbo ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES;

    BufferMapper& mapper = BufferMapper::getMapper();
    mapper.getBufferSize(b, mWidth, mHeight);
    mapper.getBufferFormat(b, mFormat);
    mapper.getBufferStride(b, stride);

    mBuffer = new GraphicBuffer(b, GraphicBuffer::WRAP_HANDLE, mWidth, mHeight, mFormat, 1, usage, stride);
    EGLClientBuffer clientBuffer = (EGLClientBuffer)mBuffer->getNativeBuffer();
    mImage = eglCreateImageKHR(mDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, 0);
    if (EGL_NO_IMAGE_KHR == mImage) {
        ALOGE("BufferTexture: Failed to create EGLImage from buffer");
        return;
    }

    glGenTextures(1, &mTexture);
    CHECK();
    glBindTexture(target, mTexture);
    CHECK();
    glEGLImageTargetTexture2DOES(target, (GLeglImageOES)mImage);
    CHECK();

    if (fbo) {
        glGenFramebuffers(1, &mFBO);
        CHECK();
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        CHECK();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
        CHECK();
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ALOGE("BufferTexture: Failed to bind fbo:%d", status);
            return;
        }
    }
}

BufferTexture::BufferTexture(uint32_t width, uint32_t height, int32_t format) {
    if (EGL_NO_DISPLAY == mDisplay) {
        mDisplay = eglGetCurrentDisplay();
    }
    if (EGL_NO_DISPLAY == mDisplay) {
        ALOGE("BufferTexture: EGL is not initialized");
        return;
    }

    mWidth = width;
    mHeight = height;
    mFormat = format;
    mBuffer = new GraphicBuffer(mWidth, mHeight, mFormat,  GraphicBuffer::USAGE_HW_TEXTURE | GraphicBuffer::USAGE_HW_RENDER);
    EGLClientBuffer clientBuffer = (EGLClientBuffer)mBuffer->getNativeBuffer();
    mImage = eglCreateImageKHR(mDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, 0);
    if (EGL_NO_IMAGE_KHR == mImage) {
        ALOGE("BufferTexture: Failed to create EGLImage from buffer");
        return;
    }

    glGenTextures(1, &mTexture);
    CHECK();
    glBindTexture(GL_TEXTURE_2D, mTexture);
    CHECK();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)mImage);
    CHECK();

    glGenFramebuffers(1, &mFBO);
    CHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
    CHECK();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
    CHECK();
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ALOGE("BufferTexture: Failed to bind fbo:%d", status);
        return;
    }
}

BufferTexture::~BufferTexture() {
    //ALOGD("BufferTexture::~BufferTexture() %p", this);
    if (mFBO) {
        glDeleteFramebuffers(1, &mFBO);
        CHECK();
    }
    if (mTexture) {
        glDeleteTextures(1, &mTexture);
        CHECK();
    }
    if (EGL_NO_IMAGE != mImage && EGL_NO_DISPLAY != mDisplay)
        eglDestroyImage(mDisplay, mImage);
}
