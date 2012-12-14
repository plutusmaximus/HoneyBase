#include "hb.h"
#include "dict.h"
#include "btree.h"
#include "skiplist.h"

static HbLog s_Log("main");

void TestCommands();

void TestMemMappedFile();

int main(int /*argc*/, char** /*argv*/)
{
    TestMemMappedFile();
    TestCommands();

    HbStringTest::Test();
    HbDictTest::TestStringString(1024);
    HbDictTest::TestStringInt(1024);
    HbDictTest::TestIntInt(1024);
    HbDictTest::TestIntString(1024);
    HbDictTest::TestMergeIntKeys(1024, 100);

    HbStopWatch sw;

    const int NUMKEYS = 1000*1000;

    s_Log.Debug("DICT");
    sw.Restart();
    HbDictTest::AddRandomKeys(NUMKEYS);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    /*sw.Restart();
    HbSkipListTest::AddRandomKeys2(1024*1024, true, 0);
    sw.Stop();
    s_Log.Debug("SKIPLIST: %f", sw.GetElapsed());*/

    s_Log.Debug("BTREEE");
    sw.Restart();
    HbBTreeTest::AddRandomKeys(NUMKEYS, true, 0);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("BTREEE(asc)");
    sw.Restart();
    HbBTreeTest::AddSortedKeys(NUMKEYS, true, 0, true);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("BTREEE(desc)");
    sw.Restart();
    HbBTreeTest::AddSortedKeys(NUMKEYS, true, 0, false);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    HbSkipListTest::AddDeleteRandomKeys(NUMKEYS, true, 0);

    //HbBTreeTest::AddRandomKeys(1024, true, 32767);
    //HbBTreeTest::AddRandomKeys(10, false, 1);
    //HbBTreeTest::AddRandomKeys(1024*1024, false, 32767);
    //HbBTreeTest::AddRandomKeys(1024*1024, false, 1);
    //HbBTreeTest::AddRandomKeys(1024*1024, true, 0);

    //HbBTreeTest::AddDups(1024*1024, 1, 1);
    //HbBTreeTest::AddDups(1024*1024, 1, 2);
    //HbBTreeTest::AddDups(1024*1024, 1, 4);
    //HbBTreeTest::AddDups(32, 1, 5);
    //HbBTreeTest::AddDups(28, 1, 5);

    //(hset (hget (hget (hget a b) c) d) foo 123)
    //(hset a.b.c.d foo 123)
    //(hget z (hget a.b.c.d.foo))
    //(hget z (range a.b.c.d.foo 35 87))
}

#include <string.h>
#include <new.h>

class Command;

class Argument
{
public:

    Argument()
        : argVal(NULL)
        , argLen(0)
        , m_SubCmd(NULL)
    {
    }

    byte* argVal;
    size_t argLen;
    Command* m_SubCmd;
};

class Command
{
public:
    Command()
        : argc(0)
        , argv(NULL)
    {
    }

    int argc;
    Argument* argv;
};

const char* ParseCmd(const char* cmdStr, const size_t len, Command** cmd);
void PrintCmd(const Command* cmd);
void ExecCmd(const Command* cmd, HbDict* dict);

void TestCommands()
{
    const char cmdStr1[] = {"*3 $3 set $3 foo $3 bar"};
    const char cmdStr2[] = {"*2 $3 get $3 foo"};
    const char cmdStr3[] = {"*4 $3 set *3 $3 get *3 $3 get *3 $3 get $1 a $1 b $1 c $1 d $3 foo $3 123"};
    const char cmdStr4[] = {"*4 $3 set *3 $3 get $1 a $1 b $3 foo $3 123"};
    const char cmdStr5[] = {"*4 $3 set *3 $3 get $1 a $1 b *3 $3 get $1 c $1 d *3 $3 get $1 e $1 f"};

    HbDict* dict = HbDict::Create();
    Command* cmd1;
    Command* cmd2;
    Command* cmd3;
    if(ParseCmd(cmdStr1, sizeof(cmdStr1), &cmd1))
    {
        PrintCmd(cmd1);
    }

    if(ParseCmd(cmdStr2, sizeof(cmdStr2), &cmd2))
    {
        PrintCmd(cmd2);
    }

    if(ParseCmd(cmdStr3, sizeof(cmdStr3), &cmd3))
    {
        PrintCmd(cmd3);
    }

    ExecCmd(cmd1, dict);
    ExecCmd(cmd2, dict);
    HbDict::Destroy(dict);
}

void PrintCmd(const Command* cmd)
{
    for(int i = 0; i < cmd->argc; ++i)
    {
        if(cmd->argv[i].argVal)
        {
            s_Log.Debug((char*)cmd->argv[i].argVal);
        }
        else
        {
            PrintCmd(cmd->argv[i].m_SubCmd);
        }
    }
}

void ExecCmd(const Command* cmd, HbDict* dict)
{
    if(!strcmp((char*)cmd->argv[0].argVal, "set"))
    {
        HbString* key = HbString::Create(cmd->argv[1].argVal, cmd->argv[1].argLen);
        HbString* val = HbString::Create(cmd->argv[2].argVal, cmd->argv[2].argLen);
        dict->Set(key, val);
    }
    else if(!strcmp((char*)cmd->argv[0].argVal, "get"))
    {
        HbString* key = HbString::Create(cmd->argv[1].argVal, cmd->argv[1].argLen);
        const HbString* val;
        if(dict->Find(key, &val))
        {
            int iii = 0;
            (void)iii;
        }
    }
}

const char* ParseCmd(const char* cmdStr, const size_t len, Command** cmd)
{
    const char* p = cmdStr;
    const char* end = cmdStr + len;
    size_t numArgs = 0;
    if('*' == *p)
    {
        for(++p; *p != ' ' && p < end; ++p)
        {
            const int n = *p - '0';
            if(n < 0 || n > 9)
            {
                return NULL;
            }
            numArgs = (numArgs * 10) + n;
        }

        Command* tmpCmd = new Command();
        tmpCmd->argv = new Argument[numArgs];

        for(size_t i = 0; i < numArgs && p < end; ++i)
        {
            ++p;

            size_t argLen = 0;
            if('$' == *p)
            {
                for(++p; *p != ' ' && p < end; ++p)
                {
                    const int n = *p - '0';
                    if(n < 0 || n > 9)
                    {
                        return NULL;
                    }
                    argLen = (argLen * 10) + n;
                }

                ++p;

                if(p + argLen <= end)
                {
                    byte* argVal = new byte[argLen+1];
                    memcpy(argVal, p, argLen);
                    argVal[argLen] = '\0';

                    tmpCmd->argv[tmpCmd->argc].argVal = argVal;
                    tmpCmd->argv[tmpCmd->argc].argLen = argLen;
                    ++tmpCmd->argc;
                    p += argLen;
                }
                else
                {
                    return NULL;
                }
            }
            else if('*' == *p)
            {
                const char* nextp = ParseCmd(p, end-p, &tmpCmd->argv[tmpCmd->argc].m_SubCmd);
                if(nextp)
                {
                    ++tmpCmd->argc;
                    p = nextp;
                }
                else
                {
                    return NULL;
                }
            }
            else
            {
                return NULL;
            }
        }

        *cmd = tmpCmd;
        return p;
    }

    return NULL;
}
