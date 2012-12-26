#include "dict.h"
#include "btree.h"
#include "skiplist.h"

#include "tests.h"

using namespace honeybase;

static Log s_Log("main");

void TestCommands();

void TestMemMappedFile();

int main(int /*argc*/, char** /*argv*/)
{
    /*TestMemMappedFile();
    TestCommands();*/

    StopWatch sw;

    const int NUMKEYS = 1000*1000;

    const KeyType keyType = KEYTYPE_BLOB;
    const ValueType valueType = VALUETYPE_INT;

    s_Log.Debug("SPEED DICT");
    sw.Restart();
    {
        HashTableSpeedTest test(keyType, valueType);
        test.AddRandomKeys(NUMKEYS);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("SPEED BTREEE");
    sw.Restart();
    {
        BTreeSpeedTest test(keyType, valueType);
        test.AddRandomKeys(NUMKEYS, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("DICT");
    sw.Restart();
    {
        HashTableTest test(keyType, valueType);
        test.AddRandomKeys(NUMKEYS);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("BTREEE");
    sw.Restart();
    {
        BTreeTest test(keyType, valueType);
        test.AddRandomKeys(NUMKEYS, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("BTREEE(asc)");
    sw.Restart();
    {
        BTreeTest test(keyType, valueType);
        test.AddSortedKeys(NUMKEYS, true, 0, true);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("BTREEE(desc)");
    sw.Restart();
    {
        BTreeTest test(keyType, valueType);
        test.AddSortedKeys(NUMKEYS, true, 0, false);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    SkipListTest::AddDeleteRandomKeys(NUMKEYS, true, 0);
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
void ExecCmd(const Command* cmd, HashTable* dict);

void TestCommands()
{
    const char cmdStr1[] = {"*3 $3 set $3 foo $3 bar"};
    const char cmdStr2[] = {"*2 $3 get $3 foo"};
    const char cmdStr3[] = {"*4 $3 set *3 $3 get *3 $3 get *3 $3 get $1 a $1 b $1 c $1 d $3 foo $3 123"};
    const char cmdStr4[] = {"*4 $3 set *3 $3 get $1 a $1 b $3 foo $3 123"};
    const char cmdStr5[] = {"*4 $3 set *3 $3 get $1 a $1 b *3 $3 get $1 c $1 d *3 $3 get $1 e $1 f"};

    HashTable* dict = HashTable::Create();
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
    HashTable::Destroy(dict);
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

void ExecCmd(const Command* cmd, HashTable* dict)
{
    if(!strcmp((char*)cmd->argv[0].argVal, "set"))
    {
        Blob* key = Blob::Create(cmd->argv[1].argVal, cmd->argv[1].argLen);
        Blob* val = Blob::Create(cmd->argv[2].argVal, cmd->argv[2].argLen);
        dict->Set(key, val);
    }
    else if(!strcmp((char*)cmd->argv[0].argVal, "get"))
    {
        Blob* key = Blob::Create(cmd->argv[1].argVal, cmd->argv[1].argLen);
        const Blob* val;
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
