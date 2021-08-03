/*
Copyright (C) 2021 Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions
and limitations under the License.


SPDX-License-Identifier: Apache-2.0

Author: Mao Marc (marc.mao@intel.com)
Date: 2021.06.09

*/

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
