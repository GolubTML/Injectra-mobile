LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := injectra
LOCAL_SRC_FILES := main.cpp
LOCAL_CPPFLAGS  := -DTEST -fPIC -std=c++17
LOCAL_LDLIBS    := -llog -ldl

include $(BUILD_SHARED_LIBRARY)
