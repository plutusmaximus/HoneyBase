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

SkipNode*
SkipNode::Create()
{
    const unsigned height = RandomHeight(SkipList::MAX_HEIGHT);
    SkipNode* node = s_Pools[height-1];
    if(!node)
    {
        const size_t size = (sizeof(SkipNode) + (sizeof(SkipNode*)*(height-1)));
        node = (SkipNode*)Heap::ZAlloc(size);

        if(node)
        {
            node->m_Height = height;
        }
    }
    else
    {
        s_Pools[height-1] = node->m_Links[0];
    }

    return node;
}

void
SkipNode::Destroy(SkipNode* node)
{
    /*if(node)
    {
        node->m_Links[0] = s_Pools[node->m_Height-1];
        s_Pools[node->m_Height-1] = node;

        if(VALUETYPE_BLOB == node->m_ValueType)
        {
            Blob::Destroy(node->m_Value.m_Blob);
            node->m_Value.m_Blob = NULL;
        }

        //free(node);
    }*/
}

///////////////////////////////////////////////////////////////////////////////
//  SkipList
///////////////////////////////////////////////////////////////////////////////
SkipList::SkipList(const KeyType keyType)
: m_KeyType(keyType)
, m_Height(0)
, m_Count(0)
, m_Capacity(0)
{
    memset(m_Head, 0, sizeof(m_Head));
}

SkipList*
SkipList::Create(const KeyType keyType)
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
        /*while(skiplist->m_Head[0])
        {
            skiplist->Delete(skiplist->m_Head[0]->m_Key);
        }*/
        Heap::Free(skiplist);
    }
}

bool
SkipList::Insert(const Key key, const Value value, const ValueType valueType)
{
    if(!m_Height)
    {
        SkipNode* node = SkipNode::Create();
        if(!hbverify(node))
        {
            return false;
        }

        m_Capacity += hbarraylen(node->m_Items);
        m_Height = node->m_Height;

        if(KEYTYPE_BLOB == m_KeyType)
        {
            node->m_Items[0].m_Key.m_Blob = key.m_Blob->Dup();
        }
        else
        {
            node->m_Items[0].m_Key = key;
        }

        if(VALUETYPE_BLOB == valueType)
        {
            node->m_Items[0].m_Value.m_Blob = value.m_Blob->Dup();
        }
        else
        {
            node->m_Items[0].m_Value = value;
        }
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
        SkipNode* prev = NULL;

        for(int i = m_Height-1; i >= 0; --i)
        {
            links[i] = links[i+1];

            for(cur = links[i][i]; cur->m_Links[i] && cur->m_Items[cur->m_NumItems-1].m_Key.LT(m_KeyType, key);
                links[i] = cur->m_Links, prev = cur, cur = cur->m_Links[i])
            {
            }
        }

        int idx = UpperBound(key, &cur->m_Items[0], &cur->m_Items[cur->m_NumItems]);

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
            else if(prev && prev->m_NumItems < hbarraylen(cur->m_Items)-1)
            {
                //Shift nodes to our left neighbor.
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
                SkipNode* node = SkipNode::Create();
                if(!hbverify(node))
                {
                    return false;
                }

                m_Capacity += hbarraylen(node->m_Items);

                const int numToMove = cur->m_NumItems/2;
                const int numLeft = cur->m_NumItems - numToMove;
                memcpy(&node->m_Items[0], &cur->m_Items[0], numToMove*sizeof(SkipItem));
                memmove(&cur->m_Items[0], &cur->m_Items[numToMove], numLeft*sizeof(SkipItem));

                node->m_NumItems = numToMove;
                cur->m_NumItems -= numToMove;

                for(; m_Height < node->m_Height; ++m_Height)
                {
                    links[m_Height] = m_Head;
                }

                for(int i = node->m_Height-1; i >= 0; --i)
                {
                    node->m_Links[i] = links[i][i];
                    links[i][i] = node;
                }

                idx -= numToMove;
                if(idx < 0)
                {
                    cur = node;
                    idx += cur->m_NumItems;
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

        if(KEYTYPE_BLOB == m_KeyType)
        {
            cur->m_Items[idx].m_Key.m_Blob = key.m_Blob->Dup();
        }
        else
        {
            cur->m_Items[idx].m_Key = key;
        }

        if(VALUETYPE_BLOB == valueType)
        {
            cur->m_Items[idx].m_Value.m_Blob = value.m_Blob->Dup();
        }
        else
        {
            cur->m_Items[idx].m_Value = value;
        }
        cur->m_Items[idx].m_ValueType = valueType;
        ++cur->m_NumItems;
        ++m_Count;
        return true;
    }

    return false;
}

bool
SkipList::Delete(const Key key)
{
    /*SkipNode** prev = m_Head;
    SkipNode* node = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        SkipNode* cur;
        for(cur = prev[i]; cur && cur->m_Key.LT(m_KeyType, key); prev = cur->m_Links, cur = prev[i])
        {
        }

        if(cur && key.EQ(m_KeyType, cur->m_Key))
        {
            prev[i] = cur->m_Links[i];

            for(int j = i-1; j >= 0 && prev[j] == cur; --j, --i)
            {
                prev[j] = cur->m_Links[j];
            }

            node = cur;
        }
        else
        {
            for(int j = i-1; j >= 0 && prev[j] == cur; --j, --i)
            {
            }
        }
    }

    for(int i = m_Height-1; i >= 0; --i)
    {
        if(m_Head[i])
        {
            break;
        }

        --m_Height;
    }

    if(node)
    {
        SkipNode::Destroy(node);
        --m_Count;
        return true;
    }*/

    return false;
}

bool
SkipList::Find(const Key key, Value* value, ValueType* valueType) const
{
    const SkipNode* const* prev = m_Head;
    const SkipNode* cur = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        for(cur = prev[i]; cur && cur->m_Items[cur->m_NumItems-1].m_Key.LT(m_KeyType, key); prev = cur->m_Links, cur = prev[i])
        {
        }

        if(i > 0 && cur && cur->m_Items[0].m_Key.LE(m_KeyType, key))
        {
            //Early out if we found the block that might contain the key
            break;
        }
    }

    if(cur)
    {
        const int idx = LowerBound(key, &cur->m_Items[0], &cur->m_Items[cur->m_NumItems]);
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

//private:

int
SkipList::LowerBound(const Key key, const SkipItem* first, const SkipItem* end) const
{
    if(end > first)
    {
        const SkipItem* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const SkipItem* item = cur;
            size_t step = count >> 1;
            item += step;
            if(item->m_Key.LT(m_KeyType, key))
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
SkipList::UpperBound(const Key key, const SkipItem* first, const SkipItem* end) const
{
    if(end > first)
    {
        const SkipItem* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const SkipItem* item = cur;
            const size_t step = count >> 1;
            item += step;
            if(!(key.LT(m_KeyType, item->m_Key)))
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