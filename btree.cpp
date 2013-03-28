#include "btree.h"

#include <new.h>
#include <string.h>

#include "skiplist.h"

namespace honeybase
{

static Log s_Log("btree");

#define UB 0

template<typename T>
static inline void MoveBytes(T* dst, const T* src, const size_t count)
{
    if(count > 0)
    {
        memmove(dst, src, count * sizeof(T));
    }
}

#if UB
#define Bound(a, b, c) UpperBound(a, b, c)
#else
#define Bound(a, b, c) LowerBound(a, b, c)
#endif

///////////////////////////////////////////////////////////////////////////////
//  BTreeIterator
///////////////////////////////////////////////////////////////////////////////
BTreeIterator::BTreeIterator()
: m_BTree(NULL)
, m_Node(NULL)
, m_Index(-1)
{
}

BTreeIterator::~BTreeIterator()
{
}

void
BTreeIterator::Clear()
{
    m_BTree = NULL;
    m_Node = NULL;
    m_Index = -1;
}

void
BTreeIterator::Init(const BTree* btree, const BTreeNode* node, const int index)
{
    m_BTree = btree;
    m_Node = node;
    m_Index = index;
}

bool
BTreeIterator::GetValue(Value* value, ValueType* valueType) const
{
    if(m_Node)
    {
        *value = m_Node->m_Items[m_Index].m_Value;
        *valueType = m_Node->m_Items[m_Index].m_ValueType;
        return true;
    }

    return false;
}

void
BTreeIterator::Advance()
{
    if(m_Node)
    {
        ++m_Index;
        if(m_Index >= m_Node->m_NumKeys)
        {
            m_Node = m_Node->m_Items[m_Node->m_MaxKeys].m_Node;

            if(m_Node)
            {
                m_Index = 0;
            }
            else
            {
                m_Index = -1;
            }
        }
    }
}

bool
BTreeIterator::operator==(const BTreeIterator& that) const
{
    hbassert(m_BTree == that.m_BTree);

    return m_Node == that.m_Node
            && m_Index == that.m_Index;
}

bool
BTreeIterator::operator!=(const BTreeIterator& that) const
{
    hbassert(m_BTree == that.m_BTree);

    return m_Node != that.m_Node
            || m_Index != that.m_Index;
}

///////////////////////////////////////////////////////////////////////////////
//  BTree
///////////////////////////////////////////////////////////////////////////////
#define TRIM_NODE   0
#define AUTO_DEFRAG 1

BTree::BTree(const ValueType keyType)
: m_Nodes(NULL)
, m_Leaves(NULL)
, m_Count(0)
, m_Capacity(0)
, m_Depth(0)
, m_KeyType(keyType)
{
}

BTree*
BTree::Create(const ValueType keyType)
{
    BTree* btree = (BTree*) Heap::ZAlloc(sizeof(BTree));
    if(btree)
    {
        new (btree) BTree(keyType);
    }

    return btree;
}

void
BTree::Destroy(BTree* btree)
{
    if(btree)
    {
        btree->DeleteAll();
        Heap::Free(btree);
    }
}

bool
BTree::Insert(const Value key, const Value value, const ValueType valueType)
{
    if(!m_Nodes)
    {
        m_Nodes = m_Leaves = AllocNode();
        if(!m_Nodes)
        {
            return false;
        }

        ++m_Depth;

        if(VALUETYPE_BLOB == m_KeyType)
        {
            key.m_Blob->Ref();
        }

        if(VALUETYPE_BLOB == valueType)
        {
            value.m_Blob->Ref();
        }

        m_Nodes->m_Keys[0] = key;
        m_Nodes->m_Items[0].m_Value = value;
        m_Nodes->m_Items[0].m_ValueType = valueType;
        ++m_Nodes->m_NumKeys;

        ++m_Count;

        return true;
    }

    BTreeNode* node = m_Nodes;
    BTreeNode* parent = NULL;
    int keyIdx = 0;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        const bool isLeaf = (depth == m_Depth-1);

#if AUTO_DEFRAG
        if(node->IsFull() && parent)
        {
            //If there's room in the left or right sibling
            //then move some items over.

            BTreeNode* sibling = NULL;
            if(keyIdx > 0)
            {
                //Left sibling
                sibling = parent->m_Items[keyIdx-1].m_Node;
                if(sibling->m_NumKeys < sibling->m_MaxKeys-1)
                {
                    const int numToMove =
                        (sibling->m_MaxKeys - sibling->m_NumKeys) / 2;

                    //Move items over to the left sibling.
                    MergeLeft(parent, keyIdx, numToMove, depth);

#if UB
                    if(key.LT(m_KeyType, parent->m_Keys[keyIdx-1]))
#else
                    if(key.LE(m_KeyType, parent->m_Keys[keyIdx-1]))
#endif
                    {
                        --keyIdx;
                        node = sibling;
                    }
                }
                else
                {
                    sibling = NULL;
                }
            }

            if(!sibling && keyIdx < parent->m_NumKeys)
            {
                //right sibling
                sibling = parent->m_Items[keyIdx+1].m_Node;
                if(sibling->m_NumKeys < sibling->m_MaxKeys-1)
                {
                    const int numToMove =
                        (sibling->m_MaxKeys - sibling->m_NumKeys) / 2;

                    //Move items over to the right sibling.
                    MergeRight(parent, keyIdx, numToMove, depth);

#if UB
                    if(key.GE(m_KeyType, parent->m_Keys[keyIdx]))
#else
                    if(key.GT(m_KeyType, parent->m_Keys[keyIdx]))
#endif
                    {
                        ++keyIdx;
                        node = sibling;
                    }
                }
                else
                {
                    sibling = NULL;
                }
            }
        }
#endif  //AUTO_DEFRAG

        if(node->IsFull())
        {
            //Split the node.

            //Guarantee the splitLoc will always be >= 2;
            //See comments below about numToCopy.
            hb_static_assert(BTreeNode::MAX_KEYS >= 4);

            const int splitLoc = node->m_NumKeys / 2;

            const int numToCopy = node->m_NumKeys-splitLoc;
            hbassert(numToCopy > 0);
            BTreeNode* newNode = AllocNode();

            if(!parent)
            {
                parent = m_Nodes = AllocNode();
                parent->m_Items[0].m_Node = node;
                ++m_Depth;
                ++depth;
            }

            //Make room in the parent for a reference to the new node.
            hbassert(parent->m_NumKeys < parent->m_MaxKeys);
            MoveBytes(&parent->m_Keys[keyIdx+1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            MoveBytes(&parent->m_Items[keyIdx+2], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);

            if(isLeaf)
            {
                //k0 k1 k2 k3 k4 k5 k6 k7
                //v0 v1 v2 v3 v4 v5 v6 v7

                //k0 k1 k2 k3    k4 k5 k6 k7
                //v0 v1 v2 v3    v4 v5 v6 v7

                memcpy(newNode->m_Keys, &node->m_Keys[splitLoc], numToCopy * sizeof(node->m_Keys[0]));
                memcpy(newNode->m_Items, &node->m_Items[splitLoc], numToCopy * sizeof(node->m_Items[0]));
                newNode->m_NumKeys = numToCopy;
                node->m_NumKeys -= numToCopy;

                //Copy the last key in the node up into the parent.
#if UB
                if(VALUETYPE_BLOB == m_KeyType)
                {
                    node->m_Keys[splitLoc].m_Blob->Ref();
                }

                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc];
#else
                if(VALUETYPE_BLOB == m_KeyType)
                {
                    node->m_Keys[splitLoc-1].m_Blob->Ref();
                }

                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc-1];
#endif

                //Insert the new node into the linked list of nodes
                BTreeNode* next = node->m_Items[node->m_MaxKeys].m_Node;
                newNode->m_Items[newNode->m_MaxKeys].m_Node = next;
                node->m_Items[node->m_MaxKeys].m_Node = newNode;
                newNode->m_Prev = node;
                if(next)
                {
                    next->m_Prev = newNode;
                }
            }
            else
            {
                // k0 k1 k2 k3 k4 k5 k6 k7
                //v0 v1 v2 v3 v4 v5 v6 v7 v8

                // k0 k1 k2 k3    k4 k5 k6 k7
                //v0 v1 v2 v3    v4 v5 v6 v7 v8

                memcpy(newNode->m_Keys, &node->m_Keys[splitLoc], (numToCopy) * sizeof(node->m_Keys[0]));
                memcpy(newNode->m_Items, &node->m_Items[splitLoc], (numToCopy+1) * sizeof(node->m_Items[0]));
                newNode->m_NumKeys = numToCopy;
                //Subtract an extra one from m_NumKeys because we'll
                //rotate the last key from node up into parent.
                //This is also why NUM_KEYS must be >= 4.  It guarantees numToCopy+1
                //will always be >= 1.
                node->m_NumKeys -= numToCopy+1;

                //Copy the last key in the node up into the parent.
                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc-1];
            }

            parent->m_Items[keyIdx+1].m_Node = newNode;
            ++parent->m_NumKeys;

#if TRIM_NODE
            TrimNode(node, depth);
#endif

            if(key.GT(m_KeyType, parent->m_Keys[keyIdx]))
            {
                node = newNode;
            }
        }

        keyIdx = Bound(key, node->m_Keys, node->m_NumKeys);
        hbassert(keyIdx >= 0);

        if(!isLeaf)
        {
            parent = node;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

    /*if(keyIdx < node->m_NumKeys && node->m_Keys[keyIdx].EQ(m_KeyType, key))
    {
        if(node->m_Items[keyIdx].m_IsDup)
        {
            node->m_Items[keyIdx].m_SkipList->Insert(->Set(value, valueType, value, valueType);
        }
        else
        {
            HashTable* ht = HashTable::Create();
            if(ht)
            {
                ht->Set(value, valueType, value, valueType);
                const Value& curValue = node->m_Items[keyIdx].m_Value;
                const ValueType curType = node->m_Items[keyIdx].m_ValueType;
                ht->Set(curValue, curType, curValue, curType);
                node->m_Items[keyIdx].m_Ht = ht;
                node->m_Items[keyIdx].m_IsDup = true;
            }
            else
            {
                //*** ERROR
            }
        }
    }
    else*/
    {
        MoveBytes(&node->m_Keys[keyIdx+1], &node->m_Keys[keyIdx], node->m_NumKeys-keyIdx);
        MoveBytes(&node->m_Items[keyIdx+1], &node->m_Items[keyIdx], node->m_NumKeys-keyIdx);

        if(VALUETYPE_BLOB == m_KeyType)
        {
            key.m_Blob->Ref();
        }

        if(VALUETYPE_BLOB == valueType)
        {
            value.m_Blob->Ref();
        }

        node->m_Keys[keyIdx] = key;
        node->m_Items[keyIdx].m_Value = value;
        node->m_Items[keyIdx].m_ValueType = valueType;
        ++node->m_NumKeys;

        hbassert(node->m_NumKeys <= node->m_MaxKeys);
    }

    ++m_Count;

    return true;
}

bool
BTree::Delete(const Value key, const Value value, const ValueType valueType)
{
    BTreeNode* node = m_Nodes;
    BTreeNode* parent = NULL;
    int parentKeyIdx = -1;

    for(int depth = 0; depth < m_Depth-1; ++depth)
    {
        if(parent)
        {
            //1.  If the total number of items between the node and the neighbor
            //    is less than max then merge with the neighbor.
            //    Remove the newly empty node.
            //2.  If the current node has only 1 item borrow one from a neighbor.
            //    If the neighbor ends up empty then remove it.
            //In both cases we avoid traversing backwards up the tree to remove
            //empty nodes after removing a leaf.

            BTreeNode* sibling = NULL;
            BTreeNode* leftSibling = parentKeyIdx > 0 ? parent->m_Items[parentKeyIdx-1].m_Node : NULL;
            BTreeNode* rightSibling = parentKeyIdx < parent->m_NumKeys ? parent->m_Items[parentKeyIdx+1].m_Node : NULL;

            //If combined number of items is less than the maximum then merge with left sibling
            if(leftSibling && (node->m_NumKeys + leftSibling->m_NumKeys) < leftSibling->m_MaxKeys)
            {
                //Merge with the left sibling.
                //Move items from the node with fewer items into the
                //node with more items.
                sibling = leftSibling;
                if(node->m_NumKeys < leftSibling->m_NumKeys)
                {
                    MergeLeft(parent, parentKeyIdx, node->m_NumKeys, depth);
                }
                else if(parentKeyIdx > 0)
                {
                    MergeRight(parent, parentKeyIdx-1, sibling->m_NumKeys, depth);
                }
            }
            //If only one item try to borrow one from sibling
            else if(leftSibling && 1 == node->m_NumKeys)
            {
                sibling = leftSibling;
                MergeRight(parent, parentKeyIdx-1, 1, depth);
            }
            //If combined number of items is less than the maximum then merge with right sibling
            else if(rightSibling && (node->m_NumKeys + rightSibling->m_NumKeys) < rightSibling->m_MaxKeys)
            {
                //Merge with the right sibling
                //Move items from the node with fewer items into the
                //node with more items.
                sibling = rightSibling;
                if(node->m_NumKeys < rightSibling->m_NumKeys)
                {
                    MergeRight(parent, parentKeyIdx, node->m_NumKeys, depth);
                }
                else if(parentKeyIdx < parent->m_NumKeys)
                {
                    MergeLeft(parent, parentKeyIdx+1, sibling->m_NumKeys, depth);
                }
            }
            //If only one item try to borrow one from sibling
            else if(rightSibling && 1 == node->m_NumKeys)
            {
                //Borrow one from the right sibling
                sibling = rightSibling;
                MergeLeft(parent, parentKeyIdx+1, 1, depth);
            }

            //If the node and/or the sibling is now empty remove it/them.
            if(0 == node->m_NumKeys)
            {
                FreeNode(node);

                if(0 == sibling->m_NumKeys)
                {
                    FreeNode(sibling);
                    //Freeing the node and the sibling implies
                    //we've removed an entire level of nodes
                    //so reduce the depth.
                    node = parent;
                    --depth;
                }
                else
                {
                    node = sibling;
                }
            }
            else if(sibling && 0 == sibling->m_NumKeys)
            {
                FreeNode(sibling);
            }
        }

        parentKeyIdx = Bound(key, node->m_Keys, node->m_NumKeys);
        parent = node;
        node = parent->m_Items[parentKeyIdx].m_Node;
    }

    int keyIdx = Bound(key, node->m_Keys, node->m_NumKeys);

#if UB
    if(keyIdx > 0 && keyIdx <= node->m_NumKeys)
#else
    if(keyIdx >= 0 && keyIdx < node->m_NumKeys)
#endif
    {
#if UB
        --keyIdx;
#endif
        if(key.EQ(m_KeyType, node->m_Keys[keyIdx])
            && valueType == node->m_Items[keyIdx].m_ValueType
            && node->m_Items[keyIdx].m_Value.EQ(valueType, value))
        {
            if(VALUETYPE_BLOB == m_KeyType)
            {
                node->m_Keys[keyIdx].m_Blob->Unref();
            }

            if(VALUETYPE_BLOB == node->m_Items[keyIdx].m_ValueType)
            {
                node->m_Items[keyIdx].m_Value.m_Blob->Unref();
            }

            if(node->m_NumKeys > 1)
            {
                //Remove the item from the leaf

                --node->m_NumKeys;

                MoveBytes(&node->m_Keys[keyIdx], &node->m_Keys[keyIdx+1], node->m_NumKeys-keyIdx);
                MoveBytes(&node->m_Items[keyIdx], &node->m_Items[keyIdx+1], node->m_NumKeys-keyIdx);

#if TRIM_NODE
                TrimNode(node, m_Depth-1);
#endif
                //DO NOT SUBMIT
                /*if(parent)
                {
                    ValidateNode(m_Depth-2, parent);
                }*/
            }
            else
            {
                //Remove the entire leaf

                if(node->m_Prev)
                {
                    node->m_Prev->m_Items[node->m_Prev->m_MaxKeys].m_Node =
                        node->m_Items[node->m_MaxKeys].m_Node;
                }

                if(node->m_Items[node->m_MaxKeys].m_Node)
                {
                    node->m_Items[node->m_MaxKeys].m_Node->m_Prev = node->m_Prev;
                }

                if(node == m_Leaves)
                {
                    //This was the first leaf - replace it with the next leaf
                    m_Leaves = node->m_Items[node->m_MaxKeys].m_Node;
                }

                FreeNode(node);

                if(parent)
                {
                    //After removing the leaf adjust the parent.

                    --parent->m_NumKeys;

                    if(parent->m_NumKeys > 0)
                    {
                        if(parentKeyIdx <= parent->m_NumKeys)
                        {
                            if(VALUETYPE_BLOB == m_KeyType)
                            {
                                parent->m_Keys[parentKeyIdx].m_Blob->Unref();
                            }

                            parent->m_Items[parentKeyIdx].m_Node = parent->m_Items[parentKeyIdx+1].m_Node;

                            MoveBytes(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);
                            MoveBytes(&parent->m_Items[parentKeyIdx], &parent->m_Items[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx+1);
                        }
                        else if(VALUETYPE_BLOB == m_KeyType)
                        {
                            parent->m_Keys[parent->m_NumKeys].m_Blob->Unref();
                        }
#if TRIM_NODE
                        TrimNode(parent, m_Depth-2);
#endif
                        //DO NOT SUBMIT
                        //ValidateNode(m_Depth-2, parent);
                    }
                    else
                    {
                        //All keys have been deleted, but we still have
                        //a leaf.  Copy the leaf up into the parent
                        //and reduce the total depth of the tree, thus the
                        //parent becomes a leaf

                        hbassert(parent == m_Nodes);

                        if(VALUETYPE_BLOB == m_KeyType)
                        {
                            parent->m_Keys[0].m_Blob->Unref();
                        }

                        BTreeNode* child;
                        child = (0 == parentKeyIdx)
                                ? parent->m_Items[1].m_Node
                                : parent->m_Items[0].m_Node;

                        MoveBytes(parent->m_Keys, child->m_Keys, child->m_NumKeys);
                        MoveBytes(parent->m_Items, child->m_Items, child->m_NumKeys);
                        parent->m_NumKeys = child->m_NumKeys;

                        hbassert(!child->m_Items[child->m_MaxKeys].m_Node);
                        hbassert(!child->m_Prev);
                        hbassert(m_Leaves == child);

                        parent->m_Items[parent->m_MaxKeys].m_Node = NULL;
                        m_Leaves = parent;

                        FreeNode(child);
                        --m_Depth;

#if TRIM_NODE
                        TrimNode(parent, m_Depth-1);
#endif
                        //DO NOT SUBMIT
                        //ValidateNode(m_Depth-1, parent);
                    }
                }
            }

            --m_Count;

            if(0 == m_Count)
            {
                hbassert(node == m_Nodes);
                m_Nodes = NULL;
            }
            return true;
        }
    }

    return false;
}

void
BTree::DeleteAll()
{
    while(m_Leaves)
    {
        Delete(m_Leaves->m_Keys[0], m_Leaves->m_Items[0].m_Value, m_Leaves->m_Items[0].m_ValueType);
    }
}

bool
BTree::Find(const Value key, Value* value, ValueType* valueType) const
{
    const BTreeNode* node;
    const BTreeNode* parent;
    int keyIdx, parentKeyIdx;
    if(Find(key, &node, &keyIdx, &parent, &parentKeyIdx))
    {
        if(node->m_Items[keyIdx].m_IsDup)
        {
            //node->m_Items[keyIdx].m_Ht->Find(key
            return false;
        }
        else
        {
            *value = node->m_Items[keyIdx].m_Value;
            *valueType = node->m_Items[keyIdx].m_ValueType;
            return true;
        }
    }

    return false;
}

void
BTree::Find(const Value startKey,
            const Value endKey,
            BTreeIterator* begin,
            BTreeIterator* end) const
{
    if(startKey.LE(m_KeyType, endKey))
    {
        BTreeNode* startNode;
        BTreeNode* endNode;
        int startKeyIdx, endKeyIdx;
        LowerBound(startKey, &startNode, &startKeyIdx);
        UpperBound(endKey, &endNode, &endKeyIdx);
        begin->Init(this, startNode, startKeyIdx);
        end->Init(this, endNode, endKeyIdx);
    }
    else
    {
        begin->Clear();
        end->Clear();
    }
}

u64
BTree::Count() const
{
    return m_Count;
}

double
BTree::GetUtilization() const
{
    return (m_Capacity > 0 ) ? (double)m_Count/m_Capacity : 0;
}

void
BTree::Validate() const
{
    if(m_Nodes)
    {
        ValidateNode(0, m_Nodes);

        //Trace down the right edge of the tree and make sure
        //we reach the last node
        BTreeNode* node = m_Nodes;
        for(int depth = 0; depth < m_Depth-1; ++depth)
        {
            node = node->m_Items[node->m_NumKeys].m_Node;
        }

        hbassert(!node->m_Items[node->m_MaxKeys].m_Node);

        unsigned count = 0;
        for(BTreeNode* node = m_Leaves; node; node = node->m_Items[node->m_MaxKeys].m_Node)
        {
            count += node->m_NumKeys;
        }

        hbassert(count == m_Count);
    }
}

//private:

bool
BTree::Find(const Value key,
            const BTreeNode** outNode,
            int* outKeyIdx,
            const BTreeNode** outParent,
            int* outParentKeyIdx) const
{
    BTreeNode* tmpNode;
    BTreeNode* tmpParent;
    const bool success =
        const_cast<BTree*>(this)->Find(key, &tmpNode, outKeyIdx, &tmpParent, outParentKeyIdx);
    *outNode = tmpNode;
    *outParent = tmpParent;
    return success;
}

bool
BTree::Find(const Value key,
            BTreeNode** outNode,
            int* outKeyIdx,
            BTreeNode** outParent,
            int* outParentKeyIdx)
{
    BTreeNode* node = m_Nodes;
    BTreeNode* parent = NULL;
    int keyIdx = -1;
    int parentKeyIdx = -1;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        keyIdx = Bound(key, node->m_Keys, node->m_NumKeys);
        if(depth < m_Depth-1)
        {
            parent = node;
            parentKeyIdx = keyIdx;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

    hbassert(keyIdx <= node->m_NumKeys);

    bool found;

#if UB
    if(keyIdx > 0)
#else
    if(keyIdx >= 0)
#endif
    {
#if UB
        --keyIdx;
#endif
        found = key.EQ(m_KeyType, node->m_Keys[keyIdx]);
    }
    else
    {
        found = false;
    }

    *outNode = node;
    *outParent = parent;
    *outKeyIdx = keyIdx;
    *outParentKeyIdx = parentKeyIdx;
    return found;
}

void
BTree::LowerBound(const Value key,
                BTreeNode** outNode,
                int* outKeyIdx) const
{
    BTreeNode* node = m_Nodes;
    int keyIdx = -1;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        keyIdx = LowerBound(key, node->m_Keys, node->m_NumKeys);
        if(depth < m_Depth-1)
        {
            node = node->m_Items[keyIdx].m_Node;
        }
    }

    hbassert(keyIdx <= node->m_NumKeys);

    *outNode = node;
    *outKeyIdx = keyIdx;
}

void
BTree::UpperBound(const Value key,
                BTreeNode** outNode,
                int* outKeyIdx) const
{
    BTreeNode* node = m_Nodes;
    int keyIdx = -1;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        keyIdx = UpperBound(key, node->m_Keys, node->m_NumKeys);
        if(depth < m_Depth-1)
        {
            node = node->m_Items[keyIdx].m_Node;
        }
    }

    hbassert(keyIdx <= node->m_NumKeys);

    *outNode = node;
    *outKeyIdx = keyIdx;
}

void
BTree::MergeLeft(BTreeNode* parent, const int keyIdx, const int count, const int depth)
{
    hbassert(keyIdx > 0);

    BTreeNode* node = parent->m_Items[keyIdx].m_Node;
    //Left sibling
    BTreeNode* sibling = parent->m_Items[keyIdx-1].m_Node;
    hbassert(node->m_NumKeys >= count);
    hbassert(!sibling->IsFull());

    if(node->m_NumKeys == count)
    {
        if(!hbverify(sibling->m_NumKeys < sibling->m_MaxKeys-1))
        {
            return;
        }
    }

    const bool isLeaf = (depth == m_Depth-1);

    if(!isLeaf)
    {
        sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
        parent->m_Keys[keyIdx-1] = node->m_Keys[count-1];

        MoveBytes(&sibling->m_Keys[sibling->m_NumKeys+1], &node->m_Keys[0], count-1);
        MoveBytes(&sibling->m_Items[sibling->m_NumKeys+1], &node->m_Items[0], count);
        MoveBytes(&node->m_Keys[0], &node->m_Keys[count], node->m_NumKeys-count);
        MoveBytes(&node->m_Items[0], &node->m_Items[count], node->m_NumKeys-count+1);
        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;

        if(0 == node->m_NumKeys)
        {
            hbassert(!sibling->IsFull());

            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            ++sibling->m_NumKeys;
            MoveBytes(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            MoveBytes(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Move count items from the node to the left sibling
        MoveBytes(&sibling->m_Keys[sibling->m_NumKeys], &node->m_Keys[0], count);
        MoveBytes(&sibling->m_Items[sibling->m_NumKeys], &node->m_Items[0], count);

        MoveBytes(&node->m_Keys[0], &node->m_Keys[count], node->m_NumKeys-count);
        MoveBytes(&node->m_Items[0], &node->m_Items[count], node->m_NumKeys-count);

        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;

        if(node->m_NumKeys > 0)
        {
#if UB
            if(VALUETYPE_BLOB == m_KeyType)
            {
                parent->m_Keys[keyIdx-1].m_Blob->Unref();
                node->m_Keys[0].m_Blob->Ref();
            }
            parent->m_Keys[keyIdx-1] = node->m_Keys[0];
#else
            if(VALUETYPE_BLOB == m_KeyType)
            {
                parent->m_Keys[keyIdx-1].m_Blob->Unref();
                sibling->m_Keys[sibling->m_NumKeys-1].m_Blob->Ref();
            }
            parent->m_Keys[keyIdx-1] = sibling->m_Keys[sibling->m_NumKeys-1];
#endif
        }
        else
        {
            MoveBytes(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            MoveBytes(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }

    if(0 == parent->m_NumKeys)
    {
        hbassert(parent == m_Nodes);

        BTreeNode* child = parent->m_Items[0].m_Node;
        MoveBytes(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            MoveBytes(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            MoveBytes(parent->m_Items, child->m_Items, child->m_NumKeys+1);
        }

        parent->m_NumKeys = child->m_NumKeys;
        child->m_NumKeys = 0;
        --m_Depth;
    }

#if TRIM_NODE
    TrimNode(sibling, depth);
    TrimNode(node, depth);
    TrimNode(parent, depth-1);
#endif
    //DO NOT SUBMIT
    //ValidateNode(depth-1, parent);
}

void
BTree::MergeRight(BTreeNode* parent, const int keyIdx, const int count, const int depth)
{
    hbassert(keyIdx < parent->m_NumKeys);

    BTreeNode* node = parent->m_Items[keyIdx].m_Node;
    //Right sibling
    BTreeNode* sibling = parent->m_Items[keyIdx+1].m_Node;
    hbassert(node->m_NumKeys >= count);
    hbassert(!sibling->IsFull());

    if(node->m_NumKeys == count)
    {
        if(!hbverify(sibling->m_NumKeys < sibling->m_MaxKeys-1))
        {
            return;
        }
    }

    const bool isLeaf = (depth == m_Depth-1);

    if(!isLeaf)
    {
        //                    kp
        // k0 k1 k2 k3 k4        k6 k7
        //v0 v1 v2 v3 v4 v5     v6 v7 v8

        //             k2
        // k0 k1           k3 k4 kp k6 k7
        //v0 v1 v2        v3 v4 v5 v6 v7 v8

        MoveBytes(&sibling->m_Keys[count], &sibling->m_Keys[0], sibling->m_NumKeys);
        MoveBytes(&sibling->m_Items[count], &sibling->m_Items[0], sibling->m_NumKeys+1);
        sibling->m_Keys[count-1] = parent->m_Keys[keyIdx];
        parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-count];
        MoveBytes(&sibling->m_Keys[0], &node->m_Keys[node->m_NumKeys-(count-1)], count-1);
        MoveBytes(&sibling->m_Items[0], &node->m_Items[node->m_NumKeys+1-count], count);
        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;

        if(0 == node->m_NumKeys)
        {
            hbassert(!sibling->IsFull());

            MoveBytes(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            MoveBytes(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            sibling->m_Items[0] = node->m_Items[0];
            ++sibling->m_NumKeys;
            MoveBytes(&parent->m_Keys[keyIdx], &parent->m_Keys[keyIdx+1], parent->m_NumKeys-keyIdx);
            MoveBytes(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Make room in the right sibling for items from the node
        MoveBytes(&sibling->m_Keys[count], &sibling->m_Keys[0], sibling->m_NumKeys);
        MoveBytes(&sibling->m_Items[count], &sibling->m_Items[0], sibling->m_NumKeys);

        //Move count items from the node to the right sibling
        MoveBytes(&sibling->m_Keys[0], &node->m_Keys[node->m_NumKeys-count], count);
        MoveBytes(&sibling->m_Items[0], &node->m_Items[node->m_NumKeys-count], count);

        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;

        if(node->m_NumKeys > 0)
        {
#if UB
            if(VALUETYPE_BLOB == m_KeyType)
            {
                parent->m_Keys[keyIdx].m_Blob->Unref();
                sibling->m_Keys[0].m_Blob->Ref();
            }
            parent->m_Keys[keyIdx] = sibling->m_Keys[0];
#else
            if(VALUETYPE_BLOB == m_KeyType)
            {
                parent->m_Keys[keyIdx].m_Blob->Unref();
                node->m_Keys[node->m_NumKeys-1].m_Blob->Ref();
            }
            parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-1];
#endif
        }
        else
        {
            MoveBytes(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            MoveBytes(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }

    if(0 == parent->m_NumKeys)
    {
        hbassert(parent == m_Nodes);

        BTreeNode* child = parent->m_Items[0].m_Node;
        MoveBytes(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            MoveBytes(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            MoveBytes(parent->m_Items, child->m_Items, child->m_NumKeys+1);
        }

        parent->m_NumKeys = child->m_NumKeys;
        child->m_NumKeys = 0;
        --m_Depth;
    }

#if TRIM_NODE
    TrimNode(sibling, depth);
    TrimNode(node, depth);
    TrimNode(parent, depth-1);
#endif
    //DO NOT SUBMIT
    //ValidateNode(depth-1, parent);
}

void
BTree::TrimNode(BTreeNode* node, const int depth)
{
    const bool isLeaf = (depth == m_Depth-1);

    for(int i = node->m_NumKeys; i < node->m_MaxKeys; ++i)
    {
        node->m_Keys[i].m_Int = 0;
    }

    if(!isLeaf)
    {
        for(int i = node->m_NumKeys+1; i < node->m_MaxKeys+1; ++i)
        {
            node->m_Items[i].m_Value.m_Int = 0;
            node->m_Items[i].m_ValueType = VALUETYPE_INT;
        }
    }
    else
    {
        for(int i = node->m_NumKeys; i < node->m_MaxKeys; ++i)
        {
            node->m_Items[i].m_Value.m_Int = 0;
            node->m_Items[i].m_ValueType = VALUETYPE_INT;
        }
    }
}

#define ALLOW_DUPS  0

#if !HB_ASSERT
void
BTree::ValidateNode(const int /*depth*/, BTreeNode* /*node*/) const
{
}

#else

void
BTree::ValidateNode(const int depth, BTreeNode* node) const
{
    const bool isLeaf = ((m_Depth-1) == depth);

    if(!isLeaf)
    {
        for(int i = 1; i < node->m_NumKeys; ++i)
        {
#if ALLOW_DUPS
            hbassert(node->m_Keys[i].GE(m_KeyType, node->m_Keys[i-1]));
#else
            hbassert(node->m_Keys[i].GT(m_KeyType, node->m_Keys[i-1]));
#endif
        }
    }
    else
    {
        //Allow duplicates in the leaves
        for(int i = 1; i < node->m_NumKeys; ++i)
        {
            hbassert(node->m_Keys[i].GE(m_KeyType, node->m_Keys[i-1]));
        }
    }

#if TRIM_NODE
    for(int i = node->m_NumKeys; i < node->m_MaxKeys; ++i)
    {
        hbassert(0 == node->m_Keys[i]);
        hbassert(node->m_Items[i].m_Node != node->m_Items[node->m_MaxKeys].m_Node
                || 0 == node->m_Items[i].m_Node);
    }
#endif

    if(!isLeaf)
    {
        //Make sure all the keys in the child are < the current key.
        //(or <= the current key if using LowerBound)
        for(int i = 0; i < node->m_NumKeys; ++i)
        {
            const Value key = node->m_Keys[i];
            const BTreeNode* child = node->m_Items[i].m_Node;
            for(int j = 0; j < child->m_NumKeys; ++j)
            {
#if ALLOW_DUPS
                hbassert(child->m_Keys[j].LE(m_KeyType, key));
#elif UB
                hbassert(child->m_Keys[j].LT(m_KeyType, key));
#else
                hbassert(child->m_Keys[j].LE(m_KeyType, key));
#endif
            }

#if !UB
            if(i < node->m_NumKeys-1 && GetKeyType() == VALUETYPE_INT)
            {
                //If the right sibling's first key is the child's last key plus 1
                //then make sure the parent's key is equal to the child's last key
                //because the child's keys must be <= the parent's key
                const BTreeNode* nextChild = node->m_Items[i+1].m_Node;
                if(child->m_Keys[child->m_NumKeys-1].m_Int+1 == nextChild->m_Keys[0].m_Int)
                {
                    hbassert(node->m_Keys[i].m_Int == child->m_Keys[child->m_NumKeys-1].m_Int);
                }
            }
#endif
        }

        //Make sure all the keys in the right sibling are >= keys in the left sibling.
        const Value key = node->m_Keys[node->m_NumKeys-1];
        const BTreeNode* rightSibling = node->m_Items[node->m_NumKeys].m_Node;
        for(int j = 0; j < rightSibling->m_NumKeys; ++j)
        {
            hbassert(rightSibling->m_Keys[j].GE(m_KeyType, key));
        }

        /*for(int i = 0; i < node->m_NumKeys; ++i)
        {
            //Make sure we haven't split duplicates
            const BTreeNode* a = node->m_Items[i].m_Node;
            const BTreeNode* b = node->m_Items[i+1].m_Node;
            if(a->m_NumKeys < a->m_MaxKeys)
            {
                hbassert(a->m_Keys[a->m_NumKeys-1] != b->m_Keys[0]);
            }
        }*/

        for(int i = 0; i < node->m_NumKeys+1; ++i)
        {
            ValidateNode(depth+1, node->m_Items[i].m_Node);
        }
    }
}

#endif  //HB_ASSERT

BTreeNode*
BTree::AllocNode()
{
    BTreeNode* node = (BTreeNode*)Heap::ZAlloc(sizeof(BTreeNode));
    if(node)
    {
        const_cast<int&>(node->m_MaxKeys) = BTreeNode::MAX_KEYS;
        m_Capacity += node->m_MaxKeys+1;
    }

    return node;
}

void
BTree::FreeNode(BTreeNode* node)
{
    if(node)
    {
        m_Capacity -= node->m_MaxKeys+1;
        Heap::Free(node);
    }
}

/*int
BTree::Bound(const Value key, const Value* first, const size_t numKeys) const
{
#if UB
    return UpperBound(key, first, numKeys);
#else
    return LowerBound(key, first, numKeys);
#endif
}*/

int
BTree::LowerBound(const Value key, const Value* first, const size_t numKeys) const
{
    const Value* cur = first;
    size_t count = numKeys;
    while(count > 0)
    {
        const size_t step = count >> 1;
        const Value* nextKey = cur + step;
        if(nextKey->LT(m_KeyType, key))
        {
            cur = nextKey + 1;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }

    return cur - first;
}

int
BTree::UpperBound(const Value key, const Value* first, const size_t numKeys) const
{
    const Value* cur = first;
    size_t count = numKeys;
    while(count > 0)
    {
        size_t step = count >> 1;
        const Value* nextKey = cur + step;
        if(!key.LT(m_KeyType, *nextKey))
        {
            cur = nextKey + 1;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }

    return cur - first;
}

static bool LT(const ValueType keyType, const Value keyA, const Value keyB,
                const ValueType valueTypeA, const Value valueA,
                const ValueType valueTypeB, const Value valueB)
{
    int cmp = keyA.Compare(keyType, keyB);

    if(0 == cmp)
    {
        if(valueTypeA == valueTypeB)
        {
            cmp = valueA.Compare(valueTypeA, valueB);
        }
        else
        {
            cmp = int(valueTypeA - valueTypeB);
        }
    }

    return cmp < 0;
}

int
BTree::UpperBound(const Value key, const ValueType valueType, const Value value,
                    const Value* firstKey, const BTreeItem* firstItem, const size_t numKeys) const
{
    const Value* curKey = firstKey;
    const BTreeItem* curItem = firstItem;
    size_t count = numKeys;
    while(count > 0)
    {
        const Value* nextKey = curKey;
        const BTreeItem* nextItem = curItem;
        size_t step = count >> 1;
        nextKey += step;
        nextItem += step;
        if(!LT(m_KeyType, key, *nextKey, valueType, value, nextItem->m_ValueType, nextItem->m_Value))
        {
            curKey = ++nextKey;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }

    return curKey - firstKey;
}

}   //namespace honeybase