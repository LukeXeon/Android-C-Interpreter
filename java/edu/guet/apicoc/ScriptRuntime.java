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
import java.lang.reflect.ParameterizedType;
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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;



/**
 * Created by Mr.小世界 on 2018/10/31.
 */

@SuppressWarnings("JniMissingFunction")
public final class ScriptRuntime implements AutoCloseable,ScriptingIO
{
    private final static String TAG = "ScriptRuntime";
    private final static Map<Class<?>, String> SCRIPTING_BASIC_TYPES
            = Collections.unmodifiableMap(new HashMap<Class<?>, String>()
    {
        {
            put(Boolean.class, "bool");
            put(Integer.class, "int");
            put(Double.class, "double");
            put(Long.class, "long");
            put(Float.class, "float");
            put(String.class, "char *");
            put(Character.class, "char");
            put(Short.class, "short");
            put(Void.class, "void");
        }
    });

    private Map<String, Object> handlers = new HashMap<>();
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
    public int doSomething(final String source)
    {
        return AccessController.doPrivileged(new PrivilegedAction<Integer>()
        {
            @Override
            public Integer run()
            {
                return doSomething0(handler, source);
            }
        });
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
    protected synchronized void finalize() throws Throwable
    {
        super.finalize();
        close();
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

    private static native int createSub0(String[] srcNames,
                                         String[] srcOrFile,
                                         String[] args,
                                         FileDescriptor stdinFd,
                                         FileDescriptor stdoutFd,
                                         FileDescriptor stderrFd);

    private static native int waitSub0(int pid);

    private static native void killSub0(int pid);

    private static native long init0(FileDescriptor stdinFd,
                                     FileDescriptor stdoutFd,
                                     FileDescriptor stderrFd);

    private static native void close0(long handler);

    private static native int doSomething0(long handler, String source);
}