#include "apicoc.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "apicoc", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "apicoc", __VA_ARGS__))


static Picoc*Cast(jlong ptr)
{
	return (Picoc*)ptr;
}

static jlong ToLong(Picoc*ptr)
{
	return (jlong)ptr;
}

EXTERN_C
JNIEXPORT jlong JNICALL
Java_edu_guet_picoc_Picoc_nativeInit(JNIEnv *env, jclass type, jint stackSize)
{
	LOGI("picoc init");
	

	Picoc*picoc = new Picoc();
	PicocInitialise(picoc, stackSize);
	PicocIncludeAllSystemHeaders(picoc);
	return ToLong(picoc);
}
EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_picoc_Picoc_nativeParse(JNIEnv *env, jclass type, jlong ptr, jstring source_)
{
	const char *source = env->GetStringUTFChars(source_, 0);
	PicocParse(Cast(ptr), nullptr,
		env->GetStringUTFChars(source_, 0),
		env->GetStringLength(source_),
		true, false, true, true);
	env->ReleaseStringUTFChars(source_, source);
}
EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_picoc_Picoc_nativeCleanup(JNIEnv *env, jclass type, jlong ptr)
{
	LOGI("picoc cleanup");
	Picoc*picoc = Cast(ptr);
	PicocCleanup(picoc);
	delete picoc;
}
EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_picoc_Picoc_nativeCallMain(JNIEnv *env, jclass type, jlong ptr, jobjectArray args)
{
	Picoc*picoc = Cast(ptr);
	if (PicocPlatformSetExitPoint(picoc))
	{
		PicocCleanup(picoc);
		return picoc->PicocExitValue;
	}
	jsize size = env->GetArrayLength(args);
	char**cargs = new char*[size];
	for (jsize i = 0; i < size; i++)
	{
		cargs[i] = (char*)env->GetStringUTFChars((jstring)env->GetObjectArrayElement(args, i), JNI_FALSE);
	}
	PicocCallMain(Cast(ptr), size, cargs);
	for (jsize i = 0; i < size; i++)
	{
		env->ReleaseStringUTFChars((jstring)env->GetObjectArrayElement(args, i), cargs[i]);
		cargs[i] = nullptr;
	}
	delete cargs;
	LOGI("picoc call main");
	return picoc->PicocExitValue;
}
EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_picoc_Picoc_nativeAddFile(JNIEnv *env, jclass type, jlong ptr, jstring path_)
{
	const char *path = env->GetStringUTFChars(path_, 0);
	PicocPlatformScanFile(Cast(ptr), path);
	env->ReleaseStringUTFChars(path_, path);
}

