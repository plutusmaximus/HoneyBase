#include "skiplist.h"

#include <algorithm>

static HbLog s_Log("skiplist");

static unsigned RandomHeight(unsigned maxHeight)
{
    unsigned height = 1;
    /*unsigned r = HbRand();

    while(!(r&1) && height < maxHeight)
    {
        ++height;
        r >>= 1;
    }*/

    while((HbRand() & 0xFFFF) < (0.5*0xFFFF) && height < maxHeight)
    {
        ++height;
    }

    return height;
}

///////////////////////////////////////////////////////////////////////////////
//  HbSkipNode
///////////////////////////////////////////////////////////////////////////////
static HbSkipNode* s_Pools[HbSkipList::MAX_HEIGHT];

HbSkipNode*
HbSkipNode::Create()
{
    const unsigned height = RandomHeight(HbSkipList::MAX_HEIGHT);
    HbSkipNode* node = s_Pools[height-1];
    if(!node)
    {
        const size_t size = (sizeof(HbSkipNode) + (sizeof(HbSkipNode*)*(height-1)));
        node = (HbSkipNode*)HbHeap::ZAlloc(size);

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

HbSkipNode*
HbSkipNode::Create(const s64 key, const HbString* value)
{
    HbSkipNode* node = Create();
    if(node)
    {
        node->m_Key = key;
        node->m_Value.m_String = value->Dup();
        node->m_ValType = HB_VALUETYPE_STRING;
    }

    return node;
}

HbSkipNode*
HbSkipNode::Create(const s64 key, const s64 value)
{
    HbSkipNode* node = Create();
    if(node)
    {
        node->m_Key = key;
        node->m_Value.m_Int = value;
        node->m_ValType = HB_VALUETYPE_INT;
    }

    return node;
}

void
HbSkipNode::Destroy(HbSkipNode* node)
{
    if(node)
    {
        node->m_Links[0] = s_Pools[node->m_Height-1];
        s_Pools[node->m_Height-1] = node;

        if(HB_VALUETYPE_STRING == node->m_ValType)
        {
            HbString::Destroy(node->m_Value.m_String);
            node->m_Value.m_String = NULL;
        }

        //free(node);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  HbSkipList
///////////////////////////////////////////////////////////////////////////////
bool
HbSkipList::Insert(const s64 key, const HbString* value)
{
    HbSkipNode* node = HbSkipNode::Create(key, value);
    return node && Insert(node);
}

bool
HbSkipList::Insert(const s64 key, const s64 value)
{
    HbSkipNode* node = HbSkipNode::Create(key, value);
    return node && Insert(node);
}

int
HbSkipList::LowerBound(const s64 key, const HbSkipItem* first, const HbSkipItem* end)
{
    if(end > first)
    {
        const HbSkipItem* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const HbSkipItem* item = cur;
            size_t step = count >> 1;
            item += step;
            if(item->m_Key < key)
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
HbSkipList::UpperBound(const s64 key, const HbSkipItem* first, const HbSkipItem* end)
{
    if(end > first)
    {
        const HbSkipItem* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const HbSkipItem* item = cur;
            const size_t step = count >> 1;
            item += step;
            if(!(key < item->m_Key))
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

bool
HbSkipList::Insert2(const s64 key, const s64 value)
{
    if(!m_Height)
    {
        HbSkipNode* node = HbSkipNode::Create();
        if(node)
        {
            m_Capacity += hbarraylen(node->m_Items);
            m_Height = node->m_Height;
            node->m_Items[0].m_Key = key;
            node->m_Items[0].m_Value.m_Int = value;
            node->m_Items[0].m_ValType = HB_VALUETYPE_INT;
            ++node->m_NumItems;
            for(int i = m_Height-1; i >= 0; --i)
            {
                m_Head[i] = node;
            }
            ++m_Count;
            return true;
        }
    }
    else
    {
        HbSkipNode** links[MAX_HEIGHT+1] = {0};

        links[m_Height] = m_Head;
        HbSkipNode* cur = NULL;

        for(int i = m_Height-1; i >= 0; --i)
        {
            links[i] = links[i+1];

            for(cur = links[i][i]; cur->m_Links[i] && cur->m_Items[cur->m_NumItems-1].m_Key < key;
                links[i] = cur->m_Links, cur = cur->m_Links[i])
            {
            }
        }

        int idx = UpperBound(key, &cur->m_Items[0], &cur->m_Items[cur->m_NumItems]);

        if(cur->m_NumItems == hbarraylen(cur->m_Items))
        {
            HbSkipNode* node = HbSkipNode::Create();
            if(!hbverify(node))
            {
                return false;
            }

            m_Capacity += hbarraylen(node->m_Items);

            const int numToMove = cur->m_NumItems/2;
            const int numLeft = cur->m_NumItems - numToMove;
            idx -= numToMove;
            memcpy(&node->m_Items[0], &cur->m_Items[0], numToMove*sizeof(HbSkipItem));
            memmove(&cur->m_Items[0], &cur->m_Items[numToMove], numLeft*sizeof(HbSkipItem));

            node->m_NumItems = numToMove;
            cur->m_NumItems -= numToMove;

            while(m_Height < node->m_Height)
            {
                links[m_Height] = m_Head;
                ++m_Height;
            }

            for(int i = node->m_Height-1; i >= 0; --i)
            {
                node->m_Links[i] = links[i][i];
                links[i][i] = node;
            }

            if(idx < 0)
            {
                cur = node;
                idx += cur->m_NumItems;
            }
        }

        if(idx < cur->m_NumItems)
        {
            memmove(&cur->m_Items[idx+1],
                    &cur->m_Items[idx],
                    (cur->m_NumItems-idx)*sizeof(HbSkipItem));
        }

        cur->m_Items[idx].m_Key = key;
        cur->m_Items[idx].m_Value.m_Int = value;
        cur->m_Items[idx].m_ValType = HB_VALUETYPE_INT;
        ++cur->m_NumItems;
        ++m_Count;
        return true;
    }

    return false;
}

bool
HbSkipList::Delete(const s64 key)
{
    HbSkipNode** prev = m_Head;
    HbSkipNode* node = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        HbSkipNode* cur;
        for(cur = prev[i]; cur && cur->m_Key < key; prev = cur->m_Links, cur = prev[i])
        {
        }

        if(cur && key == cur->m_Key)
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
        HbSkipNode::Destroy(node);
        --m_Count;
        return true;
    }

    return false;
}

bool
HbSkipList::Find(const s64 key, s64* value) const
{
    const HbSkipNode* node = Find(key);

    if(node && HB_VALUETYPE_INT == node->m_ValType)
    {
        *value = node->m_Value.m_Int;
        return true;
    }

    return false;
}

bool
HbSkipList::Find(const s64 key, const HbString** value) const
{
    const HbSkipNode* node = Find(key);

    if(node && HB_VALUETYPE_STRING == node->m_ValType)
    {
        *value = node->m_Value.m_String;
        return true;
    }

    return false;
}

bool
HbSkipList::Find2(const s64 key, s64* value) const
{
    const HbSkipNode* const* prev = m_Head;
    const HbSkipNode* cur = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        for(cur = prev[i]; cur && cur->m_Items[cur->m_NumItems-1].m_Key < key; prev = cur->m_Links, cur = prev[i])
        {
        }

        if(i > 0 && cur && cur->m_Items[0].m_Key <= key)
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
            *value = cur->m_Items[idx].m_Value.m_Int;
            return true;
        }
    }

    return false;
}

double
HbSkipList::GetUtilization() const
{
    return (m_Capacity > 0 ) ? (double)m_Count/m_Capacity : 0;
}

//private:

bool
HbSkipList::Insert(HbSkipNode* node)
{
    HbSkipNode** prev = m_Head;

    if(node->m_Height > m_Height)
    {
        m_Height = node->m_Height;
    }

    int i;
    for(i = m_Height-1; i >= node->m_Height; --i)
    {
        for(HbSkipNode* cur = prev[i]; cur && cur->m_Key < node->m_Key; prev = cur->m_Links, cur = prev[i])
        {
        }
    }

    for(; i >= 0; --i)
    {
        for(HbSkipNode* cur = prev[i]; cur && cur->m_Key < node->m_Key; prev = cur->m_Links, cur = prev[i])
        {
        }

        node->m_Links[i] = prev[i];
        prev[i] = node;
    }

    ++m_Count;
    return true;
}

const HbSkipNode*
HbSkipList::Find(const s64 key) const
{
    const HbSkipNode* const* prev = m_Head;

    for(int i = m_Height-1; i >= 0; --i)
    {
        const HbSkipNode* cur;
        for(cur = prev[i]; cur && cur->m_Key < key; prev = cur->m_Links, cur = prev[i])
        {
        }

        if(cur && key == cur->m_Key)
        {
            return cur;
        }
    }

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  HbSkipListTest
///////////////////////////////////////////////////////////////////////////////
static ptrdiff_t myrandom (ptrdiff_t i)
{
    return HbRand()%i;
}

void
HbSkipListTest::CreateRandomKeys(KV* kv, const int numKeys, const bool unique, const int range)
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
HbSkipListTest::AddRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbSkipList* skiplist = (HbSkipList*)HbHeap::ZAlloc(sizeof(HbSkipList));
    s64 value;

    HbStopWatch sw;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        skiplist->Insert(kv[i].m_Key, kv[i].m_Value);
        hbverify(skiplist->Find(kv[i].m_Key, &value));
        if(unique)
        {
             hbverify(value == kv[i].m_Value);
        }
    }
    sw.Stop();

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Find(kv[i].m_Key, &value));
        if(unique)
        {
            hbverify(value == kv[i].m_Value);
        }
    }
    sw.Stop();

    /*std::sort(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Find(kv[i].m_Key, &value));
        if(unique)
        {
            hbverify(value == kv[i].m_Value);
        }
    }
    sw.Stop();*/

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Delete(kv[i].m_Key));
    }
    sw.Stop();

    HbHeap::Free(skiplist);
    delete [] kv;
}

void
HbSkipListTest::AddRandomKeys2(const int numKeys, const bool unique, const int range)
{
    HbSkipList* skiplist = (HbSkipList*)HbHeap::ZAlloc(sizeof(HbSkipList));
    s64 value;

    HbStopWatch sw;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        skiplist->Insert2(kv[i].m_Key, kv[i].m_Value);
        hbverify(skiplist->Find2(kv[i].m_Key, &value));
        if(unique)
        {
             hbverify(value == kv[i].m_Value);
        }
    }
    sw.Stop();
    s_Log.Debug("insert: %f", sw.GetElapsed());

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Find2(kv[i].m_Key, &value));
        if(unique)
        {
            hbverify(value == kv[i].m_Value);
        }
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());

    s_Log.Debug("utilization: %f", skiplist->GetUtilization());

    /*std::sort(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Find2(kv[i].m_Key, &value));
        if(unique)
        {
            hbverify(value == kv[i].m_Value);
        }
    }
    sw.Stop();*/

    /*sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbverify(skiplist->Delete2(kv[i].m_Key));
    }
    sw.Stop();*/

    HbHeap::Free(skiplist);
    delete [] kv;
}

void
HbSkipListTest::AddDeleteRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbSkipList* skiplist = (HbSkipList*)HbHeap::ZAlloc(sizeof(HbSkipList));
    s64 value;

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
            skiplist->Insert(kv[idx].m_Key, kv[idx].m_Value);
            kv[idx].m_Added = true;
            hbverify(skiplist->Find(kv[idx].m_Key, &value));
            if(unique)
            {
                hbverify(value == kv[idx].m_Value);
            }
        }
        else
        {
            hbverify(skiplist->Delete(kv[idx].m_Key));
            kv[idx].m_Added = false;
            hbverify(!skiplist->Find(kv[idx].m_Key, &value));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            hbverify(skiplist->Find(kv[i].m_Key, &value));
            if(unique)
            {
                hbverify(value == kv[i].m_Value);
            }
        }
        else
        {
            hbverify(!skiplist->Find(kv[i].m_Key, &value));
        }
    }

    HbHeap::Free(skiplist);
    delete [] kv;
}
