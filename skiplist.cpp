#include "skiplist.h"

#include <algorithm>

namespace honeybase
{

static Log s_Log("skiplist");

static unsigned RandomHeight(unsigned maxHeight)
{
    unsigned height = 1;
    /*unsigned r = Rand();

    while(!(r&1) && height < maxHeight)
    {
        ++height;
        r >>= 1;
    }*/

    while((Rand() & 0xFFFF) < (0.5*0xFFFF) && height < maxHeight)
    {
        ++height;
    }

    return height;
}

///////////////////////////////////////////////////////////////////////////////
//  SkipNode
///////////////////////////////////////////////////////////////////////////////
static SkipNode* s_Pools[SkipList::MAX_HEIGHT];

static size_t s_NumNodesAllocated;

SkipNode*
SkipNode::Create(const int maxHeight)
{
    const unsigned height = RandomHeight(maxHeight);
    SkipNode* node = s_Pools[height-1];
    if(!node)
    {
        const size_t size = (sizeof(SkipNode) + (sizeof(SkipNode*)*(height-1)));
        node = (SkipNode*)Heap::ZAlloc(size);

        if(node)
        {
            node->m_Height = height;
            ++s_NumNodesAllocated;
        }
    }
    else
    {
        s_Pools[height-1] = node->m_Links[0];
        ++s_NumNodesAllocated;
    }

    return node;
}

void
SkipNode::Destroy(SkipNode* node)
{
    if(node)
    {
        node->m_Links[0] = s_Pools[node->m_Height-1];
        s_Pools[node->m_Height-1] = node;
        --s_NumNodesAllocated;

        //free(node);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  SkipList
///////////////////////////////////////////////////////////////////////////////
SkipList::SkipList(const ValueType keyType)
: m_Height(0)
, m_MaxHeight(16)//MAX_HEIGHT)
, m_Count(0)
, m_Capacity(0)
{
    memset(m_Head, 0, sizeof(m_Head));
}

SkipList*
SkipList::Create(const ValueType keyType)
{
    SkipList* skiplist = (SkipList*) Heap::ZAlloc(sizeof(SkipList));
    if(skiplist)
    {
        new (skiplist) SkipList(keyType);
    }

    return skiplist;
}

void
SkipList::Destroy(SkipList* skiplist)
{
    if(skiplist)
    {
        while(skiplist->m_Head[0])
        {
            skiplist->Delete(skiplist->m_Head[0]->m_Items[0].m_Key,
                            skiplist->m_Head[0]->m_Items[0].m_KeyType);
        }
        Heap::Free(skiplist);
    }
}

bool
SkipList::Insert(const Value key, const ValueType keyType, const Value value, const ValueType valueType)
{
    if(!m_Height)
    {
        SkipNode* node = SkipNode::Create(m_MaxHeight);
        if(!hbverify(node))
        {
            return false;
        }

        m_Capacity += hbarraylen(node->m_Items);
        m_Height = node->m_Height;

        if(VALUETYPE_BLOB == keyType)
        {
            key.m_Blob->Ref();
        }

        if(VALUETYPE_BLOB == valueType)
        {
            value.m_Blob->Ref();
        }

        node->m_Items[0].m_Key = key;
        node->m_Items[0].m_Value = value;
        node->m_Items[0].m_KeyType = keyType;
        node->m_Items[0].m_ValueType = valueType;
        ++node->m_NumItems;
        for(int i = m_Height-1; i >= 0; --i)
        {
            m_Head[i] = node;
        }
        ++m_Count;
        return true;
    }
    else
    {
        //For each level in the skiplist keep track of the last node
        //probed while searching for a match.  This will be used
        //to update links at each level if we insert a new node.
        SkipNode** links[MAX_HEIGHT+1] = {0};

        links[m_Height] = m_Head;
        SkipNode* cur = NULL;
        SkipNode* pred = NULL;

        for(int i = m_Height-1; i >= 0; --i)
        {
            links[i] = links[i+1];

            cur = links[i][i];

            const SkipItem* item = cur ? &cur->m_Items[cur->m_NumItems-1] : NULL;

            for(; cur && item->LT(keyType, key);
                links[i] = cur->m_Links, pred = cur, cur = cur->m_Links[i], item = cur ? &cur->m_Items[cur->m_NumItems-1] : NULL)
            {
                hbassert(links[i][i]->m_Height > i);
            }
        }

        if(!cur)
        {
            cur = pred;
        }

        int idx = UpperBound(key, keyType, &cur->m_Items[0], cur->m_NumItems);

        if(cur->m_NumItems == hbarraylen(cur->m_Items))
        {
            SkipNode* next = cur->m_Links[0];
            if(next && next->m_NumItems < hbarraylen(cur->m_Items)-1)
            {
                //Shift nodes to our right neighbor.
                const int numToMove = (hbarraylen(cur->m_Items) - next->m_NumItems)/2;
                const int srcOffset = cur->m_NumItems - numToMove;
                memmove(&next->m_Items[numToMove], &next->m_Items[0], next->m_NumItems*sizeof(SkipItem));
                memcpy(&next->m_Items[0], &cur->m_Items[srcOffset], numToMove*sizeof(SkipItem));
                cur->m_NumItems -= numToMove;
                next->m_NumItems += numToMove;

                if(idx >= cur->m_NumItems)
                {
                    idx -= cur->m_NumItems;
                    cur = next;
                }
            }
            else if(cur->m_Prev && cur->m_Prev->m_NumItems < hbarraylen(cur->m_Items)-1)
            {
                //Shift nodes to our left neighbor.
                SkipNode* prev = cur->m_Prev;
                const int numToMove = (hbarraylen(cur->m_Items) - prev->m_NumItems)/2;
                const int numLeft = cur->m_NumItems - numToMove;
                memcpy(&prev->m_Items[prev->m_NumItems], &cur->m_Items[0], numToMove*sizeof(SkipItem));
                memcpy(&cur->m_Items[0], &cur->m_Items[numToMove], numLeft*sizeof(SkipItem));
                cur->m_NumItems -= numToMove;
                prev->m_NumItems += numToMove;

                idx -= numToMove;
                if(idx < 0)
                {
                    cur = prev;
                    idx += cur->m_NumItems;
                }
            }
            else
            {
                //Split the node
                SkipNode* node = SkipNode::Create(m_MaxHeight);
                if(!hbverify(node))
                {
                    return false;
                }

                m_Capacity += hbarraylen(node->m_Items);

                const int numToMove = cur->m_NumItems/2;
                const int numLeft = cur->m_NumItems - numToMove;

                memcpy(&node->m_Items[0], &cur->m_Items[numLeft], numToMove*sizeof(SkipItem));

                node->m_NumItems = numToMove;
                cur->m_NumItems = numLeft;

                for(int i = 0; i < cur->m_Height && i < node->m_Height; ++i)
                {
                    node->m_Links[i] = cur->m_Links[i];
                    cur->m_Links[i] = node;
                }

                for(int i = cur->m_Height; i < node->m_Height && i < m_Height; ++i)
                {
                    node->m_Links[i] = links[i][i];
                    links[i][i] = node;
                }

                for(; m_Height < node->m_Height; ++m_Height)
                {
                    m_Head[m_Height] = node;
                }

                if(idx >= numLeft)
                {
                    cur = node;
                    idx -= numLeft;
                }
            }
        }

        if(idx < cur->m_NumItems)
        {
            memmove(&cur->m_Items[idx+1],
                    &cur->m_Items[idx],
                    (cur->m_NumItems-idx)*sizeof(SkipItem));
        }

        cur->m_Items[idx].m_Key = key;

        if(VALUETYPE_BLOB == keyType)
        {
            key.m_Blob->Ref();
        }

        if(VALUETYPE_BLOB == valueType)
        {
            value.m_Blob->Ref();
        }

        cur->m_Items[idx].m_Key = key;
        cur->m_Items[idx].m_Value = value;
        cur->m_Items[idx].m_KeyType = keyType;
        cur->m_Items[idx].m_ValueType = valueType;
        ++cur->m_NumItems;
        ++m_Count;

        if(m_Count > (u64(1)<<m_MaxHeight))
        {
            ++m_MaxHeight;
        }

        return true;
    }

    return false;
}

bool
SkipList::Delete(const Value key, const ValueType keyType)
{
    //For each level in the skiplist keep track of the last node
    //probed while searching for a match.  This will be used
    //to update links at each level if we remove a node.
    SkipNode** links[MAX_HEIGHT+1] = {0};

    links[m_Height] = m_Head;
    SkipNode* cur = NULL;
    SkipNode* prev = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        links[i] = links[i+1];

        for(cur = links[i][i];
            cur->m_Links[i] && cur->m_Items[cur->m_NumItems-1].LT(keyType, key);
            links[i] = cur->m_Links, prev = cur, cur = cur->m_Links[i])
        {
        }
    }

    int idx = LowerBound(key, keyType, &cur->m_Items[0], cur->m_NumItems);

    if(idx >= 0
        && idx < cur->m_NumItems
        && key.EQ(keyType, cur->m_Items[idx].m_Key))
    {
        if(VALUETYPE_BLOB == keyType)
        {
            cur->m_Items[idx].m_Key.m_Blob->Unref();
        }

        if(VALUETYPE_BLOB == cur->m_Items[idx].m_ValueType)
        {
            cur->m_Items[idx].m_Value.m_Blob->Unref();
        }

        --cur->m_NumItems;
        --m_Count;

        if(cur->m_NumItems > 0)
        {
            memmove(&cur->m_Items[idx],
                    &cur->m_Items[idx+1],
                    (cur->m_NumItems-idx)*sizeof(SkipItem));
        }
        else
        {
            for(int i = cur->m_Height-1; i >= 0; --i)
            {
                hbassert(links[i][i] == cur);
                //if(links[i][i] == cur)
                {
                    links[i][i] = cur->m_Links[i];
                }
            }

            SkipNode::Destroy(cur);

            for(int i = m_Height-1; i >= 0; --i)
            {
                if(!m_Head[i])
                {
                    --m_Height;
                }
            }
        }

        return true;
    }

    return false;
}

bool
SkipList::Find(const Value key, const ValueType keyType, Value* value, ValueType* valueType) const
{
    const SkipNode* const* prev = m_Head;
    const SkipNode* cur = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        for(cur = prev[i]; cur && cur->m_Items[cur->m_NumItems-1].LT(keyType, key); prev = cur->m_Links, cur = prev[i])
        {
        }

        if(i > 0 && cur && cur->m_Items[0].LE(keyType, key))
        {
            //Early out if we found the block that might contain the key
            break;
        }
    }

    if(cur)
    {
        const int idx = LowerBound(key, keyType, &cur->m_Items[0], cur->m_NumItems);
        if(idx < cur->m_NumItems)
        {
            *value = cur->m_Items[idx].m_Value;
            *valueType = cur->m_Items[idx].m_ValueType;
            return true;
        }
    }

    return false;
}

u64
SkipList::Count() const
{
    return m_Count;
}

double
SkipList::GetUtilization() const
{
    return (m_Capacity > 0 ) ? (double)m_Count/m_Capacity : 0;
}

void
SkipList::Validate() const
{
    for(int i = 0; i < m_Height; ++i)
    {
        const SkipNode* cur = m_Head[i];
        hbassert(cur);

        for(; cur; cur = cur->m_Links[i])
        {
            hbassert(cur->m_Height > i);
            for(int j = 0; j < cur->m_Height && cur->m_Links[j]; ++j)
            {
                hbassert(cur->m_Items[0].LE(cur->m_Links[j]->m_Items[0].m_KeyType,
                                            cur->m_Links[j]->m_Items[0].m_Key));
            }
        }
    }

    SkipNode* const* links[MAX_HEIGHT] = {0};

    for(int i = 0; i < m_Height; ++i)
    {
        links[i] = m_Head;
    }

    size_t nodeIdx = 0;
    for(const SkipNode* cur = m_Head[0]; cur; cur = cur->m_Links[0], ++nodeIdx)
    {
        for(int i = 0; i < cur->m_Height; ++i)
        {
            hbassert(links[i][i] == cur);
            links[i] = cur->m_Links;
        }
    }
}

//private:

int
SkipList::LowerBound(const Value key, const ValueType keyType, const SkipItem* first, const size_t numItems) const
{
    const SkipItem* cur = first;
    size_t count = numItems;
    while(count > 0)
    {
        const size_t step = count >> 1;
        const SkipItem* item = cur + step;
        if(item->LT(keyType, key))
        {
            cur = item + 1;
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
SkipList::UpperBound(const Value key, const ValueType keyType, const SkipItem* first, const size_t numItems) const
{
    const SkipItem* cur = first;
    size_t count = numItems;
    while(count > 0)
    {
        const size_t step = count >> 1;
        const SkipItem* item = cur + step;
        if(item->LE(keyType, key))
        //if(!key.LT(m_KeyType, item->m_Key))
        {
            cur = item + 1;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }

    return cur - first;
}

}   //namespace honeybase