#define _CRT_SECURE_NO_WARNINGS

#include "dict.h"

#include <algorithm>
#include <assert.h>
#include <new.h>
#include <stdio.h>
#include <string.h>

#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define UB 1

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
HbDictItem::Create(const byte* key, const size_t keylen,
                    const byte* value, const size_t vallen)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_STRING;

        if(NULL == (item->m_Key.m_String = HbString::Create(key, keylen))
            || NULL == (item->m_Value.m_String = HbString::Create(value, vallen)))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const byte* key, const size_t keylen, const s64 value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_INT;
        item->m_Value.m_Int = value;

        if(NULL == (item->m_Key.m_String = HbString::Create(key, keylen)))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const byte* key, const size_t keylen, const double value)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_DOUBLE;
        item->m_Value.m_Double = value;

        if(NULL == (item->m_Key.m_String = HbString::Create(key, keylen)))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::CreateDict(const byte* key, const size_t keylen)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_DICT;

        if(NULL == (item->m_Key.m_String = HbString::Create(key, keylen))
            || NULL == (item->m_Value.m_Dict = HbDict::Create()))
        {
            Destroy(item);
            item = NULL;
        }
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const s64 key, const byte* value, const size_t vallen)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_STRING;
		item->m_Key.m_Int = key;

        if(NULL == (item->m_Value.m_String = HbString::Create(value, vallen)))
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
        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_INT;
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
        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_DOUBLE;
        item->m_Key.m_Int = key;
        item->m_Value.m_Double = value;
    }

    return item;
}

HbDictItem*
HbDictItem::CreateDict(const s64 key)
{
    HbDictItem* item = Create();
    if(item)
    {
        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_DICT;
        item->m_Key.m_Int = key;

        if(NULL == (item->m_Value.m_Dict = HbDict::Create()))
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
        if(HB_ITEMTYPE_STRING == item->m_KeyType)
        {
            HbString::Destroy(item->m_Key.m_String);
            item->m_Key.m_String = NULL;
        }

        if(HB_ITEMTYPE_STRING == item->m_ValType)
        {
            HbString::Destroy(item->m_Value.m_String);
            item->m_Value.m_String = NULL;
        }
        else if(HB_ITEMTYPE_DICT == item->m_ValType)
        {
            HbDict::Destroy(item->m_Value.m_Dict);
            item->m_Value.m_Dict = NULL;
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
, m_KeyType(HB_ITEMTYPE_INVALID)
, m_ValType(HB_ITEMTYPE_INVALID)
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
        for(int i = 0; i < HB_ARRAYLEN(dict->m_Slots); ++i)
        {
            Slot* slots = dict->m_Slots[i];

            if(!slots)
            {
                continue;
            }

            for(int j = 0; j < dict->m_NumSlots; ++j)
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
HbDict::Find(const s64 key, s64* value) const
{
    HbDictItem** item = const_cast<HbDict*>(this)->Find(key);
    if(*item && (*item)->m_ValType == HB_ITEMTYPE_INT)
    {
        *value = (*item)->m_Value.m_Int;
        return true;
    }

    return false;
}

HbDictItem**
HbDict::Find(const byte* key, const size_t keylen)
{
	Slot* slot;
	return Find(key, keylen, &slot);
}

HbDictItem**
HbDict::Find(const s64 key)
{
	Slot* slot;
	return Find(key, &slot);
}

HbDictItem**
HbDict::Find(const HbString* key)
{
	Slot* slot;
	return Find(key, &slot);
}

void
HbDict::Set(HbDictItem* newItem)
{
	if(m_NumSlots*2 <= m_Count)
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
                    assert(!replaced);

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

bool
HbDict::Merge(HbDictItem* mergeItem, const size_t mergeOffset)
{
    if(hbVerify(HB_ITEMTYPE_STRING == mergeItem->m_ValType))
    {
	    Slot* slot;
        HbDictItem** pOldItem = Find(mergeItem->m_Key, mergeItem->m_KeyType, &slot);

        if(*pOldItem)
	    {
            if(hbVerify(HB_ITEMTYPE_STRING == (*pOldItem)->m_ValType))
            {
                byte* oldData;
                const byte* mergeData;
                const size_t oldLen = (*pOldItem)->m_Value.m_String->GetData(&oldData);
                const size_t mergeLen = mergeItem->m_Value.m_String->GetData(&mergeData);
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

            HbDictItem* newItem;
            const byte* mergeData;
            const size_t mergeLen = mergeItem->m_Value.m_String->GetData(&mergeData);
            const size_t newLen = mergeOffset + mergeLen;

	        switch(mergeItem->m_KeyType)
	        {
	        case HB_ITEMTYPE_STRING:
                {
                    const byte* keyData;
                    const size_t keyLen = mergeItem->m_Key.m_String->GetData(&keyData);
		            newItem = HbDictItem::Create(keyData, keyLen, NULL,  newLen);
                }
		        break;
	        case HB_ITEMTYPE_INT:
		        newItem = HbDictItem::Create(mergeItem->m_Key.m_Int, NULL,  newLen);
		        break;
            default:
                newItem = NULL;
                break;
	        }

            if(newItem)
            {
                byte* newData;
                newItem->m_Value.m_String->GetData(&newData);
                memcpy(&newData[mergeOffset], mergeData, mergeLen);
                Set(newItem);
                return true;
            }
        }
    }

    return false;
}

bool
HbDict::Clear(const byte* key, const size_t keylen)
{
	Slot* slot;
    HbDictItem** item = Find(key, keylen, &slot);
    if(*item)
	{
		HbDictItem* next = (*item)->m_Next;
        HbDictItem::Destroy(*item);
		*item = next;

		--slot->m_Count;
		assert(slot->m_Count >= 0);
		--m_Count;
		assert(m_Count >= 0);

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
		assert(slot->m_Count >= 0);
		--m_Count;
		assert(m_Count >= 0);

        return true;
	}

    return false;
}

s64
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

HbDictItem**
HbDict::Find(const byte* key, const size_t keylen, Slot** slot)
{
	HbDictItem** item = NULL;
    const u32 hash = HashString(key, keylen);
    for(int i = 0; i < 2 && m_Slots[i]; ++i)
    {
	    const unsigned idx = hash & (m_NumSlots-1);
	    *slot = &m_Slots[i][idx];
	    item = &(*slot)->m_Item;

	    for(; *item; item = &(*item)->m_Next)
	    {
		    if((*item)->m_Key.m_String->EQ(key, keylen))
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
HbDict::Find(const HbDictValue& key, const HbItemType keyType, Slot** slot)
{
	switch(keyType)
	{
	case HB_ITEMTYPE_STRING:
        return Find(key.m_String, slot);
	case HB_ITEMTYPE_INT:
		return Find(key.m_Int, slot);
    default:
        slot = NULL;
        break;
	}

    return NULL;
}

void
HbDict::Set(HbDictItem* newItem, bool* replaced)
{
	Slot* slot;
    HbDictItem** pitem;

	switch(newItem->m_KeyType)
	{
	case HB_ITEMTYPE_STRING:
        {
            const byte* data;
            const size_t keylen = newItem->m_Key.m_String->GetData(&data);
		    pitem = Find(data, keylen, &slot);
        }
		break;
	case HB_ITEMTYPE_INT:
		pitem = Find(newItem->m_Key.m_Int, &slot);
		break;
    default:
        pitem = NULL;
        slot = NULL;
        break;
	}

    assert(pitem);

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

///////////////////////////////////////////////////////////////////////////////
//  HbDictTest
///////////////////////////////////////////////////////////////////////////////
static ptrdiff_t myrandom (ptrdiff_t i)
{
    return HbRand()%i;
}

#if !HB_ASSERT
void
HbDictTest::TestStringString(const int /*numKeys*/)
{
}
#else
void
HbDictTest::TestStringString(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char key[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);
        HbDictItem* item = HbDictItem::Create((byte*)key, keylen, (byte*)key, keylen);
        dict->Set(item);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->Find((byte*)key, keylen);
		assert(*pitem);
		assert((*pitem)->m_Value.m_String->EQ((byte*)key, keylen));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->Find((byte*)key, keylen);
		assert(*pitem);
		dict->Clear((byte*)key, keylen);
        pitem = dict->Find((byte*)key, keylen);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);
}
#endif

#if !HB_ASSERT
void
HbDictTest::TestStringInt(const int /*numKeys*/)
{
}
#else
void
HbDictTest::TestStringInt(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char key[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);
        HbDictItem* item = HbDictItem::Create((byte*)key, keylen, (s64)i);
        dict->Set(item);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->Find((byte*)key, keylen);
		assert(*pitem);
		assert(i == (*pitem)->m_Value.m_Int);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->Find((byte*)key, keylen);
		assert(*pitem);
		dict->Clear((byte*)key, keylen);
        pitem = dict->Find((byte*)key, keylen);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);
}
#endif

#if !HB_ASSERT
void
HbDictTest::TestIntInt(const int /*numKeys*/)
{
}
#else
void
HbDictTest::TestIntInt(const int numKeys)
{
    HbDict* dict = HbDict::Create();

    for(int i = 0; i < numKeys; ++i)
    {
        HbDictItem* item = HbDictItem::Create((s64)i, (s64)i);
        dict->Set(item);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        HbDictItem** pitem = dict->Find((s64)i);
		assert(*pitem);
        assert(i == (*pitem)->m_Value.m_Int);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        HbDictItem** pitem = dict->Find((s64)i);
		assert(*pitem);
		dict->Clear((s64)i);
        pitem = dict->Find((s64)i);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);
}
#endif

#if !HB_ASSERT
void
HbDictTest::TestIntString(const int /*numKeys*/)
{
}
#else
void
HbDictTest::TestIntString(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char value[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(value, "foo%d", i);
        const size_t vallen = strlen(value);
        HbDictItem* item = HbDictItem::Create((s64)i, (byte*)value, vallen);
        dict->Set(item);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(value, "foo%d", i);
        const size_t vallen = strlen(value);

        HbDictItem** pitem = dict->Find((s64)i);
		assert(*pitem);
		assert((*pitem)->m_Value.m_String->EQ((byte*)value, vallen));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        HbDictItem** pitem = dict->Find((s64)i);
		assert(*pitem);
		dict->Clear((s64)i);
        pitem = dict->Find((s64)i);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);
}
#endif
#if !HB_ASSERT
void
HbDictTest::TestMerge(const int /*numKeys*/)
{
}
#else

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

            HbDictItem* item = HbDictItem::Create(p->m_Key, (byte*)&alphabet[abOffset], len);
            assert(dict->Merge(item, offset));
            HbDictItem::Destroy(item);

            memcpy(&p->m_Test[offset], &alphabet[abOffset], len);
        }

        std::random_shuffle(&kv[0], &kv[numKeys]);

        //Check they've been added
        p = kv;
        for(int i = 0; i < numKeys; ++i, ++p)
        {
            HbDictItem** pitem = dict->Find(p->m_Key);
		    assert(*pitem);
            const byte* data;
            const size_t len = (*pitem)->m_Value.m_String->GetData(&data);
            assert(len == p->m_FinalLen);
            assert(0 == memcmp(data, p->m_Test, p->m_FinalLen));
        }
    }

    //Delete the items
    p = kv;
    for(int i = 0; i < numKeys; ++i, ++p)
    {
        dict->Clear(p->m_Key);
        HbDictItem** pitem = dict->Find(p->m_Key);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);

    delete [] kv;
}
#endif

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
        HbDictItem* item = HbDictItem::Create(kv[i].m_Key, (s64)kv[i].m_Value);
        if(hbVerify(item))
        {
            dict->Set(item);
            HB_ASSERTONLY(bool found =)
                dict->Find(kv[i].m_Key, &value);
            assert(found);
            assert(value == kv[i].m_Value);
        }
    }
    sw.Stop();

    std::random_shuffle(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        assert(dict->Find(kv[i].m_Key, &value));
        assert(value == kv[i].m_Value);
    }
    sw.Stop();

    /*std::sort(&kv[0], &kv[numKeys]);

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        assert(dict->Find(kv[i].m_Key, &value));
        assert(value == kv[i].m_Value);
    }
    sw.Stop();*/

    sw.Restart();
    for(int i = 0; i < numKeys; ++i)
    {
        hbVerify(dict->Clear(kv[i].m_Key));
    }
    sw.Stop();

    HbDict::Destroy(dict);
    delete [] kv;
}
void
HbDictTest::AddDeleteRandomKeys(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    HB_ASSERTONLY(s64 value);

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
            HbDictItem* item = HbDictItem::Create(kv[i].m_Key, (s64)kv[i].m_Value);
            if(hbVerify(item))
            {
                dict->Set(item);
                kv[idx].m_Added = true;
                assert(dict->Find(kv[idx].m_Key, &value));
                assert(value == kv[idx].m_Value);
            }
        }
        else
        {
            hbVerify(dict->Clear(kv[idx].m_Key));
            kv[idx].m_Added = false;
            assert(!dict->Find(kv[idx].m_Key, &value));
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            assert(dict->Find(kv[i].m_Key, &value));
            assert(value == kv[i].m_Value);
        }
        else
        {
            assert(!dict->Find(kv[i].m_Key, &value));
        }
    }

    HbDict::Destroy(dict);
    delete [] kv;
}


