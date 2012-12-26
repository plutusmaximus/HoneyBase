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

SkipNode*
SkipNode::Create(const s64 key, const Blob* value)
{
    SkipNode* node = Create();
    if(node)
    {
        node->m_Key = key;
        node->m_Value.m_Blob = value->Dup();
        node->m_ValType = VALUETYPE_BLOB;
    }

    return node;
}

SkipNode*
SkipNode::Create(const s64 key, const s64 value)
{
    SkipNode* node = Create();
    if(node)
    {
        node->m_Key = key;
        node->m_Value.m_Int = value;
        node->m_ValType = VALUETYPE_INT;
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

        if(VALUETYPE_BLOB == node->m_ValType)
        {
            Blob::Destroy(node->m_Value.m_Blob);
            node->m_Value.m_Blob = NULL;
        }

        //free(node);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  SkipList
///////////////////////////////////////////////////////////////////////////////
bool
SkipList::Insert(const s64 key, const Blob* value)
{
    SkipNode* node = SkipNode::Create(key, value);
    return node && Insert(node);
}

bool
SkipList::Insert(const s64 key, const s64 value)
{
    SkipNode* node = SkipNode::Create(key, value);
    return node && Insert(node);
}

int
SkipList::LowerBound(const s64 key, const SkipItem* first, const SkipItem* end)
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
SkipList::UpperBound(const s64 key, const SkipItem* first, const SkipItem* end)
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
SkipList::Insert2(const s64 key, const s64 value)
{
    if(!m_Height)
    {
        SkipNode* node = SkipNode::Create();
        if(node)
        {
            m_Capacity += hbarraylen(node->m_Items);
            m_Height = node->m_Height;
            node->m_Items[0].m_Key = key;
            node->m_Items[0].m_Value.m_Int = value;
            node->m_Items[0].m_ValType = VALUETYPE_INT;
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
        SkipNode** links[MAX_HEIGHT+1] = {0};

        links[m_Height] = m_Head;
        SkipNode* cur = NULL;

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
            SkipNode* node = SkipNode::Create();
            if(!hbverify(node))
            {
                return false;
            }

            m_Capacity += hbarraylen(node->m_Items);

            const int numToMove = cur->m_NumItems/2;
            const int numLeft = cur->m_NumItems - numToMove;
            idx -= numToMove;
            memcpy(&node->m_Items[0], &cur->m_Items[0], numToMove*sizeof(SkipItem));
            memmove(&cur->m_Items[0], &cur->m_Items[numToMove], numLeft*sizeof(SkipItem));

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
                    (cur->m_NumItems-idx)*sizeof(SkipItem));
        }

        cur->m_Items[idx].m_Key = key;
        cur->m_Items[idx].m_Value.m_Int = value;
        cur->m_Items[idx].m_ValType = VALUETYPE_INT;
        ++cur->m_NumItems;
        ++m_Count;
        return true;
    }

    return false;
}

bool
SkipList::Delete(const s64 key)
{
    SkipNode** prev = m_Head;
    SkipNode* node = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        SkipNode* cur;
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
        SkipNode::Destroy(node);
        --m_Count;
        return true;
    }

    return false;
}

bool
SkipList::Find(const s64 key, s64* value) const
{
    const SkipNode* node = Find(key);

    if(node && VALUETYPE_INT == node->m_ValType)
    {
        *value = node->m_Value.m_Int;
        return true;
    }

    return false;
}

bool
SkipList::Find(const s64 key, const Blob** value) const
{
    const SkipNode* node = Find(key);

    if(node && VALUETYPE_BLOB == node->m_ValType)
    {
        *value = node->m_Value.m_Blob;
        return true;
    }

    return false;
}

bool
SkipList::Find2(const s64 key, s64* value) const
{
    const SkipNode* const* prev = m_Head;
    const SkipNode* cur = NULL;

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
SkipList::GetUtilization() const
{
    return (m_Capacity > 0 ) ? (double)m_Count/m_Capacity : 0;
}

//private:

bool
SkipList::Insert(SkipNode* node)
{
    SkipNode** prev = m_Head;

    if(node->m_Height > m_Height)
    {
        m_Height = node->m_Height;
    }

    int i;
    for(i = m_Height-1; i >= node->m_Height; --i)
    {
        for(SkipNode* cur = prev[i]; cur && cur->m_Key < node->m_Key; prev = cur->m_Links, cur = prev[i])
        {
        }
    }

    for(; i >= 0; --i)
    {
        for(SkipNode* cur = prev[i]; cur && cur->m_Key < node->m_Key; prev = cur->m_Links, cur = prev[i])
        {
        }

        node->m_Links[i] = prev[i];
        prev[i] = node;
    }

    ++m_Count;
    return true;
}

const SkipNode*
SkipList::Find(const s64 key) const
{
    const SkipNode* const* prev = m_Head;

    for(int i = m_Height-1; i >= 0; --i)
    {
        const SkipNode* cur;
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
//  SkipListTest
///////////////////////////////////////////////////////////////////////////////
static ptrdiff_t myrandom (ptrdiff_t i)
{
    return Rand()%i;
}

void
SkipListTest::CreateRandomKeys(KV* kv, const int numKeys, const bool unique, const int range)
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
            kv[i].m_Key = Rand() % range;
        }
    }
}

void
SkipListTest::AddRandomKeys(const int numKeys, const bool unique, const int range)
{
    SkipList* skiplist = (SkipList*)Heap::ZAlloc(sizeof(SkipList));
    s64 value;

    StopWatch sw;

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

    Heap::Free(skiplist);
    delete [] kv;
}

void
SkipListTest::AddRandomKeys2(const int numKeys, const bool unique, const int range)
{
    SkipList* skiplist = (SkipList*)Heap::ZAlloc(sizeof(SkipList));
    s64 value;

    StopWatch sw;

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

    Heap::Free(skiplist);
    delete [] kv;
}

void
SkipListTest::AddDeleteRandomKeys(const int numKeys, const bool unique, const int range)
{
    SkipList* skiplist = (SkipList*)Heap::ZAlloc(sizeof(SkipList));
    s64 value;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
    }

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = Rand() % numKeys;
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

    Heap::Free(skiplist);
    delete [] kv;
}

}   //namespace honeybase