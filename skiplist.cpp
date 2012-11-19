#include "skiplist.h"

#include <algorithm>

static unsigned RandomHeight(unsigned maxHeight)
{
    unsigned height = 1;
    unsigned r = HbRand();

    while(!(r&1) && height < maxHeight)
    {
        ++height;
        r >>= 1;
    }

    /*while((HbRand() & 0xFF) < 0x80 && height <= maxHeight)
    {
        ++height;
    }*/

    return height;
}

///////////////////////////////////////////////////////////////////////////////
//  HbSkipNode
///////////////////////////////////////////////////////////////////////////////
static HbSkipNode* s_Pools[HbSkipList::MAX_HEIGHT];

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
        node->m_Next[0].m_Node = s_Pools[node->m_Height-1];
        s_Pools[node->m_Height-1] = node;

        if(HB_VALUETYPE_STRING == node->m_ValType)
        {
            HbString::Destroy(node->m_Value.m_String);
            node->m_Value.m_String = NULL;
        }

        //free(node);
    }
}

//private:

HbSkipNode*
HbSkipNode::Create()
{
    const unsigned height = RandomHeight(HbSkipList::MAX_HEIGHT);
    HbSkipNode* node = s_Pools[height-1];
    if(!node)
    {
        node = (HbSkipNode*)HbHeap::ZAlloc(sizeof(HbSkipNode) + (sizeof(HbSkipLink)*(height-1)));

        if(node)
        {
            node->m_Height = height;
        }
    }
    else
    {
        s_Pools[height-1] = node->m_Next[0].m_Node;
    }

    return node;
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

bool
HbSkipList::Delete(const s64 key)
{
    HbSkipLink* prev = m_Head;
    HbSkipNode* node = NULL;

    for(int i = m_Height-1; i >= 0; --i)
    {
        HbSkipNode* cur;
        for(cur = prev[i].m_Node; cur && cur->m_Key < key; prev = cur->m_Next, cur = prev[i].m_Node)
        {
        }

        if(cur && key == cur->m_Key)
        {
            prev[i] = cur->m_Next[i];

            for(int j = i-1; j >= 0 && prev[j].m_Node == cur; --j, --i)
            {
                prev[j] = cur->m_Next[j];
            }

            node = cur;
        }
        else
        {
            for(int j = i-1; j >= 0 && prev[j].m_Node == cur; --j, --i)
            {
            }
        }
    }

    for(int i = m_Height-1; i >= 0; --i)
    {
        if(m_Head[i].m_Node)
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

//private:

bool
HbSkipList::Insert(HbSkipNode* node)
{
    HbSkipLink* prev = m_Head;

    if(node->m_Height > m_Height)
    {
        m_Height = node->m_Height;
    }

    int i;
    for(i = (int)m_Height-1; i >= (int)node->m_Height; --i)
    {
        for(HbSkipNode* cur = prev[i].m_Node; cur && cur->m_Key < node->m_Key; prev = cur->m_Next, cur = prev[i].m_Node)
        {
        }
    }

    for(; i >= 0; --i)
    {
        for(HbSkipNode* cur = prev[i].m_Node; cur && cur->m_Key < node->m_Key; prev = cur->m_Next, cur = prev[i].m_Node)
        {
        }

        node->m_Next[i] = prev[i];
        prev[i].m_Node = node;
    }

    ++m_Count;
    return true;
}

const HbSkipNode*
HbSkipList::Find(const s64 key) const
{
    const HbSkipLink* prev = m_Head;

    for(int i = m_Height-1; i >= 0; --i)
    {
        HbSkipNode* cur;
        for(cur = prev[i].m_Node; cur && cur->m_Key < key; prev = cur->m_Next, cur = prev[i].m_Node)
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
