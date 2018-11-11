#pragma once
#include <jni.h>
#include <android/log.h>
#ifdef __cplusplus
#define BINGE_EXTERN_C extern "C"{
#define END_EXTERN_C }
#define EXTERN_C extern "C"
#else
#define BINGE_EXTERN_C 
#define END_EXTERN_C 
#define EXTERN_C 
#endif
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "apicoc", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "apicoc", __VA_ARGS__))