#include "tests.h"

#include "btree.h"
#include "dict.h"
#include "skiplist.h"

#include <algorithm>
#include <functional>

namespace honeybase
{

#define KEY_SIZE_BLOB       24
#define VALUE_SIZE_BLOB     100

static Log s_Log("tests");

static ptrdiff_t myrandom (ptrdiff_t i)
{
    return Rand()%i;
}

static bool EQ(const Value a, ValueType valtypeA, const Value b, const ValueType valtypeB)
{
    if(valtypeA == valtypeB)
    {
        switch(valtypeA)
        {
        case VALUETYPE_INT:
            return a.m_Int == b.m_Int;
        case VALUETYPE_DOUBLE:
            return a.m_Double == b.m_Double;
        case VALUETYPE_BLOB:
            return a.m_Blob->EQ(b.m_Blob);
        }
    }

    return false;
}

static void CreateRandomString(char* buf, const size_t bufSize)
{
    u32 words[6];
    const int numWords = ((((bufSize*6)+7)/8)+3)/4;
    char* p = buf;
    const char* eob = p + bufSize;
    for(int i = 0; i < numWords && p < eob; i += 6)
    {
        for(int j = 0; j < 6; ++j)
        {
            words[j] = Rand();
        }

        p += Base64Encode((byte*)words, 24, p, eob-p);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  KV
///////////////////////////////////////////////////////////////////////////////
void
KV::CreateKeys(const KeyType keyType,
                const size_t keySize,
                const ValueType valueType,
                const size_t valueSize,
                const TestKeyOrder keyOrder,
                KV* kv,
                const int numKeys)
{
    if(KEYTYPE_INT == keyType)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            kv[i].m_KeyType = keyType;
            kv[i].m_Key.m_Int = i;
        }
    }
    else if(KEYTYPE_DOUBLE == keyType)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            kv[i].m_KeyType = keyType;
            kv[i].m_Key.m_Double = i / 3.14159;
        }
    }
    else if(KEYTYPE_BLOB == keyType)
    {
        const int numBytes = ((keySize*6)+7)/8;
        const int numWords = (numBytes+3)/4;
        const size_t sizeofData = numWords * sizeof(u32);
        u32* data = (u32*)malloc(sizeofData);
        char* keyBuf = (char*)malloc(keySize+1);
        for(int i = 0; i < numKeys; ++i)
        {
            for(int j = 0; j < numWords; ++j)
            {
                data[j] = i+j;
            }
            Base64Encode((byte*)data, sizeofData, keyBuf, keySize+1);
            kv[i].m_Key.m_Blob = Blob::Create((byte*)keyBuf, strlen(keyBuf));
        }

        free(data);
        free(keyBuf);
    }

    if(VALUETYPE_INT == valueType)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            kv[i].m_ValueType = valueType;
            kv[i].m_Value.m_Int = i;
        }
    }
    else if(VALUETYPE_DOUBLE == valueType)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            kv[i].m_ValueType = valueType;
            kv[i].m_Value.m_Double = i / 3.14159;
        }
    }
    else if(VALUETYPE_BLOB == valueType)
    {
        const int numBytes = ((valueSize*6)+7)/8;
        const int numWords = (numBytes+3)/4;
        const size_t sizeofData = numWords * sizeof(u32);
        u32* data = (u32*)malloc(sizeofData);
        char* valBuf = (char*)malloc(valueSize+1);
        for(int i = 0; i < numKeys; ++i)
        {
            for(int j = 0; j < numWords; ++j)
            {
                data[j] = i+j;
            }
            Base64Encode((byte*)data, sizeofData, valBuf, valueSize+1);
            kv[i].m_Value.m_Blob = Blob::Create((byte*)valBuf, strlen(valBuf));
        }

        free(data);
        free(valBuf);
    }

    if(KEYORDER_RANDOM == keyOrder)
    {
        std::random_shuffle(&kv[0], &kv[numKeys]);
    }
}

KV::~KV()
{
    if(KEYTYPE_BLOB == m_KeyType)
    {
        Blob::Destroy(m_Key.m_Blob);
    }

    if(VALUETYPE_BLOB == m_ValueType)
    {
        Blob::Destroy(m_Value.m_Blob);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  HashTableTest
///////////////////////////////////////////////////////////////////////////////
HashTableTest::HashTableTest(const KeyType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
HashTableTest::Test(const int numKeys)
{
    HashTable* dict = HashTable::Create();
    Value value;
    ValueType valueType;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, KEYORDER_RANDOM, kv, numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        dict->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
        hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(dict->Clear(kv[i].m_Key, m_KeyType));
        hbverify(!dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
    }

    hbverify(0 == dict->Count());

    HashTable::Destroy(dict);

    delete [] kv;
}

struct KV_Patch
{
    static const int SECTION_LEN    = 256;
    Key m_Key;
    unsigned m_FinalLen;
    char m_Test[SECTION_LEN*3];
};

void
HashTableTest::TestMergeIntKeys(const int numKeys, const int numIterations)
{
    char alphabet[1024];
    CreateRandomString(alphabet, sizeof(alphabet));

    HashTable* dict = HashTable::Create();
    Value value;
    ValueType valueType;

    //Create random length strings, with random offsets
    KV_Patch* kv = new KV_Patch[numKeys];
    memset(kv, 0, numKeys*sizeof(*kv));

    KV_Patch* p = kv;
    for(int i = 0; i < numKeys; ++i, ++p)
    {
        p->m_Key.m_Int = i;
    }

    std::random_shuffle(&kv[0], &kv[numKeys]);

    for(int iter = 0; iter < numIterations; ++iter)
    {
        p = kv;
        for(int i = 0; i < numKeys; ++i, ++p)
        {
            const unsigned offset = Rand(0, KV_Patch::SECTION_LEN-1);
            const unsigned len = Rand(1, KV_Patch::SECTION_LEN);
            if(offset+len > p->m_FinalLen)
            {
                p->m_FinalLen = offset + len;
            }

            const unsigned abOffset = Rand() % (sizeof(alphabet) - len);
            byte hbsBuf[KV_Patch::SECTION_LEN*2];
            const Blob* value =
                Blob::Encode((byte*)&alphabet[abOffset], len,
                            hbsBuf, sizeof(hbsBuf));

            hbverify(dict->Patch(p->m_Key, KEYTYPE_INT, 1, &value, &offset));

            memcpy(&p->m_Test[offset], &alphabet[abOffset], len);
        }

        //std::random_shuffle(&kv[0], &kv[numKeys]);

        //Check they've been added
        p = kv;
        for(int i = 0; i < numKeys; ++i, ++p)
        {
            hbverify(dict->Find(p->m_Key, KEYTYPE_INT, &value, &valueType));
            const byte* data;
            const size_t len = value.m_Blob->GetData(&data);
            hbverify(len == p->m_FinalLen);
            hbverify(0 == memcmp(data, p->m_Test, p->m_FinalLen));
        }
    }

    //Delete the items
    p = kv;
    for(int i = 0; i < numKeys; ++i, ++p)
    {
        hbverify(dict->Clear(p->m_Key, KEYTYPE_INT));
        hbverify(!dict->Find(p->m_Key, KEYTYPE_INT, &value, &valueType));
    }

    hbverify(0 == dict->Count());

    HashTable::Destroy(dict);

    delete [] kv;
}

void
HashTableTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder)
{
    HashTable* dict = HashTable::Create();
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool added = dict->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
        if(added)
        {
            hbverify(dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("set: %f", sw.GetElapsed());

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool found = dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
        hbassert(found);
        hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    
    if(KEYORDER_RANDOM == keyOrder)
    {
        std::sort(&kv[0], &kv[numKeys]);

        sw.Restart();
        for(int i = 0; i < numKeys; ++i)
        {
            hbverify(dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
        sw.Stop();
        s_Log.Debug("sorted find: %f", sw.GetElapsed());
    }

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool cleared = dict->Clear(kv[i].m_Key, m_KeyType);
        hbassert(cleared);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());

    HashTable::Destroy(dict);
    delete [] kv;
}

void
HashTableTest::AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder)
{
    HashTable* dict = HashTable::Create();
    Value value;
    ValueType valueType;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = Rand() % numKeys;
        if(!kv[idx].m_Added)
        {
            const bool added = dict->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
            if(added)
            {
                kv[idx].m_Added = true;
                hbverify(dict->Find(kv[idx].m_Key, m_KeyType, &value, &valueType));
                hbverify(EQ(value, valueType, kv[idx].m_Value, m_ValueType));
            }
        }
        else
        {
            const bool cleared = dict->Clear(kv[idx].m_Key, m_KeyType);
            hbassert(cleared);
            kv[idx].m_Added = false;
            hbverify(!dict->Find(kv[idx].m_Key, m_KeyType, &value, &valueType));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            const bool found = dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
            hbassert(found);
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
        else
        {
            const bool found = dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
            hbassert(!found);
        }
    }

    HashTable::Destroy(dict);
    delete [] kv;
}

///////////////////////////////////////////////////////////////////////////////
//  HashTableSpeedTest
///////////////////////////////////////////////////////////////////////////////
HashTableSpeedTest::HashTableSpeedTest(const KeyType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
HashTableSpeedTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder)
{
    HashTable* dict = HashTable::Create();
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        dict->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("set: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    //std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        dict->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        dict->Clear(kv[i].m_Key, m_KeyType);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    HashTable::Destroy(dict);
    delete [] kv;
}

///////////////////////////////////////////////////////////////////////////////
//  BTreeTest
///////////////////////////////////////////////////////////////////////////////
BTreeTest::BTreeTest(const KeyType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
BTreeTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    BTree* btree = BTree::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        HB_ASSERTONLY(const bool added =) btree->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);
        hbassert(added);
        HB_ASSERTONLY(const bool found =) btree->Find(kv[i].m_Key, &value, &valueType);
        hbassert(found);
        if(unique)
        {
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
        //btree->Validate();
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());

    btree->Validate();

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        HB_ASSERTONLY(const bool found =) btree->Find(kv[i].m_Key, &value, &valueType);
        hbassert(found);
        if(unique)
        {
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());

    if(KEYORDER_RANDOM == keyOrder)
    {
        std::sort(&kv[0], &kv[numKeys]);

        sw.Restart();
        for(int i = 0; i < numKeys; ++i)
        {
            hbverify(btree->Find(kv[i].m_Key, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
            }
        }
        sw.Stop();
        s_Log.Debug("sorted find: %f", sw.GetElapsed());
    }

    s_Log.Debug("utilization: %f", btree->GetUtilization());

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        HB_ASSERTONLY(const bool deleted =) btree->Delete(kv[i].m_Key);
        hbassert(deleted);
        //btree.Validate();
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());

    btree->Validate();

    BTree::Destroy(btree);

    delete [] kv;
}

void
BTreeTest::AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    BTree* btree = BTree::Create(m_KeyType);
    Value value;
    ValueType valueType;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = Rand() % numKeys;
        if(!kv[idx].m_Added)
        {
            btree->Insert(kv[idx].m_Key, kv[idx].m_Value, m_ValueType);
            //btree->Validate();
            kv[idx].m_Added = true;
            hbassert(btree->Find(kv[idx].m_Key, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[idx].m_Value, m_ValueType));
            }
        }
        else
        {
            const bool deleted = btree->Delete(kv[idx].m_Key);
            hbassert(deleted);
            //btree->Validate();
            kv[idx].m_Added = false;
            hbassert(!btree->Find(kv[idx].m_Key, &value, &valueType));
        }
    }

    btree->Validate();

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            const bool found = btree->Find(kv[i].m_Key, &value, &valueType);
            hbassert(found);
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
            }
        }
        else
        {
            hbassert(!btree->Find(kv[i].m_Key, &value, &valueType));
        }
    }

    //btree.DumpStats();

    btree->Validate();

    //btree.Dump(true);

    BTree::Destroy(btree);

    delete [] kv;
}

void
BTreeTest::AddDups(const int numKeys, const int min, const int max)
{
    BTree* btree = BTree::Create(KEYTYPE_INT);
    int range = max - min;
    Key key;
    Value value;
    if(0 == range)
    {
        key.Set(s64(min));
        for(int i = 0; i < numKeys; ++i)
        {
            value.m_Int = i;
            btree->Insert(key, value, VALUETYPE_INT);
        }
    }
    else
    {
        for(int i = 0; i < numKeys; ++i)
        {
            value.m_Int = i;
            key.Set(s64((i%range)+min));
            btree->Insert(key, value, VALUETYPE_INT);
        }
    }

    btree->Validate();

    if(0 == range)
    {
        Key key;
        key.Set(s64(min));
        for(int i = 0; i < numKeys; ++i)
        {
            hbverify(btree->Delete(key));
        }
    }
    else
    {
        for(int i = 0; i < numKeys; ++i)
        {
            Key key;
            key.Set(s64((i%range)+min));
            hbverify(btree->Delete(key));
            btree->Validate();
        }
    }

    btree->Validate();

    hbassert(0 == btree->Count());

    BTree::Destroy(btree);
}

///////////////////////////////////////////////////////////////////////////////
//  BTreeSpeedTest
///////////////////////////////////////////////////////////////////////////////
BTreeSpeedTest::BTreeSpeedTest(const KeyType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
BTreeSpeedTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    BTree* btree = BTree::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        btree->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    //std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        btree->Find(kv[i].m_Key, &value, &valueType);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    s_Log.Debug("utilization: %f", btree->GetUtilization());

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        btree->Delete(kv[i].m_Key);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    BTree::Destroy(btree);

    delete [] kv;
}

///////////////////////////////////////////////////////////////////////////////
//  SkipListTest
///////////////////////////////////////////////////////////////////////////////
SkipListTest::SkipListTest(const KeyType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
SkipListTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);
        hbverify(skiplist->Find(kv[i].m_Key, &value, &valueType));
        if(unique)
        {
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Find(kv[i].m_Key, &value, &valueType));
        if(unique)
        {
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    if(KEYORDER_RANDOM == keyOrder)
    {
        std::sort(&kv[0], &kv[numKeys]);

        sw.Restart();
        for(int i = 0; i < numKeys; ++i)
        {
            hbverify(skiplist->Find(kv[i].m_Key, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
            }
        }
        sw.Stop();
        s_Log.Debug("sorted find: %f", sw.GetElapsed());
    }

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Delete(kv[i].m_Key));
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    SkipList::Destroy(skiplist);

    delete [] kv;
}

void
SkipListTest::AddKeys2(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Insert2(kv[i].m_Key, kv[i].m_Value, m_ValueType);
        hbverify(skiplist->Find2(kv[i].m_Key, &value, &valueType));
        if(unique)
        {
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Find2(kv[i].m_Key, &value, &valueType));
        if(unique)
        {
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    s_Log.Debug("utilization: %f", skiplist->GetUtilization());

    if(KEYORDER_RANDOM == keyOrder)
    {
        std::sort(&kv[0], &kv[numKeys]);

        sw.Restart();
        for(int i = 0; i < numKeys; ++i)
        {
            hbverify(skiplist->Find2(kv[i].m_Key, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
            }
        }
        sw.Stop();
        s_Log.Debug("sorted find: %f", sw.GetElapsed());
    }

    /*sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Delete2(kv[i].m_Key));
    }
    sw.Stop();

    SkipList::Destroy(skiplist);*/

    delete [] kv;
}

void
SkipListTest::AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = Rand() % numKeys;
        if(!kv[idx].m_Added)
        {
            skiplist->Insert(kv[idx].m_Key, kv[idx].m_Value, m_ValueType);
            kv[idx].m_Added = true;
            hbverify(skiplist->Find(kv[idx].m_Key, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[idx].m_Value, m_ValueType));
            }
        }
        else
        {
            hbverify(skiplist->Delete(kv[idx].m_Key));
            kv[idx].m_Added = false;
            hbverify(!skiplist->Find(kv[idx].m_Key, &value, &valueType));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            hbverify(skiplist->Find(kv[i].m_Key, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
            }
        }
        else
        {
            hbverify(!skiplist->Find(kv[i].m_Key, &value, &valueType));
        }
    }

    //SkipList::Destroy(skiplist);

    delete [] kv;
}

///////////////////////////////////////////////////////////////////////////////
//  SkipListSpeedTest
///////////////////////////////////////////////////////////////////////////////
SkipListSpeedTest::SkipListSpeedTest(const KeyType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
SkipListSpeedTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    //std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Find(kv[i].m_Key, &value, &valueType);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    s_Log.Debug("utilization: %f", skiplist->GetUtilization());

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Delete(kv[i].m_Key);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    SkipList::Destroy(skiplist);

    delete [] kv;
}

void
SkipListSpeedTest::AddKeys2(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = new KV[numKeys];
    KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Insert2(kv[i].m_Key, kv[i].m_Value, m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    //std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Find2(kv[i].m_Key, &value, &valueType);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    s_Log.Debug("utilization: %f", skiplist->GetUtilization());

    /*sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Delete(kv[i].m_Key);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    SkipList::Destroy(skiplist);*/

    delete [] kv;
}

}   //namespace honeybase