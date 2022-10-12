#include "AlphaVideo.h"

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

const GLfloat gTriangleVertices[] = {
    -1.0f, 1.0f,
    -1.0f, -1.0f,
    1.0f, -1.0f,
    1.0f, 1.0f,
};

static const char gVertexShader[] =
    "attribute vec4 vPosition;\n"
    "varying vec2 texCoords;\n"
    "void main() {\n"
    "  texCoords = vec2(vPosition.x + 1.0, (vPosition.y + 1.0) / 2.0);\n"
    "  gl_Position = vPosition;\n"
    "}\n";

static const char gFragmentShader[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "uniform samplerExternalOES texSampler;\n"
    "varying vec2 texCoords;\n"
    "void main() {\n"
    "  if (texCoords.x < 1.0) {\n"
    "      gl_FragColor = texture2D(texSampler, texCoords);\n"
    "  } else {\n"
    "      vec4 color = texture2D(texSampler, vec2(texCoords.x - 1.0, texCoords.y));\n"
    "      gl_FragColor = color.wwww;\n"
    "  }\n"
    "}\n";

AlphaVideo::AlphaVideo() {}
AlphaVideo::~AlphaVideo() {
    for (auto& it : mBufferTextures) {
        delete it.second;
    }
    mBufferTextures.clear();
    mOutput = nullptr;
    mAlphaVideoProgram = nullptr;
}

void AlphaVideo::draw(GLuint tex, uint32_t width, uint32_t height) {
    GLuint prog = mAlphaVideoProgram->getProgram();
    GLint locPos = mAlphaVideoProgram->getAttribLocation("vPosition");
    GLint locTexSampler = mAlphaVideoProgram->getUniformLocation("texSampler");

    glBindFramebuffer(GL_FRAMEBUFFER, mOutput->getFBO());
    CHECK();
    glViewport(0, 0, width, height);
    CHECK();
    glUseProgram(prog);
    CHECK();
    glVertexAttribPointer(locPos, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
    CHECK();
    glEnableVertexAttribArray(locPos);
    CHECK();
    glUniform1i(locTexSampler, 0);
    CHECK();
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, tex);
    CHECK();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    CHECK();
    glFinish();
    CHECK();
}

int AlphaVideo::run() {
    if (!mProgramLoaded) {
        mAlphaVideoProgram = std::make_unique<ShaderProgram>(gVertexShader, gFragmentShader);
        mProgramLoaded = true;
    }
    if (mCurrHandle) {
        BufferTexture* bufTex = nullptr;
        if (mBufferTextures.find(mCurrHandle) == mBufferTextures.end()) {
            bufTex = new BufferTexture(mCurrHandle);
            mBufferTextures.insert(std::make_pair(mCurrHandle, bufTex));
        } else {
            bufTex = mBufferTextures.at(mCurrHandle);
        }
        uint32_t w = bufTex->getWidth();
        uint32_t h = bufTex->getHeight();
        if (!mOutputCreated) {
            mOutput = std::make_unique<BufferTexture>(w, h, bufTex->getFormat());
            mOutputCreated = true;
        }
        draw(bufTex->getTexture(), w, h);
    }
    return 0;
}
