#ifndef __BUFFER_DUMPER_H__
#define __BUFFER_DUMPER_H__

#include <png.h>

#include <hardware/hwcomposer2.h>
#include <system/graphics.h>

class BufferDumper {
  struct ImageData {
    uint32_t width;
    uint32_t height;
    uint32_t stride;  // in bytes
    uint32_t bpc;
    bool alpha;
    uint8_t* data;
  };

 public:
  ~BufferDumper();

  static BufferDumper& getBufferDumper() {
    static BufferDumper sInst;
    return sInst;
  }

  int dumpBuffer(buffer_handle_t b, uint32_t frameNum);

 private:
  BufferDumper();
  int preparePngHeader(FILE* file,
                       const ImageData* image,
                       png_structp& png,
                       png_infop& info);
  int fillPngData(const ImageData* image, png_structp& png);
  int dumpToPng(const char* path, const ImageData* image);

 private:
  const char* kDumpFolder = "/data/hwc-dump";
  bool mEnableDump = true;
};
#endif