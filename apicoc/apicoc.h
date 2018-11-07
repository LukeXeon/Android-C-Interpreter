#pragma once
#define BINGE_EXTERN_C extern "C"{
#define END_EXTERN_C }
#define EXTERN_C extern "C"

#include <pthread.h>
#include <jni.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <android/log.h>
#include <map>
#include <mutex>
BINGE_EXTERN_C
#include "picoc.h"
END_EXTERN_C
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "apicoc", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "apicoc", __VA_ARGS__))
using namespace std;