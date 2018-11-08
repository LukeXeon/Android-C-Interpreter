#include "apicoc.h"
#include "interpreter.h"

#define PICOC_STACK_SIZE (128*1024)              /* space for the the stack */

static struct {
	JavaVM*javaVM;
	jfieldID descriptorField;
	jclass accessExClass;
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
	caches.accessExClass =
		(jclass)env->NewGlobalRef(env->FindClass("java/lang/IllegalAccessException"));
	return JNI_VERSION_1_6;
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_Interpreter_createSub0(JNIEnv *env, jclass type, jobjectArray srcNames,
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
		if (srcNames != nullptr)
		{
			c_src_names = new char*[c_length];
		}
		c_src_or_file = new char*[c_length];
		for (int i = 0; i < c_length; i++)
		{
			if (c_src_names != nullptr)
			{
				jstring name_string = (jstring)env->GetObjectArrayElement(srcNames, i);
				const char *c_name = env->GetStringUTFChars(name_string, 0);
				c_src_names[i] = strcpy(new char[strlen(c_name) + 1], c_name);
				env->ReleaseStringUTFChars(name_string, c_name);
			}
			jstring src_string = (jstring)env->GetObjectArrayElement(srcOrFile, i);
			const char *c_src = env->GetStringUTFChars(src_string, 0);
			c_src_or_file[i] = strcpy(new char[strlen(c_src) + 1], c_src);
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
			for (int i = 0; i < c_length; i++)
			{
				if (c_src_names != nullptr)
				{
					delete c_src_names[i];
				}
				delete c_src_or_file[i];
			}
			if (c_src_names != nullptr)
			{
				delete c_src_names;
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
		picoc->pool = pool_create(normal_t);
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
Java_edu_guet_apicoc_Interpreter_waitSub0(JNIEnv *env, jclass type, jint pid) {
	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_killSub0(JNIEnv *env, jclass type, jint pid) {
	kill(pid, SIGKILL);
}

EXTERN_C
JNIEXPORT jlong JNICALL
Java_edu_guet_apicoc_Interpreter_init0(JNIEnv *env, jclass type, jobject stdinFd, jobject stdoutFd,
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
	return ToLong(picoc);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_addSource0(JNIEnv *env, jclass type, jlong ptr, jstring ramdonName_, jstring source_) {
	LOGI("add source");
	const char *ramdonName = env->GetStringUTFChars(ramdonName_, 0);
	const char *source = env->GetStringUTFChars(source_, 0);
	PicocParse(ToPtr(ptr), ramdonName,
		env->GetStringUTFChars(source_, 0),
		strlen(source),
		true, false, true, true);
	env->ReleaseStringUTFChars(ramdonName_, ramdonName);
	env->ReleaseStringUTFChars(source_, source);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_cleanup0(JNIEnv *env, jclass type, jlong ptr) {
	if (ptr == 0) {
		return;
	}
	LOGI("picoc cleanup");
	Picoc *picoc = ToPtr(ptr);
	PicocCleanup(picoc);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_close0(JNIEnv *env, jclass type, jlong ptr) {
	if (ptr == 0) {
		return;
	}
	LOGI("picoc close");
	Picoc *pc = ToPtr(ptr);
	close(fileno(pc->CStdIn));
	close(fileno(pc->CStdOut));
	close(fileno(pc->CStdErr));
	Java_edu_guet_apicoc_Interpreter_cleanup0(env, type, ptr);
	delete pc;
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_Interpreter_callMain0(JNIEnv *env, jclass type, jlong ptr, jobjectArray args) {
	LOGI("picoc call main");
	Picoc *picoc = ToPtr(ptr);
	jsize size = env->GetArrayLength(args);
	char **cargs = new char *[size];
	for (jsize i = 0; i < size; i++) {
		jstring jstr = (jstring)env->GetObjectArrayElement(args, i);
		const char * source = env->GetStringUTFChars(jstr, 0);
		cargs[i] = strcpy(new char[strlen(source) + 1], source);
		env->ReleaseStringUTFChars(jstr, source);
	}
	picoc->pool = pool_create(manager_t);
	if (PicocPlatformSetExitPoint(picoc))
	{
		PicocCleanup(picoc);
		return picoc->PicocExitValue;
	}
	else
	{
		PicocCallMain(picoc, size, cargs);
	}
	for (jsize i = 0; i < size; i++) {
		delete cargs[i];
	}
	pool_destroy(picoc->pool);
	delete cargs;
	return picoc->PicocExitValue;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_addFile0(JNIEnv *env, jclass type, jlong ptr, jstring path_) {
	LOGI("add File");
	const char *path = env->GetStringUTFChars(path_, 0);
	PicocPlatformScanFile(ToPtr(ptr), path);
	env->ReleaseStringUTFChars(path_, path);
}