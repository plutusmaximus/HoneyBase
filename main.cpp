#define _CRT_SECURE_NO_WARNINGS
#include "hb.h"
#include "dict.h"
#include "btree.h"
#include "skiplist.h"

#include <stdio.h>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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

    sw.Restart();
    HbDictTest::AddRandomKeys(1024*1024);
    sw.Stop();
    s_Log.Debug("DICT: %f", sw.GetElapsed());

    sw.Restart();
    HbSkipListTest::AddRandomKeys(1024*1024, true, 0);
    sw.Stop();
    s_Log.Debug("SKIPLIST: %f", sw.GetElapsed());

    sw.Restart();
    HbIndexTest::AddRandomKeys(1024*1024, true, 0);
    sw.Stop();
    s_Log.Debug("BTREEE: %f", sw.GetElapsed());

    //HbIndexTest::AddRandomKeys(1024, true, 32767);
    //HbIndexTest::AddRandomKeys(10, false, 1);
    //HbIndexTest::AddRandomKeys(1024*1024, false, 32767);
    //HbIndexTest::AddRandomKeys(1024*1024, false, 1);
    //HbIndexTest::AddRandomKeys(1024*1024, true, 0);

    //HbIndexTest::AddSortedKeys(1024*1024, true, 0, true);
    //HbIndexTest::AddSortedKeys(1024*1024, true, 0, false);

    //HbIndexTest::AddDups(1024*1024, 1, 1);
    //HbIndexTest::AddDups(1024*1024, 1, 2);
    //HbIndexTest::AddDups(1024*1024, 1, 4);
    //HbIndexTest::AddDups(32, 1, 5);
    //HbIndexTest::AddDups(28, 1, 5);
}
