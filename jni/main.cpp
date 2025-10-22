#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <stddef.h>
#include <thread>
#include <unistd.h>

#define LOG_TAG "Injectra"
#define ILOG(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ELOG(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* REAL_LIB = "libmain_orig.so";
static const char* UNITY_LIB = "libunity.so";

static void* real_handle = nullptr;

using fn_ANativeActivity_onCreate = void(*)(void*, void*, size_t);
static fn_ANativeActivity_onCreate real_ANativeActivity_onCreate = nullptr;

using fn_android_main = void(*)(void*);
static fn_android_main real_android_main = nullptr;

using fn_JNI_OnLoad = jint(*)(JavaVM*, void*);
static fn_JNI_OnLoad real_JNI_OnLoad = nullptr;

static bool ensure_real_loaded() 
{
    if (real_handle) return true;

    real_handle = dlopen(REAL_LIB, RTLD_NOW | RTLD_GLOBAL);
    if (!real_handle) 
    {
        ELOG("Failed to dlopen %s: %s", REAL_LIB, dlerror());
        return false;
    }
    ILOG("Loaded real lib: %s -> %p", REAL_LIB, real_handle);

    real_ANativeActivity_onCreate = (fn_ANativeActivity_onCreate)dlsym(real_handle, "ANativeActivity_onCreate");
    real_android_main = (fn_android_main)dlsym(real_handle, "android_main");
    real_JNI_OnLoad = (fn_JNI_OnLoad)dlsym(real_handle, "JNI_OnLoad");

    ILOG("Symbols: ANativeActivity_onCreate=%p android_main=%p JNI_OnLoad=%p", (void*)real_ANativeActivity_onCreate, (void*)real_android_main, (void*)real_JNI_OnLoad);

    return true;
}

extern "C"
void ANativeActivity_onCreate(void* activity, void* savedState, size_t savedStateSize) 
{
    ILOG("Wrapper: ANativeActivity_onCreate called");

    if (!ensure_real_loaded()) 
    {
        ELOG("Wrapper: cannot load real lib, aborting ANativeActivity_onCreate");
        return;
    }

    if (real_ANativeActivity_onCreate) 
    {
        ILOG("Wrapper: calling real ANativeActivity_onCreate");
        real_ANativeActivity_onCreate(activity, savedState, savedStateSize);
    } 
    else 
    {
        ELOG("Wrapper: real ANativeActivity_onCreate not found");
    }

    void* unity_lib = dlopen(UNITY_LIB, RTLD_NOW | RTLD_GLOBAL);
    if (unity_lib)
        ILOG("Opened libunity.so successfully");
    else
        ELOG("Failed to open %s: %s", UNITY_LIB, dlerror());
}

extern "C"
void android_main(void* state) 
{
    ILOG("Wrapper: android_main called");
    if (!ensure_real_loaded()) 
    {
        ELOG("Wrapper: cannot load real lib, aborting android_main");
        return;
    }
    if (real_android_main) 
    {
        real_android_main(state);
    } 
    else 
    {
        ELOG("Wrapper: real android_main not found");
    }
}

extern "C"
jint JNI_OnLoad(JavaVM* vm, void* reserved) 
{
    ILOG("Wrapper: JNI_OnLoad called");

    if (!ensure_real_loaded()) 
    {
        ELOG("Wrapper: cannot load real lib in JNI_OnLoad");
        return JNI_VERSION_1_6;
    }

    if (real_JNI_OnLoad) 
    {
        ILOG("Wrapper: calling real JNI_OnLoad");
        jint result = real_JNI_OnLoad(vm, reserved);

        std::thread([]() {
            void* unity = nullptr;
            for (int i = 0; i < 100; i++) {
                unity = dlopen("libunity.so", RTLD_NOW | RTLD_GLOBAL);
                if (unity) break;
                usleep(100000); // 100 мс
            }
            if (unity)
                ILOG("Injectra: libunity.so successfully loaded: %p", unity);
            else
                ELOG("Injectra: failed to find libunity.so after waiting");
        }).detach();

        return result;
    }

    ILOG("Opened libunity.so!");

    return JNI_VERSION_1_6;
}

__attribute__((constructor))
static void wrapper_ctor()
{
    ILOG("Wrapper: constructor run");
}