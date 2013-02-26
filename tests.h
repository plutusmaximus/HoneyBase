#include "hb.h"

namespace honeybase
{

enum TestKeyOrder
{
    KEYORDER_RANDOM,
    KEYORDER_ASCENDING,
    KEYORDER_DESCENDING
};

class KV
{
public:
    static KV* CreateKeys(const KeyType keyType,
                            const size_t keySize,
                            const ValueType valueType,
                            const size_t valueSize,
                            const TestKeyOrder keyOrder,
                            const int numKeys);

    static void DestroyKeys(const KeyType keyType,
                            const ValueType valueType,
                            KV* kv,
                            const int numKeys);

    Key m_Key;
    Value m_Value;
    KeyType m_KeyType;
    ValueType m_ValueType;

    bool m_Added : 1;

    bool operator<(const KV& a) const
    {
        return m_Key.LT(m_KeyType, a.m_Key);
    }

    bool operator>(const KV& a) const
    {
        return m_Key.GT(m_KeyType, a.m_Key);
    }

private:

    KV()
    : m_KeyType(KEYTYPE_INT)
    , m_ValueType(VALUETYPE_INT)
    , m_Added(false)
    {
    }

    //~KV();
};

class HashTableTest
{
public:

    HashTableTest(const KeyType keyType, const ValueType valueType);

    void Test(const int numKeys);

    void TestMergeIntKeys(const int numKeys, const int numIterations);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class HashTableSpeedTest
{
public:

    HashTableSpeedTest(const KeyType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder);

    //void AddDeleteRandomKeys(const int numKeys);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class BTreeTest
{
public:

    BTreeTest(const KeyType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);
    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);
    void AddDups(const int numKeys, const int min, const int max);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class BTreeSpeedTest
{
public:

    BTreeSpeedTest(const KeyType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class SkipListTest
{
public:

    SkipListTest(const KeyType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class SkipListSpeedTest
{
public:

    SkipListSpeedTest(const KeyType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class SortedSetTest
{
public:

    SortedSetTest(const KeyType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class SortedSetSpeedTest
{
public:

    SortedSetSpeedTest(const KeyType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

}   //namespace honeybase