#ifndef __HB_BTREE_H__
#define __HB_BTREE_H__

#include "hb.h"

class HbBTreeNode;

class HbIterator
{
    friend class HbBTree;
public:
    HbIterator();
    ~HbIterator();

    void Clear();

    bool HasNext() const;
    bool HasCurrent() const;

    bool Next();

    HbKey GetKey();
    s64 GetValue();

private:

    HbBTreeNode* m_Cur;
    int m_ItemIndex;
};

class HbBTreeItem
{
public:

    union
    {
        s64 m_Int;
        HbString* m_String;
        HbBTreeNode* m_Node;
    };
};

class HbBTreeNode
{
public:

    static const int MAX_KEYS = 96;

    HbBTreeNode* m_Prev;

    int m_NumKeys;
    const int m_MaxKeys;

    //FRACNODES
    HbKey* m_Keys;
    HbBTreeItem m_Items[1];
    //HbKey m_Keys[MAX_KEYS];
    //HbBTreeItem m_Items[MAX_KEYS+1];

    inline bool IsFull() const
    {
        return m_NumKeys == m_MaxKeys;
    }

private:
    HbBTreeNode();
    ~HbBTreeNode();
    HbBTreeNode(const HbBTreeNode&);
    HbBTreeNode& operator=(const HbBTreeNode&);
};

class HbBTree
{
public:

    static HbBTree* Create(const HbKeyType keyType);
    static void Destroy(HbBTree* btree);

    bool Insert(const HbKey key, const s64 value);

    bool Delete(const HbKey key);

    void DeleteAll();

    bool Find(const HbKey key, s64* value) const;

    bool GetFirst(HbIterator* it);

    HbKeyType GetKeyType() const
    {
        return m_KeyType;
    }

    u64 Count() const;

    double GetUtilization() const;

    void Validate();

private:

    bool Find(const HbKey key,
            const HbBTreeNode** outNode,
            int* outKeyIdx,
            const HbBTreeNode** outParent,
            int* outParentKeyIdx) const;
    bool Find(const HbKey key,
            HbBTreeNode** outNode,
            int* outKeyIdx,
            HbBTreeNode** outParent,
            int* outParentKeyIdx);

    void MergeLeft(HbBTreeNode* parent, const int keyIdx, const int count, const int depth);
    void MergeRight(HbBTreeNode* parent, const int keyIdx, const int count, const int depth);

    void TrimNode(HbBTreeNode* node, const int depth);

    void ValidateNode(const int depth, HbBTreeNode* node);

    HbBTreeNode* AllocNode(const int numKeys);
    void FreeNode(HbBTreeNode* node);

    static int Bound(const HbKeyType keyType, const HbKey key, const HbKey* first, const HbKey* end);
    static int LowerBound(const HbKeyType keyType, const HbKey key, const HbKey* first, const HbKey* end);
    static int UpperBound(const HbKeyType keyType, const HbKey key, const HbKey* first, const HbKey* end);

    HbBTreeNode* m_Nodes;

    HbBTreeNode* m_Leaves;

    u64 m_Count;
    u64 m_Capacity;
    int m_Depth;
    const HbKeyType m_KeyType;

    explicit HbBTree(const HbKeyType keyType);
    HbBTree();
    ~HbBTree();
    HbBTree(const HbBTree&);
    HbBTree& operator=(const HbBTree&);
};

class HbBTreeTest
{
public:
    struct KV
    {
        HbKey m_Key;
        s64 m_Value;
        bool m_Added : 1;

        KV()
        : m_Added(false)
        {
        }

        bool operator<(const KV& a) const
        {
            return m_Key.m_Int < a.m_Key.m_Int;
        }
    };

    static void CreateRandomKeys(const HbKeyType keyType, KV* kv, const int numKeys, const bool unique, const int range);
    static void AddRandomKeys(const int numKeys, const bool unique, const int range);
    static void AddDeleteRandomKeys(const int numKeys, const bool unique, const int range);
    static void AddSortedKeys(const int numKeys, const bool unique, const int range, const bool ascending);
    static void AddDups(const int numKeys, const int min, const int max);
};

#endif  //__HB_BTREE_H__
