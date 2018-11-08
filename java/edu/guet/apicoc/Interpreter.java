package edu.guet.apicoc;

import android.support.annotation.WorkerThread;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;


/**
 * Created by Mr.小世界 on 2018/10/31.
 */

@SuppressWarnings("JniMissingFunction")
public final class Interpreter implements AutoCloseable
{
    private final static String TAG = "Interpreter";
    private InputStream stderr;
    private InputStream stdout;
    private OutputStream stdin;
    private long handler;

    static
    {
        System.loadLibrary("apicoc");
    }

    public Interpreter()
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
                stdin = createOutputStream(stdinFd);
                stdout = createInputStream(stdoutFd);
                stderr = createInputStream(stderrFd);
                return null;
            }
        });
    }

    public OutputStream getOutputStream()
    {
        return stdin;
    }

    public InputStream getInputStream()
    {
        return stdout;
    }

    public InputStream getErrorStream()
    {
        return stderr;
    }

    public static Process callInSubProcess(List<File> files, List<String> args)
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
        return new InterpreterProcess(null, filePaths, callArgs);
    }

    public static Process callInSubProcess(Map<String, String> scripts, List<String> args)
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
        return new InterpreterProcess(names, srcs, callArgs);
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
                    stdin.close();
                    stdout.close();
                    stderr.close();
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

    public synchronized void cleanup()
    {
        AccessController.doPrivileged(new PrivilegedAction<Void>()
        {
            @Override
            public Void run()
            {
                cleanup0(handler);
                return null;
            }
        });
    }

    public synchronized void addSource(final String fileName, final String source)
    {
        AccessController.doPrivileged(new PrivilegedAction<Void>()
        {
            @Override
            public Void run()
            {
                addSource0(handler, Objects.requireNonNull(fileName), Objects.requireNonNull(source));
                return null;
            }
        });
    }

    public synchronized void addFile(final File file)
    {
        AccessController.doPrivileged(new PrivilegedAction<Void>()
        {
            @Override
            public Void run()
            {
                addFile0(handler, Objects.requireNonNull(file).getAbsolutePath());
                return null;
            }
        });
    }

    @WorkerThread
    public synchronized int callMain(List<String> args)
    {
        final List<String> constArgs
                = Collections.unmodifiableList(args == null ? Collections.<String>emptyList() : args);
        return AccessController.doPrivileged(new PrivilegedAction<Integer>()
        {
            @Override
            public Integer run()
            {
                return callMain0(handler, constArgs.toArray(new String[constArgs.size()]));
            }
        });
    }

    @WorkerThread
    public synchronized int callMain()
    {
        return callMain(null);
    }

    @Override
    protected synchronized void finalize() throws Throwable
    {
        super.finalize();
        close();
    }

    private static final class InterpreterProcess extends Process
    {
        private static final String TAG = "InterpreterProcess";
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
        private InputStream stderr;
        private InputStream stdout;
        private OutputStream stdin;

        private InterpreterProcess(final String[] srcNames, final String[] srcOrFile, final String[] args)
        {
            AccessController.doPrivileged(new PrivilegedAction<Object>()
            {
                @Override
                public Object run()
                {
                    FileDescriptor stdinFd = new FileDescriptor();
                    FileDescriptor stdoutFd = new FileDescriptor();
                    FileDescriptor stderrFd = new FileDescriptor();
                    pid = createSub0(srcNames, srcOrFile, args, stdinFd, stdoutFd, stderrFd);
                    Log.i(TAG, "run: " + pid);
                    stdin = createOutputStream(stdinFd);
                    stdout = createInputStream(stdoutFd);
                    stderr = createInputStream(stderrFd);
                    processReaperExecutor.execute(new Runnable()
                    {
                        @Override
                        public void run()
                        {
                            exitCode = waitSub0(pid);
                            Log.i(TAG, "exitCode: " + exitCode);
                            hasExited = true;
                            synchronized (InterpreterProcess.this)
                            {
                                InterpreterProcess.this.notifyAll();
                            }
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

    private static OutputStream createOutputStream(final FileDescriptor fileDescriptor)
    {
        return AccessController.doPrivileged(new PrivilegedAction<OutputStream>()
        {
            @Override
            public OutputStream run()
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

    private static InputStream createInputStream(final FileDescriptor fileDescriptor)
    {
        return AccessController.doPrivileged(new PrivilegedAction<InputStream>()
        {
            @Override
            public InputStream run()
            {
                try
                {
                    return new BufferedInputStream(FileInputStream.class
                            .getConstructor(FileDescriptor.class, boolean.class)
                            .newInstance(fileDescriptor, true));
                } catch (Exception e)
                {
                    throw new RuntimeException(e);
                }
            }
        });
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

    private static native void close0(long ptr);

    private static native void cleanup0(long ptr);

    private static native int callMain0(long ptr,
                                        String[] args);

    private static native int addSource0(long ptr,
                                         String fileName,
                                         String source);

    private static native int addFile0(long ptr,
                                       String filePath);

}