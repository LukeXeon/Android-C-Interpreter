#pragma once
#include <jni.h>
#include <android/log.h>
#ifdef __cplusplus
#define BEGIN_EXTERN_C extern "C"{
#define END_EXTERN_C }
#define EXTERN_C extern "C"
#else
#define BEGIN_EXTERN_C 
#define END_EXTERN_C 
#define EXTERN_C 
#endif
#define LOGI(...) (__android_log_print(ANDROID_LOG_INFO, "apicoc", __VA_ARGS__))
#define LOGW(...) (__android_log_print(ANDROID_LOG_WARN, "apicoc", __VA_ARGS__))
#define VLOGI(format,ap) (__android_log_vprint(ANDROID_LOG_INFO, "apicoc", format,ap))
#define VLOGW(format,ap) (__android_log_vprint(ANDROID_LOG_WARN, "apicoc", format,ap))
