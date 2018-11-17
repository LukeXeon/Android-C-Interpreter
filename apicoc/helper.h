#pragma once
#include "pch.h"

#include <jni.h>
BEGIN_EXTERN_C
#include <stdio.h>
#include "interpreter.h"

jobject ToJObject(char sig, JNIEnv * env, AnyValue * val);

void ToAnyValue(char sig, JNIEnv * env, Picoc * pc, AnyValue * val, jobject obj);

jlong ToJLong(void * ptr);

JNIEnv * GetCurrentEnv();

jobjectArray NewJObjectArray(JNIEnv * env, jsize length);

jobject OnInvokeHandler(JNIEnv * env, jobject obj, int index, jobjectArray arg);

jobject NewStringWrapper(JNIEnv * env,char * str);

char * GetStringHandler(JNIEnv * env, jobject obj);

bool CheckStringIndex(JNIEnv * env, jobject obj, int index);

void ThrowIllegalEx(JNIEnv * env, const char * message);

Picoc * ToPicocHandler(jlong handler);

void MapingIOToPipes(JNIEnv*env, int c_io[3], jobject j_fd[3]);

void OpenStream(int c_io[3], FILE* streams[3]);

void DumpStdIOToPipes(int c_io[3]);

void InitIO(Picoc*pc, FILE*streams[3]);

END_EXTERN_C