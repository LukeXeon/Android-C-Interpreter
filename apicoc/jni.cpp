#include "pch.h"
#include "helper.h"
BEGIN_EXTERN_C
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "interpreter.h"
#include "extend/pool.h"
#include "extend/runtime.h"
#include "picoc.h"
END_EXTERN_C
#define PICOC_STACK_SIZE (128*1024)              /* space for the the stack */

//runtime
EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_ScriptRuntime_createSub0(JNIEnv *env, jclass type, jint mode,
	jobjectArray srcOrFile, jobjectArray args,
	jobject stdinFd, jobject stdoutFd, jobject stderrFd) {
	int c_fd[3] = { 0 };
	int child_pid = 0;
	int c_length = 0;
	char **c_src_or_file = nullptr;
	int c_argc = env->GetArrayLength(args);
	char ** c_argv = nullptr;
	jobject j_fd[3] = { stdinFd,stdoutFd,stderrFd };
	MapingIOToPipes(env, c_fd, j_fd);
	c_length = env->GetArrayLength(srcOrFile);
	if (c_length != 0)
	{
		c_src_or_file = new char*[c_length];
		for (int index = 0; index < c_length; index++)
		{
			jstring src_string = (jstring)env->GetObjectArrayElement(srcOrFile, index);
			int length = env->GetStringUTFLength(src_string);
			c_src_or_file[index] = new char[length + 1];
			env->GetStringUTFRegion(src_string, 0, length, c_src_or_file[index]);
		}
	}
	if (c_argc != 0)
	{
		c_argv = new char *[c_argc];
		for (int index = 0; index < c_argc; index++)
		{
			jstring arg_string = (jstring)env->GetObjectArrayElement(args, index);
			int length = env->GetStringUTFLength(arg_string);
			c_argv[index] = new char[length + 1];
			env->GetStringUTFRegion(arg_string, 0, length, c_argv[index]);
		}
	}
	while ((child_pid = fork()) == -1);
	if (child_pid != 0)
	{
		if (c_length != 0)
		{
			for (int index = 0; index < c_length; index++)
			{
				delete c_src_or_file[index];
			}
			delete c_src_or_file;
		}
		if (c_argc != 0)
		{
			for (int i = 0; i < c_argc; i++)
			{
				delete c_argv[i];
			}
			delete c_argv;
		}
		return child_pid;
	}
	else
	{
		FILE* c_streams[3] = { nullptr };
		Picoc *picoc = new Picoc();
		PicocInitialise(picoc, getenv("STACKSIZE")
			? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE);
		PicocIncludeAllSystemHeaders(picoc);
		DumpStdIOToPipes(c_fd);
		OpenStream(c_fd, c_streams);
		InitIO(picoc, c_streams);
		if (c_length != 0)
		{
			if (mode == 0)
			{
				for (int i = 0; i < c_length; i++)
				{
					PicocParse(picoc, "scripting",
						c_src_or_file[i],
						strlen(c_src_or_file[i]),
						true, false, true, true);
				}
			}
			else
			{
				for (int i = 0; i < c_length; i++)
				{
					PicocPlatformScanFile(picoc, c_src_or_file[i]);
				}
			}
		}
		picoc->Pool = PoolCreate(normal_t);
		picoc->Wrapper = nullptr;
		if (PicocPlatformSetExitPoint(picoc))
		{
			PicocCleanup(picoc);
			exit(picoc->PicocExitValue);
		}
		else
		{
			PicocCallMain(picoc, c_argc, c_argv);
			exit(picoc->PicocExitValue);
			//不用做清理内存操作
		}

	}
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_ScriptRuntime_waitSub0(JNIEnv *env, jclass type, jint pid) {
	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptRuntime_killSub0(JNIEnv *env, jclass type, jint pid) {
	kill(pid, SIGKILL);
}

EXTERN_C
JNIEXPORT jlong JNICALL
Java_edu_guet_apicoc_ScriptRuntime_init0(JNIEnv *env, jobject obj, jobject stdinFd, jobject stdoutFd,
	jobject stderrFd) {
	LOGI("picoc init");
	int c_fd[3] = { 0 };
	FILE*c_streams[3] = { nullptr };
	jobject j_fd[3] = { stdinFd,stdoutFd,stderrFd };
	Picoc *picoc = new Picoc();
	PicocInitialise(picoc, getenv("STACKSIZE") ? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE);
	PicocIncludeAllSystemHeaders(picoc);
	MapingIOToPipes(env, c_fd, j_fd);
	OpenStream(c_fd, c_streams);
	InitIO(picoc, c_streams);
	picoc->Wrapper = env->NewWeakGlobalRef(obj);
	picoc->Pool = PoolCreate(manager_t);
	return ToJLong(picoc);
}

EXTERN_C
JNIEXPORT jboolean JNICALL
Java_edu_guet_apicoc_ScriptRuntime_doSomething0(JNIEnv *env, jclass type, jlong handler, jstring source)
{
	LOGI("picoc do something");
	struct ParseState Parser;
	enum ParseResult Ok;
	Picoc *pc = ToPicocHandler(handler);
	char *RegFileName = TableStrRegister(pc, "scripting");
	const char* j_src = env->GetStringUTFChars(source, 0);
	int j_src_length = strlen(j_src) + 1;
	void *Tokens = LexAnalyse(pc, RegFileName, j_src, j_src_length, NULL);
	if (PicocPlatformSetExitPoint(pc))
	{
		HeapFreeMem(pc, Tokens);
		env->ReleaseStringUTFChars(source, j_src);
		return JNI_FALSE;
	}
	LexInitParser(&Parser, pc, j_src, Tokens, RegFileName, true, true);
	do {
		Ok = ParseStatement(&Parser, TRUE);
	} while (Ok == ParseResultOk);
	HeapFreeMem(pc, Tokens);
	env->ReleaseStringUTFChars(source, j_src);
	return Ok != ParseResultError;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptRuntime_close0(JNIEnv *env, jclass type, jlong ptr)
{
	LOGI("picoc close");
	Picoc *pc = ToPicocHandler(ptr);
	PoolDestroy(pc->Pool);
	close(fileno(pc->CStdIn));
	close(fileno(pc->CStdOut));
	close(fileno(pc->CStdErr));
	PicocCleanup(pc);
	delete pc;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptRuntime_registerHandler0(JNIEnv *env, jclass type, jlong ptr, jstring text_)
{
	LOGI("picoc add handler");
	Picoc *pc = ToPicocHandler(ptr);
	const char* text = env->GetStringUTFChars(text_, 0);
	PicocParse(pc, "__internal_call", text, strlen(text) + 1, true, false, false, true);
	env->ReleaseStringUTFChars(text_, text);
}

EXTERN_C
JNIEXPORT jboolean JNICALL
Java_edu_guet_apicoc_ScriptRuntime_containsName0(JNIEnv *env, jclass type, jlong ptr, jstring name_)
{
	struct Value *value;
	Picoc*pc = ToPicocHandler(ptr);
	const char * name = env->GetStringUTFChars(name_, 0);
	jboolean result = TableGet(&pc->GlobalTable, name, &value, NULL, NULL, NULL);
	env->ReleaseStringUTFChars(name_, name);
	return result;
}


EXTERN_C
JNIEXPORT jobject JNICALL
Java_edu_guet_apicoc_ScriptRuntime_allocString0(JNIEnv *env, jclass type, jlong ptr, int cap)
{
	Picoc*pc = ToPicocHandler(ptr);
	char*buffer = (char*)PoolCalloc(pc->Pool, cap + 1, sizeof(char));
	buffer[cap] = '\0';
	return NewStringWrapper(env, buffer);
}

//string

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_ScriptingString_length(JNIEnv *env, jobject obj)
{
	char * handler = GetStringHandler(env, obj);
	if (handler == nullptr)
	{
		return 0;
	}
	if (!taddr(handler))
	{
		ThrowIllegalEx(env, "eorrr pointer");
		return 0;
	}
	return strlen(handler);
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_ScriptingString_compareTo(JNIEnv *env, jobject obj, jobject obj2)
{
	char*lref = GetStringHandler(env, obj);
	char*rref = GetStringHandler(env, obj2);
	if (lref == nullptr&&rref == nullptr)
	{
		return 0;
	}
	else if ((lref != nullptr&&rref == nullptr) || (lref == nullptr&&rref != nullptr))
	{
		return -1;
	}
	else if (!taddr(lref) || !taddr(rref))
	{
		return -1;
	}
	else
	{
		return strcmp(lref, rref);
	}
}

EXTERN_C
JNIEXPORT jstring JNICALL
Java_edu_guet_apicoc_ScriptingString_toString(JNIEnv *env, jobject obj)
{
	char * handler = GetStringHandler(env, obj);
	if (handler == nullptr)
	{
		return env->NewStringUTF("");
	}
	else if (!taddr(handler))
	{
		ThrowIllegalEx(env, "eorrr pointer");
		return nullptr;
	}
	return env->NewStringUTF(handler);
}

EXTERN_C
JNIEXPORT jbyte JNICALL
Java_edu_guet_apicoc_ScriptingString_charAt(JNIEnv *env, jobject obj,jint index)
{
	char * handler = GetStringHandler(env, obj);
	if (!taddr(handler))
	{
		ThrowIllegalEx(env, "eorrr pointer");
		return 0;
	}
	if (CheckStringIndex(env, obj, index))
	{
		return handler[index];
	}
	return 0;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptingString_setCharAt(JNIEnv *env, jobject obj, jbyte ch, jint index)
{
	char * handler = GetStringHandler(env, obj);
	if (!taddr(handler))
	{
		ThrowIllegalEx(env, "eorrr pointer");
		return;
	}
	if (CheckStringIndex(env, obj, index))
	{
		handler[index] = ch;
	}
}

EXTERN_C
JNIEXPORT jboolean JNICALL
Java_edu_guet_apicoc_ScriptingString_isAlive(JNIEnv *env, jobject obj)
{
	char * handler = GetStringHandler(env, obj);
	if (handler == nullptr)
	{
		return true;
	}
	return taddr(handler);
}