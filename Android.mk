#
# Copyright 2016 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

TARGET_USE_HWCOMPOSER_VHAL := true

ifeq ($(TARGET_USE_HWCOMPOSER_VHAL), true)

LOCAL_PATH := $(call my-dir)

#####################lib###########################
include $(CLEAR_VARS)

TARGET_USES_HWC2 := true

# HWC VNC is only used for internal test, please don't enable it for external release
ENABLE_HWC_VNC := true
ENABLE_HWC_VNC_TEST := true
#ENABLE_LAYER_DUMP := true
#VIDEO_STREAMING_OPT := true

LOCAL_CFLAGS := -g -DLOG_TAG=\"hwc_vhal\" -g -Wno-missing-field-initializers -Wno-unused-parameter
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES
LOCAL_CPPFLAGS := -g -std=c++17 -Wall -Werror -Wno-unused-parameter
LOCAL_LDFLAGS :=  -g

ifeq ($(TARGET_USES_HWC2), false)
LOCAL_SRC_FILES := \
        common/RemoteDisplay.cpp \
        common/RemoteDisplayMgr.cpp \
        hwc1/Hwc1Device.cpp \
        hwc1/Hwc1Display.cpp \

else
LOCAL_CPPFLAGS += \
	-DHWC2_INCLUDE_STRINGIFICATION \
	-DHWC2_USE_CPP11 \
        -DSUPPORT_HWC_2_0 \

ifneq ($(filter 8, $(PLATFORM_VERSION)),)
LOCAL_CPPFLAGS += \
        -DSUPPORT_HWC_2_1
endif
ifneq ($(filter 9, $(PLATFORM_VERSION)),)
LOCAL_CPPFLAGS += \
        -DSUPPORT_HWC_2_1 \
        -DSUPPORT_HWC_2_2
endif
ifneq ($(filter 10 11, $(PLATFORM_VERSION)),)
LOCAL_CPPFLAGS += \
        -DSUPPORT_HWC_2_1 \
        -DSUPPORT_HWC_2_2 \
        -DSUPPORT_HWC_2_3
endif

ifeq ($(VIDEO_STREAMING_OPT),true)
LOCAL_CPPFLAGS += -DVIDEO_STREAMING_OPT
endif

LOCAL_SRC_FILES := \
        common/RemoteDisplay.cpp \
        common/RemoteDisplayMgr.cpp \
        common/LocalDisplay.cpp \
        common/BufferMapper.cpp \
        renderer/RenderThread.cpp \
        renderer/BufferTexture.cpp \
        renderer/ShaderProgram.cpp \
        renderer/VisibleBoundDetect.cpp \
        renderer/FastBufferDump.cpp \
        renderer/AlphaVideo.cpp \
        hwc2/Hwc2Device.cpp \
        hwc2/Hwc2Display.cpp \
        hwc2/Hwc2Layer.cpp \

LOCAL_STATIC_LIBRARIES += libpng libz

endif

ifeq ($(ENABLE_LAYER_DUMP), true)
LOCAL_SRC_FILES += \
        common/BufferDumper.cpp \

LOCAL_CPPFLAGS += \
        -DENABLE_LAYER_DUMP=1

LOCAL_STATIC_LIBRARIES += libpng libz

endif

ifeq ($(ENABLE_HWC_VNC), true)
LOCAL_SRC_FILES += \
        vnc/VncDisplay.cpp \
        vnc/DirectInput.cpp \
        vnc/Keymap.cpp \

LOCAL_CPPFLAGS += \
        -DENABLE_HWC_VNC=1

LOCAL_C_INCLUDES += \
        external/libvncserver \
        external/zlib \
        $(LOCAL_PATH)/vnc \

LOCAL_STATIC_LIBRARIES := libvncserver libz libpng libjpeg libssl
endif

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH) \
        $(LOCAL_PATH)/common \
        $(LOCAL_PATH)/renderer \
        system/core/libsync/include \

LOCAL_SHARED_LIBRARIES := \
        liblog \
        libcutils \
        libutils \
        libhardware \
        libsync \
        libui \
        libEGL \
        libGLESv3 \
        libcrypto

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := hwcomposer.intel_sw
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
include $(BUILD_SHARED_LIBRARY)

ifeq ($(ENABLE_HWC_VNC_TEST), true)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        common/BufferMapper.cpp \
        vnc/VncDisplay.cpp \
        vnc/DirectInput.cpp \
        vnc/Keymap.cpp \
        vnc/VncDisplayTest.cpp \

LOCAL_CPPFLAGS += \
        -DENABLE_HWC_VNC=1

LOCAL_C_INCLUDES += \
        external/libvncserver \
        external/zlib \
        $(LOCAL_PATH)/common \
        $(LOCAL_PATH)/vnc \

LOCAL_SHARED_LIBRARIES := \
        liblog \
        libcutils \
        libutils \
        libhardware \
        libcrypto

LOCAL_STATIC_LIBRARIES := \
        libvncserver \
        libz \
        libpng \
        libjpeg \
        libssl \

LOCAL_MODULE:= vncdisplay-test

include $(BUILD_EXECUTABLE)
endif

endif
