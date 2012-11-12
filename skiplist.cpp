#include "skiplist.h"

#include <algorithm>
#include <assert.h>
#include <malloc.h>

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

bool
HbSkipList::Insert(const s64 key, const s64 value)
{
    HbSkipNode* node = AllocNode();
    if(node)
    {
        node->m_Key = key;
        node->m_Value = value;

        HbSkipLink* prev = m_Head;

        if(node->m_Height > m_Height)
        {
            m_Height = node->m_Height;
        }

        int i;
        for(i = (int)m_Height-1; i >= (int)node->m_Height; --i)
        {
            for(HbSkipNode* cur = prev[i].m_Node; cur && cur->m_Key < key; prev = cur->m_Next, cur = prev[i].m_Node)
            {
            }
        }

        for(; i >= 0; --i)
        {
            for(HbSkipNode* cur = prev[i].m_Node; cur && cur->m_Key < key; prev = cur->m_Next, cur = prev[i].m_Node)
            {
            }

            node->m_Next[i] = prev[i];
            prev[i].m_Node = node;
        }

        ++m_Count;
        return true;
    }

    return false;
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
        FreeNode(node);
        --m_Count;
        return true;
    }

    return false;
}

bool
HbSkipList::Find(const s64 key, s64* value) const
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
            *value = cur->m_Value;
            return true;
        }
    }

    return false;
}

//private:

static HbSkipNode* s_Pools[HbSkipList::MAX_HEIGHT];

HbSkipNode*
HbSkipList::AllocNode()
{
    const unsigned height = RandomHeight(MAX_HEIGHT);
    HbSkipNode* node = s_Pools[height-1];
    if(!node)
    {
        node = (HbSkipNode*)calloc(1, sizeof(HbSkipNode) + (sizeof(HbSkipLink)*(height-1)));

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

void
HbSkipList::FreeNode(HbSkipNode* node)
{
    if(node)
    {
        node->m_Next[0].m_Node = s_Pools[node->m_Height-1];
        s_Pools[node->m_Height-1] = node;
        //free(node);
    }
}

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
    HbSkipList* skiplist = (HbSkipList*)calloc(1, sizeof(HbSkipList));
    s64 value;

    HbStopWatch sw;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        skiplist->Insert(kv[i].m_Key, kv[i].m_Value);
        bool found = skiplist->Find(kv[i].m_Key, &value);
        assert(found);
        if(found && unique)
        {
             assert(value == kv[i].m_Value);
        }
    }
    sw.Stop();

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        assert(skiplist->Find(kv[i].m_Key, &value));
        if(unique)
        {
            assert(value == kv[i].m_Value);
        }
    }
    sw.Stop();

    /*std::sort(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        assert(skiplist->Find(kv[i].m_Key, &value));
        if(unique)
        {
            assert(value == kv[i].m_Value);
        }
    }
    sw.Stop();*/

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbVerify(skiplist->Delete(kv[i].m_Key));
    }
    sw.Stop();

    free(skiplist);
    delete [] kv;
}
void
HbSkipListTest::AddDeleteRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbSkipList* skiplist = (HbSkipList*)calloc(1, sizeof(HbSkipList));
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
            skiplist->Insert(kv[idx].m_Key, kv[idx].m_Value);
            kv[idx].m_Added = true;
            assert(skiplist->Find(kv[idx].m_Key, &value));
            if(unique)
            {
                assert(value == kv[idx].m_Value);
            }
        }
        else
        {
            hbVerify(skiplist->Delete(kv[idx].m_Key));
            kv[idx].m_Added = false;
            assert(!skiplist->Find(kv[idx].m_Key, &value));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            assert(skiplist->Find(kv[i].m_Key, &value));
            if(unique)
            {
                assert(value == kv[i].m_Value);
            }
        }
        else
        {
            assert(!skiplist->Find(kv[i].m_Key, &value));
        }
    }

    free(skiplist);
    delete [] kv;
}

