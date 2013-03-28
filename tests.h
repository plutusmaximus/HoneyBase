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
    static KV* CreateKeys(const ValueType keyType,
                            const size_t keySize,
                            const ValueType valueType,
                            const size_t valueSize,
                            const TestKeyOrder keyOrder,
                            const int numKeys);

    static KV* CreateRandomKeys(const size_t keySize,
                                const ValueType valueType,
                                const size_t valueSize,
                                const TestKeyOrder keyOrder,
                                const int numKeys);

    static void DestroyKeys(KV* kv,
                            const int numKeys);

    Value m_Key;
    Value m_Value;
    ValueType m_KeyType;
    ValueType m_ValueType;

    bool m_Added : 1;

    bool operator<(const KV& a) const
    {
        return m_KeyType < a.m_KeyType
                || (m_KeyType == a.m_KeyType && m_Key.LT(m_KeyType, a.m_Key));
    }

    bool operator>(const KV& a) const
    {
        return m_KeyType > a.m_KeyType
                || (m_KeyType == a.m_KeyType && m_Key.GT(m_KeyType, a.m_Key));
    }

private:

    KV()
    : m_KeyType(VALUETYPE_INT)
    , m_ValueType(VALUETYPE_INT)
    , m_Added(false)
    {
    }

    //~KV();
};

class HashTableTest
{
public:

    HashTableTest(const ValueType keyType, const ValueType valueType);

    void Test(const int numKeys);

    void TestMergeIntKeys(const int numKeys, const int numIterations);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
};

class HashTableSpeedTest
{
public:

    HashTableSpeedTest(const ValueType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder);

    //void AddDeleteRandomKeys(const int numKeys);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
};

class BTreeTest
{
public:

    BTreeTest(const ValueType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);
    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);
    void AddDups(const int numKeys, const int min, const int max);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
};

class BTreeSpeedTest
{
public:

    BTreeSpeedTest(const ValueType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
};

class SkipListTest
{
public:

    SkipListTest(const ValueType keyType, const ValueType valueType);

    SkipListTest(const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
    bool m_RandomKeyTypes   : 1;
};

class SkipListSpeedTest
{
public:

    SkipListSpeedTest(const ValueType keyType, const ValueType valueType);

    SkipListSpeedTest(const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
    bool m_RandomKeyTypes   : 1;
};

class SortedSetTest
{
public:

    SortedSetTest(const ValueType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
};

class SortedSetSpeedTest
{
public:

    SortedSetSpeedTest(const ValueType keyType, const ValueType valueType);

    void AddKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

    void AddDeleteKeys(const int numKeys, const TestKeyOrder keyOrder, const bool unique, const int range);

private:

    const ValueType m_KeyType;
    const ValueType m_ValueType;
};

}   //namespace honeybase