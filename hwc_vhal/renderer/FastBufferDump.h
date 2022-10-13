#ifndef _FAST_BUFFER_DUMP_H_
#define _FAST_BUFFER_DUMP_H_

#include <string>
#include <png.h>

#include "RenderThread.h"
#include "BufferTexture.h"

class FastBufferDump : public RenderTask {
public:
    FastBufferDump(uint32_t num, uint32_t z, buffer_handle_t b, int type);

private:
    struct ImageData {
        uint32_t width;
        uint32_t height;
        uint32_t stride;  // in bytes
        uint32_t bpc;
        bool alpha;
        uint8_t* data;
    };

private:
    int run() override;
    void prepareFolder();
    int preparePngHeader(FILE* file, const ImageData* image, png_structp& png, png_infop& info);
    int fillPngData(const ImageData* image, png_structp& png);
    int dumpToPng(const char* path, const ImageData* image);

    const char* kDumpFolder = "/data/hwc-dump";
    uint32_t mFrameNum = 0;
    uint32_t mZOrder = 0;
    buffer_handle_t mHandle = nullptr;
    int  mDumpType = 0;
    bool mEnableDump = false;
};

#endif
