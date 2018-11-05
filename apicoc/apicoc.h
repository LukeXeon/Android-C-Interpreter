#pragma once
#include "pch.h"
BINGE_EXTERN_C
#include "picoc.h"
END_EXTERN_C
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "apicoc", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "apicoc", __VA_ARGS__))
