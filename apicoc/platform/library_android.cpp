#include <jni.h>
#include <unordered_map>
using namespace std;
#define  LOG_BUFFER_SIZE (1024)
extern "C"
{
#include "../interpreter.h"
#include "../pch.h"
#include "../extend/runtime.h"
#include "../extend/pool.h"
#include "../helper.h"

	extern int StdioBasePrintf(struct ParseState *Parser, FILE *Stream, char *StrOut, int StrOutLen, char *Format, struct StdVararg *Args);

	void RuntimeSetupFunc(Picoc*)
	{
	}

	void RuntimeInternalCall(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		LOGI("picoc call handler");
		JNIEnv * env = GetCurrentEnv();
		jobject Wrapper = Parser->pc->Wrapper;
		struct Value * ThisArg = (Param + 3)[0];
		if (Wrapper != nullptr&&Param[0]->Val->Pointer == Parser->pc)
		{
			if (env != nullptr && !env->IsSameObject(Wrapper, NULL))
			{
				const char * PramTypeList = (const char*)Param[2]->Val->Pointer;
				jsize Length = strlen(PramTypeList);
				jobjectArray Args = NewJObjectArray(env, Length - 1);
				for (int i = 1; i < Length; i++)
				{
					ThisArg = (struct Value *)((char *)ThisArg + MEM_ALIGN(sizeof(struct Value) + TypeStackSizeValue(ThisArg)));
					jobject item = ToJObject(PramTypeList[i], env, ThisArg->Val);
					env->SetObjectArrayElement(Args, i - 1, item);
					env->DeleteLocalRef(item);
				}
				jobject Result = OnInvokeHandler(env, Wrapper, Param[1]->Val->Integer, Args);
				env->DeleteLocalRef(Args);
				if (env->ExceptionCheck())
				{
					env->ExceptionDescribe();
					env->ExceptionClear();
					ProgramFail(Parser, "throw java exception");
				}
				ToAnyValue(PramTypeList[0], env, Parser->pc, Param[3]->Val, Result);
				env->DeleteLocalRef(Result);
			}
		}
	}

	void  RuntimeTAddr(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		ReturnValue->Val->Integer = taddr(Param[0]->Val->Pointer);
	}

	void  RuntimeTStr(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		ReturnValue->Val->Integer = tstr(Param[0]->Val->Pointer, Param[1]->Val->Integer);
	}

	void RuntimeHeapSize(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		ReturnValue->Val->LongInteger = PoolSize(Parser->pc->Pool);
	}

	void RuntimeInternalCall(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs);

	void RuntimeLogW(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		struct StdVararg PrintfArgs;
		PrintfArgs.Param = Param;
		PrintfArgs.NumArgs = NumArgs - 1;
		char LogBuffer[LOG_BUFFER_SIZE];
		StdioBasePrintf(Parser, NULL, LogBuffer, LOG_BUFFER_SIZE, (char*)Param[0]->Val->Pointer, &PrintfArgs);
		ReturnValue->Val->Integer = __android_log_write(ANDROID_LOG_WARN, "apicoc", LogBuffer);
	}

	void RuntimeLogI(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		struct StdVararg PrintfArgs;
		PrintfArgs.Param = Param;
		PrintfArgs.NumArgs = NumArgs - 1;
		char LogBuffer[LOG_BUFFER_SIZE];
		StdioBasePrintf(Parser, NULL, LogBuffer, LOG_BUFFER_SIZE, (char*)Param[0]->Val->Pointer, &PrintfArgs);
		ReturnValue->Val->Integer = __android_log_write(ANDROID_LOG_INFO, "apicoc", LogBuffer);
	}

	void RuntimeLineno(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		ReturnValue->Val->Integer = Parser->Line;
	}

	/* list of all library functions and their prototypes */
	struct LibraryFunction UnixFunctions[] =
	{
		{ RuntimeLineno,         "int lineno();" },
		{ RuntimeLogW,           "int logw(char *, ...);" },
		{ RuntimeHeapSize,       "long heapsize();" },
		{ RuntimeTAddr,          "bool taddr(void*);" },
		{ RuntimeTStr,           "bool tstr(void*,int);" },
		{ RuntimeLogI,           "int logi(char *, ...);" },
		{ RuntimeInternalCall,   "int __internal_call(void*,int,char *,void *, ...);" },
		//带有“__”双下划线的都是运行时内部使用的代码（地址验证,下标,类型签名,返回值地址,参数列表）
		{ NULL,         NULL }
	};

	void PlatformLibraryInit(Picoc *pc)
	{
		IncludeRegister(pc, "runtime.h", &RuntimeSetupFunc, &UnixFunctions[0], NULL);
	}
}