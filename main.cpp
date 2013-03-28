#include "dict.h"
#include "btree.h"
#include "skiplist.h"
#include "error.h"

#include "tests.h"

#include "network.h"

using namespace honeybase;

static Log s_Log("main");

void TestCommands();

void TestMemMappedFile();

int main(int /*argc*/, char** /*argv*/)
{
    //Network::Startup(4321);

    //TestMemMappedFile();
    //TestCommands();

    StopWatch sw;

    const int NUMKEYS = 1000*1000;

    const ValueType keyType = VALUETYPE_BLOB;
    const ValueType valueType = VALUETYPE_BLOB;
    const TestKeyOrder keyOrder = KEYORDER_RANDOM;

    /*s_Log.Debug("SPEED DICT");
    sw.Restart();
    {
        HashTableSpeedTest test(keyType, valueType);
        test.AddKeys(NUMKEYS, keyOrder);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());*/

    /*s_Log.Debug("SPEED BTREEE");
    sw.Restart();
    {
        BTreeSpeedTest test(keyType, valueType);
        test.AddKeys(NUMKEYS, keyOrder, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());*/

    s_Log.Debug("SPEED SKIPLIST");
    sw.Restart();
    {
        //SkipListSpeedTest test(keyType, valueType);
        SkipListSpeedTest test(valueType);
        test.AddKeys(NUMKEYS, keyOrder, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());

    /*s_Log.Debug("SPEED SORTEDSET");
    sw.Restart();
    {
        SortedSetSpeedTest test(keyType, valueType);
        test.AddKeys(NUMKEYS, keyOrder, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());*/

    /*s_Log.Debug("DICT");
    sw.Restart();
    {
        HashTableTest test(keyType, valueType);
        test.AddKeys(NUMKEYS, keyOrder);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());*/

    /*s_Log.Debug("BTREEE");
    sw.Restart();
    {
        BTreeTest test(keyType, valueType);
        test.AddKeys(NUMKEYS, keyOrder, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());*/

    /*s_Log.Debug("SKIPLIST");
    sw.Restart();
    {
        //SkipListTest test(keyType, valueType);
        SkipListTest test(valueType);
        test.AddKeys(NUMKEYS, keyOrder, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());*/

    /*s_Log.Debug("SORTEDSET");
    sw.Restart();
    {
        SortedSetTest test(keyType, valueType);
        test.AddKeys(NUMKEYS, keyOrder, true, 0);
    }
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());
    hbassert(0 == Blob::GlobalBlobCount());*/
}

#include <string.h>

#include "command.h"

//void PrintCmd(const Command* cmd);

void TestCommands()
{
    const char cmdStr1[] = {"*3\r\n$3\r\nset\r\n$3\r\nfoo\r\n$3\r\nbar\r\n*2\r\n$3\r\nget\r\n$3\r\nfoo\r\n"};

    HashTable* ht = HashTable::Create();

    Command cmd;
    Error err;

    for(int i = 0; i < (int)strlen(cmdStr1);)
    {
        i += cmd.Parse((u8*)&cmdStr1[i], 1, &err);
        if(cmd.IsComplete())
        {
            cmd.Exec(ht, &err);
            cmd.Reset();
        }
    }

    ht->Unref();
}

/*void PrintCmd(const Command* cmd)
{
    for(int i = 0; i < cmd->argc; ++i)
    {
        if(cmd->argv[i].m_Arg.m_Blob)
        {
            char arg[256] = {0};
            cmd->argv[i].m_Arg.m_Blob->CopyData((byte*)arg, sizeof(arg)-1);
            s_Log.Debug(arg);
        }
        else
        {
            PrintCmd(cmd->argv[i].m_SubCmd);
        }
    }
}*/

