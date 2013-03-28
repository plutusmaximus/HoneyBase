#ifndef __HB_BTREE_H__
#define __HB_BTREE_H__

#include "hb.h"

namespace honeybase
{

class BTree;
class BTreeNode;
class SkipList;

class BTreeItem
{
public:

    union
    {
        Value m_Value;
        BTreeNode* m_Node;
        SkipList* m_SkipList;
    };

    ValueType m_ValueType   : 4;
    bool m_IsDup            : 1;
};

class BTreeNode
{
public:

    static const int MAX_KEYS = 96;

    BTreeNode* m_Prev;

    int m_NumKeys;
    const int m_MaxKeys;

    Value m_Keys[MAX_KEYS];
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

class BTreeIterator
{
    friend class BTree;

public:
    BTreeIterator();
    ~BTreeIterator();

    void Clear();

    bool GetValue(Value* value, ValueType* valueType) const;

    void Advance();

    bool operator==(const BTreeIterator& that) const;
    bool operator!=(const BTreeIterator& that) const;

private:

    void Init(const BTree* btree,
            const BTreeNode* node,
            const int index);

    const BTree* m_BTree;
    const BTreeNode* m_Node;
    int m_Index;

    BTreeIterator(const BTreeIterator&);
    BTreeIterator& operator=(const BTreeIterator&);
};

class BTree
{
public:

    static BTree* Create(const ValueType keyType);
    static void Destroy(BTree* btree);

    bool Insert(const Value key, const Value value, const ValueType valueType);

    bool Delete(const Value key, const Value value, const ValueType valueType);

    void DeleteAll();

    bool Find(const Value key, Value* value, ValueType* valueType) const;

    void Find(const Value startKey,
                const Value endKey,
                BTreeIterator* begin,
                BTreeIterator* end) const;

    ValueType GetKeyType() const
    {
        return m_KeyType;
    }

    u64 Count() const;

    double GetUtilization() const;

    void Validate() const;

private:

    bool Find(const Value key,
            const BTreeNode** outNode,
            int* outKeyIdx,
            const BTreeNode** outParent,
            int* outParentKeyIdx) const;
    bool Find(const Value key,
            BTreeNode** outNode,
            int* outKeyIdx,
            BTreeNode** outParent,
            int* outParentKeyIdx);

    void LowerBound(const Value key, BTreeNode** outNode, int* outKeyIdx) const;
    void UpperBound(const Value key, BTreeNode** outNode, int* outKeyIdx) const;

    void MergeLeft(BTreeNode* parent, const int keyIdx, const int count, const int depth);
    void MergeRight(BTreeNode* parent, const int keyIdx, const int count, const int depth);

    void TrimNode(BTreeNode* node, const int depth);

    void ValidateNode(const int depth, BTreeNode* node) const;

    BTreeNode* AllocNode();
    void FreeNode(BTreeNode* node);

    //int Bound(const Value key, const Value* first, const size_t numKeys) const;
    int LowerBound(const Value key, const Value* first, const size_t numKeys) const;
    int UpperBound(const Value key, const Value* first, const size_t numKeys) const;

    int Bound(const Value key, const ValueType valueType, const Value value,
            const Value* firstKey, const BTreeItem* firstItem, const size_t numKeys) const;
    int UpperBound(const Value key, const ValueType valueType, const Value value,
                    const Value* firstKey, const BTreeItem* firstItem, const size_t numKeys) const;


    BTreeNode* m_Nodes;

    BTreeNode* m_Leaves;

    int m_Depth;
    const ValueType m_KeyType;
    u64 m_Count;
    u64 m_Capacity;

    explicit BTree(const ValueType keyType);
    BTree();
    ~BTree();
    BTree(const BTree&);
    BTree& operator=(const BTree&);
};

}   //namespace honeybase

#endif  //__HB_BTREE_H__
