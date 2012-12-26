#ifndef __HB_BTREE_H__
#define __HB_BTREE_H__

#include "hb.h"

namespace honeybase
{

class BTreeNode;

class BTreeIterator
{
    friend class BTree;
public:
    BTreeIterator();
    ~BTreeIterator();

    void Clear();

    bool HasNext() const;
    bool HasCurrent() const;

    bool Next();

    Key GetKey();
    Value GetValue();
    ValueType GetValueType();

private:

    BTreeNode* m_Cur;
    int m_ItemIndex;
};

class BTreeItem
{
public:

    union
    {
        Value m_Value;
        BTreeNode* m_Node;
    };

    ValueType m_ValueType;
};

class BTreeNode
{
public:

    static const int MAX_KEYS = 96;

    BTreeNode* m_Prev;

    int m_NumKeys;
    const int m_MaxKeys;

    //FRACNODES
    Key* m_Keys;
    BTreeItem m_Items[1];
    //Key m_Keys[MAX_KEYS];
    //BTreeItem m_Items[MAX_KEYS+1];

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

    static BTree* Create(const KeyType keyType);
    static void Destroy(BTree* btree);

    bool Insert(const Key key, const Value value, const ValueType valueType);

    bool Delete(const Key key);

    void DeleteAll();

    bool Find(const Key key, Value* value, ValueType* valueType) const;

    bool GetFirst(BTreeIterator* it);

    KeyType GetKeyType() const
    {
        return m_KeyType;
    }

    u64 Count() const;

    double GetUtilization() const;

    void Validate();

private:

    bool Find(const Key key,
            const BTreeNode** outNode,
            int* outKeyIdx,
            const BTreeNode** outParent,
            int* outParentKeyIdx) const;
    bool Find(const Key key,
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

    static int Bound(const KeyType keyType, const Key key, const Key* first, const Key* end);
    static int LowerBound(const KeyType keyType, const Key key, const Key* first, const Key* end);
    static int UpperBound(const KeyType keyType, const Key key, const Key* first, const Key* end);

    BTreeNode* m_Nodes;

    BTreeNode* m_Leaves;

    u64 m_Count;
    u64 m_Capacity;
    int m_Depth;
    const KeyType m_KeyType;

    explicit BTree(const KeyType keyType);
    BTree();
    ~BTree();
    BTree(const BTree&);
    BTree& operator=(const BTree&);
};

}   //namespace honeybase

#endif  //__HB_BTREE_H__
