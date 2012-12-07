#include "hb.h"
#include "dict.h"
#include "btree.h"
#include "skiplist.h"

static HbLog s_Log("main");

int main(int /*argc*/, char** /*argv*/)
{
    HbStringTest::Test();
    HbDictTest::TestStringString(1024);
    HbDictTest::TestStringInt(1024);
    HbDictTest::TestIntInt(1024);
    HbDictTest::TestIntString(1024);
    HbDictTest::TestMergeIntKeys(1024, 100);

    HbStopWatch sw;

    s_Log.Debug("DICT");
    sw.Restart();
    HbDictTest::AddRandomKeys(1024*1024);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    /*sw.Restart();
    HbSkipListTest::AddRandomKeys2(1024*1024, true, 0);
    sw.Stop();
    s_Log.Debug("SKIPLIST: %f", sw.GetElapsed());*/

    s_Log.Debug("BTREEE");
    sw.Restart();
    HbBTreeTest::AddRandomKeys(1024*1024, true, 0);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("BTREEE(asc)");
    sw.Restart();
    HbBTreeTest::AddSortedKeys(1024*1024, true, 0, true);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    s_Log.Debug("BTREEE(desc)");
    sw.Restart();
    HbBTreeTest::AddSortedKeys(1024*1024, true, 0, false);
    sw.Stop();
    s_Log.Debug("total: %f", sw.GetElapsed());

    HbSkipListTest::AddDeleteRandomKeys(1024*1024, true, 0);

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
}
