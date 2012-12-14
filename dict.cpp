#define _CRT_SECURE_NO_WARNINGS

#include "dict.h"

#include <algorithm>
#include <new.h>
#include <stdio.h>
#include <string.h>

static HbLog s_Log("dict");

#define FNV1_32_INIT ((u32)0x811c9dc5)

static u32 FnvHashBufInitVal(const byte *buf, size_t len, u32 hval)
{
    const byte *bp = buf;         /* start of buffer */
    const byte *be = bp + len;    /* beyond end of buffer */

    /*
    * FNV-1a hash each octet in the buffer
    */
    while (bp < be)
    {
        /* xor the bottom with the current octet */
        hval ^= (u32)*bp++;

        /* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(NO_FNV_GCC_OPTIMIZATION)
        hval *= FNV_32_PRIME;
#else
        hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
#endif
    }

    /* return our new hash value */
    return hval;
}

/*static u32 FnvHashBuf(const byte *buf, size_t len)
{
return FnvHashBufInitVal(buf, len, FNV1_32_INIT);
}*/

template<typename T>
static inline void Move(T* dst, const T* src, const size_t count)
{
    if(count > 0)
    {
        memmove(dst, src, count * sizeof(T));
    }
}


static size_t s_NumDictItems;

///////////////////////////////////////////////////////////////////////////////
//  HbDictItem
///////////////////////////////////////////////////////////////////////////////
HbDictItem*
HbDictItem::Create(const HbString* key, const HbString* value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_STRING;
        item->m_ValType = HB_VALUETYPE_STRING;

        if(NULL == (item->m_Key.m_String = key->Dup())
            || NULL == (item->m_Value.m_String = value->Dup()))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const HbString* key, const s64 value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_STRING;
        item->m_ValType = HB_VALUETYPE_INT;
        item->m_Value.m_Int = value;

        if(NULL == (item->m_Key.m_String = key->Dup()))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const HbString* key, const double value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_STRING;
        item->m_ValType = HB_VALUETYPE_DOUBLE;
        item->m_Value.m_Double = value;

        if(NULL == (item->m_Key.m_String = key->Dup()))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const s64 key, const HbString* value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_INT;
        item->m_ValType = HB_VALUETYPE_STRING;
        item->m_Key.m_Int = key;

        if(NULL == (item->m_Value.m_String = value->Dup()))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const s64 key, const s64 value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_INT;
        item->m_ValType = HB_VALUETYPE_INT;
        item->m_Key.m_Int = key;
        item->m_Value.m_Int = value;
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const s64 key, const double value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_INT;
        item->m_ValType = HB_VALUETYPE_DOUBLE;
        item->m_Key.m_Int = key;
        item->m_Value.m_Double = value;
    }

    return item;
}

HbDictItem*
HbDictItem::CreateEmpty(const HbString* key, const size_t len)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_STRING;
        item->m_ValType = HB_VALUETYPE_STRING;

        if(NULL == (item->m_Key.m_String = key->Dup())
            || NULL == (item->m_Value.m_String = HbString::Create(len)))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::CreateEmpty(const s64 key, const size_t len)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_KEYTYPE_INT;
        item->m_ValType = HB_VALUETYPE_STRING;
        item->m_Key.m_Int = key;

        if(NULL == (item->m_Value.m_String = HbString::Create(len)))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

void
HbDictItem::Destroy(HbDictItem* item)
{
    if(item)
    {
        if(HB_VALUETYPE_STRING == item->m_KeyType)
        {
            HbString::Destroy(item->m_Key.m_String);
            item->m_Key.m_String = NULL;
        }

        if(HB_VALUETYPE_STRING == item->m_ValType)
        {
            HbString::Destroy(item->m_Value.m_String);
            item->m_Value.m_String = NULL;
        }

        item->~HbDictItem();
        HbHeap::Free(item);
        --s_NumDictItems;
    }
}

//private:

HbDictItem*
HbDictItem::Create()
{
    HbDictItem* item = (HbDictItem*) HbHeap::ZAlloc(sizeof(HbDictItem));

    if(item)
    {
        new(item) HbDictItem();
        ++s_NumDictItems;
    }

    return item;
}

HbDictItem::HbDictItem()
    : m_Next(0)
    , m_KeyType(HB_KEYTYPE_INVALID)
    , m_ValType(HB_VALUETYPE_INVALID)
{
}

HbDictItem::~HbDictItem()
{
}

///////////////////////////////////////////////////////////////////////////////
//  HbDict
///////////////////////////////////////////////////////////////////////////////
HbDict::HbDict()
    : m_Count(0)
    , m_NumSlots(0)
    , m_HashSalt(FNV1_32_INIT)
{
    m_Slots[0] = m_Slots[1] = NULL;
}

HbDict::~HbDict()
{
}

HbDict*
HbDict::Create()
{
    HbDict* dict = (HbDict*) HbHeap::ZAlloc(sizeof(HbDict));
    if(dict)
    {
        new (dict) HbDict();
        dict->m_Slots[0] = (Slot*) HbHeap::ZAlloc(INITIAL_NUM_SLOTS * sizeof(Slot));
        if(dict->m_Slots[0])
        {
            dict->m_NumSlots = INITIAL_NUM_SLOTS;
        }
        else
        {
            HbDict::Destroy(dict);
            dict = NULL;
        }
    }

    return dict;
}

void
HbDict::Destroy(HbDict* dict)
{
    if(dict)
    {
        for(int i = 0; i < hbarraylen(dict->m_Slots); ++i)
        {
            Slot* slots = dict->m_Slots[i];

            if(!slots)
            {
                continue;
            }

            for(size_t j = 0; j < dict->m_NumSlots; ++j)
            {
                HbDictItem* item = slots[j].m_Item;
                while(item)
                {
                    HbDictItem* next = item->m_Next;
                    HbDictItem::Destroy(item);
                    item = next;
                }
            }

            HbHeap::Free(slots);
            dict->m_Slots[i] = NULL;
        }

        dict->~HbDict();
        HbHeap::Free(dict);
    }
}

bool
HbDict::Set(const HbString* key, const HbString* value)
{
    HbDictItem* item = HbDictItem::Create(key, value);
    if(item)
    {
        Set(item);
        return true;
    }

    return false;
}

bool
HbDict::Set(const HbString* key, const s64 value)
{
    HbDictItem* item = HbDictItem::Create(key, value);
    if(item)
    {
        Set(item);
        return true;
    }

    return false;
}

bool
HbDict::Set(const s64 key, const HbString* value)
{
    HbDictItem* item = HbDictItem::Create(key, value);
    if(item)
    {
        Set(item);
        return true;
    }

    return false;
}

bool
HbDict::Set(const s64 key, const s64 value)
{
    HbDictItem* item = HbDictItem::Create(key, value);
    if(item)
    {
        Set(item);
        return true;
    }

    return false;
}

bool
HbDict::Clear(const s64 key)
{
    Slot* slot;
    HbDictItem** item = Find(key, &slot);
    if(*item)
    {
        HbDictItem* next = (*item)->m_Next;
        //HbDictItem::Destroy(*item);
        *item = next;

        --slot->m_Count;
        hbassert(slot->m_Count >= 0);
        --m_Count;
        hbassert(m_Count >= 0);

        return true;
    }

    return false;
}

bool
HbDict::Clear(const HbString* key)
{
    Slot* slot;
    HbDictItem** item = Find(key, &slot);
    if(*item)
    {
        HbDictItem* next = (*item)->m_Next;
        //HbDictItem::Destroy(*item);
        *item = next;

        --slot->m_Count;
        hbassert(slot->m_Count >= 0);
        --m_Count;
        hbassert(m_Count >= 0);

        return true;
    }

    return false;
}

bool
HbDict::Find(const HbString* key, const HbString** value) const
{
    Slot* slot;
    HbDictItem** item = const_cast<HbDict*>(this)->Find(key, &slot);
    if(*item && (*item)->m_ValType == HB_VALUETYPE_STRING)
    {
        *value = (*item)->m_Value.m_String;
        return true;
    }

    return false;
}

bool
HbDict::Find(const HbString* key, s64* value) const
{
    Slot* slot;
    HbDictItem** item = const_cast<HbDict*>(this)->Find(key, &slot);
    if(*item && (*item)->m_ValType == HB_VALUETYPE_INT)
    {
        *value = (*item)->m_Value.m_Int;
        return true;
    }

    return false;
}

bool
HbDict::Find(const s64 key, const HbString** value) const
{
    Slot* slot;
    HbDictItem** item = const_cast<HbDict*>(this)->Find(key, &slot);
    if(*item && (*item)->m_ValType == HB_VALUETYPE_STRING)
    {
        *value = (*item)->m_Value.m_String;
        return true;
    }

    return false;
}

bool
HbDict::Find(const s64 key, s64* value) const
{
    Slot* slot;
    HbDictItem** item = const_cast<HbDict*>(this)->Find(key, &slot);
    if(*item && (*item)->m_ValType == HB_VALUETYPE_INT)
    {
        *value = (*item)->m_Value.m_Int;
        return true;
    }

    return false;
}

bool
HbDict::Merge(const HbString* key, const HbString* value, const size_t mergeOffset)
{
    Slot* slot;
    HbDictItem** pOldItem = Find(key, &slot);

    if(*pOldItem)
    {
        if(hbverify(HB_VALUETYPE_STRING == (*pOldItem)->m_ValType))
        {
            byte* oldData;
            const byte* mergeData;
            const size_t oldLen = (*pOldItem)->m_Value.m_String->GetData(&oldData);
            const size_t mergeLen = value->GetData(&mergeData);
            if(mergeOffset + mergeLen <= oldLen)
            {
                //New data will be merged within the bounds of
                //the old data.

                //Use memmove because mergeItem and oldItem could be
                //the same item.
                memmove(&oldData[mergeOffset], mergeData, mergeLen);
            }
            else
            {
                //New data extends past the end of the old data.

                const size_t newLen = mergeOffset + mergeLen;
                HbString* newStr = HbString::Create(newLen);
                if(newStr)
                {
                    byte* newData;
                    newStr->GetData(&newData);
                    if(mergeOffset < oldLen)
                    {
                        memcpy(newData, oldData, mergeOffset);
                    }
                    else
                    {
                        memcpy(newData, oldData, oldLen);
                    }

                    memcpy(&newData[mergeOffset], mergeData, mergeLen);

                    HbString::Destroy((*pOldItem)->m_Value.m_String);
                    (*pOldItem)->m_Value.m_String = newStr;
                }
            }

            return true;
        }
    }
    else
    {
        //Item doesn't exist yet, create it.

        const byte* mergeData;
        const size_t mergeLen = value->GetData(&mergeData);
        const size_t newLen = mergeOffset + mergeLen;

        HbDictItem* newItem = HbDictItem::CreateEmpty(key, newLen);

        if(newItem)
        {
            byte* newData;
            newItem->m_Value.m_String->GetData(&newData);
            memcpy(&newData[mergeOffset], mergeData, mergeLen);
            Set(newItem);
            return true;
        }
    }

    return false;
}

bool
HbDict::Merge(const s64 key, const HbString* value, const size_t mergeOffset)
{
    Slot* slot;
    HbDictItem** pOldItem = Find(key, &slot);

    if(*pOldItem)
    {
        if(hbverify(HB_VALUETYPE_STRING == (*pOldItem)->m_ValType))
        {
            byte* oldData;
            const byte* mergeData;
            const size_t oldLen = (*pOldItem)->m_Value.m_String->GetData(&oldData);
            const size_t mergeLen = value->GetData(&mergeData);
            if(mergeOffset + mergeLen <= oldLen)
            {
                //New data will be merged within the bounds of
                //the old data.

                //Use memmove because mergeItem and oldItem could be
                //the same item.
                memmove(&oldData[mergeOffset], mergeData, mergeLen);
            }
            else
            {
                //New data extends past the end of the old data.

                const size_t newLen = mergeOffset + mergeLen;
                HbString* newStr = HbString::Create(newLen);
                if(newStr)
                {
                    byte* newData;
                    newStr->GetData(&newData);
                    if(mergeOffset < oldLen)
                    {
                        memcpy(newData, oldData, mergeOffset);
                    }
                    else
                    {
                        memcpy(newData, oldData, oldLen);
                    }

                    memcpy(&newData[mergeOffset], mergeData, mergeLen);

                    HbString::Destroy((*pOldItem)->m_Value.m_String);
                    (*pOldItem)->m_Value.m_String = newStr;
                }
            }

            return true;
        }
    }
    else
    {
        //Item doesn't exist yet, create it.

        const byte* mergeData;
        const size_t mergeLen = value->GetData(&mergeData);
        const size_t newLen = mergeOffset + mergeLen;

        HbDictItem* newItem = HbDictItem::CreateEmpty(key, newLen);

        if(newItem)
        {
            byte* newData;
            newItem->m_Value.m_String->GetData(&newData);
            memcpy(&newData[mergeOffset], mergeData, mergeLen);
            Set(newItem);
            return true;
        }
    }

    return false;
}

size_t
HbDict::Count() const
{
    return m_Count;
}

//private:

u32
HbDict::HashString(const byte* string, const size_t stringLen) const
{
    return FnvHashBufInitVal(string, stringLen, m_HashSalt);
}

void
HbDict::Set(HbDictItem* newItem)
{
    if(m_Count >= m_NumSlots*2)
    {
        Slot* newSlots = (Slot*) HbHeap::ZAlloc(m_NumSlots * 2 * sizeof(Slot));
        if(newSlots)
        {
            Slot* oldSlots = m_Slots[0];
            m_Slots[0] = newSlots;
            const unsigned oldNumSlots = m_NumSlots;
            m_NumSlots *= 2;

            for(int i = 0; i < (int)oldNumSlots; ++i)
            {
                HbDictItem* item = oldSlots[i].m_Item;
                while(item)
                {
                    HbDictItem* next = item->m_Next;
                    item->m_Next = NULL;

                    bool replaced;
                    this->Set(item, &replaced);
                    hbassert(!replaced);

                    item = next;
                }
            }

            HbHeap::Free(oldSlots);
        }
    }

    bool replaced;
    this->Set(newItem, &replaced);

    if(!replaced)
    {
        ++m_Count;
    }
}

void
HbDict::Set(HbDictItem* newItem, bool* replaced)
{
    Slot* slot;
    HbDictItem** pitem;

    switch(newItem->m_KeyType)
    {
    case HB_VALUETYPE_STRING:
        pitem = Find(newItem->m_Key.m_String, &slot);
        break;
    case HB_VALUETYPE_INT:
        pitem = Find(newItem->m_Key.m_Int, &slot);
        break;
    default:
        pitem = NULL;
        slot = NULL;
        break;
    }

    hbassert(pitem);

    if(!*pitem)
    {
        ++slot->m_Count;
        *replaced = false;
    }
    else
    {
        HbDictItem::Destroy(*pitem);
        *replaced = true;
    }

    *pitem = newItem;
}

HbDictItem**
HbDict::Find(const HbString* key, Slot** slot)
{
    HbDictItem** item = NULL;
    const byte* keyData;
    const size_t keylen = key->GetData(&keyData);
    const u32 hash = HashString(keyData, keylen);
    for(int i = 0; i < 2 && m_Slots[i]; ++i)
    {
        const unsigned idx = hash & (m_NumSlots-1);
        *slot = &m_Slots[i][idx];
        item = &(*slot)->m_Item;

        for(; *item; item = &(*item)->m_Next)
        {
            if((*item)->m_Key.m_String->EQ(keyData, keylen))
            {
                return item;
            }
        }
    }

    return item;
}

HbDictItem**
HbDict::Find(const s64 key, Slot** slot)
{
    HbDictItem** item = NULL;
    const u32 hash = HashString((const byte*)&key, sizeof(key));
    for(int i = 0; i < 2 && m_Slots[i]; ++i)
    {
        const unsigned idx = hash & (m_NumSlots-1);
        *slot = &m_Slots[i][idx];
        item = &(*slot)->m_Item;

        for(; *item; item = &(*item)->m_Next)
        {
            if((*item)->m_Key.m_Int == key)
            {
                return item;
            }
        }
    }

    return item;
}

///////////////////////////////////////////////////////////////////////////////
//  HbDictTest
///////////////////////////////////////////////////////////////////////////////
static ptrdiff_t myrandom (ptrdiff_t i)
{
    return HbRand()%i;
}

void
HbDictTest::TestStringString(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char keyBuf[32];
    byte hbsBuf[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(keyBuf, "foo%d", i);
        HbString* key = HbString::Encode((byte*)keyBuf, strlen(keyBuf), hbsBuf, sizeof(hbsBuf));
        dict->Set(key, key);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(keyBuf, "foo%d", i);
        HbString* key = HbString::Encode((byte*)keyBuf, strlen(keyBuf), hbsBuf, sizeof(hbsBuf));
        const HbString* value;

        hbverify(dict->Find(key, &value));
        hbverify(value->EQ(key));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(keyBuf, "foo%d", i);
        HbString* key = HbString::Encode((byte*)keyBuf, strlen(keyBuf), hbsBuf, sizeof(hbsBuf));
        const HbString* value;

        hbverify(dict->Clear(key));
        hbverify(!dict->Find(key, &value));
    }

    hbverify(0 == dict->Count());

    HbDict::Destroy(dict);
}

void
HbDictTest::TestStringInt(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char keyBuf[32];
    byte hbsBuf[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(keyBuf, "foo%d", i);
        HbString* key = HbString::Encode((byte*)keyBuf, strlen(keyBuf), hbsBuf, sizeof(hbsBuf));
        dict->Set(key, (s64)i);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(keyBuf, "foo%d", i);
        HbString* key = HbString::Encode((byte*)keyBuf, strlen(keyBuf), hbsBuf, sizeof(hbsBuf));
        s64 value;

        hbverify(dict->Find(key, &value));
        hbverify(i == value);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(keyBuf, "foo%d", i);
        HbString* key = HbString::Encode((byte*)keyBuf, strlen(keyBuf), hbsBuf, sizeof(hbsBuf));
        s64 value;

        hbverify(dict->Clear(key));
        hbverify(!dict->Find(key, &value));
    }

    hbverify(0 == dict->Count());

    HbDict::Destroy(dict);
}

void
HbDictTest::TestIntInt(const int numKeys)
{
    HbDict* dict = HbDict::Create();

    for(s64 i = 0; i < numKeys; ++i)
    {
        dict->Set(i, (s64)i);
    }

    for(s64 i = 0; i < numKeys; ++i)
    {
        s64 value;
        hbverify(dict->Find(i, &value));
        hbverify((s64)i == value);
    }

    for(s64 i = 0; i < numKeys; ++i)
    {
        s64 value;
        hbverify(dict->Clear(i));
        hbverify(!dict->Find(i, &value));
    }

    hbverify(0 == dict->Count());

    HbDict::Destroy(dict);
}

void
HbDictTest::TestIntString(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char valBuf[32];
    byte hbsBuf[32];

    for(s64 i = 0; i < numKeys; ++i)
    {
        sprintf(valBuf, "foo%d", i);
        HbString* value = HbString::Encode((byte*)valBuf, strlen(valBuf), hbsBuf, sizeof(hbsBuf));
        dict->Set(i, value);
    }

    for(s64 i = 0; i < numKeys; ++i)
    {
        sprintf(valBuf, "foo%d", i);
        const HbString* value;

        hbverify(dict->Find(i, &value));
        hbverify(value->EQ(HbString::Encode((byte*)valBuf, strlen(valBuf), hbsBuf, sizeof(hbsBuf))));
    }

    for(s64 i = 0; i < numKeys; ++i)
    {
        hbverify(dict->Clear(i));
        const HbString* value;
        hbverify(!dict->Find(i, &value));
    }

    hbverify(0 == dict->Count());

    HbDict::Destroy(dict);
}

struct KV_int_string
{
    static const int SECTION_LEN    = 256;
    int m_Key;
    unsigned m_FinalLen;
    char m_Test[SECTION_LEN*3];
};
void
HbDictTest::TestMergeIntKeys(const int numKeys, const int numIterations)
{
    static const char alphabet[] =
    {
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
    };

    HbDict* dict = HbDict::Create();

    //Create random length strings, with random offsets
    KV_int_string* kv = new KV_int_string[numKeys];
    memset(kv, 0, numKeys*sizeof(*kv));

    KV_int_string* p = kv;
    for(int i = 0; i < numKeys; ++i, ++p)
    {
        p->m_Key = i;
    }

    std::random_shuffle(&kv[0], &kv[numKeys]);

    for(int iter = 0; iter < numIterations; ++iter)
    {
        p = kv;
        for(int i = 0; i < numKeys; ++i, ++p)
        {
            const unsigned offset = HbRand(0, KV_int_string::SECTION_LEN-1);
            const unsigned len = HbRand(1, KV_int_string::SECTION_LEN);
            if(offset+len > p->m_FinalLen)
            {
                p->m_FinalLen = offset + len;
            }

            const unsigned abOffset = HbRand() % (sizeof(alphabet) - len);
            byte hbsBuf[KV_int_string::SECTION_LEN*2];
            const HbString* value =
                HbString::Encode((byte*)&alphabet[abOffset], len,
                                    hbsBuf, sizeof(hbsBuf));

            hbverify(dict->Merge(p->m_Key, value, offset));

            memcpy(&p->m_Test[offset], &alphabet[abOffset], len);
        }

        //std::random_shuffle(&kv[0], &kv[numKeys]);

        //Check they've been added
        p = kv;
        for(int i = 0; i < numKeys; ++i, ++p)
        {
            const HbString* value;
            hbverify(dict->Find(p->m_Key, &value));
            const byte* data;
            const size_t len = value->GetData(&data);
            hbverify(len == p->m_FinalLen);
            hbverify(0 == memcmp(data, p->m_Test, p->m_FinalLen));
        }
    }

    //Delete the items
    p = kv;
    for(int i = 0; i < numKeys; ++i, ++p)
    {
        const HbString* value;
        hbverify(dict->Clear(p->m_Key));
        hbverify(!dict->Find(p->m_Key, &value));
    }

    hbverify(0 == dict->Count());

    HbDict::Destroy(dict);

    delete [] kv;
}

void
HbDictTest::CreateRandomKeys(KV* kv, const int numKeys)
{
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Key = i;
    }
    std::random_shuffle(&kv[0], &kv[numKeys], myrandom);
}

void
HbDictTest::AddRandomKeys(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    s64 value;

    HbStopWatch sw;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        const bool added = dict->Set(kv[i].m_Key, (s64)kv[i].m_Value);
        if(added)
        {
            hbverify(dict->Find(kv[i].m_Key, &value));
            hbverify(value == kv[i].m_Value);
        }
    }
    sw.Stop();
    s_Log.Debug("set: %f", sw.GetElapsed());

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool found = dict->Find(kv[i].m_Key, &value);
        hbassert(found);
        hbverify(value == kv[i].m_Value);
    }
    sw.Stop();
    s_Log.Debug("find: %f", sw.GetElapsed());
    
    /*std::sort(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
    hbassert(dict->Find(kv[i].m_Key, &value));
    hbassert(value == kv[i].m_Value);
    }
    sw.Stop();*/

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        const bool cleared =dict->Clear(kv[i].m_Key);
        hbassert(cleared);
    }
    sw.Stop();
    s_Log.Debug("delete: %f", sw.GetElapsed());

    HbDict::Destroy(dict);
    delete [] kv;
}
void
HbDictTest::AddDeleteRandomKeys(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    s64 value;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys);
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
    }

    for(int i = 0; i < numKeys; ++i)
    {
        int idx = HbRand() % numKeys;
        if(!kv[idx].m_Added)
        {
            const bool added = dict->Set(kv[i].m_Key, (s64)kv[i].m_Value);
            if(added)
            {
                kv[idx].m_Added = true;
                hbverify(dict->Find(kv[idx].m_Key, &value));
                hbverify(value == kv[idx].m_Value);
            }
        }
        else
        {
            const bool cleared = dict->Clear(kv[idx].m_Key);
            hbassert(cleared);
            kv[idx].m_Added = false;
            hbverify(!dict->Find(kv[idx].m_Key, &value));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            const bool found = dict->Find(kv[i].m_Key, &value);
            hbassert(found);
            hbverify(value == kv[i].m_Value);
        }
        else
        {
            const bool found = dict->Find(kv[i].m_Key, &value);
            hbassert(!found);
        }
    }

    HbDict::Destroy(dict);
    delete [] kv;
}


