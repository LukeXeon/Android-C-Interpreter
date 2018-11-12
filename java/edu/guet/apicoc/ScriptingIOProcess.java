package edu.guet.apicoc;

/**
 * Created by Mr.小世界 on 2018/11/10.
 */

public abstract class ScriptingIOProcess
        extends Process
        implements ScriptingIO
{
    @Override
    public void close() throws Exception
    {
        destroy();
    }
}
