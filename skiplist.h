#ifndef __HB_SKIPLIST_H__
#define __HB_SKIPLIST_H__

#include "hb.h"

class HbSkipNode;
class HbSkipList;

class HbSkipItem
{
    friend class HbSkipList;

    s64 m_Key;
    HbValue m_Value;
    HbValueType m_ValType;
};

class HbSkipNode
{
public:
    static HbSkipNode* Create();
    static HbSkipNode* Create(const s64 key, const HbString* value);
    static HbSkipNode* Create(const s64 key, const s64 value);
    static void Destroy(HbSkipNode* node);

    static const int BLOCK_LEN  = 64;

    HbSkipItem m_Items[BLOCK_LEN];
    int m_NumItems;

    s64 m_Key;
    HbValue m_Value;
    int m_Height;

    HbValueType m_ValType : 4;

    //This must be the last member in var in the class.
    HbSkipNode* m_Links[1];

private:

    HbSkipNode();
    ~HbSkipNode();
    HbSkipNode(const HbSkipNode&);
    HbSkipNode& operator=(const HbSkipNode&);
};

class HbSkipList
{
public:
    static const int MAX_HEIGHT = 32;

    bool Insert(const s64 key, const HbString* value);
    bool Insert(const s64 key, const s64 value);

    bool Insert2(const s64 key, const s64 value);

    bool Delete(const s64 key);

    bool Find(const s64 key, s64* value) const;
    bool Find(const s64 key, const HbString** value) const;

    bool Find2(const s64 key, s64* value) const;

    double GetUtilization() const;

    int m_Height;
    unsigned m_Count;
    unsigned m_Capacity;
    HbSkipNode* m_Head[MAX_HEIGHT];

private:
    
    bool Insert(HbSkipNode* node);
    const HbSkipNode* Find(const s64 key) const;

    static int LowerBound(const s64 key, const HbSkipItem* first, const HbSkipItem* end);
    static int UpperBound(const s64 key, const HbSkipItem* first, const HbSkipItem* end);

    HbSkipList();
    ~HbSkipList();
    HbSkipList(const HbSkipList&);
    HbSkipList& operator=(const HbSkipList&);
};

class HbSkipListTest
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

#endif  //__HB_SKIPLIST_H__
