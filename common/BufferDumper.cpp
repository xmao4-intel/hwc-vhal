#undef NDEBUG
//#define LOG_NDEBUG 0
#include <cutils/log.h>

#include <errno.h>
#include <sys/stat.h>

#include "BufferDumper.h"
#include "BufferMapper.h"
#include <unistd.h>

BufferDumper::BufferDumper() {
  ALOGV("BufferDumper::%s", __func__);

  struct stat st;
  bool exist = false;

  int ret = stat(kDumpFolder, &st);
  if (ret < 0) {
    if (errno != ENOENT) {
      ALOGE("Failed to access %s:%s", kDumpFolder, strerror(errno));
      return;
    }
  } else {
    if (!(st.st_mode & S_IFDIR)) {  // not a folder
      ALOGD("%s exits but it is not a folder, try to recreate it as folder",
            kDumpFolder);
      if (unlink(kDumpFolder) < 0) {
        ALOGE("Can't remote %s:%s", kDumpFolder, strerror(errno));
        mEnableDump = false;
        return;
      }
    } else {
      exist = true;
    }
  }
  // now create the folder
  if (!exist) {
    if (mkdir(kDumpFolder, 0777) < 0) {
      mEnableDump = false;
      ALOGE("%s doesn't exit but failed to create it:%s", kDumpFolder,
            strerror(errno));
    }
  }
  // now check the write access to the folder
  if (access(kDumpFolder, F_OK | W_OK) < 0) {
    mEnableDump = false;
    ALOGE("Can't write %s, dump is disabled", kDumpFolder);
  }
}

BufferDumper::~BufferDumper() {
  ALOGV("BufferDumper::%s", __func__);
}

int BufferDumper::dumpBuffer(buffer_handle_t b, uint32_t frameNum) {
  ALOGV("BufferDumper::%s", __func__);

  if (!mEnableDump)
    return 0;

  uint8_t* rgb = nullptr;
  uint32_t stride = 0;
  uint32_t w = 0, h = 0;

  auto& mapper = BufferMapper::getMapper();
  mapper.getBufferSize(b, w, h);
  mapper.lockBuffer(b, rgb, stride);
  if (rgb && w && h) {
    char path[128];
    snprintf(path, 128, "%s/fb-%d.png", kDumpFolder, frameNum);
    ImageData image;
    image.width = w;
    image.height = h;
    image.stride = stride * 4;
    image.bpc = 8;
    image.alpha = true;
    image.data = rgb;
    dumpToPng(path, &image);
  } else {
    ALOGE("Failed to lock front buffer\n");
  }
  mapper.unlockBuffer(b);
  return 0;
}

int BufferDumper::preparePngHeader(FILE* file,
                                   const ImageData* image,
                                   png_structp& png,
                                   png_infop& info) {
  ALOGV("BufferDumper::%s", __func__);

  if (!file || !image)
    return -1;

  png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
  if (!png) {
    ALOGE("Faild to create png_create_write_struct");
    return -1;
  }
  info = png_create_info_struct(png);
  if (!info) {
    ALOGE("Faild to create png_create_info_struct");
    return -1;
  }
  if (setjmp(png_jmpbuf(png))) {
    ALOGE("Error in create png headers");
    return -1;
  }
  png_init_io(png, file);
  if (setjmp(png_jmpbuf(png))) {
    ALOGE("Error while init io");
    return -1;
  }
  int colorType = image->alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
  png_set_IHDR(png, info, image->width, image->height, image->bpc, colorType,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);
  png_write_info(png, info);
  if (setjmp(png_jmpbuf(png))) {
    ALOGE("Failed to write png info");
    return -1;
  }
  return 0;
}

int BufferDumper::fillPngData(const ImageData* image, png_structp& png) {
  ALOGV("BufferDumper::%s", __func__);

  if (!image)
    return -1;

  uint32_t i;
  for (i = 0; i < image->height; i++) {
    png_write_row(png, image->data + i * image->stride);
  }
  if (setjmp(png_jmpbuf(png))) {
    ALOGE("BufferDumper::Failed to write image data");
    return -1;
  }
  png_write_end(png, nullptr);

  return 0;
}

int BufferDumper::dumpToPng(const char* path, const ImageData* image) {
  ALOGV("BufferDumper::%s", __func__);

  if (!path || !image)
    return -1;

  png_structp png;
  png_infop info;
  FILE* fp = nullptr;

  fp = fopen(path, "wb");
  if (!fp) {
    ALOGE("Failed to create PNG file %s", path);
    return -1;
  }

  if (preparePngHeader(fp, image, png, info) < 0) {
    return -1;
  }
  if (fillPngData(image, png) < 0) {
    return -1;
  }

  png_destroy_write_struct(&png, &info);
  fclose(fp);
  return 0;
}
