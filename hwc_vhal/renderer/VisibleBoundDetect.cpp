#include "VisibleBoundDetect.h"

#include <GLES3/gl3.h>
#include <GLES3/gl32.h>

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

static const char gImagePixelStatShader[] =
    "#version 320 es\n"
    "const int kBlkWidth = 16;\n"
    "const int kBlkHeight = 16;\n"
    "struct PixelStat {\n"
    "    uvec4 count;\n"
    "    ivec4 bound;\n"
    "};\n"
    "uniform int width;\n"
    "uniform int height;\n"
    "layout (local_size_x = 1, local_size_y = 1) in;\n"
    "layout (rgba8ui, binding = 0) readonly uniform  highp uimage2D input_image;\n"
    "layout(binding = 1) writeonly buffer Output {\n"
    "    PixelStat stat[];\n"
    "} output0;\n"
    "void main(void) {\n"
    "    uvec4 count = uvec4(0, 0, 0, 0);\n"
    "    ivec4 bound = ivec4(0x7fff, 0x7fff, 0, 0);\n"
    "    ivec2 blkIdx = ivec2(gl_GlobalInvocationID.xy);\n"
    "    int dstIdx = blkIdx.y * int(gl_NumWorkGroups.x) + blkIdx.x;\n"
    "    for (int y = 0; y < kBlkHeight; y++) { \n"
    "      for (int x = 0; x < kBlkWidth; x++) { \n"
    "        ivec2 srcIdx = ivec2(blkIdx.x * kBlkWidth, blkIdx.y * kBlkHeight) + ivec2(x, y);\n"
    "        if (srcIdx.y >= height) break;\n"
    "        if (srcIdx.x >= width) break;\n"
    "        uvec4 pixel = imageLoad(input_image, srcIdx); \n"
    "        if (pixel == uvec4(0, 0, 0, 0)) \n"
    "            count.x ++;\n"
    "        else if (pixel == uvec4(0, 0, 0, 0xff))\n"
    "            count.y ++;\n"
    "        else if (pixel.xyz == uvec3(0, 0, 0))\n"
    "            count.z ++;\n"
    "        else {\n"
    "            count.w ++;\n"
    "            bound.xy = min(bound.xy, srcIdx);\n"
    "            bound.zw = max(bound.zw, srcIdx);\n"
    "        };\n"
    "      } \n"
    "    } \n"
    "    output0.stat[dstIdx].count = count;\n"
    "    output0.stat[dstIdx].bound = bound;\n"
    "}\n";

static const char gPixelStatMerge[] =
    "#version 320 es\n"
    "struct PixelStat {\n"
    "    uvec4 count;\n"
    "    ivec4 bound;\n"
    "};\n"
    "uniform int width;\n"
    "uniform int height;\n"
    "layout (local_size_x = 1, local_size_y = 1) in;\n"
    "layout(binding = 0) readonly buffer Input {\n"
    "    PixelStat stat[];\n"
    "} input0;\n"
    "layout(binding = 1) writeonly buffer Output {\n"
    "    PixelStat stat[];\n"
    "} output0;\n"
    "void main(void) {\n"
    "    uvec4 count = uvec4(0, 0, 0, 0);\n"
    "    ivec4 bound = ivec4(0x7fff, 0x7fff, 0, 0);\n"
    "    ivec2 blkIdx = ivec2(gl_GlobalInvocationID.xy);\n"
    "    int dstIdx = blkIdx.y * int(gl_NumWorkGroups.x) + blkIdx.x;\n"
    "    for (int y = 0; y < 8; y++) { \n"
    "      for (int x = 0; x < 8; x++) { \n"
    "        if (blkIdx.y * 8 + y >= height) break;\n"
    "        if (blkIdx.x * 8 + x >= width) break;\n"
    "        int srcIdx = (blkIdx.y * 8 + y) * width + blkIdx.x * 8 + x;\n"
    "        count += input0.stat[srcIdx].count;\n"
    "        bound.xy = min(bound.xy, input0.stat[srcIdx].bound.xy);\n"
    "        bound.zw = max(bound.zw, input0.stat[srcIdx].bound.zw);\n"
    "      } \n"
    "    } \n"
    "    output0.stat[dstIdx].count = count;\n"
    "    output0.stat[dstIdx].bound = bound;\n"
    "}\n";

VisibleBoundDetect::VisibleBoundDetect() {
    memset(&mResult, 0, sizeof(mResult));
}
VisibleBoundDetect::~VisibleBoundDetect() {
    //ALOGD("VisibleBoundDetect::~VisibleBoundDetect() %p", this);
    for (auto& it : mBufferTextures) {
        delete it.second;
    }
    mBufferTextures.clear();
    if (mPixelStatBuffer[0]) {
        glDeleteBuffers(2, &mPixelStatBuffer[0]);
        CHECK();
    }
    mImagePixelStat = nullptr;
    mPixelStatMerge = nullptr;
}

void VisibleBoundDetect::createPixelStatBuffers(uint32_t count) {
    glGenBuffers(2, &mPixelStatBuffer[0]);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, mPixelStatBuffer[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(PixelStat), nullptr, GL_STATIC_DRAW);
    }
}

void VisibleBoundDetect::runImagePixelStat(GLuint tex, uint32_t width, uint32_t height, uint32_t outW, uint32_t outH) {
    GLuint prog = mImagePixelStat->getProgram();
    GLint locWidth = mImagePixelStat->getUniformLocation("width");
    GLint locHeight = mImagePixelStat->getUniformLocation("height");

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
    CHECK();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mPixelStatBuffer[0]);
    CHECK();

    glUseProgram(prog);
    CHECK();
    glUniform1i(locWidth, width);
    glUniform1i(locHeight, height);
    glDispatchCompute(outW, outH, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    CHECK();
}


void VisibleBoundDetect::runPixelStatMerge(uint32_t inW, uint32_t inH) {
    uint32_t outW, outH;
    GLuint prog = mPixelStatMerge->getProgram();
    GLint locWidth = mPixelStatMerge->getUniformLocation("width");
    GLint locHeight = mPixelStatMerge->getUniformLocation("height");
    bool swap = false;

    while (true) {
        outW = (inW + 7) / 8;
        outH = (inH + 7) / 8;

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mPixelStatBuffer[swap ? 1 : 0]);
        CHECK();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mPixelStatBuffer[swap ? 0 : 1]);
        CHECK();
        glUseProgram(prog);
        CHECK();

        glUniform1i(locWidth, inW);
        glUniform1i(locHeight, inH);
        glDispatchCompute(outW, outH, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        CHECK();

        // next iterate
        if (outW == 1 && outH == 1)
            break;
        swap = !swap;
        inW = outW;
        inH = outH;
    }
    readbackResult(mPixelStatBuffer[swap ? 0 : 1]);
}

void VisibleBoundDetect::readbackResult(GLuint ssbo) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    CHECK();
    PixelStat* stat = (PixelStat*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(PixelStat), GL_MAP_READ_BIT);
    CHECK();
    if (stat) {
       memcpy(&mResult, stat, sizeof(PixelStat));
    }
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    CHECK();
}

int VisibleBoundDetect::run() {
    if (!mProgramLoaded) {
        mImagePixelStat = std::make_unique<ShaderProgram>(gImagePixelStatShader);
        mPixelStatMerge = std::make_unique<ShaderProgram>(gPixelStatMerge);
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
        uint32_t outW = (w + 7) / 16;
        uint32_t outH = (h + 7) / 16;

        if (!mStatBufferCreated) {
            createPixelStatBuffers(outW * outH);
            mStatBufferCreated = true;
        }
        runImagePixelStat(bufTex->getTexture(), w, h, outW, outH);
        runPixelStatMerge(outW, outH);
        return 0;
    }
    return -1;
}
