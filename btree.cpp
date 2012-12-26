#include "btree.h"

#include <new.h>
#include <string.h>

namespace honeybase
{

static Log s_Log("btree");

#define UB 1

template<typename T>
static inline void MoveBytes(T* dst, const T* src, const size_t count)
{
    if(count > 0)
    {
        memmove(dst, src, count * sizeof(T));
    }
}

///////////////////////////////////////////////////////////////////////////////
//  BTreeIterator
///////////////////////////////////////////////////////////////////////////////
BTreeIterator::BTreeIterator()
{
    Clear();
}

BTreeIterator::~BTreeIterator()
{
}

void
BTreeIterator::Clear()
{
    m_Cur = NULL;
    m_ItemIndex = 0;
}

bool
BTreeIterator::HasNext() const
{
    if(m_Cur)
    {
        return (m_ItemIndex < m_Cur->m_NumKeys-1)
                || (NULL != m_Cur->m_Items[m_Cur->m_MaxKeys].m_Node);
    }

    return false;
}

bool
BTreeIterator::HasCurrent() const
{
    return NULL != m_Cur;
}

bool
BTreeIterator::Next()
{
    if(m_Cur)
    {
        ++m_ItemIndex;

        if(m_ItemIndex >= m_Cur->m_NumKeys)
        {
            m_Cur = m_Cur->m_Items[m_Cur->m_MaxKeys].m_Node;
            m_ItemIndex = 0;
        }
    }

    return (NULL != m_Cur);
}

Key
BTreeIterator::GetKey()
{
    return m_Cur->m_Keys[m_ItemIndex];
}

Value
BTreeIterator::GetValue()
{
    return m_Cur->m_Items[m_ItemIndex].m_Value;
}

ValueType
BTreeIterator::GetValueType()
{
    return m_Cur->m_Items[m_ItemIndex].m_ValueType;
}

///////////////////////////////////////////////////////////////////////////////
//  BTree
///////////////////////////////////////////////////////////////////////////////
#define TRIM_NODE   0
#define AUTO_DEFRAG 1

BTree::BTree(const KeyType keyType)
: m_Nodes(NULL)
, m_Leaves(NULL)
, m_Count(0)
, m_Capacity(0)
, m_Depth(0)
, m_KeyType(keyType)
{
}

BTree*
BTree::Create(const KeyType keyType)
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
BTree::Insert(const Key key, const Value value, const ValueType valueType)
{
    if(!m_Nodes)
    {
        m_Nodes = m_Leaves = AllocNode(BTreeNode::MAX_KEYS);
        if(!m_Nodes)
        {
            return false;
        }

        ++m_Depth;

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
            BTreeNode* newNode = AllocNode(BTreeNode::MAX_KEYS);

            if(!parent)
            {
                parent = m_Nodes = AllocNode(BTreeNode::MAX_KEYS);
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
                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc];
#else
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

        keyIdx = Bound(m_KeyType, key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        hbassert(keyIdx >= 0);

        if(!isLeaf)
        {
            parent = node;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

    MoveBytes(&node->m_Keys[keyIdx+1], &node->m_Keys[keyIdx], node->m_NumKeys-keyIdx);
    MoveBytes(&node->m_Items[keyIdx+1], &node->m_Items[keyIdx], node->m_NumKeys-keyIdx);

    node->m_Keys[keyIdx] = key;
    node->m_Items[keyIdx].m_Value = value;
    node->m_Items[keyIdx].m_ValueType = valueType;
    ++node->m_NumKeys;

    hbassert(node->m_NumKeys <= node->m_MaxKeys);

    ++m_Count;

    return true;
}

bool
BTree::Delete(const Key key)
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

        parentKeyIdx = Bound(m_KeyType, key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        parent = node;
        node = parent->m_Items[parentKeyIdx].m_Node;
    }

    int keyIdx = Bound(m_KeyType, key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);

#if UB
    if(keyIdx > 0 && keyIdx <= node->m_NumKeys)
#else
    if(keyIdx >= 0 && keyIdx < node->m_NumKeys)
#endif
    {
#if UB
        --keyIdx;
#endif
        if(key.EQ(m_KeyType, node->m_Keys[keyIdx]))
        {
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
                            parent->m_Items[parentKeyIdx].m_Node = parent->m_Items[parentKeyIdx+1].m_Node;

                            MoveBytes(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);
                            MoveBytes(&parent->m_Items[parentKeyIdx], &parent->m_Items[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx+1);
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
        Delete(m_Leaves->m_Keys[0]);
    }
}

bool
BTree::Find(const Key key, Value* value, ValueType* valueType) const
{
    const BTreeNode* node;
    const BTreeNode* parent;
    int keyIdx, parentKeyIdx;
    if(Find(key, &node, &keyIdx, &parent, &parentKeyIdx))
    {
        *value = node->m_Items[keyIdx].m_Value;
        *valueType = node->m_Items[keyIdx].m_ValueType;
        return true;
    }

    return false;
}

bool
BTree::GetFirst(BTreeIterator* it)
{
    if(m_Leaves)
    {
        it->m_Cur = m_Leaves;
        return true;
    }
    else
    {
        it->Clear();
    }

    return false;
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
BTree::Validate()
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
BTree::Find(const Key key,
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
BTree::Find(const Key key,
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
        keyIdx = Bound(m_KeyType, key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
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

        /*for(int i = 0; i < count; ++i)
        {
            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            parent->m_Keys[keyIdx-1] = node->m_Keys[0];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            MoveBytes(&node->m_Keys[0], &node->m_Keys[1], node->m_NumKeys-1);
            MoveBytes(&node->m_Items[0], &node->m_Items[1], node->m_NumKeys);
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }*/

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
            parent->m_Keys[keyIdx-1] = node->m_Keys[0];
#else
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

        /*for(int i = 0; i < count; ++i)
        {
            MoveBytes(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            MoveBytes(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-1];
            sibling->m_Items[0] = node->m_Items[node->m_NumKeys];
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }*/

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
            parent->m_Keys[keyIdx] = sibling->m_Keys[0];
#else
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
        node->m_Keys[i].Set(s64(0));
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
BTree::ValidateNode(const int /*depth*/, BTreeNode* /*node*/)
{
}

#else

void
BTree::ValidateNode(const int depth, BTreeNode* node)
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
        //Make sure all the keys in the child are <= the current key.
        for(int i = 0; i < node->m_NumKeys; ++i)
        {
            const Key key = node->m_Keys[i];
            const BTreeNode* child = node->m_Items[i].m_Node;
            for(int j = 0; j < child->m_NumKeys; ++j)
            {
#if ALLOW_DUPS
                hbassert(child->m_Keys[j].LE(m_KeyType, key));
#else
                hbassert(child->m_Keys[j].LT(m_KeyType, key));
#endif
            }

#if !UB
            if(i < node->m_NumKeys-1)
            {
                //If the right sibling's first key is the child's last key plus 1
                //then make sure the parent's key is equal to the child's last key
                //because the child's keys must be <= the parent's key
                const BTreeNode* nextChild = node->m_Items[i+1].m_Node;
                if(child->m_Keys[child->m_NumKeys-1]+1 == nextChild->m_Keys[0])
                {
                    hbassert(node->m_Keys[i] == child->m_Keys[child->m_NumKeys-1]);
                }
            }
#endif
        }

        //Make sure all the keys in the right sibling are >= keys in the left sibling.
        const Key key = node->m_Keys[node->m_NumKeys-1];
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
BTree::AllocNode(const int numKeys)
{
    const size_t size = sizeof(BTreeNode) + (numKeys*(sizeof(Key)+sizeof(BTreeItem)));
    BTreeNode* node = (BTreeNode*)Heap::ZAlloc(size);
    //BTreeNode* node = (BTreeNode*)Heap::ZAlloc(sizeof(BTreeNode));
    if(node)
    {
        const_cast<int&>(node->m_MaxKeys) = numKeys;
        m_Capacity += node->m_MaxKeys+1;

        node->m_Keys = (Key*)&node->m_Items[numKeys+1];
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

int
BTree::Bound(const KeyType keyType, const Key key, const Key* first, const Key* end)
{
#if UB
    return UpperBound(keyType, key, first, end);
#else
    return LowerBound(keyType, key, first, end);
#endif
}

int
BTree::LowerBound(const KeyType keyType, const Key key, const Key* first, const Key* end)
{
    //DUPS
    //if(end >= first)
    if(end > first)
    {
        const Key* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const Key* item = cur;
            size_t step = count >> 1;
            item += step;
            if(item->LT(keyType, key))
            {
                cur = ++item;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }

        return cur - first;
    }

    return -1;
}

int
BTree::UpperBound(const KeyType keyType, const Key key, const Key* first, const Key* end)
{
    //DUPS
    //if(end >= first)
    if(end > first)
    {
        const Key* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const Key* item = cur;
            size_t step = count >> 1;
            item += step;
            if(!(key.LT(keyType, *item)))
            {
                cur = ++item;
                count -= step + 1;
            }
            else
            {
                count = step;
            }
        }

        return cur - first;
    }

    return -1;
}

}   //namespace honeybase