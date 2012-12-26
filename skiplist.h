#ifndef __HB_SKIPLIST_H__
#define __HB_SKIPLIST_H__

#include "hb.h"

namespace honeybase
{

class SkipNode;
class SkipList;

class SkipItem
{
    friend class SkipList;

    s64 m_Key;
    Value m_Value;
    ValueType m_ValType;
};

class SkipNode
{
public:
    static SkipNode* Create();
    static SkipNode* Create(const s64 key, const Blob* value);
    static SkipNode* Create(const s64 key, const s64 value);
    static void Destroy(SkipNode* node);

    static const int BLOCK_LEN  = 64;

    SkipItem m_Items[BLOCK_LEN];
    int m_NumItems;

    s64 m_Key;
    Value m_Value;
    int m_Height;

    ValueType m_ValType : 4;

    //This must be the last member in var in the class.
    SkipNode* m_Links[1];

private:

    SkipNode();
    ~SkipNode();
    SkipNode(const SkipNode&);
    SkipNode& operator=(const SkipNode&);
};

class SkipList
{
public:
    static const int MAX_HEIGHT = 32;

    bool Insert(const s64 key, const Blob* value);
    bool Insert(const s64 key, const s64 value);

    bool Insert2(const s64 key, const s64 value);

    bool Delete(const s64 key);

    bool Find(const s64 key, s64* value) const;
    bool Find(const s64 key, const Blob** value) const;

    bool Find2(const s64 key, s64* value) const;

    double GetUtilization() const;

    int m_Height;
    unsigned m_Count;
    unsigned m_Capacity;
    SkipNode* m_Head[MAX_HEIGHT];

private:
    
    bool Insert(SkipNode* node);
    const SkipNode* Find(const s64 key) const;

    static int LowerBound(const s64 key, const SkipItem* first, const SkipItem* end);
    static int UpperBound(const s64 key, const SkipItem* first, const SkipItem* end);

    SkipList();
    ~SkipList();
    SkipList(const SkipList&);
    SkipList& operator=(const SkipList&);
};

class SkipListTest
{
public:
    struct KV
    {
        int m_Key;
        int m_Value;
        bool m_Added : 1;

        KV()
        : m_Added(false)
        {
        }

        bool operator<(const KV& a) const
        {
            return m_Key < a.m_Key;
        }
    };

    static void CreateRandomKeys(KV* kv, const int numKeys, const bool unique, const int range);

    static void AddRandomKeys(const int numKeys, const bool unique, const int range);
    static void AddRandomKeys2(const int numKeys, const bool unique, const int range);

    static void AddDeleteRandomKeys(const int numKeys, const bool unique, const int range);
};

}   //namespace honeybase

#endif  //__HB_SKIPLIST_H__
