package edu.guet.apicoc;

import android.support.annotation.WorkerThread;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.regex.Pattern;


/**
 * Created by Mr.小世界 on 2018/10/31.
 */

@SuppressWarnings("JniMissingFunction")
public final class ScriptRuntime
        implements AutoCloseable,
        ScriptingIO
{
    private final static String TAG = "ScriptRuntime";
    private final static Map<Class<?>, String> SCRIPT_BASIC_TYPE
            = new HashMap<Class<?>, String>()
    {
        {
            put(String.class, "char *");
            put(Void.class, "void");
            put(void.class, "void");
            put(Integer.class, "int");
            put(int.class, "int");
            put(Long.class, "long");
            put(long.class, "long");
            put(Double.class, "double");
            put(double.class, "double");
            put(Float.class, "float");
            put(float.class, "float");
            put(Character.class, "char");
            put(char.class, "char");
            put(Short.class, "short");
            put(short.class, "short");
        }
    };

    private Map<String, MethodHandler> handlers = new HashMap<>();

    private PipedInputStream stderr;
    private PipedInputStream stdout;
    private PipedOutputStream stdin;
    private long handler;

    static
    {
        System.loadLibrary("apicoc");
    }

    public ScriptRuntime()
    {
        AccessController.doPrivileged(new PrivilegedAction<Void>()
        {
            @Override
            public Void run()
            {
                FileDescriptor stdinFd = new FileDescriptor();
                FileDescriptor stdoutFd = new FileDescriptor();
                FileDescriptor stderrFd = new FileDescriptor();
                handler = init0(stdinFd, stdoutFd, stderrFd);
                stdin = new PipedOutputStream(stdinFd);
                stdout = new PipedInputStream(stdoutFd);
                stderr = new PipedInputStream(stderrFd);
                return null;
            }
        });
    }

    @Override
    public OutputStream getOutputStream()
    {
        return stdin;
    }

    @Override
    public InputStream getInputStream()
    {
        return stdout;
    }

    @Override
    public InputStream getErrorStream()
    {
        return stderr;
    }

    public static ScriptingIOProcess exec(List<File> files, List<String> args)
    {
        files = Collections.unmodifiableList(files == null ? Collections.<File>emptyList() : files);
        args = Collections.unmodifiableList(args == null ? Collections.<String>emptyList() : args);
        String[] filePaths = new String[files.size()];
        String[] callArgs = new String[args.size()];
        for (int i = 0; i < files.size(); i++)
        {
            filePaths[i] = Objects.requireNonNull(files.get(i)).getAbsolutePath();
        }
        for (int i = 0; i < args.size(); i++)
        {
            callArgs[i] = Objects.requireNonNull(args.get(i));
        }
        return new ScriptingProcess(null, filePaths, callArgs);
    }

    public static ScriptingIOProcess exec(Map<String, String> scripts, List<String> args)
    {
        scripts = Collections.unmodifiableMap(scripts == null ? Collections.<String, String>emptyMap() : scripts);
        args = Collections.unmodifiableList(args == null ? Collections.<String>emptyList() : args);
        String[] callArgs = new String[args.size()];
        String[] names = new String[scripts.size()];
        String[] srcs = new String[scripts.size()];
        int index = 0;
        for (Map.Entry<String, String> entry : scripts.entrySet())
        {
            names[index] = Objects.requireNonNull(entry.getKey());
            srcs[index] = Objects.requireNonNull(entry.getValue());
            index++;
        }
        for (int i = 0; i < args.size(); i++)
        {
            callArgs[i] = Objects.requireNonNull(args.get(i));
        }
        return new ScriptingProcess(names, srcs, callArgs);
    }

    @WorkerThread
    public synchronized boolean doSomething(final String source)
    {
        return AccessController.doPrivileged(new PrivilegedAction<Boolean>()
        {
            @Override
            public Boolean run()
            {
                return doSomething0(handler, source);
            }
        });
    }

    public synchronized void registerHandler(Object target)
    {
        for (Map.Entry<String, MethodHandler> entry
                : formTarget0(target).entrySet())
        {
            handlers.put(entry.getKey(), entry.getValue());
            registerHandler0(handler, UUID.randomUUID().toString(),
                    tokenAnalysis0(entry.getKey(),
                            entry.getValue().method));
        }
    }

    private Object onInvoke(String name, Object[] args)
    {
        try
        {
            MethodHandler methodHandler = handlers.get(name);
            return methodHandler.method.invoke(methodHandler.target, args);
        } catch (Exception e)
        {
            throw new RuntimeException(e);
        }
    }

    @Override
    public synchronized void close() throws IOException
    {
        try
        {
            AccessController.doPrivileged(new PrivilegedExceptionAction<Void>()
            {
                @Override
                public Void run() throws IOException
                {
                    stdin.processExited();
                    stdout.processExited();
                    stderr.processExited();
                    close0(handler);
                    handler = 0;
                    return null;
                }
            });
        } catch (PrivilegedActionException e)
        {
            throw (IOException) e.getException();
        }
    }

    @Override
    protected void finalize() throws Throwable
    {
        super.finalize();
        close();
    }

    private static final class MethodHandler
    {
        private final Method method;
        private final Object target;

        private MethodHandler(Object target, Method method)
        {
            this.target = target;
            this.method = method;
        }
    }

    private static final class ScriptingProcess extends ScriptingIOProcess
    {
        private static final String TAG = "ScriptingProcess";
        private static final ExecutorService processReaperExecutor
                = AccessController.doPrivileged(new PrivilegedAction<ExecutorService>()
        {
            @Override
            public ExecutorService run()
            {
                return Executors.newCachedThreadPool();
            }
        });

        private int pid;
        private int exitCode;
        private boolean hasExited;
        private PipedInputStream stderr;
        private PipedInputStream stdout;
        private PipedOutputStream stdin;

        private ScriptingProcess(final String[] srcNames,
                                 final String[] srcOrFile,
                                 final String[] args)
        {
            AccessController.doPrivileged(new PrivilegedAction<Object>()
            {
                @Override
                public Object run()
                {
                    FileDescriptor stdinFd = new FileDescriptor();
                    final FileDescriptor stdoutFd = new FileDescriptor();
                    FileDescriptor stderrFd = new FileDescriptor();
                    pid = createSub0(srcNames, srcOrFile, args, stdinFd, stdoutFd, stderrFd);
                    stdin = new PipedOutputStream(stdinFd);
                    stdout = new PipedInputStream(stdoutFd);
                    stderr = new PipedInputStream(stderrFd);
                    processReaperExecutor.execute(new Runnable()
                    {
                        @Override
                        public void run()
                        {
                            int exitCode = waitSub0(pid);
                            hasExited = true;
                            synchronized (ScriptingProcess.this)
                            {
                                hasExited = true;
                                ScriptingProcess.this.exitCode = exitCode;
                                ScriptingProcess.this.notifyAll();
                            }
                            stdin.processExited();
                            stdout.processExited();
                            stderr.processExited();
                        }
                    });
                    return null;
                }
            });
        }

        @Override
        public OutputStream getOutputStream()
        {
            return stdin;
        }

        @Override
        public InputStream getInputStream()
        {
            return stdout;
        }

        @Override
        public InputStream getErrorStream()
        {
            return stderr;
        }

        @Override
        public synchronized int waitFor() throws InterruptedException
        {
            while (!hasExited)
            {
                wait();
            }
            return exitCode;
        }

        @Override
        public synchronized int exitValue()
        {
            if (!hasExited)
            {
                throw new IllegalThreadStateException("process hasn't exited");
            }
            return exitCode;
        }

        @Override
        public void destroy()
        {
            synchronized (this)
            {
                if (!hasExited)
                {
                    killSub0(pid);
                }
            }
            try
            {
                stdin.close();
            } catch (IOException ignored)
            {
            }
            try
            {
                stdout.close();
            } catch (IOException ignored)
            {
            }
            try
            {
                stderr.close();
            } catch (IOException ignored)
            {
            }
        }
    }

    private static class PipedOutputStream extends BufferedOutputStream
    {
        private PipedOutputStream(FileDescriptor fileDescriptor)
        {
            super(formFd(fileDescriptor));
        }

        private static FileOutputStream formFd(final FileDescriptor fileDescriptor)
        {
            return AccessController.doPrivileged(new PrivilegedAction<FileOutputStream>()
            {
                @Override
                public FileOutputStream run()
                {
                    try
                    {
                        return FileOutputStream.class
                                .getConstructor(FileDescriptor.class, boolean.class)
                                .newInstance(fileDescriptor, true);
                    } catch (Exception e)
                    {
                        throw new RuntimeException(e);
                    }
                }
            });
        }

        private synchronized void processExited()
        {
            OutputStream out = this.out;
            if (out != null)
            {
                try
                {
                    out.close();
                } catch (IOException ignored)
                {
                    // We know of no reason to get an IOException, but if
                    // we do, there's nothing else to do but carry on.
                }
                this.out = NullOutputStream.INSTANCE;
            }
        }
    }

    private static class PipedInputStream extends BufferedInputStream
    {
        private PipedInputStream(FileDescriptor fileDescriptor)
        {
            super(formFd(fileDescriptor));
        }

        private static FileInputStream formFd(final FileDescriptor fileDescriptor)
        {
            return AccessController.doPrivileged(new PrivilegedAction<FileInputStream>()
            {
                @Override
                public FileInputStream run()
                {
                    try
                    {
                        return FileInputStream.class.getConstructor(FileDescriptor.class, boolean.class)
                                .newInstance(fileDescriptor, true);
                    } catch (Exception e)
                    {
                        throw new RuntimeException(e);
                    }
                }
            });
        }

        private static byte[] drainInputStream(InputStream in)
                throws IOException
        {
            if (in == null) return null;
            int n = 0;
            int j;
            byte[] a = null;
            while ((j = in.available()) > 0)
            {
                a = (a == null) ? new byte[j] : Arrays.copyOf(a, n + j);
                n += in.read(a, n, j);
            }
            return (a == null || n == a.length) ? a : Arrays.copyOf(a, n);
        }

        private synchronized void processExited()
        {
            // Most BufferedInputStream methods are synchronized, but close()
            // is not, and so we have to handle concurrent racing close().
            try
            {
                InputStream in = this.in;
                if (in != null)
                {
                    byte[] stragglers = drainInputStream(in);
                    in.close();
                    this.in = (stragglers == null) ? NullInputStream.INSTANCE :
                            new ByteArrayInputStream(stragglers);
                    if (buf == null) // asynchronous close()?
                    {
                        this.in = null;
                    }
                }
            } catch (IOException ignored)
            {
                // probably an asynchronous close().
            }
        }
    }

    private static class NullInputStream extends InputStream
    {
        private static final NullInputStream INSTANCE = new NullInputStream();

        private NullInputStream()
        {
        }

        @Override
        public int read()
        {
            return -1;
        }

        @Override
        public int available()
        {
            return 0;
        }
    }

    private static class NullOutputStream extends OutputStream
    {
        private static final NullOutputStream INSTANCE = new NullOutputStream();

        private NullOutputStream()
        {
        }

        @Override
        public void write(int b) throws IOException
        {
            throw new IOException("Stream closed");
        }
    }

    private Map<String, MethodHandler> formTarget0(Object target)
    {
        Pattern pattern = Pattern.compile("[_a-zA-z][_a-zA-z0-9]*");
        Method[] methods = target.getClass().getMethods();
        Map<String, MethodHandler> result = new HashMap<>();
        if (methods != null && methods.length != 0)
        {
            for (Method method : methods)
            {
                HandlerTarget handlerTarget
                        = method.getAnnotation(HandlerTarget.class);
                if (Modifier.isPublic(method.getModifiers())
                        && !Modifier.isStatic(method.getModifiers())
                        && handlerTarget != null)
                {
                    String name = handlerTarget.name();
                    if (!pattern.matcher(Objects
                            .requireNonNull(name)).matches()
                            && !handlers.containsKey(name))
                    {
                        throw new IllegalArgumentException("this name is illegal '" + name + "'");
                    }
                    if (!SCRIPT_BASIC_TYPE.containsKey(method.getReturnType()))
                    {
                        throw new IllegalArgumentException(method.getReturnType().getName());
                    }
                    for (Class<?> pramType : method.getParameterTypes())
                    {
                        if (void.class.equals(pramType)
                                || Void.class.equals(pramType)
                                || !SCRIPT_BASIC_TYPE.containsKey(pramType))
                        {
                            throw new IllegalArgumentException(pramType.getName());
                        }
                    }
                    result.put(name, new MethodHandler(target, method));
                }
            }
        }
        return result;
    }

    private static String tokenAnalysis0(String name, Method method)
    {
        StringBuilder builder = new StringBuilder();
        Class<?> returnType = method.getReturnType();
        builder.append(SCRIPT_BASIC_TYPE
                .get(returnType))
                .append(' ')
                .append(name)
                .append('(');
        Class<?>[] pramType = method.getParameterTypes();
        if (pramType != null && pramType.length != 0)
        {
            for (int index = 0; index < pramType.length; index++)
            {
                builder.append(SCRIPT_BASIC_TYPE
                        .get(pramType[index]))
                        .append(' ')
                        .append('_')
                        .append(index);
                if (index != pramType.length - 1)
                {
                    builder.append(',');
                }
            }
        }
        builder.append(')').append('{');
        if (returnType.equals(void.class) || returnType.equals(Void.class))
        {
            builder.append("__handler(")
                    .append('\"')
                    .append(name)
                    .append('\"')
                    .append(',')
                    .append("NULL")
                    .append(',');
        } else
        {
            builder.append(SCRIPT_BASIC_TYPE.get(returnType))
                    .append(' ')
                    .append('_')
                    .append('r')
                    .append(';')
                    .append("__handler(")
                    .append('\"')
                    .append(name)
                    .append('\"')
                    .append(',')
                    .append('&')
                    .append('_')
                    .append('r')
                    .append(',');
        }
        if (pramType != null && pramType.length != 0)
        {
            for (int index = 0; index < pramType.length; index++)
            {
                builder.append('_')
                        .append(index);
                if (index != pramType.length - 1)
                {
                    builder.append(',');
                }
            }
        }
        if (returnType.equals(void.class) || returnType.equals(Void.class))
        {
            builder.append(')')
                    .append(';')
                    .append('}');
        } else
        {
            builder.append(')')
                    .append(';')
                    .append("return")
                    .append(' ')
                    .append('_')
                    .append('r')
                    .append(';')
                    .append('}');
        }
        return builder.toString();
    }

    private static native int createSub0(String[] srcNames,
                                         String[] srcOrFile,
                                         String[] args,
                                         FileDescriptor stdinFd,
                                         FileDescriptor stdoutFd,
                                         FileDescriptor stderrFd);

    private static native int waitSub0(int pid);

    private static native void killSub0(int pid);

    private native long init0(FileDescriptor stdinFd,
                              FileDescriptor stdoutFd,
                              FileDescriptor stderrFd);

    private static native void close0(long handler);

    private static native boolean doSomething0(long handler, String source);

    private static native void registerHandler0(long handler, String id, String registerText);
}