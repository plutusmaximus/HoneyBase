#define _CRT_SECURE_NO_WARNINGS

#include "dict.h"

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
    HbDictItem* item = (HbDictItem*) HbHeap::Alloc(sizeof(HbDictItem));

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
	: m_Slots(NULL)
	, m_Count(0)
	, m_NumSlots(0)
	, m_HashSalt(FNV1_32_INIT)
{
}

HbDict::~HbDict()
{
}

HbDict*
HbDict::Create()
{
    HbDict* dict = (HbDict*) HbHeap::Alloc(sizeof(HbDict));
    if(dict)
    {
        new (dict) HbDict();
		dict->m_Slots = (Slot*) HbHeap::Alloc(INITIAL_NUM_SLOTS * sizeof(Slot));
		if(dict->m_Slots)
		{
            memset(dict->m_Slots, 0, INITIAL_NUM_SLOTS*sizeof(Slot));
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
        if(dict->m_Slots)
        {
            for(int i = 0; i < dict->m_NumSlots; ++i)
            {
		        HbDictItem* item = dict->m_Slots[i].m_Item;
                HbDictItem* next = item ? item->m_Next : NULL;
                for(; item; item = next, next = next->m_Next)
                {
                    HbDictItem::Destroy(item);
                }
            }

            HbHeap::Free(dict->m_Slots);
            dict->m_Slots = NULL;
        }

        dict->~HbDict();
        HbHeap::Free(dict);
    }
}

HbDictItem**
HbDict::FindItem(const byte* key, const size_t keylen)
{
	Slot* slot;
	return FindItem(key, keylen, &slot);
}

HbDictItem**
HbDict::FindItem(const s64 key)
{
	Slot* slot;
	return FindItem(key, &slot);
}

void
HbDict::Set(HbDictItem* newItem)
{
	Slot* slot;
    HbDictItem** item;

	switch(newItem->m_KeyType)
	{
	case HB_ITEMTYPE_STRING:
        {
            const byte* data;
            const size_t keylen = newItem->m_Key.m_String->GetData(&data);
		    item = FindItem(data, keylen, &slot);
        }
		break;
	case HB_ITEMTYPE_INT:
		item = FindItem(newItem->m_Key.m_Int, &slot);
		break;
    default:
        item = NULL;
        slot = NULL;
        break;
	}

    assert(item);

    if(!*item)
    {
		++slot->m_Count;
        ++m_Count;
    }
    else
    {
        HbDictItem::Destroy(*item);
    }

    *item = newItem;
}

void
HbDict::Set(const byte* key, const size_t keylen,
            const byte* value, const size_t vallen)
{
    HbDictItem* newItem = HbDictItem::Create(key, keylen, value, vallen);
    Set(newItem);
}

void
HbDict::Set(const byte* key, const size_t keylen, const s64 value)
{
    HbDictItem* newItem = HbDictItem::Create(key, keylen, value);
    Set(newItem);
}

void
HbDict::Set(const byte* key, const size_t keylen, const double value)
{
    HbDictItem* newItem = HbDictItem::Create(key, keylen, value);
    Set(newItem);
}

HbDict*
HbDict::SetDict(const byte* key, const size_t keylen)
{
    HbDictItem* newItem = HbDictItem::CreateDict(key, keylen);
    Set(newItem);
    return newItem->m_Value.m_Dict;
}

void
HbDict::Set(const s64 key, const byte* value, const size_t vallen)
{
    HbDictItem* newItem = HbDictItem::Create(key, value, vallen);
    Set(newItem);
}

void
HbDict::Set(const s64 key, const s64 value)
{
    HbDictItem* newItem = HbDictItem::Create(key, value);
    Set(newItem);
}

void
HbDict::Set(const s64 key, const double value)
{
    HbDictItem* newItem = HbDictItem::Create(key, value);
    Set(newItem);
}

HbDict*
HbDict::SetDict(const s64 key)
{
    HbDictItem* newItem = HbDictItem::CreateDict(key);
    Set(newItem);
    return newItem->m_Value.m_Dict;
}

void
HbDict::Clear(const byte* key, const size_t keylen)
{
	Slot* slot;
    HbDictItem** item = FindItem(key, keylen, &slot);
    if(*item)
	{
		HbDictItem* next = (*item)->m_Next;
        HbDictItem::Destroy(*item);
		*item = next;

		--slot->m_Count;
		assert(slot->m_Count >= 0);
		--m_Count;
		assert(m_Count >= 0);
	}
}

void
HbDict::Clear(const s64 key)
{
	Slot* slot;
    HbDictItem** item = FindItem(key, &slot);
    if(*item)
	{
		HbDictItem* next = (*item)->m_Next;
        HbDictItem::Destroy(*item);
		*item = next;

		--slot->m_Count;
		assert(slot->m_Count >= 0);
		--m_Count;
		assert(m_Count >= 0);
	}
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
HbDict::FindItem(const byte* key, const size_t keylen, Slot** slot)
{
	HbDictItem** item = NULL;
	HbDict* dict = this;
	while(dict)
	{
		const unsigned idx = dict->HashString(key, keylen) & (dict->m_NumSlots-1);
		*slot = &dict->m_Slots[idx];
		item = &(*slot)->m_Item;
		if(*item && (*item)->m_ValType == HB_ITEMTYPE_DICT)
		{
			dict = (*item)->m_Value.m_Dict;
		}
		else
		{
			dict = NULL;
		}
	}

	for(; *item; item = &(*item)->m_Next)
	{
		if((*item)->m_Key.m_String->EQ(key, keylen))
		{
			return item;
		}
	}

    return item;
}

HbDictItem**
HbDict::FindItem(const s64 key, Slot** slot)
{
	HbDictItem** item = NULL;
	HbDict* dict = this;
	while(dict)
	{
		const unsigned idx = key & (dict->m_NumSlots-1);
		*slot = &dict->m_Slots[idx];
		item = &(*slot)->m_Item;
		if(*item && (*item)->m_ValType == HB_ITEMTYPE_DICT)
		{
			dict = (*item)->m_Value.m_Dict;
		}
		else
		{
			dict = NULL;
		}
	}

	for(; *item; item = &(*item)->m_Next)
	{
		if(key == (*item)->m_Key.m_Int)
		{
			return item;
		}
	}

    return item;
}

///////////////////////////////////////////////////////////////////////////////
//  HbDictTest
///////////////////////////////////////////////////////////////////////////////
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

        dict->Set((byte*)key, keylen, (byte*)key, keylen);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->FindItem((byte*)key, keylen);
		assert(*pitem);
		assert((*pitem)->m_Value.m_String->EQ((byte*)key, keylen));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->FindItem((byte*)key, keylen);
		assert(*pitem);
		dict->Clear((byte*)key, keylen);
        pitem = dict->FindItem((byte*)key, keylen);
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

        dict->Set((byte*)key, keylen, (s64)i);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->FindItem((byte*)key, keylen);
		assert(*pitem);
		assert(i == (*pitem)->m_Value.m_Int);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);
        const size_t keylen = strlen(key);

        HbDictItem** pitem = dict->FindItem((byte*)key, keylen);
		assert(*pitem);
		dict->Clear((byte*)key, keylen);
        pitem = dict->FindItem((byte*)key, keylen);
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
        dict->Set((s64)i, (s64)i);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        HbDictItem** pitem = dict->FindItem((s64)i);
		assert(*pitem);
        assert(i == (*pitem)->m_Value.m_Int);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        HbDictItem** pitem = dict->FindItem((s64)i);
		assert(*pitem);
		dict->Clear((s64)i);
        pitem = dict->FindItem((s64)i);
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

        dict->Set((s64)i, (byte*)value, vallen);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(value, "foo%d", i);
        const size_t vallen = strlen(value);

        HbDictItem** pitem = dict->FindItem((s64)i);
		assert(*pitem);
		assert((*pitem)->m_Value.m_String->EQ((byte*)value, vallen));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        HbDictItem** pitem = dict->FindItem((s64)i);
		assert(*pitem);
		dict->Clear((s64)i);
        pitem = dict->FindItem((s64)i);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);
}
#endif
