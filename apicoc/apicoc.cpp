#include "apicoc.h"
#include "interpreter.h"
using namespace std;
#define PICOC_STACK_SIZE (128*1024)              /* space for the the stack */

static struct {
	JavaVM*javaVM;
	jfieldID descriptorField;
} caches;

static void SetFd(JNIEnv *env, jobject fileDescriptor, int value) {
	env->SetIntField(fileDescriptor, caches.descriptorField, value);
}

static Picoc*ToPtr(jlong ptr) {
	return (Picoc *)ptr;
}

static jlong ToLong(Picoc*ptr) {
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
	SetFd(env, j_fd[0], stdin_pipe[1]);
	SetFd(env, j_fd[1], stdout_pipe[0]);
	SetFd(env, j_fd[2], stderr_pipe[0]);
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

EXTERN_C
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void *reserved)
{
	LOGI("%s", "apicoc lib load");
	caches.javaVM = vm;
	JNIEnv*env = nullptr;
	vm->GetEnv((void**)(&env), JNI_VERSION_1_6);
	caches.descriptorField =
		env->GetFieldID(env->FindClass("java/io/FileDescriptor"), "descriptor", "I");
	return JNI_VERSION_1_6;
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_ScriptRuntime_createSub0(JNIEnv *env, jclass type, jobjectArray srcNames,
	jobjectArray srcOrFile, jobjectArray args,
	jobject stdinFd, jobject stdoutFd, jobject stderrFd) {
	int c_fd[3] = { 0 };
	int child_pid = 0;
	int c_length = 0;
	char **c_src_names = nullptr;
	char **c_src_or_file = nullptr;
	int c_argc = env->GetArrayLength(args);
	char ** c_argv = nullptr;
	jobject j_fd[3] = { stdinFd,stdoutFd,stderrFd };
	MapingIOToPipes(env, c_fd, j_fd);
	c_length = env->GetArrayLength(srcOrFile);
	if (c_length != 0)
	{
		function<void(int)> action = [&env, &c_src_or_file, &srcOrFile](int index)->void
		{
			jstring src_string = (jstring)env->GetObjectArrayElement(srcOrFile, index);
			const char *c_src = env->GetStringUTFChars(src_string, 0);
			c_src_or_file[index] = strcpy(new char[strlen(c_src) + 1], c_src);
			env->ReleaseStringUTFChars(src_string, c_src);
		};
		if (srcNames != nullptr)
		{
			c_src_names = new char*[c_length];
			action = [action, &c_src_names, &env, &srcNames](int index)->void
			{
				jstring name_string = (jstring)env->GetObjectArrayElement(srcNames, index);
				const char *c_name = env->GetStringUTFChars(name_string, 0);
				c_src_names[index] = strcpy(new char[strlen(c_name) + 1], c_name);
				env->ReleaseStringUTFChars(name_string, c_name);
				action(index);
			};
		}
		c_src_or_file = new char*[c_length];
		for (int i = 0; i < c_length; i++)
		{
			action(i);
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
			function<void(int)> action1 = [&c_src_names](int index)->void
			{
				delete c_src_names[index];
			};
			function<void()> action2 = [&c_src_or_file]()->void
			{ 
				delete c_src_or_file;
			};
			if (c_src_names != nullptr)
			{
				action1 = [action1, &c_src_or_file](int index)->void
				{
					delete c_src_or_file[index];
					action1(index);
				};
				action2 = [action2, &c_src_names]()->void
				{
					delete c_src_names;
					action2();
				};
			}
			for (int i = 0; i < c_length; i++)
			{
				action1(i);
			}
			action2();
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
		PicocInitialise(picoc, getenv("STACKSIZE") ? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE);
		PicocIncludeAllSystemHeaders(picoc);
		DumpStdIOToPipes(c_fd);
		OpenStream(c_fd, c_streams);
		InitIO(picoc, c_streams);
		if (c_length != 0)
		{
			if (c_src_names != nullptr)
			{
				for (int i = 0; i < c_length; i++)
				{
					PicocParse(picoc, c_src_names[i],
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
		picoc->JVM = nullptr;
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
	return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptRuntime_killSub0(JNIEnv *env, jclass type, jint pid) {
	kill(pid, SIGKILL);
}

EXTERN_C
JNIEXPORT jlong JNICALL
Java_edu_guet_apicoc_ScriptRuntime_init0(JNIEnv *env, jclass type, jobject stdinFd, jobject stdoutFd,
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
	picoc->JVM = caches.javaVM;
	picoc->Pool = pool_create(manager_t);
	return ToLong(picoc);
}


EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptRuntime_doSomething0(JNIEnv *env, jclass type, jlong handler, jstring source)
{
	LOGI("picoc do something");
	Picoc *pc = ToPtr(handler);
	const char* j_src = env->GetStringUTFChars(source, 0);
	int c_src_length = strlen(j_src) + 1;
	char * c_src = strcpy(new char[c_src_length], j_src);
	PicocParse(pc, "scripting", c_src, c_src_length, true, true, true, true);
	delete c_src;
	env->ReleaseStringUTFChars(source, j_src);
	pool_release(pc->Pool);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_ScriptRuntime_close0(JNIEnv *env, jclass type, jlong ptr) {
	if (ptr == 0) {
		return;
	}
	LOGI("picoc close");
	Picoc *pc = ToPtr(ptr);
	close(fileno(pc->CStdIn));
	close(fileno(pc->CStdOut));
	close(fileno(pc->CStdErr));
	PicocCleanup(pc);
	pool_destroy(pc->Pool);
	delete pc;
}
