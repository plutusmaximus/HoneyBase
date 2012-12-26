#include "hb.h"

namespace honeybase
{

class KV
{
public:
    static void CreateRandomKeys(const KeyType keyType,
                                const size_t keySize,
                                const ValueType valueType,
                                const size_t valueSize,
                                KV* kv,
                                const int numKeys);

    Key m_Key;
    Value m_Value;
    KeyType m_KeyType;
    ValueType m_ValueType;

    bool m_Added : 1;

    KV()
    : m_KeyType(KEYTYPE_INT)
    , m_ValueType(VALUETYPE_INT)
    , m_Added(false)
    {
    }

    ~KV();

    bool operator<(const KV& a) const
    {
        switch(m_KeyType)
        {
            case KEYTYPE_INT:
                return m_Key.m_Int < a.m_Key.m_Int;
            case KEYTYPE_DOUBLE:
                return m_Key.m_Double < a.m_Key.m_Double;
            case KEYTYPE_BLOB:
                return m_Key.m_Blob->LT(a.m_Key.m_Blob);
        }

        return m_Key.m_Int < a.m_Key.m_Int;
    }

    bool operator>(const KV& a) const
    {
        switch(m_KeyType)
        {
            case KEYTYPE_INT:
                return m_Key.m_Int > a.m_Key.m_Int;
            case KEYTYPE_DOUBLE:
                return m_Key.m_Double > a.m_Key.m_Double;
            case KEYTYPE_BLOB:
                return m_Key.m_Blob->GT(a.m_Key.m_Blob);
        }

        return m_Key.m_Int > a.m_Key.m_Int;
    }
};

class HashTableTest
{
public:

    HashTableTest(const KeyType keyType, const ValueType valueType);

    void Test(const int numKeys);

    void TestMergeIntKeys(const int numKeys, const int numIterations);

    void AddRandomKeys(const int numKeys);

    void AddDeleteRandomKeys(const int numKeys);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class HashTableSpeedTest
{
public:

    HashTableSpeedTest(const KeyType keyType, const ValueType valueType);

    void AddRandomKeys(const int numKeys);

    //void AddDeleteRandomKeys(const int numKeys);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class BTreeTest
{
public:

    BTreeTest(const KeyType keyType, const ValueType valueType);

    void AddRandomKeys(const int numKeys, const bool unique, const int range);
    void AddDeleteRandomKeys(const int numKeys, const bool unique, const int range);
    void AddSortedKeys(const int numKeys, const bool unique, const int range, const bool ascending);
    void AddDups(const int numKeys, const int min, const int max);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

class BTreeSpeedTest
{
public:

    BTreeSpeedTest(const KeyType keyType, const ValueType valueType);

    void AddRandomKeys(const int numKeys, const bool unique, const int range);

private:

    const KeyType m_KeyType;
    const ValueType m_ValueType;
};

}   //namespace honeybase