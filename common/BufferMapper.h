#ifndef __BUFFER_MAPPER_H__
#define __BUFFER_MAPPER_H__

#include <hardware/gralloc1.h>
#include <hardware/hwcomposer2.h>
#include <system/graphics.h>

class BufferMapper {
 public:
  ~BufferMapper();

  static BufferMapper& getMapper() {
    static BufferMapper sInst;
    return sInst;
  }

  int getBufferSize(buffer_handle_t b, uint32_t& w, uint32_t& h);
  int getBufferFormat(buffer_handle_t b, int32_t& f);
  int getBufferStride(buffer_handle_t b, uint32_t& s);
  int lockBuffer(buffer_handle_t b, uint8_t*& data, uint32_t& s);
  int unlockBuffer(buffer_handle_t b);

 private:
  BufferMapper();
  int getGrallocDevice();

 private:
  gralloc1_device_t* mGralloc = nullptr;
  GRALLOC1_PFN_LOCK pfnLock = nullptr;
  GRALLOC1_PFN_UNLOCK pfnUnlock = nullptr;
  GRALLOC1_PFN_GET_DIMENSIONS pfnGetDimensions = nullptr;
  GRALLOC1_PFN_GET_FORMAT pfnGetFormat = nullptr;
  GRALLOC1_PFN_GET_STRIDE pfnGetStride = nullptr;
};
#endif