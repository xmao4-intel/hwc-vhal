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

Author: Xue Yifei (yifei.xue@intel.com)
Date: 2021.06.09

*/

#include <stdio.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "LocalDisplay.h"

int getResFromFb(int& w, int& h) {
  struct fb_var_screeninfo fbVar;
  int fd = open("/dev/fb0", O_RDWR);
  if (fd >= 0) {
    ioctl(fd, FBIOGET_VSCREENINFO, &fbVar);
    w = fbVar.xres;
    h = fbVar.yres;
    close(fd);
  }

  return -1;
}

int getResFromDebugFs(int& w, int& h) {
  const char* path = "/sys/kernel/debug/dri/0/i915_display_info";
  char lineBuf[256];
  int ret = -1;
  int crtc;

  FILE* fp = fopen(path, "rb");
  if (!fp)
    return -1;

  while (!feof(fp)) {
    fgets(lineBuf, 256, fp);
    if (sscanf(lineBuf, "CRTC %d: pipe: A, active=yes, (size=%dx%d)", &crtc, &w,
               &h) == 3) {
      ret = 0;
      break;
    }
  }
  fclose(fp);
  return ret;
}

int getResFromKms(int& w, int& h) {
  return -1;
}
