#ifndef __HB_BTREE_H__
#define __HB_BTREE_H__

#include "hb.h"

namespace honeybase
{

class BTreeNode;

class BTreeItem
{
public:

    union
    {
        Value m_Value;
        BTreeNode* m_Node;
    };

    ValueType m_ValueType   : 4;
};

class BTreeNode
{
public:

    static const int MAX_KEYS = 96;

    BTreeNode* m_Prev;

    int m_NumKeys;
    const int m_MaxKeys;

    Key m_Keys[MAX_KEYS];
    BTreeItem m_Items[MAX_KEYS+1];

    inline bool IsFull() const
    {
        return m_NumKeys == m_MaxKeys;
    }

private:
    BTreeNode();
    ~BTreeNode();
    BTreeNode(const BTreeNode&);
    BTreeNode& operator=(const BTreeNode&);
};

class BTree
{
public:

    enum ItemFlags
    {
        ITEMFLAG_COPY_KEY   = 0x01,
        ITEMFLAG_COPY_VALUE = 0x02,
    };

    static BTree* Create(const KeyType keyType);
    static void Destroy(BTree* btree);

    bool Insert(const Key& key, const Value& value, const ValueType valueType);//, const unsigned flags);

    bool Delete(const Key& key);

    void DeleteAll();

    bool Find(const Key& key, Value* value, ValueType* valueType) const;

    KeyType GetKeyType() const
    {
        return m_KeyType;
    }

    u64 Count() const;

    double GetUtilization() const;

    void Validate();

private:

    bool Find(const Key& key,
            const BTreeNode** outNode,
            int* outKeyIdx,
            const BTreeNode** outParent,
            int* outParentKeyIdx) const;
    bool Find(const Key& key,
            BTreeNode** outNode,
            int* outKeyIdx,
            BTreeNode** outParent,
            int* outParentKeyIdx);

    void MergeLeft(BTreeNode* parent, const int keyIdx, const int count, const int depth);
    void MergeRight(BTreeNode* parent, const int keyIdx, const int count, const int depth);

    void TrimNode(BTreeNode* node, const int depth);

    void ValidateNode(const int depth, BTreeNode* node);

    BTreeNode* AllocNode(const int numKeys);
    void FreeNode(BTreeNode* node);

    static int Bound(const KeyType keyType, const Key& key, const Key* first, const size_t numKeys);
    static int LowerBound(const KeyType keyType, const Key& key, const Key* first, const size_t numKeys);
    static int UpperBound(const KeyType keyType, const Key& key, const Key* first, const size_t numKeys);

    static int Bound(const KeyType keyType, const Key& key, const ValueType valueType, const Value& value,
                        const Key* firstKey, const BTreeItem* firstItem, const size_t numKeys);
    static int UpperBound(const KeyType keyType, const Key& key, const ValueType valueType, const Value& value,
                        const Key* firstKey, const BTreeItem* firstItem, const size_t numKeys);

    BTreeNode* m_Nodes;

    BTreeNode* m_Leaves;

    int m_Depth;
    const KeyType m_KeyType;
    u64 m_Count;
    u64 m_Capacity;

    explicit BTree(const KeyType keyType);
    BTree();
    ~BTree();
    BTree(const BTree&);
    BTree& operator=(const BTree&);
};

}   //namespace honeybase

#endif  //__HB_BTREE_H__
