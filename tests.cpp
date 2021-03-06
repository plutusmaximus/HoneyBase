#include "tests.h"

#include "btree.h"
#include "dict.h"
#include "skiplist.h"
#include "sortedset.h"

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

static bool EQ(const Value& a, ValueType valtypeA, const Value& b, const ValueType valtypeB)
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
            return 0 == a.m_Blob->Compare(b.m_Blob);
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

class KVDescendingPredicate
{
public:

    bool operator()(const KV& a, const KV& b)
    {
        return a.m_Key.GT(b.m_KeyType, b.m_Key);
    }
};

///////////////////////////////////////////////////////////////////////////////
//  KV
///////////////////////////////////////////////////////////////////////////////
KV*
KV::CreateKeys(const ValueType keyType,
                const size_t keySize,
                const ValueType valueType,
                const size_t valueSize,
                const TestKeyOrder keyOrder,
                const int numKeys)
{
    KV* kv = new KV[numKeys];

    if(kv)
    {
        if(VALUETYPE_INT == keyType)
        {
            if(KEYORDER_DESCENDING == keyOrder)
            {
                for(int i = numKeys-1; i >= 0; --i)
                {
                    kv[i].m_KeyType = keyType;
                    kv[i].m_Key.m_Int = i;
                }
            }
            else
            {
                for(int i = 0; i < numKeys; ++i)
                {
                    kv[i].m_KeyType = keyType;
                    kv[i].m_Key.m_Int = i;
                }
            }
        }
        else if(VALUETYPE_DOUBLE == keyType)
        {
            if(KEYORDER_DESCENDING == keyOrder)
            {
                for(int i = numKeys-1; i >= 0; --i)
                {
                    kv[i].m_KeyType = keyType;
                    kv[i].m_Key.m_Double = i / 3.14159;
                }
            }
            else
            {
                for(int i = 0; i < numKeys; ++i)
                {
                    kv[i].m_KeyType = keyType;
                    kv[i].m_Key.m_Double = i / 3.14159;
                }
            }
        }
        else if(VALUETYPE_BLOB == keyType)
        {
            const int numBytes = ((keySize*6)+7)/8;
            const int numWords = (numBytes+3)/4;
            const size_t sizeofData = numWords * sizeof(u32);
            char* keyBuf = (char*)malloc(keySize+1);
            for(int i = 0; i < numKeys; ++i)
            {
                CreateRandomString(keyBuf, keySize+1);
                kv[i].m_KeyType = keyType;
                kv[i].m_Key.m_Blob = Blob::Create((byte*)keyBuf, strlen(keyBuf));
            }

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
            char* valBuf = (char*)malloc(valueSize+1);
            for(int i = 0; i < numKeys; ++i)
            {
                CreateRandomString(valBuf, valueSize+1);
                kv[i].m_ValueType = valueType;
                kv[i].m_Value.m_Blob = Blob::Create((byte*)valBuf, strlen(valBuf));
            }

            free(valBuf);
        }

        if(KEYORDER_RANDOM == keyOrder)
        {
            std::random_shuffle(&kv[0], &kv[numKeys]);
        }
        else if(KEYORDER_DESCENDING == keyOrder && VALUETYPE_BLOB == keyType)
        {
            KVDescendingPredicate pred;
            std::sort(&kv[0], &kv[numKeys], pred);
        }
    }

    return kv;
}

KV*
KV::CreateRandomKeys(const size_t keySize,
                    const ValueType valueType,
                    const size_t valueSize,
                    const TestKeyOrder keyOrder,
                    const int numKeys)
{
    KV* kv = new KV[numKeys];

    if(kv)
    {
        const int numBytes = ((keySize*6)+7)/8;
        const int numWords = (numBytes+3)/4;
        const size_t sizeofData = numWords * sizeof(u32);
        char* keyBuf = (char*)malloc(keySize+1);

        for(int i = 0; i < numKeys; ++i)
        {
            const ValueType keyType = (ValueType)(Rand() % 3);

            kv[i].m_KeyType = keyType;

            switch(keyType)
            {
            case VALUETYPE_INT:
                kv[i].m_Key.m_Int = i;
                break;
            case VALUETYPE_DOUBLE:
                kv[i].m_Key.m_Double = i / 3.14159;
                break;
            case VALUETYPE_BLOB:
                CreateRandomString(keyBuf, keySize+1);
                kv[i].m_Key.m_Blob = Blob::Create((byte*)keyBuf, strlen(keyBuf));
                break;
            }
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
            char* valBuf = (char*)malloc(valueSize+1);
            for(int i = 0; i < numKeys; ++i)
            {
                CreateRandomString(valBuf, valueSize+1);
                kv[i].m_ValueType = valueType;
                kv[i].m_Value.m_Blob = Blob::Create((byte*)valBuf, strlen(valBuf));
            }

            free(valBuf);
        }

        if(KEYORDER_RANDOM == keyOrder)
        {
            std::random_shuffle(&kv[0], &kv[numKeys]);
        }
        else if(KEYORDER_DESCENDING == keyOrder)
        {
            KVDescendingPredicate pred;
            std::sort(&kv[0], &kv[numKeys], pred);
        }
    }

    return kv;
}

void
KV::DestroyKeys(KV* kv, const int numKeys)
{
    for(int i = 0; i < numKeys; ++i)
    {
        if(VALUETYPE_BLOB == kv[i].m_KeyType)
        {
            //Blob::Destroy(m_Key.m_Blob);
            kv[i].m_Key.m_Blob->Unref();
        }

        if(VALUETYPE_BLOB == kv[i].m_ValueType)
        {
            //Blob::Destroy(m_Value.m_Blob);
            kv[i].m_Value.m_Blob->Unref();
        }
    }

    delete [] kv;
}

///////////////////////////////////////////////////////////////////////////////
//  HashTableTest
///////////////////////////////////////////////////////////////////////////////
HashTableTest::HashTableTest(const ValueType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
HashTableTest::Test(const int numKeys)
{
    HashTable* ht = HashTable::Create();
    Value value;
    ValueType valueType;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, KEYORDER_RANDOM, numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        ht->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
        hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(ht->Clear(kv[i].m_Key, m_KeyType));
        hbverify(!ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
    }

    hbverify(0 == ht->Count());

    ht->Unref();

    KV::DestroyKeys(kv, numKeys);
}

struct KV_Patch
{
    static const int SECTION_LEN    = 256;
    Value m_Key;
    unsigned m_FinalLen;
    char m_Test[SECTION_LEN*3];
};

void
HashTableTest::TestMergeIntKeys(const int numKeys, const int numIterations)
{
    char alphabet[1024];
    CreateRandomString(alphabet, sizeof(alphabet));

    HashTable* ht = HashTable::Create();
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
            Blob* blob = Blob::Create((byte*)&alphabet[abOffset], len);
            const Blob* value = blob;
            /*byte blobBuf[KV_Patch::SECTION_LEN*2];
            const Blob* value =
                Blob::Encode((byte*)&alphabet[abOffset], len,
                            blobBuf, sizeof(blobBuf));*/

            hbverify(ht->Patch(p->m_Key, VALUETYPE_INT, 1, &value, &offset));
            blob->Unref();

            memcpy(&p->m_Test[offset], &alphabet[abOffset], len);
        }

        //std::random_shuffle(&kv[0], &kv[numKeys]);

        //Check they've been added
        p = kv;
        for(int i = 0; i < numKeys; ++i, ++p)
        {
            hbverify(ht->Find(p->m_Key, VALUETYPE_INT, &value, &valueType));
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
        hbverify(ht->Clear(p->m_Key, VALUETYPE_INT));
        hbverify(!ht->Find(p->m_Key, VALUETYPE_INT, &value, &valueType));
    }

    hbverify(0 == ht->Count());

    ht->Unref();

    delete [] kv;
}

void
HashTableTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder)
{
    HashTable* ht = HashTable::Create();
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool added = ht->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
        if(added)
        {
            hbverify(ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("set: %f", sw.GetElapsed());

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool found = ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
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
            hbverify(ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
        sw.Stop();
        s_Log.Debug("sorted find: %f", sw.GetElapsed());
    }

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool cleared = ht->Clear(kv[i].m_Key, m_KeyType);
        hbassert(cleared);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());

    ht->Unref();

    KV::DestroyKeys(kv, numKeys);
}

void
HashTableTest::AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder)
{
    HashTable* ht = HashTable::Create();
    Value value;
    ValueType valueType;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = Rand() % numKeys;
        if(!kv[idx].m_Added)
        {
            const bool added = ht->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
            if(added)
            {
                kv[idx].m_Added = true;
                hbverify(ht->Find(kv[idx].m_Key, m_KeyType, &value, &valueType));
                hbverify(EQ(value, valueType, kv[idx].m_Value, m_ValueType));
            }
        }
        else
        {
            const bool cleared = ht->Clear(kv[idx].m_Key, m_KeyType);
            hbassert(cleared);
            kv[idx].m_Added = false;
            hbverify(!ht->Find(kv[idx].m_Key, m_KeyType, &value, &valueType));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            const bool found = ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
            hbassert(found);
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
        else
        {
            const bool found = ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
            hbassert(!found);
        }
    }

    ht->Unref();

    KV::DestroyKeys(kv, numKeys);
}

///////////////////////////////////////////////////////////////////////////////
//  HashTableSpeedTest
///////////////////////////////////////////////////////////////////////////////
HashTableSpeedTest::HashTableSpeedTest(const ValueType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
HashTableSpeedTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder)
{
    HashTable* ht = HashTable::Create();
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    Blob::sm_StopWatch.Clear();
    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        ht->Set(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("set: %f", sw.GetElapsed());
    s_Log.Debug("compare: %f", Blob::sm_StopWatch.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    //std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        ht->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        ht->Clear(kv[i].m_Key, m_KeyType);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    ht->Unref();

    KV::DestroyKeys(kv, numKeys);
}

///////////////////////////////////////////////////////////////////////////////
//  BTreeTest
///////////////////////////////////////////////////////////////////////////////
BTreeTest::BTreeTest(const ValueType keyType, const ValueType valueType)
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

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        HB_ASSERTONLY(const bool added =) btree->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);
        hbassert(added);

        btree->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);
        btree->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);

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
        HB_ASSERTONLY(const bool deleted =) btree->Delete(kv[i].m_Key, kv[i].m_Value, kv[i].m_ValueType);
        hbassert(deleted);
        //btree.Validate();
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());

    btree->Validate();

    BTree::Destroy(btree);

    KV::DestroyKeys(kv, numKeys);
}

void
BTreeTest::AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    BTree* btree = BTree::Create(m_KeyType);
    Value value;
    ValueType valueType;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

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
            const bool deleted = btree->Delete(kv[idx].m_Key, kv[idx].m_Value, kv[idx].m_ValueType);
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

    KV::DestroyKeys(kv, numKeys);
}

void
BTreeTest::AddDups(const int numKeys, const int min, const int max)
{
    BTree* btree = BTree::Create(VALUETYPE_INT);
    int range = max - min;
    Value key;
    Value value;
    if(0 == range)
    {
        key.m_Int = min;
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
            key.m_Int = (i%range)+min;
            btree->Insert(key, value, VALUETYPE_INT);
        }
    }

    btree->Validate();

    /*if(0 == range)
    {
        Value key;
        key.m_Int = min;
        for(int i = 0; i < numKeys; ++i)
        {
            hbverify(btree->Delete(key));
        }
    }
    else
    {
        for(int i = 0; i < numKeys; ++i)
        {
            Value key;
            key.m_Int = (i%range)+min;
            hbverify(btree->Delete(key));
            btree->Validate();
        }
    }*/

    btree->Validate();

    hbassert(0 == btree->Count());

    BTree::Destroy(btree);
}

///////////////////////////////////////////////////////////////////////////////
//  BTreeSpeedTest
///////////////////////////////////////////////////////////////////////////////
BTreeSpeedTest::BTreeSpeedTest(const ValueType keyType, const ValueType valueType)
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

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    Blob::sm_StopWatch.Clear();
    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        btree->Insert(kv[i].m_Key, kv[i].m_Value, m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("compare: %f", Blob::sm_StopWatch.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    /*Value startKey, endKey;
    startKey.m_Int = 445;
    endKey.m_Int = 455;
    //btree->Find(startKey, &value, &valueType);
    //btree->Delete(startKey, value, valueType);
    BTreeIterator it, stop;
    btree->Find(endKey, startKey, &it, &stop);
    while(it != stop)
    {
        it.Advance();
    }*/

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
        btree->Delete(kv[i].m_Key, kv[i].m_Value, kv[i].m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    BTree::Destroy(btree);

    KV::DestroyKeys(kv, numKeys);
}

///////////////////////////////////////////////////////////////////////////////
//  SkipListTest
///////////////////////////////////////////////////////////////////////////////
SkipListTest::SkipListTest(const ValueType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
, m_RandomKeyTypes(false)
{
}

SkipListTest::SkipListTest(const ValueType valueType)
: m_KeyType(VALUETYPE_INT)
, m_ValueType(valueType)
, m_RandomKeyTypes(true)
{
}

void
SkipListTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = m_RandomKeyTypes
                ? KV::CreateRandomKeys(KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys)
                : KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Insert(kv[i].m_Key, kv[i].m_KeyType, kv[i].m_Value, m_ValueType);
        HB_ASSERTONLY(const bool found =) skiplist->Find(kv[i].m_Key, kv[i].m_KeyType, &value, &valueType);
        hbassert(found);
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
        HB_ASSERTONLY(const bool found =) skiplist->Find(kv[i].m_Key, kv[i].m_KeyType, &value, &valueType);
        hbassert(found);
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
            HB_ASSERTONLY(const bool found =) skiplist->Find(kv[i].m_Key, kv[i].m_KeyType, &value, &valueType);
            hbassert(found);
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
            }
        }
        sw.Stop();
        s_Log.Debug("sorted find: %f", sw.GetElapsed());
    }

    skiplist->Validate();

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Delete(kv[i].m_Key, kv[i].m_KeyType));
    }
    sw.Stop();

    SkipList::Destroy(skiplist);

    KV::DestroyKeys(kv, numKeys);
}

void
SkipListTest::AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = Rand() % numKeys;
        if(!kv[idx].m_Added)
        {
            skiplist->Insert(kv[idx].m_Key, m_KeyType, kv[idx].m_Value, m_ValueType);
            kv[idx].m_Added = true;
            hbverify(skiplist->Find(kv[idx].m_Key, m_KeyType, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[idx].m_Value, m_ValueType));
            }
        }
        else
        {
            hbverify(skiplist->Delete(kv[idx].m_Key, m_KeyType));
            kv[idx].m_Added = false;
            hbverify(!skiplist->Find(kv[idx].m_Key, m_KeyType, &value, &valueType));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            hbverify(skiplist->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
            if(unique)
            {
                hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
            }
        }
        else
        {
            hbverify(!skiplist->Find(kv[i].m_Key, m_KeyType, &value, &valueType));
        }
    }

    //SkipList::Destroy(skiplist);

    KV::DestroyKeys(kv, numKeys);
}

///////////////////////////////////////////////////////////////////////////////
//  SkipListSpeedTest
///////////////////////////////////////////////////////////////////////////////
SkipListSpeedTest::SkipListSpeedTest(const ValueType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
, m_RandomKeyTypes(false)
{
}

SkipListSpeedTest::SkipListSpeedTest(const ValueType valueType)
: m_KeyType(VALUETYPE_INT)
, m_ValueType(valueType)
, m_RandomKeyTypes(true)
{
}

void
SkipListSpeedTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SkipList* skiplist = SkipList::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = m_RandomKeyTypes
                ? KV::CreateRandomKeys(KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys)
                : KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Insert(kv[i].m_Key, m_KeyType, kv[i].m_Value, m_ValueType);
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    //std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Find(kv[i].m_Key, m_KeyType, &value, &valueType);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    s_Log.Debug("utilization: %f", skiplist->GetUtilization());

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        skiplist->Delete(kv[i].m_Key, m_KeyType);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    SkipList::Destroy(skiplist);

    KV::DestroyKeys(kv, numKeys);
}

///////////////////////////////////////////////////////////////////////////////
//  SortedSetTest
///////////////////////////////////////////////////////////////////////////////
SortedSetTest::SortedSetTest(const ValueType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
SortedSetTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SortedSet* set = SortedSet::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        set->Set(kv[i].m_Value, m_ValueType, kv[i].m_Key);
        HB_ASSERTONLY(const bool found =) set->Find(kv[i].m_Key, &value, &valueType);
        hbassert(found);
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
        HB_ASSERTONLY(const bool found =) set->Find(kv[i].m_Key, &value, &valueType);
        hbassert(found);
        if(unique)
        {
            hbverify(EQ(value, valueType, kv[i].m_Value, m_ValueType));
        }
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    s_Log.Debug("utilization: %f", set->GetUtilization());

    if(KEYORDER_RANDOM == keyOrder)
    {
        std::sort(&kv[0], &kv[numKeys]);

        sw.Restart();
        for(int i = 0; i < numKeys; ++i)
        {
            HB_ASSERTONLY(const bool found =) set->Find(kv[i].m_Key, &value, &valueType);
            hbassert(found);
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
        hbverify(set->Clear(kv[i].m_Value, kv[i].m_ValueType));
    }
    sw.Stop();

    SortedSet::Destroy(set);

    KV::DestroyKeys(kv, numKeys);
}

///////////////////////////////////////////////////////////////////////////////
//  SortedSetSpeedTest
///////////////////////////////////////////////////////////////////////////////
SortedSetSpeedTest::SortedSetSpeedTest(const ValueType keyType, const ValueType valueType)
: m_KeyType(keyType)
, m_ValueType(valueType)
{
}

void
SortedSetSpeedTest::AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range)
{
    SortedSet* set = SortedSet::Create(m_KeyType);
    Value value;
    ValueType valueType;

    StopWatch sw;

    KV* kv = KV::CreateKeys(m_KeyType, KEY_SIZE_BLOB, m_ValueType, VALUE_SIZE_BLOB, keyOrder, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        set->Set(kv[i].m_Value, m_ValueType, kv[i].m_Key);
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        set->Find(kv[i].m_Key, &value, &valueType);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    s_Log.Debug("ops/sec: %f", numKeys/sw.GetElapsed());

    s_Log.Debug("utilization: %f", set->GetUtilization());

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(set->Clear(kv[i].m_Value, kv[i].m_ValueType));
    }
    sw.Stop();

    SortedSet::Destroy(set);

    KV::DestroyKeys(kv, numKeys);
}

}   //namespace honeybase