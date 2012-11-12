#include "btree.h"

#include <algorithm>
#include <assert.h>
#include <string.h>

#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define UB 1
template<typename T>
static inline void Move(T* dst, const T* src, const size_t count)
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
            HbIndexNode* sibling = NULL;
            if(keyIdx > 0)
            {
                //Left sibling
                sibling = parent->m_Items[keyIdx-1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1
                    //Don't split dups
                    && node->m_Keys[0] != node->m_Keys[1])
                {
                    MergeLeft(parent, keyIdx, 1, depth);

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
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1
                    //Don't split dups
                    && node->m_Keys[node->m_NumKeys-1] != node->m_Keys[node->m_NumKeys-2])
                {
                    MergeRight(parent, keyIdx, 1, depth);

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
            STATIC_ASSERT(HbIndexNode::NUM_KEYS >= 4);

            int splitLoc = HbIndexNode::NUM_KEYS / 2;

            //If we have dups, don't split the dups.
            /*if(splitLoc < HbIndexNode::NUM_KEYS-1)
            {
                if(isLeaf)
                {
                    if(node->m_Keys[splitLoc] == node->m_Keys[splitLoc-1])
                    {
                        splitLoc += Bound(node->m_Keys[splitLoc],
                                            &node->m_Keys[splitLoc],
                                            &node->m_Keys[HbIndexNode::NUM_KEYS]);
                        assert(splitLoc <= HbIndexNode::NUM_KEYS);

                        //If the dups extend to the end of the node it's ok to split
                        //on the last dup because the item currently being inserted
                        //will fill the hole.
                        if(splitLoc == HbIndexNode::NUM_KEYS)
                        {
                            --splitLoc;
                        }
                    }
                }
                else
                {
                    if(node->m_Keys[splitLoc] == node->m_Keys[splitLoc+1])
                    {
                        splitLoc += Bound(node->m_Keys[splitLoc],
                                            &node->m_Keys[splitLoc],
                                            &node->m_Keys[HbIndexNode::NUM_KEYS]);
                        assert(splitLoc <= HbIndexNode::NUM_KEYS);

                        //If the dups extend to the end of the node it's ok to split
                        //on the last dup because the item currently being inserted
                        //will fill the hole.
                        if(splitLoc == HbIndexNode::NUM_KEYS)
                        {
                            --splitLoc;
                        }
                    }
                }
            }*/

            HbIndexNode* newNode = AllocNode();
            const int numToCopy = HbIndexNode::NUM_KEYS-splitLoc;
            assert(numToCopy > 0);

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
            }

            if(!parent)
            {
                parent = m_Nodes = AllocNode();
                parent->m_Items[0].m_Node = node;
                ++m_Depth;
                ++depth;
            }

            assert(parent->m_NumKeys <= HbIndexNode::NUM_KEYS);
            Move(&parent->m_Keys[keyIdx+1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx+2], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);

            if(isLeaf)
            {
#if UB
                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc];
#else
                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc-1];
#endif

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
        assert(keyIdx >= 0);

        if(!isLeaf)
        {
            parent = node;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

    Move(&node->m_Keys[keyIdx+1], &node->m_Keys[keyIdx], node->m_NumKeys-keyIdx);
    Move(&node->m_Items[keyIdx+1], &node->m_Items[keyIdx], node->m_NumKeys-keyIdx);

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
        if(parent)// && parent->m_NumKeys > 1)
        {
            HbIndexNode* sibling = NULL;
            HbIndexNode* leftSibling = parentKeyIdx > 0 ? parent->m_Items[parentKeyIdx-1].m_Node : NULL;
            HbIndexNode* rightSibling = parentKeyIdx < parent->m_NumKeys ? parent->m_Items[parentKeyIdx+1].m_Node : NULL;
            if(leftSibling && (node->m_NumKeys + leftSibling->m_NumKeys) < HbIndexNode::NUM_KEYS)
            {
                //Merge with the left sibling
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
            else if(leftSibling && 1 == node->m_NumKeys)
            {
                //Borrow one from the left sibling
                sibling = leftSibling;
                //DO NOT SUBMIT
                if(leftSibling->m_NumKeys >= 2
                    && leftSibling->m_Keys[leftSibling->m_NumKeys-1] == leftSibling->m_Keys[leftSibling->m_NumKeys-2])
                {
                    //splitting dups
                    OutputDebugString("HERE\n");
                }
                MergeRight(parent, parentKeyIdx-1, 1, depth);
            }
            else if(rightSibling && (node->m_NumKeys + rightSibling->m_NumKeys) < HbIndexNode::NUM_KEYS)
            {
                //Merge with the right sibling
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
            else if(rightSibling && 1 == node->m_NumKeys)
            {
                //Borrow one from the right sibling
                sibling = rightSibling;
                //DO NOT SUBMIT
                if(rightSibling->m_NumKeys >= 2
                    && rightSibling->m_Keys[0] == rightSibling->m_Keys[1])
                {
                    //splitting dups
                    OutputDebugString("HERE\n");
                }
                MergeLeft(parent, parentKeyIdx+1, 1, depth);
            }

            if(0 == node->m_NumKeys)
            {
                FreeNode(node);

                if(0 == sibling->m_NumKeys)
                {
                    FreeNode(sibling);
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
                --node->m_NumKeys;

                Move(&node->m_Keys[keyIdx], &node->m_Keys[keyIdx+1], node->m_NumKeys-keyIdx);
                Move(&node->m_Items[keyIdx], &node->m_Items[keyIdx+1], node->m_NumKeys-keyIdx);

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
                    m_Leaves = node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
                }

                FreeNode(node);

                if(parent)
                {
                    --parent->m_NumKeys;

                    if(parent->m_NumKeys > 0)
                    {
                        if(parentKeyIdx <= parent->m_NumKeys)
                        {
                            parent->m_Items[parentKeyIdx].m_Node = parent->m_Items[parentKeyIdx+1].m_Node;

                            Move(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);
                            Move(&parent->m_Items[parentKeyIdx], &parent->m_Items[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx+1);
                        }
#if TRIM_NODE
                        TrimNode(parent, m_Depth-2);
#endif
                        //DO NOT SUBMIT
                        //ValidateNode(m_Depth-2, parent);
                    }
                    else
                    {
                        assert(parent == m_Nodes);

                        HbIndexNode* child;
                        child = (0 == parentKeyIdx)
                                ? parent->m_Items[1].m_Node
                                : parent->m_Items[0].m_Node;

                        Move(parent->m_Keys, child->m_Keys, child->m_NumKeys);
                        Move(parent->m_Items, child->m_Items, child->m_NumKeys);
                        parent->m_NumKeys = child->m_NumKeys;

                        assert(!child->m_Items[HbIndexNode::NUM_KEYS].m_Node);
                        parent->m_Items[HbIndexNode::NUM_KEYS].m_Node = NULL;
                        if(child->m_Prev)
                        {
                            child->m_Prev->m_Items[HbIndexNode::NUM_KEYS].m_Node = NULL;
                        }

                        if(child == m_Leaves)
                        {
                            m_Leaves = parent;
                        }

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
                assert(node == m_Nodes);
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

        assert(!node->m_Items[HbIndexNode::NUM_KEYS].m_Node);

        unsigned count = 0;
        for(HbIndexNode* node = m_Leaves; node; node = node->m_Items[HbIndexNode::NUM_KEYS].m_Node)
        {
            count += node->m_NumKeys;
        }

        assert(count == m_Count);
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
    assert(keyIdx > 0);

    HbIndexNode* node = parent->m_Items[keyIdx].m_Node;
    //Left sibling
    HbIndexNode* sibling = parent->m_Items[keyIdx-1].m_Node;
    assert(node->m_NumKeys >= count);
    assert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

    if(node->m_NumKeys == count)
    {
        if(!hbVerify(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1))
        {
            return;
        }
    }

    const bool isLeaf = (depth == m_Depth-1);

    if(!isLeaf)
    {
        for(int i = 0; i < count; ++i)
        {
            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            parent->m_Keys[keyIdx-1] = node->m_Keys[0];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            Move(&node->m_Keys[0], &node->m_Keys[1], node->m_NumKeys-1);
            Move(&node->m_Items[0], &node->m_Items[1], node->m_NumKeys);
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }

        if(0 == node->m_NumKeys)
        {
            assert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            ++sibling->m_NumKeys;
            Move(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Move count items from the node to the left sibling
        Move(&sibling->m_Keys[sibling->m_NumKeys], &node->m_Keys[0], count);
        Move(&sibling->m_Items[sibling->m_NumKeys], &node->m_Items[0], count);

        Move(&node->m_Keys[0], &node->m_Keys[count], node->m_NumKeys-count);
        Move(&node->m_Items[0], &node->m_Items[count], node->m_NumKeys-count);

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
            Move(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }

    if(0 == parent->m_NumKeys)
    {
        assert(parent == m_Nodes);

        HbIndexNode* child = parent->m_Items[0].m_Node;
        Move(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys+1);
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
    assert(keyIdx < parent->m_NumKeys);

    HbIndexNode* node = parent->m_Items[keyIdx].m_Node;
    //Right sibling
    HbIndexNode* sibling = parent->m_Items[keyIdx+1].m_Node;
    assert(node->m_NumKeys >= count);
    assert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

    if(node->m_NumKeys == count)
    {
        if(!hbVerify(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1))
        {
            return;
        }
    }

    const bool isLeaf = (depth == m_Depth-1);

    if(!isLeaf)
    {
        for(int i = 0; i < count; ++i)
        {
            Move(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            Move(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-1];
            sibling->m_Items[0] = node->m_Items[node->m_NumKeys];
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }

        if(0 == node->m_NumKeys)
        {
            assert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

            Move(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            Move(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            sibling->m_Items[0] = node->m_Items[0];
            ++sibling->m_NumKeys;
            Move(&parent->m_Keys[keyIdx], &parent->m_Keys[keyIdx+1], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Make room in the right sibling for items from the node
        Move(&sibling->m_Keys[count], &sibling->m_Keys[0], sibling->m_NumKeys);
        Move(&sibling->m_Items[count], &sibling->m_Items[0], sibling->m_NumKeys);

        //Move count items from the node to the right sibling
        Move(&sibling->m_Keys[0], &node->m_Keys[node->m_NumKeys-count], count);
        Move(&sibling->m_Items[0], &node->m_Items[node->m_NumKeys-count], count);

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
            Move(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }

    if(0 == parent->m_NumKeys)
    {
        assert(parent == m_Nodes);

        HbIndexNode* child = parent->m_Items[0].m_Node;
        Move(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys+1);
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
            assert(node->m_Keys[i] >= node->m_Keys[i-1]);
#else
            assert(node->m_Keys[i] > node->m_Keys[i-1]);
#endif
        }
    }
    else
    {
        //Allow duplicates in the leaves
        for(int i = 1; i < node->m_NumKeys; ++i)
        {
            assert(node->m_Keys[i] >= node->m_Keys[i-1]);
        }
    }

#if TRIM_NODE
    for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
    {
        assert(0 == node->m_Keys[i]);
        assert(node->m_Items[i].m_Node != node->m_Items[HbIndexNode::NUM_KEYS].m_Node
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
                assert(child->m_Keys[j] <= key);
#else
                assert(child->m_Keys[j] < key);
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
                    assert(node->m_Keys[i] == child->m_Keys[child->m_NumKeys-1]);
                }
            }
#endif
        }

        //Make sure all the keys in the right sibling are >= keys in the left sibling.
        const s64 key = node->m_Keys[node->m_NumKeys-1];
        const HbIndexNode* rightSibling = node->m_Items[node->m_NumKeys].m_Node;
        for(int j = 0; j < rightSibling->m_NumKeys; ++j)
        {
            assert(rightSibling->m_Keys[j] >= key);
        }

        /*for(int i = 0; i < node->m_NumKeys; ++i)
        {
            //Make sure we haven't split duplicates
            const HbIndexNode* a = node->m_Items[i].m_Node;
            const HbIndexNode* b = node->m_Items[i+1].m_Node;
            if(a->m_NumKeys < HbIndexNode::NUM_KEYS)
            {
                assert(a->m_Keys[a->m_NumKeys-1] != b->m_Keys[0]);
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

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);
    static int iii;
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        index.Insert(kv[i].m_Key, kv[i].m_Value);
        assert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
            assert(value == kv[i].m_Value);
        }
        //index.Validate();
    }

    //index.Validate();

    std::random_shuffle(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
            assert(value == kv[i].m_Value);
        }
    }

    /*std::sort(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
            assert(value == kv[i].m_Value);
        }
    }*/

    for(int i = 0; i < numKeys; ++i)
    {
        hbVerify(index.Delete(kv[i].m_Key));
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
            assert(index.Find(kv[idx].m_Key, &value));
            if(unique)
            {
                assert(value == kv[idx].m_Value);
            }
        }
        else
        {
            hbVerify(index.Delete(kv[idx].m_Key));
            //index.Validate();
            kv[idx].m_Added = false;
            assert(!index.Find(kv[idx].m_Key, &value));
        }
    }

//    index.Validate();

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            assert(index.Find(kv[i].m_Key, &value));
            if(unique)
            {
                assert(value == kv[i].m_Value);
            }
        }
        else
        {
            assert(!index.Find(kv[i].m_Key, &value));
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
    assert(count == numKeys);
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
            hbVerify(index.Delete(min));
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
            hbVerify(index.Delete((i%range)+min));
            index.Validate();
        }
    }

    index.Validate();

    assert(0 == index.Count());
}