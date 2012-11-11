#ifndef __HB_BTREE_H__
#define __HB_BTREE_H__

#include "hb.h"

class HbIndexNode;

class HbIterator
{
    friend class HbIndex;
public:
    HbIterator();
    ~HbIterator();

    void Clear();

    bool HasNext() const;
    bool HasCurrent() const;

    bool Next();

    s64 GetKey();
    s64 GetValue();

private:

    HbIndexNode* m_Cur;
    int m_ItemIndex;
};

class HbIndexItem
{
public:

    HbIndexItem()
        : m_Value(0)
    {
    }

    union
    {
        s64 m_Value;
        HbIndexNode* m_Node;
    };
};

class HbIndexNode
{
public:

    static const int NUM_KEYS = 256;

    HbIndexNode();

    bool HasDups() const;

    s64 m_Keys[NUM_KEYS];
    HbIndexItem m_Items[NUM_KEYS+1];

    HbIndexNode* m_Prev;

    int m_NumKeys;
};

class HbIndex
{
public:

    HbIndex();

    bool Insert(const s64 key, const s64 value);

    bool Delete(const s64 key);

    bool Find(const s64 key, s64* value) const;

    bool GetFirst(HbIterator* it);

    u64 Count() const;

    void Validate();

private:

    bool Find(const s64 key,
            const HbIndexNode** outNode,
            int* outKeyIdx,
            const HbIndexNode** outParent,
            int* outParentKeyIdx) const;
    bool Find(const s64 key,
            HbIndexNode** outNode,
            int* outKeyIdx,
            HbIndexNode** outParent,
            int* outParentKeyIdx);

    void MergeLeft(HbIndexNode* parent, const int keyIdx, const int count, const int depth);
    void MergeRight(HbIndexNode* parent, const int keyIdx, const int count, const int depth);

    void TrimNode(HbIndexNode* node, const int depth);

    void ValidateNode(const int depth, HbIndexNode* node);

    HbIndexNode* AllocNode();
    void FreeNode(HbIndexNode* node);

    static int Bound(const s64 key, const s64* first, const s64* end);
    static int LowerBound(const s64 key, const s64* first, const s64* end);
    static int UpperBound(const s64 key, const s64* first, const s64* end);

    HbIndexNode* m_Nodes;

    HbIndexNode* m_Leaves;

    u64 m_Count;
    u64 m_Capacity;
    int m_Depth;
};

class HbIndexTest
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
    static void AddDeleteRandomKeys(const int numKeys, const bool unique, const int range);
    static void AddSortedKeys(const int numKeys, const bool unique, const int range, const bool ascending);
    static void AddDups(const int numKeys, const int min, const int max);
};

#endif  //__HB_BTREE_H__
