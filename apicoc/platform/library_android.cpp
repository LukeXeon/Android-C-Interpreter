#include <jni.h>
#include <unordered_map>
using namespace std;
#define  LOG_BUFFER_SIZE (1024)
extern "C"
{
#include "../interpreter.h"
#include "../pch.h"
#include "../print.h"

	void RuntimeSetupFunc(Picoc*pc)
	{

	}

	void RuntimeHeapSize(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs);

	void __InternalCall(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs);

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
		{ RuntimeLineno,    "int lineno();" },
		{ RuntimeLogW,      "int logw(char *, ...);" },
		{ RuntimeHeapSize,  "long heapsize();" },
		{ RuntimeLogI,      "int logi(char *, ...);" },
		{ __InternalCall,   "int __internal_call(void*,int,char *,void *, ...);" },
		//带有“__”双下划线的都是运行时内部使用的代码（地址验证,下标,类型签名,返回值地址,参数列表）
		{ NULL,         NULL }
	};

	void PlatformLibraryInit(Picoc *pc)
	{
		IncludeRegister(pc, "runtime.h", &RuntimeSetupFunc, &UnixFunctions[0], NULL);
	}
}