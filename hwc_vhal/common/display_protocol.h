/*
** Copyright 2016 Intel Corporation
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef _H_DISPLAY_PROTOCOL_
#define _H_DISPLAY_PROTOCOL_

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <cutils/native_handle.h>

/* vHAL mode : 0 - client mode (default),  1 - server mode */
#define GRALLOC_PERFORM_VHAL_MODE 0x70000001

#define DD_EVENT_DISPINFO_REQ 0x1000
#define DD_EVENT_DISPINFO_ACK 0x1001
#define DD_EVENT_CREATE_BUFFER 0x1002
#define DD_EVENT_REMOVE_BUFFER 0x1003
#define DD_EVENT_DISPLAY_REQ 0x1004
#define DD_EVENT_DISPLAY_ACK 0x1005
#define DD_EVENT_SERVER_IP_REQ 0x1006
#define DD_EVENT_SERVER_IP_ACK 0x1007
#define DD_EVENT_SERVER_IP_SET 0x1008
#define DD_EVENT_SET_ROTATION 0x1009

#define DD_EVENT_CREATE_LAYER 0x1100
#define DD_EVENT_REMOVE_LAYER 0x1101
#define DD_EVENT_UPDATE_LAYERS 0x1102
#define DD_EVENT_PRESENT_LAYERS_REQ 0x1103
#define DD_EVENT_PRESENT_LAYERS_ACK 0x1104

#define DD_EVENT_DISPPORT_REQ 0x1700
#define DD_EVENT_DISPPORT_ACK 0x1701


// define framebuffer id as the max
#define LAYER_ID_FRAMEBUFFER 0xffffffffffffffff

typedef struct _display_flags {
  union {
    uint32_t value;
    struct {
      uint32_t version : 8;  // 0 - legacy, 1 - support layer
      uint32_t mode : 2;  // 0 - legacy, 1 -layers only, 2 - both fb and layers
    };
  };
} display_flags;

typedef struct _display_info_t {
  unsigned int flags;
  unsigned int width;
  unsigned int height;
  int stride;
  int format;
  float xdpi;
  float ydpi;
  float fps;
  int minSwapInterval;
  int maxSwapInterval;
  int numFramebuffers;
} display_info_t;

typedef struct _buffer_info_t {
  uint64_t bufferId;
  int data[0];  // local handle
} buffer_info_t;

typedef struct _display_event_t {
  unsigned int type;
  unsigned int size;
  unsigned int id;
  unsigned int pad;
} display_event_t;

typedef struct _display_info_event_t {
  display_event_t event;
  display_info_t info;
} display_info_event_t;

typedef struct _buffer_info_event_t {
  display_event_t event;
  buffer_info_t info;
} buffer_info_event_t;

typedef struct _rotation_event_t {
  display_event_t event;
  int rotation;
} rotation_event_t;

typedef struct _create_layer_event_t {
  display_event_t event;
  uint64_t layerId;
} create_layer_event_t;

typedef struct _remove_layer_event_t {
  display_event_t event;
  uint64_t layerId;
} remove_layer_event_t;

typedef struct _rect_t {
  int left;
  int top;
  int right;
  int bottom;
} rect_t;

typedef struct _layer_info_t {
  uint64_t layerId;
  uint32_t type;
  uint32_t stackId;
  uint32_t taskId;
  uint32_t userId;
  uint32_t index;
  rect_t srcCrop;
  rect_t dstFrame;
  uint32_t transform;
  uint32_t z;
  int32_t blendMode;
  float planeAlpha;
  uint32_t color;
  uint32_t changed;
  char name[96];
} layer_info_t;

typedef struct _update_layers_event_t {
  display_event_t event;
  uint32_t numLayers;
  layer_info_t layers[0];
} update_layers_event_t;

typedef struct _layer_buffer_info_t {
  uint64_t layerId;
  uint64_t bufferId;
  int fence;
  uint32_t changed;
} layer_buffer_info_t;

typedef struct _present_layers_req_event_t {
  display_event_t event;
  uint32_t numLayers;
  layer_buffer_info_t layers[0];
} present_layers_req_event_t;

typedef struct _present_layers_ack_event_t {
  display_event_t event;
  uint32_t flags;
  int releaseFence;
  uint32_t numLayers;
  layer_buffer_info_t layers[0];
} present_layers_ack_event_t;

typedef struct _display_port_t {
  unsigned int port;
  unsigned int reserve;
} display_port_t;

typedef struct _display_port_event_t {
  display_event_t event;
  display_port_t dispPort;
} display_port_event_t;

#endif  // _H_DISPLAY_PROTOCOL_
