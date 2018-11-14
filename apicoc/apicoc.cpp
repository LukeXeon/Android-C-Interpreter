#include "apicoc.h"
#include "interpreter.h"
using namespace std;
#define PICOC_STACK_SIZE (128*1024)              /* space for the the stack */

struct Transform
{
	Transform() {}
	Transform(function<jobject(JNIEnv*, union AnyValue*)>&&toJObject,
		function<void(JNIEnv*, Picoc*, union AnyValue*, jobject)>&&setReturn) 
		:toJObject(toJObject), setReturn(setReturn) {}
	function<jobject(JNIEnv*, union AnyValue*)> toJObject;
	function<void(JNIEnv*, Picoc*, union AnyValue*, jobject)> setReturn;
};

struct 
{
	//field
	JavaVM*javaVM;
	jclass objectClass;
	//method
	unordered_map<char, Transform> transforms;
	function<void(JNIEnv*, jobject, int)> setDescriptor;
	function<jobject(JNIEnv*, jobject, int, jobjectArray)> onInvokeHandler;
} Helper;

static Picoc*ToHandler(jlong ptr) 
{
	return (Picoc *)ptr;
}

static jlong ToJLong(void*ptr) 
{
	return (jlong)ptr;
}

static void MapingIOToPipes(JNIEnv*env, int c_io[3], jobject j_fd[3])
{
	int stdin_pipe[2];
	int stdout_pipe[2];
	int stderr_pipe[2];
	pipe(stdin_pipe);
	pipe(stdout_pipe);
	pipe(stderr_pipe);
	c_io[0] = stdin_pipe[0];//r
	c_io[1] = stdout_pipe[1];//w
	c_io[2] = stderr_pipe[1];//w
	Helper.setDescriptor(env, j_fd[0], stdin_pipe[1]);//w
	Helper.setDescriptor(env, j_fd[1], stdout_pipe[0]);//r
	Helper.setDescriptor(env, j_fd[2], stderr_pipe[0]);//r
}

static void OpenStream(int c_io[3], FILE* streams[3])
{
	streams[0] = fdopen(c_io[0], "r");
	streams[1] = fdopen(c_io[1], "w");
	streams[2] = fdopen(c_io[1], "w");
	setvbuf(streams[1], NULL, _IONBF, 0);
	setvbuf(streams[2], NULL, _IONBF, 0);
}

static void DumpStdIOToPipes(int c_io[3])
{
	dup2(c_io[0], STDIN_FILENO);
	dup2(c_io[1], STDOUT_FILENO);
	dup2(c_io[2], STDERR_FILENO);
}

static void InitIO(Picoc*pc, FILE*streams[3])
{
	pc->CStdIn = streams[0];
	pc->CStdOut = streams[1];
	pc->CStdErr = streams[2];
}

static void InitHelperFiled(JNIEnv*env)
{
	Helper.objectClass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
}

static void InitHelperMethod(JNIEnv*env)
{
	jfieldID fdField = env->GetFieldID(env->FindClass("java/io/FileDescriptor"), "descriptor", "I");
	Helper.setDescriptor = [fdField](JNIEnv*env, jobject fdObject, int fd)->void
	{
		env->SetIntField(fdObject, fdField, fd);
	};
	jmethodID invokeMethod = env->GetMethodID(env->FindClass("edu/guet/apicoc/ScriptRuntime"), "onInvoke",
		"(I[Ljava/lang/Object;)Ljava/lang/Object;");
	Helper.onInvokeHandler = [invokeMethod](JNIEnv*env, jobject target, int index, jobjectArray args)->jobject
	{
		return env->CallObjectMethod(target, invokeMethod, index, args);
	};

	jclass Zclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Boolean"));
	jclass Iclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Integer"));
	jclass Jclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Long"));
	jclass Dclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Double"));
	jclass Fclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Float"));
	jclass Cclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Character"));
	jclass Sclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Short"));
	jclass Bclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Byte"));

	jmethodID Zmethod = env->GetStaticMethodID(Zclass, "valueOf", "(Z)Ljava/lang/Boolean;");
	jmethodID Imethod = env->GetStaticMethodID(Iclass, "valueOf", "(I)Ljava/lang/Integer;");
	jmethodID Jmethod = env->GetStaticMethodID(Jclass, "valueOf", "(J)Ljava/lang/Long;");
	jmethodID Dmethod = env->GetStaticMethodID(Dclass, "valueOf", "(D)Ljava/lang/Double;");
	jmethodID Fmethod = env->GetStaticMethodID(Fclass, "valueOf", "(F)Ljava/lang/Float;");
	jmethodID Cmethod = env->GetStaticMethodID(Cclass, "valueOf", "(C)Ljava/lang/Character;");
	jmethodID Smethod = env->GetStaticMethodID(Sclass, "valueOf", "(S)Ljava/lang/Short;");
	jmethodID Bmethod = env->GetStaticMethodID(Bclass, "valueOf", "(B)Ljava/lang/Byte;");

	jmethodID Zmethod2 = env->GetMethodID(Zclass, "booleanValue", "()Z");
	jmethodID Imethod2 = env->GetMethodID(Iclass, "intValue", "()I");
	jmethodID Jmethod2 = env->GetMethodID(Jclass, "longValue", "()J");
	jmethodID Dmethod2 = env->GetMethodID(Dclass, "doubleValue", "()D");
	jmethodID Fmethod2 = env->GetMethodID(Fclass, "floatValue", "()F");
	jmethodID Cmethod2 = env->GetMethodID(Cclass, "charValue", "()C");
	jmethodID Smethod2 = env->GetMethodID(Sclass, "shortValue", "()S");
	jmethodID Bmethod2 = env->GetMethodID(Bclass, "byteValue", "()B");

	Helper.transforms.emplace('Z', Transform([Zclass, Zmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Zclass, Zmethod, (jboolean)val->Integer);
	},
		[Zmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((int*)val->Pointer) = env->CallBooleanMethod(obj, Zmethod2);
	}));
	Helper.transforms.emplace('I', Transform([Iclass, Imethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Iclass, Imethod, (jint)val->Integer);
	},
		[Imethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((int*)val->Pointer) = env->CallIntMethod(obj, Imethod2);
	}));
	Helper.transforms.emplace('J', Transform([Jclass, Jmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Jclass, Jmethod, (jlong)val->LongInteger);
	},
		[Jmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((long*)val->Pointer) = env->CallLongMethod(obj, Jmethod2);
	}));
	Helper.transforms.emplace('D', Transform([Dclass, Dmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Dclass, Dmethod, (jdouble)val->FP);
	},
		[Dmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((double*)val->Pointer) = env->CallDoubleMethod(obj, Dmethod2);
	}));
	Helper.transforms.emplace('F', Transform([Fclass, Fmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Fclass, Fmethod, (jfloat)val->FP);
	},
		[Fmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((double*)val->Pointer) = env->CallFloatMethod(obj, Fmethod2);
	}));
	Helper.transforms.emplace('C', Transform([Cclass, Cmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Cclass, Cmethod, (jchar)val->UnsignedShortInteger);
	},
		[Cmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((unsigned short*)val->Pointer) = env->CallCharMethod(obj, Cmethod2);
	}));
	Helper.transforms.emplace('S', Transform([Sclass, Smethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Sclass, Smethod, (jshort)val->ShortInteger);
	},
		[Smethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((short*)val->Pointer) = env->CallShortMethod(obj, Smethod2);
	}));
	Helper.transforms.emplace('B', Transform([Bclass, Bmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Bclass, Bmethod, (jbyte)val->Character);
	},
		[Bmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((char*)val->Pointer) = env->CallByteMethod(obj, Bmethod2);
	}));
	Helper.transforms.emplace('L', Transform([](JNIEnv* env, union AnyValue* val)->jobject
	{
		return val->Pointer != nullptr ? env->NewStringUTF((const char*)val->Pointer) : nullptr;
	},
		[](JNIEnv* env, Picoc*pc, union AnyValue*val, jobject obj)->void
	{
		jstring text = (jstring)obj;
		if (text != nullptr)
		{
			const char* text_ = env->GetStringUTFChars(text, 0);
			*((char**)val->Pointer) = strcpy((char*)pool_calloc(pc->Pool, 1, sizeof(char[strlen(text_) + 1])), text_);
			env->ReleaseStringUTFChars(text, text_);
		}
	}));
	Helper.transforms.emplace('V', Transform([](JNIEnv* env, union AnyValue* val)->jobject {return nullptr;},
		[](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void {}));

}

EXTERN_C
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void *reserved)
{
	LOGI("apicoc lib load");
	Helper.javaVM = vm;
	JNIEnv*env = nullptr;
	vm->GetEnv((void**)(&env), JNI_VERSION_1_6);
	InitHelperFiled(env);
	InitHelperMethod(env);
	return JNI_VERSION_1_6;
}

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
			const char *c_src = env->GetStringUTFChars(src_string, 0);
			c_src_or_file[index] = strcpy(new char[strlen(c_src) + 1], c_src);
			env->ReleaseStringUTFChars(src_string, c_src);
		}
	}
	if (c_argc != 0)
	{
		c_argv = new char *[c_argc];
		for (int i = 0; i < c_argc; i++)
		{
			jstring arg_string = (jstring)env->GetObjectArrayElement(srcOrFile, i);
			const char * arg = env->GetStringUTFChars(arg_string, 0);
			c_argv[i] = strcpy(new char[strlen(arg) + 1], arg);
			env->ReleaseStringUTFChars(arg_string, arg);
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
		picoc->Pool = pool_create(normal_t);
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
	picoc->Pool = pool_create(manager_t);
	return ToJLong(picoc);
}

EXTERN_C
JNIEXPORT jboolean JNICALL
Java_edu_guet_apicoc_ScriptRuntime_doSomething0(JNIEnv *env, jclass type, jlong handler, jstring source)
{
	LOGI("picoc do something");
	struct ParseState Parser;
	enum ParseResult Ok;
	Picoc *pc = ToHandler(handler);
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
	Picoc *pc = ToHandler(ptr);
	close(fileno(pc->CStdIn));
	close(fileno(pc->CStdOut));
	close(fileno(pc->CStdErr));
	PicocCleanup(pc);
	pool_destroy(pc->Pool);
	delete pc;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptRuntime_registerHandler0(JNIEnv *env, jclass type, jlong ptr, jstring text_)
{
	LOGI("picoc add handler");
	Picoc *pc = ToHandler(ptr);
	const char* text = env->GetStringUTFChars(text_, 0);
	PicocParse(pc, "__internal_call", text, strlen(text) + 1, true, false, false, true);
	env->ReleaseStringUTFChars(text_, text);
}

EXTERN_C
JNIEXPORT jboolean JNICALL
Java_edu_guet_apicoc_ScriptRuntime_containsName0(JNIEnv *env, jclass type, jlong ptr, jstring name_)
{
	struct Value *value;
	Picoc*pc = ToHandler(ptr);
	const char * name = env->GetStringUTFChars(name_, 0);
	jboolean result = TableGet(&pc->GlobalTable, name, &value, NULL, NULL, NULL);
	env->ReleaseStringUTFChars(name_, name);
	return result;
}

EXTERN_C
JNIEXPORT void JNICALL 
__InternalCall(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	LOGI("picoc call handler");
	JNIEnv * env = nullptr;
	jobject Wrapper = Parser->pc->Wrapper;
	struct Value * ThisArg = (Param + 3)[0];
	if (Wrapper != nullptr&&Param[0]->Val->Pointer == Parser->pc)
	{
		Helper.javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
		if (env != nullptr && !env->IsSameObject(Wrapper, NULL))
		{
			const char * PramTypeList = (const char*)Param[2]->Val->Pointer;
			jsize Length = strlen(PramTypeList);
			jobjectArray Args = env->NewObjectArray(Length - 1, Helper.objectClass, NULL);
			for (int i = 1; i < Length; i++)
			{
				ThisArg = (struct Value *)((char *)ThisArg + MEM_ALIGN(sizeof(struct Value) + TypeStackSizeValue(ThisArg)));
				jobject item = Helper.transforms[PramTypeList[i]].toJObject(env, ThisArg->Val);
				env->SetObjectArrayElement(Args, i - 1, item);
				env->DeleteLocalRef(item);
			}
			jobject Result = Helper.onInvokeHandler(env, Wrapper, Param[1]->Val->Integer, Args);
			env->DeleteLocalRef(Args);
			Helper.transforms[PramTypeList[0]].setReturn(env, Parser->pc, Param[3]->Val, Result);
			env->DeleteLocalRef(Result);
		}
	}
}