#include "btree.h"

#include <algorithm>
#include <string.h>

#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static HbLog s_Log("btree");

#define UB 1

template<typename T>
static inline void hbMove(T* dst, const T* src, const size_t count)
{
    if(count > 0)
    {
        memmove(dst, src, count * sizeof(T));
    }
}

///////////////////////////////////////////////////////////////////////////////
//  HbIterator
///////////////////////////////////////////////////////////////////////////////
HbIterator::HbIterator()
{
    Clear();
}

HbIterator::~HbIterator()
{
}

void
HbIterator::Clear()
{
    m_Cur = NULL;
    m_ItemIndex = 0;
}

bool
HbIterator::HasNext() const
{
    if(m_Cur)
    {
        return (m_ItemIndex < m_Cur->m_NumKeys-1)
                || (NULL != m_Cur->m_Items[HbIndexNode::NUM_KEYS].m_Node);
    }

    return false;
}

bool
HbIterator::HasCurrent() const
{
    return NULL != m_Cur;
}

bool
HbIterator::Next()
{
    if(m_Cur)
    {
        ++m_ItemIndex;

        if(m_ItemIndex >= m_Cur->m_NumKeys)
        {
            m_Cur = m_Cur->m_Items[HbIndexNode::NUM_KEYS].m_Node;
            m_ItemIndex = 0;
        }
    }

    return (NULL != m_Cur);
}

s64
HbIterator::GetKey()
{
    return m_Cur->m_Keys[m_ItemIndex];
}

s64
HbIterator::GetValue()
{
    return m_Cur->m_Items[m_ItemIndex].m_Value;
}

///////////////////////////////////////////////////////////////////////////////
//  HbIndexNode
///////////////////////////////////////////////////////////////////////////////
HbIndexNode::HbIndexNode()
: m_Prev(NULL)
, m_NumKeys(0)
{
    memset(m_Keys, 0, sizeof(m_Keys));
}

bool
HbIndexNode::HasDups() const
{
    return m_NumKeys >= 2
            && m_Keys[m_NumKeys-1] == m_Keys[m_NumKeys-2];
}

///////////////////////////////////////////////////////////////////////////////
//  HbIndex
///////////////////////////////////////////////////////////////////////////////
#define TRIM_NODE   0
#define AUTO_DEFRAG 1

HbIndex::HbIndex()
: m_Nodes(NULL)
, m_Leaves(NULL)
, m_Count(0)
, m_Capacity(0)
, m_Depth(0)
{
}

bool
HbIndex::Insert(const s64 key, const s64 value)
{
    if(!m_Nodes)
    {
        m_Nodes = m_Leaves = AllocNode();
        if(!m_Nodes)
        {
            return false;
        }

        ++m_Depth;

        m_Nodes->m_Keys[0] = key;
        m_Nodes->m_Items[0].m_Value = value;
        ++m_Nodes->m_NumKeys;

        ++m_Count;

        return true;
    }

    HbIndexNode* node = m_Nodes;
    HbIndexNode* parent = NULL;
    int keyIdx = 0;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        const bool isLeaf = (depth == m_Depth-1);

#if AUTO_DEFRAG
        if(HbIndexNode::NUM_KEYS == node->m_NumKeys && parent)
        {
            //If there's room in the left or right sibling
            //then move some items over.

            HbIndexNode* sibling = NULL;
            if(keyIdx > 0)
            {
                //Left sibling
                sibling = parent->m_Items[keyIdx-1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1)
                {
                    const int numToMove =
                        (HbIndexNode::NUM_KEYS - sibling->m_NumKeys) / 2;

                    //Move items over to the left sibling.
                    MergeLeft(parent, keyIdx, numToMove, depth);

#if UB
                    if(key < parent->m_Keys[keyIdx-1])
#else
                    if(key <= parent->m_Keys[keyIdx-1])
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
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1)
                {
                    const int numToMove =
                        (HbIndexNode::NUM_KEYS - sibling->m_NumKeys) / 2;

                    //Move items over to the right sibling.
                    MergeRight(parent, keyIdx, numToMove, depth);

#if UB
                    if(key >= parent->m_Keys[keyIdx])
#else
                    if(key > parent->m_Keys[keyIdx])
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

        if(HbIndexNode::NUM_KEYS == node->m_NumKeys)
        {
            //Split the node.

            //Guarantee the splitLoc will always be >= 2;
            //See comments below about numToCopy.
            hb_static_assert(HbIndexNode::NUM_KEYS >= 4);

            int splitLoc = HbIndexNode::NUM_KEYS / 2;

            HbIndexNode* newNode = AllocNode();
            const int numToCopy = HbIndexNode::NUM_KEYS-splitLoc;
            hbassert(numToCopy > 0);

            if(!parent)
            {
                parent = m_Nodes = AllocNode();
                parent->m_Items[0].m_Node = node;
                ++m_Depth;
                ++depth;
            }

            //Make room in the parent for a reference to the new node.
            hbassert(parent->m_NumKeys <= HbIndexNode::NUM_KEYS);
            hbMove(&parent->m_Keys[keyIdx+1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            hbMove(&parent->m_Items[keyIdx+2], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);

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
                HbIndexNode* next = node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
                newNode->m_Items[HbIndexNode::NUM_KEYS].m_Node = next;
                node->m_Items[HbIndexNode::NUM_KEYS].m_Node = newNode;
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

            if(key > parent->m_Keys[keyIdx])
            {
                node = newNode;
            }
        }

        keyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        hbassert(keyIdx >= 0);

        if(!isLeaf)
        {
            parent = node;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

    hbMove(&node->m_Keys[keyIdx+1], &node->m_Keys[keyIdx], node->m_NumKeys-keyIdx);
    hbMove(&node->m_Items[keyIdx+1], &node->m_Items[keyIdx], node->m_NumKeys-keyIdx);

    node->m_Keys[keyIdx] = key;
    node->m_Items[keyIdx].m_Value = value;
    ++node->m_NumKeys;

    ++m_Count;

    return true;
}

bool
HbIndex::Delete(const s64 key)
{
    HbIndexNode* node = m_Nodes;
    HbIndexNode* parent = NULL;
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

            HbIndexNode* sibling = NULL;
            HbIndexNode* leftSibling = parentKeyIdx > 0 ? parent->m_Items[parentKeyIdx-1].m_Node : NULL;
            HbIndexNode* rightSibling = parentKeyIdx < parent->m_NumKeys ? parent->m_Items[parentKeyIdx+1].m_Node : NULL;

            //If combined number of items is less than the maximum then merge with left sibling
            if(leftSibling && (node->m_NumKeys + leftSibling->m_NumKeys) < HbIndexNode::NUM_KEYS)
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
            else if(rightSibling && (node->m_NumKeys + rightSibling->m_NumKeys) < HbIndexNode::NUM_KEYS)
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

        parentKeyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        parent = node;
        node = parent->m_Items[parentKeyIdx].m_Node;
    }

    int keyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);

#if UB
    if(keyIdx > 0 && keyIdx <= node->m_NumKeys)
#else
    if(keyIdx >= 0 && keyIdx < node->m_NumKeys)
#endif
    {
#if UB
        --keyIdx;
#endif
        if(key == node->m_Keys[keyIdx])
        {
            if(node->m_NumKeys > 1)
            {
                //Remove the item from the leaf

                --node->m_NumKeys;

                hbMove(&node->m_Keys[keyIdx], &node->m_Keys[keyIdx+1], node->m_NumKeys-keyIdx);
                hbMove(&node->m_Items[keyIdx], &node->m_Items[keyIdx+1], node->m_NumKeys-keyIdx);

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
                    node->m_Prev->m_Items[HbIndexNode::NUM_KEYS].m_Node =
                        node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
                }

                if(node->m_Items[HbIndexNode::NUM_KEYS].m_Node)
                {
                    node->m_Items[HbIndexNode::NUM_KEYS].m_Node->m_Prev = node->m_Prev;
                }

                if(node == m_Leaves)
                {
                    //This was the first leaf - replace it with the next leaf
                    m_Leaves = node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
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

                            hbMove(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);
                            hbMove(&parent->m_Items[parentKeyIdx], &parent->m_Items[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx+1);
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

                        HbIndexNode* child;
                        child = (0 == parentKeyIdx)
                                ? parent->m_Items[1].m_Node
                                : parent->m_Items[0].m_Node;

                        hbMove(parent->m_Keys, child->m_Keys, child->m_NumKeys);
                        hbMove(parent->m_Items, child->m_Items, child->m_NumKeys);
                        parent->m_NumKeys = child->m_NumKeys;

                        hbassert(!child->m_Items[HbIndexNode::NUM_KEYS].m_Node);
                        hbassert(!child->m_Prev);
                        hbassert(m_Leaves == child);

                        parent->m_Items[HbIndexNode::NUM_KEYS].m_Node = NULL;
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

bool
HbIndex::Find(const s64 key, s64* value) const
{
    const HbIndexNode* node;
    const HbIndexNode* parent;
    int keyIdx, parentKeyIdx;
    if(Find(key, &node, &keyIdx, &parent, &parentKeyIdx))
    {
        *value = node->m_Items[keyIdx].m_Value;
        return true;
    }

    return false;
}

bool
HbIndex::GetFirst(HbIterator* it)
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
HbIndex::Count() const
{
    return m_Count;
}

double
HbIndex::GetUtilization() const
{
    return (m_Capacity > 0 ) ? (double)m_Count/m_Capacity : 0;
}

void
HbIndex::Validate()
{
    if(m_Nodes)
    {
        ValidateNode(0, m_Nodes);

        //Trace down the right edge of the tree and make sure
        //we reach the last node
        HbIndexNode* node = m_Nodes;
        for(int depth = 0; depth < m_Depth-1; ++depth)
        {
            node = node->m_Items[node->m_NumKeys].m_Node;
        }

        hbassert(!node->m_Items[HbIndexNode::NUM_KEYS].m_Node);

        unsigned count = 0;
        for(HbIndexNode* node = m_Leaves; node; node = node->m_Items[HbIndexNode::NUM_KEYS].m_Node)
        {
            count += node->m_NumKeys;
        }

        hbassert(count == m_Count);
    }
}

//private:

bool
HbIndex::Find(const s64 key,
                const HbIndexNode** outNode,
                int* outKeyIdx,
                const HbIndexNode** outParent,
                int* outParentKeyIdx) const
{
    HbIndexNode* tmpNode;
    HbIndexNode* tmpParent;
    const bool success =
        const_cast<HbIndex*>(this)->Find(key, &tmpNode, outKeyIdx, &tmpParent, outParentKeyIdx);
    *outNode = tmpNode;
    *outParent = tmpParent;
    return success;
}

bool
HbIndex::Find(const s64 key,
                HbIndexNode** outNode,
                int* outKeyIdx,
                HbIndexNode** outParent,
                int* outParentKeyIdx)
{
    HbIndexNode* node = m_Nodes;
    HbIndexNode* parent = NULL;
    int keyIdx = -1;
    int parentKeyIdx = -1;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        keyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        if(depth < m_Depth-1)
        {
            parent = node;
            parentKeyIdx = keyIdx;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

#if UB
    if(keyIdx > 0 && keyIdx <= node->m_NumKeys)
#else
    if(keyIdx >= 0 && keyIdx < node->m_NumKeys)
#endif
    {
#if UB
        --keyIdx;
#endif
        if(key == node->m_Keys[keyIdx])
        {
            *outNode = node;
            *outParent = parent;
            *outKeyIdx = keyIdx;
            *outParentKeyIdx = parentKeyIdx;
            return true;
        }
    }

    *outNode = *outParent = NULL;
    *outKeyIdx = *outParentKeyIdx = -1;
    return false;
}

void
HbIndex::MergeLeft(HbIndexNode* parent, const int keyIdx, const int count, const int depth)
{
    hbassert(keyIdx > 0);

    HbIndexNode* node = parent->m_Items[keyIdx].m_Node;
    //Left sibling
    HbIndexNode* sibling = parent->m_Items[keyIdx-1].m_Node;
    hbassert(node->m_NumKeys >= count);
    hbassert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

    if(node->m_NumKeys == count)
    {
        if(!hbverify(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1))
        {
            return;
        }
    }

    const bool isLeaf = (depth == m_Depth-1);

    if(!isLeaf)
    {
        sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
        parent->m_Keys[keyIdx-1] = node->m_Keys[count-1];
        hbMove(&sibling->m_Keys[sibling->m_NumKeys+1], &node->m_Keys[0], count-1);
        hbMove(&sibling->m_Items[sibling->m_NumKeys+1], &node->m_Items[0], count);
        hbMove(&node->m_Keys[0], &node->m_Keys[count], node->m_NumKeys-count);
        hbMove(&node->m_Items[0], &node->m_Items[count], node->m_NumKeys-count+1);
        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;

        /*for(int i = 0; i < count; ++i)
        {
            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            parent->m_Keys[keyIdx-1] = node->m_Keys[0];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            hbMove(&node->m_Keys[0], &node->m_Keys[1], node->m_NumKeys-1);
            hbMove(&node->m_Items[0], &node->m_Items[1], node->m_NumKeys);
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }*/

        if(0 == node->m_NumKeys)
        {
            hbassert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            ++sibling->m_NumKeys;
            hbMove(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            hbMove(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Move count items from the node to the left sibling
        hbMove(&sibling->m_Keys[sibling->m_NumKeys], &node->m_Keys[0], count);
        hbMove(&sibling->m_Items[sibling->m_NumKeys], &node->m_Items[0], count);

        hbMove(&node->m_Keys[0], &node->m_Keys[count], node->m_NumKeys-count);
        hbMove(&node->m_Items[0], &node->m_Items[count], node->m_NumKeys-count);

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
            hbMove(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            hbMove(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }

    if(0 == parent->m_NumKeys)
    {
        hbassert(parent == m_Nodes);

        HbIndexNode* child = parent->m_Items[0].m_Node;
        hbMove(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            hbMove(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            hbMove(parent->m_Items, child->m_Items, child->m_NumKeys+1);
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
HbIndex::MergeRight(HbIndexNode* parent, const int keyIdx, const int count, const int depth)
{
    hbassert(keyIdx < parent->m_NumKeys);

    HbIndexNode* node = parent->m_Items[keyIdx].m_Node;
    //Right sibling
    HbIndexNode* sibling = parent->m_Items[keyIdx+1].m_Node;
    hbassert(node->m_NumKeys >= count);
    hbassert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

    if(node->m_NumKeys == count)
    {
        if(!hbverify(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1))
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

        hbMove(&sibling->m_Keys[count], &sibling->m_Keys[0], sibling->m_NumKeys);
        hbMove(&sibling->m_Items[count], &sibling->m_Items[0], sibling->m_NumKeys+1);
        sibling->m_Keys[count-1] = parent->m_Keys[keyIdx];
        parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-count];
        hbMove(&sibling->m_Keys[0], &node->m_Keys[node->m_NumKeys-(count-1)], count-1);
        hbMove(&sibling->m_Items[0], &node->m_Items[node->m_NumKeys+1-count], count);
        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;

        /*for(int i = 0; i < count; ++i)
        {
            hbMove(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            hbMove(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-1];
            sibling->m_Items[0] = node->m_Items[node->m_NumKeys];
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }*/

        if(0 == node->m_NumKeys)
        {
            hbassert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

            hbMove(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            hbMove(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            sibling->m_Items[0] = node->m_Items[0];
            ++sibling->m_NumKeys;
            hbMove(&parent->m_Keys[keyIdx], &parent->m_Keys[keyIdx+1], parent->m_NumKeys-keyIdx);
            hbMove(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Make room in the right sibling for items from the node
        hbMove(&sibling->m_Keys[count], &sibling->m_Keys[0], sibling->m_NumKeys);
        hbMove(&sibling->m_Items[count], &sibling->m_Items[0], sibling->m_NumKeys);

        //Move count items from the node to the right sibling
        hbMove(&sibling->m_Keys[0], &node->m_Keys[node->m_NumKeys-count], count);
        hbMove(&sibling->m_Items[0], &node->m_Items[node->m_NumKeys-count], count);

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
            hbMove(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            hbMove(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }

    if(0 == parent->m_NumKeys)
    {
        hbassert(parent == m_Nodes);

        HbIndexNode* child = parent->m_Items[0].m_Node;
        hbMove(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            hbMove(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            hbMove(parent->m_Items, child->m_Items, child->m_NumKeys+1);
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
HbIndex::TrimNode(HbIndexNode* node, const int depth)
{
    const bool isLeaf = (depth == m_Depth-1);

    for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
    {
        node->m_Keys[i] = 0;
    }

    if(!isLeaf)
    {
        for(int i = node->m_NumKeys+1; i < HbIndexNode::NUM_KEYS+1; ++i)
        {
            node->m_Items[i].m_Value = 0;
        }
    }
    else
    {
        for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
        {
            node->m_Items[i].m_Value = 0;
        }
    }
}

#define ALLOW_DUPS  1

#if !HB_ASSERT
void
HbIndex::ValidateNode(const int /*depth*/, HbIndexNode* /*node*/)
{
}

#else

void
HbIndex::ValidateNode(const int depth, HbIndexNode* node)
{
    const bool isLeaf = ((m_Depth-1) == depth);

    if(!isLeaf)
    {
        for(int i = 1; i < node->m_NumKeys; ++i)
        {
#if ALLOW_DUPS
            hbassert(node->m_Keys[i] >= node->m_Keys[i-1]);
#else
            hbassert(node->m_Keys[i] > node->m_Keys[i-1]);
#endif
        }
    }
    else
    {
        //Allow duplicates in the leaves
        for(int i = 1; i < node->m_NumKeys; ++i)
        {
            hbassert(node->m_Keys[i] >= node->m_Keys[i-1]);
        }
    }

#if TRIM_NODE
    for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
    {
        hbassert(0 == node->m_Keys[i]);
        hbassert(node->m_Items[i].m_Node != node->m_Items[HbIndexNode::NUM_KEYS].m_Node
                || 0 == node->m_Items[i].m_Node);
    }
#endif

    if(!isLeaf)
    {
        //Make sure all the keys in the child are <= the current key.
        for(int i = 0; i < node->m_NumKeys; ++i)
        {
            const s64 key = node->m_Keys[i];
            const HbIndexNode* child = node->m_Items[i].m_Node;
            for(int j = 0; j < child->m_NumKeys; ++j)
            {
#if ALLOW_DUPS
                hbassert(child->m_Keys[j] <= key);
#else
                hbassert(child->m_Keys[j] < key);
#endif
            }

#if !UB
            if(i < node->m_NumKeys-1)
            {
                //If the right sibling's first key is the child's last key plus 1
                //then make sure the parent's key is equal to the child's last key
                //because the child's keys must be <= the parent's key
                const HbIndexNode* nextChild = node->m_Items[i+1].m_Node;
                if(child->m_Keys[child->m_NumKeys-1]+1 == nextChild->m_Keys[0])
                {
                    hbassert(node->m_Keys[i] == child->m_Keys[child->m_NumKeys-1]);
                }
            }
#endif
        }

        //Make sure all the keys in the right sibling are >= keys in the left sibling.
        const s64 key = node->m_Keys[node->m_NumKeys-1];
        const HbIndexNode* rightSibling = node->m_Items[node->m_NumKeys].m_Node;
        for(int j = 0; j < rightSibling->m_NumKeys; ++j)
        {
            hbassert(rightSibling->m_Keys[j] >= key);
        }

        /*for(int i = 0; i < node->m_NumKeys; ++i)
        {
            //Make sure we haven't split duplicates
            const HbIndexNode* a = node->m_Items[i].m_Node;
            const HbIndexNode* b = node->m_Items[i+1].m_Node;
            if(a->m_NumKeys < HbIndexNode::NUM_KEYS)
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

HbIndexNode*
HbIndex::AllocNode()
{
    HbIndexNode* node = new HbIndexNode();
    if(node)
    {
        m_Capacity += HbIndexNode::NUM_KEYS+1;
    }

    return node;
}

void
HbIndex::FreeNode(HbIndexNode* node)
{
    if(node)
    {
        m_Capacity -= HbIndexNode::NUM_KEYS+1;
        delete node;
    }
}

int
HbIndex::Bound(const s64 key, const s64* first, const s64* end)
{
#if UB
    return UpperBound(key, first, end);
#else
    return LowerBound(key, first, end);
#endif
}

int
HbIndex::LowerBound(const s64 key, const s64* first, const s64* end)
{
    if(end > first)
    {
        const s64* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const s64* item = cur;
            size_t step = count >> 1;
            item += step;
            if(*item < key)
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
HbIndex::UpperBound(const s64 key, const s64* first, const s64* end)
{
    if(end > first)
    {
        const s64* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const s64* item = cur;
            size_t step = count >> 1;
            item += step;
            if(!(key < *item))
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

///////////////////////////////////////////////////////////////////////////////
//  HbIndexTest
///////////////////////////////////////////////////////////////////////////////

static ptrdiff_t myrandom (ptrdiff_t i)
{
    return HbRand()%i;
}

void
HbIndexTest::CreateRandomKeys(KV* kv, const int numKeys, const bool unique, const int range)
{
    if(unique)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            kv[i].m_Key = i;
        }
        std::random_shuffle(&kv[0], &kv[numKeys], myrandom);
    }
    else
    {
        for(int i = 0; i < numKeys; ++i)
        {
            kv[i].m_Key = HbRand() % range;
        }
    }
}

void
HbIndexTest::AddRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbIndex index;
    HB_ASSERTONLY(s64 value);

    HbStopWatch sw;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        index.Insert(kv[i].m_Key, kv[i].m_Value);
        hbassert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
            hbassert(value == kv[i].m_Value);
        }
        //index.Validate();
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());

    //index.Validate();

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbassert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
            hbassert(value == kv[i].m_Value);
        }
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());

    s_Log.Debug("utilization: %f", index.GetUtilization());

    /*std::sort(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        hbassert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
            hbassert(value == kv[i].m_Value);
        }
    }*/

    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(index.Delete(kv[i].m_Key));
        //index.Validate();
    }

    //index.Validate();
    delete [] kv;
}

void
HbIndexTest::AddDeleteRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbIndex index;
    HB_ASSERTONLY(s64 value);

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
    }

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = HbRand() % numKeys;
        if(!kv[idx].m_Added)
        {
            index.Insert(kv[idx].m_Key, kv[idx].m_Value);
            //index.Validate();
            kv[idx].m_Added = true;
            hbassert(index.Find(kv[idx].m_Key, &value));
            if(unique)
            {
                hbassert(value == kv[idx].m_Value);
            }
        }
        else
        {
            hbverify(index.Delete(kv[idx].m_Key));
            //index.Validate();
            kv[idx].m_Added = false;
            hbassert(!index.Find(kv[idx].m_Key, &value));
        }
    }

//    index.Validate();

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            hbassert(index.Find(kv[i].m_Key, &value));
            if(unique)
            {
                hbassert(value == kv[i].m_Value);
            }
        }
        else
        {
            hbassert(!index.Find(kv[i].m_Key, &value));
        }
    }

    //index.DumpStats();

    //index.Validate();

    //index.Dump(true);
}

void
HbIndexTest::AddSortedKeys(const int numKeys, const bool unique, const int range, const bool ascending)
{
    HbIndex index;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        if(!ascending)
        {
            kv[i].m_Key = -kv[i].m_Key;
        }
    }

    std::sort(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Key = -kv[i].m_Key;
        index.Insert(kv[i].m_Key, kv[i].m_Value);
    }

    index.Validate();

    int count = 0;
    HbIterator it;
    for(bool b = index.GetFirst(&it); b; b = it.Next())
    {
        ++count;
    }
    hbassert(count == numKeys);
}

void
HbIndexTest::AddDups(const int numKeys, const int min, const int max)
{
    HbIndex index;
    int range = max - min;
    if(0 == range)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            index.Insert(min, i);
        }
    }
    else
    {
        for(int i = 0; i < numKeys; ++i)
        {
            index.Insert((i%range)+min, i);
        }
    }

    index.Validate();

    if(0 == range)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            hbverify(index.Delete(min));
        }
    }
    else
    {
        static int iii;
        for(int i = 0; i < numKeys; ++i)
        {
            if(19==i)
            {
                iii = 0;
            }
            hbverify(index.Delete((i%range)+min));
            index.Validate();
        }
    }

    index.Validate();

    hbassert(0 == index.Count());
}