#ifndef __UIO_DISPLAY__H
#define __UIO_DISPLAY__H

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include "BufferMapper.h"
#include "common/KVMFR.h"

#define ALIGN_DN(x) ((uintptr_t)(x) & ~0x7F)
#define ALIGN_UP(x) ALIGN_DN(x + 0x7F)
#define MAX_FRAMES 2

class UioDisplay {

 public:
  struct app
  {
    KVMFRHeader * shmHeader;
    uint8_t     * pointerData;
    unsigned int  pointerDataSize;
    unsigned int  pointerOffset;
    uint8_t     * frames;
    unsigned int  frameSize;
    uint8_t     * frame[MAX_FRAMES];
    unsigned int  frameOffset[MAX_FRAMES];
    bool          running;
  };

 public:
  UioDisplay(int id, int w, int h);
  ~UioDisplay();
  int postFb(buffer_handle_t fb);
  int init();

 private:
  int mDisplayId = 0;
  struct app app;
  int frame_id = 0;
  uint32_t mWidth = 720;
  uint32_t mHeight = 1280;

 private:
  int uioOpenFile(const char * shmDevice, const char * file);
  int shmOpenDev(const char * shmDevice);

};

#endif
