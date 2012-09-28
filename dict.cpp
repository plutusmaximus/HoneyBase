#define _CRT_SECURE_NO_WARNINGS

#include "hb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <algorithm>
#include <assert.h>

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

static unsigned m_w = 77665;    /* must not be zero */
static unsigned m_z = 22559;    /* must not be zero */

unsigned HbRand()
{
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;  /* 32-bit result */
}

static const size_t PAGE_SIZE   = 8*1024;

class HbHeapBucket
{
public:
    HbHeapBucket()
        : m_Free(NULL)
        , m_End(NULL)
    {
        m_Next = m_Prev = this;
    }

    void LinkBefore(HbHeapBucket* b)
    {
        m_Next = b;
        m_Prev = b->m_Prev;
        b->m_Prev = b->m_Prev->m_Next = this;
    }

    void LinkAfter(HbHeapBucket* b)
    {
        m_Prev = b;
        m_Next = b->m_Next;
        b->m_Next = b->m_Next->m_Prev = this;
    }

    void Unlink()
    {
        m_Prev->m_Next = m_Next;
        m_Next->m_Prev = m_Prev;
        m_Next = m_Prev = this;
    }

    byte* m_Free;
    byte* m_End;
    HbHeapBucket* m_Next;
    HbHeapBucket* m_Prev;
};

/*class HbHandleBase
{
public:

    HbHandleBase()
    {
    }

    template<typename T>
    static HbHandle<T>* NewHandle(T* item)
    {
        if(s_NextHandle < sizeof(s_Handles)/sizeof(s_Handles[0]))
        {
            HbHandleBase* handle = &s_Handles[s_NextHandle++];
            handle->m_Item = item;
            return HbHandle<T>* handle;
        }

        return NULL;
    }

    static void FreeHandle(HbHandleBase* handle);

protected:

    void* m_Item;

private:

    static HbHandleBase s_Handles[1024];
    static size_t s_NextHandle;

    HbHandleBase(const HbHandleBase&);
    HbHandleBase& operator=(const HbHandleBase&);
};

void
HbHandleBase::FreeHandle(HbHandleBase* handle)
{
    handle->m_Item = NULL;
}

template<typename T>
class HbHandle : public HbHandleBase
{
public:

    T* Pin()
    {
        return (T*)m_Item;
    }

private:

    HbHandle(const HbHandle<T>&);
    HbHandle<T>& operator=(const HbHandle<T>&);
};*/

///////////////////////////////////////////////////////////////////////////////
//  HbHeap
///////////////////////////////////////////////////////////////////////////////
static size_t s_SizeofDictItems;
static size_t s_NumDictItems;
class HbHeap
{
public:

	void* AllocMem(size_t size)
	{
		return calloc(1, size);
	}

	HbDict::Slot* AllocDictSlots(const size_t numSlots)
	{
		return (HbDict::Slot*)AllocMem(sizeof(HbDict::Slot)*numSlots);
	}

    HbDictItem* AllocDictItem(const size_t sizeofData)
    {
		const size_t size = sizeof(HbDictItem)+sizeofData;
        HbDictItem* item = (HbDictItem*) AllocMem(size);

        if(item)
        {
            new(item) HbDictItem();
			s_SizeofDictItems += size;
			++s_NumDictItems;
        }

        return item;
    }

    HbIndexNode* AllocIndexNode()
    {
        HbIndexNode* node = (HbIndexNode*) AllocMem(sizeof(HbIndexNode));

        if(node)
        {
            return new(node) HbIndexNode();
        }

        return NULL;
    }

	void FreeMem(void* mem)
	{
		free(mem);
	}

    void FreeDictItem(HbDictItem* item)
    {
		HbDict::Slot* slots =
			(HB_ITEMTYPE_DICT == item->m_ValType) ? item->m_Value.m_Dict->m_Slots : NULL;
		item->~HbDictItem();
        FreeMem(item);
		if(slots)
		{
			FreeMem(slots);
		}

        --s_NumDictItems;
    }

    void FreeIndexNode(HbIndexNode* node)
    {
		node->~HbIndexNode();
        FreeMem(node);
    }
};

static HbHeap s_Heap;

///////////////////////////////////////////////////////////////////////////////
//  HbString
///////////////////////////////////////////////////////////////////////////////
HbString*
HbString::Create(const char* str)
{
    return Create((byte*)str, strlen(str));
}

HbString*
HbString::Create(const byte* str, const size_t len)
{
    int lenBytes = 0;
    size_t tmpLen = len;

    while(tmpLen)
    {
        tmpLen >>= 7;
        ++lenBytes;
    }

    const size_t memLen = sizeof(HbString) + len + lenBytes;
    HbString* hbs = (HbString*) s_Heap.AllocMem(memLen);
    if(hbs)
    {
        new (hbs) HbString();

        hbs->m_Bytes = (byte*) &hbs[1];

        int offset = 0;
        tmpLen = len;
        for(int i = lenBytes-1; i > 0; --i)
        {
            hbs->m_Bytes[offset++] = 0x80 | (tmpLen & 0x7F);
            tmpLen >>= 7;
        }

        hbs->m_Bytes[offset++] = tmpLen & 0x7F;

        memcpy(&hbs->m_Bytes[offset], str, len);
    }

    return hbs;
}

void
HbString::Destroy(HbString* hbs)
{
    hbs->~HbString();
    s_Heap.FreeMem(hbs);
}

size_t
HbString::Length() const
{
    if(m_Bytes)
    {
        size_t len = 0;
        const byte* p = m_Bytes;
        int shift = 0;

        while(*p & 0x80)
        {
            len |= (*p++ & 0x7F) << shift;
            shift += 7;
        }

        len |= (*p & 0x7F) << shift;

        return len;
    }

    return 0;
}

bool
HbString::operator==(const HbString& that)
{
    const size_t mylen = Length();
    return (that.Length() == mylen
            && (0 == mylen || 0 == memcmp(m_Bytes, that.m_Bytes, mylen)));
}

bool
HbString::operator!=(const HbString& that)
{
    const size_t mylen = Length();
    return (that.Length() != mylen ||
            (0 != mylen && 0 != memcmp(m_Bytes, that.m_Bytes, mylen)));
}

//private:

HbString::HbString()
    : m_Bytes(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////
//  HbStringTest
///////////////////////////////////////////////////////////////////////////////
void
HbStringTest::Test()
{
    HbString* hbs = HbString::Create("Foo");
    assert(hbs->Length() == 3);
    HbString::Destroy(hbs);

    size_t len;
    byte* str;

    len = 0x7F + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    assert(hbs->Length() == len);
    HbString::Destroy(hbs);
    delete [] str;

    len = 0x7FFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    assert(hbs->Length() == len);
    HbString::Destroy(hbs);
    delete [] str;

    len = 0x7FFFFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    assert(hbs->Length() == len);
    HbString::Destroy(hbs);
    delete [] str;
}
///////////////////////////////////////////////////////////////////////////////
//  HbDictItem
///////////////////////////////////////////////////////////////////////////////
HbDictItem*
HbDictItem::Create(const char* key, const char* value)
{
    size_t keylen = strlen(key);
    HbDictItem* item = s_Heap.AllocDictItem(keylen+strlen(value)+2);
    if(item)
    {
        item->m_Key.m_String = (char*)&item[1];
        item->m_Value.m_String = item->m_Key.m_String + keylen + 1;
        strcpy((char*)item->m_Key.m_String, key);
        strcpy((char*)item->m_Value.m_String, value);

        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_STRING;
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const char* key, const s64 value)
{
    size_t keylen = strlen(key);
    HbDictItem* item = s_Heap.AllocDictItem(keylen+1);
    if(item)
    {
        item->m_Key.m_String = (char*)&item[1];
        strcpy((char*)item->m_Key.m_String, key);
        item->m_Value.m_Int = value;

        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_INT;
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const char* key, const double value)
{
    size_t keylen = strlen(key);
    HbDictItem* item = s_Heap.AllocDictItem(keylen+1);
    if(item)
    {
        item->m_Key.m_String = (char*)&item[1];
        strcpy((char*)item->m_Key.m_String, key);
        item->m_Value.m_Double = value;

        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_DOUBLE;
    }

    return item;
}

HbDictItem*
HbDictItem::CreateDict(const char* key)
{
    size_t keylen = strlen(key);
    HbDictItem* item = s_Heap.AllocDictItem(keylen+1+sizeof(HbDict));
	assert(item);
    if(item)
    {
        item->m_Key.m_String = (char*)&item[1];
        item->m_Value.m_Dict = (HbDict*)(item->m_Key.m_String + keylen + 1);
        new (item->m_Value.m_Dict) HbDict;
        strcpy((char*)item->m_Key.m_String, key);

        item->m_KeyType = HB_ITEMTYPE_STRING;
        item->m_ValType = HB_ITEMTYPE_DICT;
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const s64 key, const char* value)
{
    HbDictItem* item = s_Heap.AllocDictItem(strlen(value)+1);
    if(item)
    {
		item->m_Key.m_Int = key;
        item->m_Value.m_String = (char*)&item[1];
        strcpy((char*)item->m_Value.m_String, value);

        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_STRING;
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const s64 key, const s64 value)
{
    HbDictItem* item = s_Heap.AllocDictItem(0);
    if(item)
    {
        item->m_Key.m_Int = key;
        item->m_Value.m_Int = value;

        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_INT;
    }

    return item;
}

HbDictItem*
HbDictItem::Create(const s64 key, const double value)
{
    HbDictItem* item = s_Heap.AllocDictItem(0);
    if(item)
    {
        item->m_Key.m_Int = key;
        item->m_Value.m_Double = value;

        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_DOUBLE;
    }

    return item;
}

HbDictItem*
HbDictItem::CreateDict(const s64 key)
{
    HbDictItem* item = s_Heap.AllocDictItem(sizeof(HbDict));
	assert(item);
    if(item)
    {
        item->m_Key.m_Int = key;
        item->m_Value.m_Dict = (HbDict*)&item[1];
        new (item->m_Value.m_Dict) HbDict;

        item->m_KeyType = HB_ITEMTYPE_INT;
        item->m_ValType = HB_ITEMTYPE_DICT;
    }

    return item;
}

void
HbDictItem::Destroy(HbDictItem* item)
{
    s_Heap.FreeDictItem(item);
}

HbDictItem::HbDictItem()
{
}

HbDictItem::~HbDictItem()
{
    if(HB_ITEMTYPE_DICT == m_ValType)
    {
        m_Value.m_Dict->~HbDict();
    }

    memset(this, 0, sizeof(this));
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
    for(int i = 0; i < m_NumSlots; ++i)
    {
		HbDictItem* item = m_Slots[i].m_Item;
        HbDictItem* next = item ? item->m_Next : NULL;
        for(; item; item = next, next = next->m_Next)
        {
            HbDictItem::Destroy(item);
        }
    }

    memset(this, 0, sizeof(*this));
}

HbDict*
HbDict::Create()
{
    HbDictItem* item = s_Heap.AllocDictItem(sizeof(HbDict));
    if(item)
    {
        item->m_Key.m_String = NULL;
		HbDict* dict = item->m_Value.m_Dict = (HbDict*)&item[1];
        new (dict) HbDict;
		dict->m_Slots = s_Heap.AllocDictSlots(HbDict::INITIAL_NUM_SLOTS);
		if(dict->m_Slots)
		{
			dict->m_NumSlots = HbDict::INITIAL_NUM_SLOTS;
			item->m_KeyType = HB_ITEMTYPE_INVALID;
			item->m_ValType = HB_ITEMTYPE_DICT;
		}
		else
		{
			s_Heap.FreeDictItem(item);
			item = NULL;
		}
    }

    return item ? item->m_Value.m_Dict : NULL;
}

void
HbDict::Destroy(HbDict* dict)
{
    dict->~HbDict();

    u8* mem = (u8*)dict;
    mem -= sizeof(HbDictItem);

    s_Heap.FreeDictItem((HbDictItem*)mem);
}

HbDictItem**
HbDict::FindItem(const char* key)
{
	Slot* slot;
	return FindItem(key, &slot);
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
		item = FindItem(newItem->m_Key.m_String, &slot);
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

	/*if(slot->m_Count >= 8)
	{
		HbDictItem* dict = HbDictItem::CreateDict("");

		dict->m_Value.m_Dict->m_HashSalt = m_HashSalt+1;

		for(HbDictItem* tmpItem = slot->m_Item; NULL != tmpItem;)
		{
			HbDictItem* next = tmpItem->m_Next;
			tmpItem->m_Next = NULL;
			dict->m_Value.m_Dict->Set(tmpItem);
			tmpItem = next;
		}
		slot->m_Item = dict;
		slot->m_Count = 1;
	}*/
}

void
HbDict::Set(const char* key, const char* value)
{
    HbDictItem* newItem = HbDictItem::Create(key, value);
    Set(newItem);
}

void
HbDict::Set(const char* key, const s64 value)
{
    HbDictItem* newItem = HbDictItem::Create(key, value);
    Set(newItem);
}

void
HbDict::Set(const char* key, const double value)
{
    HbDictItem* newItem = HbDictItem::Create(key, value);
    Set(newItem);
}

HbDict*
HbDict::SetDict(const char* key)
{
    HbDictItem* newItem = HbDictItem::CreateDict(key);
    Set(newItem);
    return newItem->m_Value.m_Dict;
}

void
HbDict::Set(const s64 key, const char* value)
{
    HbDictItem* newItem = HbDictItem::Create(key, value);
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
HbDict::Clear(const char* key)
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
HbDict::HashString(const char* str)
{
    return FnvHashBufInitVal((byte*)str, strlen(str), m_HashSalt);
}

HbDictItem**
HbDict::FindItem(const char* key, Slot** slot)
{
	HbDictItem** item = NULL;
	HbDict* dict = this;
	while(dict)
	{
		const unsigned idx = dict->HashString(key) & (dict->m_NumSlots-1);
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
		if(0 == strcmp((*item)->m_Key.m_String, key))
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
void
HbDictTest::TestStringString(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char key[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);

        dict->Set(key, key);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);

        HbDictItem** pitem = dict->FindItem(key);
		assert(*pitem);
		assert(0 == strcmp((*pitem)->m_Value.m_String, key));
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);

        HbDictItem** pitem = dict->FindItem(key);
		assert(*pitem);
		dict->Clear(key);
        pitem = dict->FindItem(key);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);
}

void
HbDictTest::TestStringInt(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char key[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);

        dict->Set(key, (s64)i);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);

        HbDictItem** pitem = dict->FindItem(key);
		assert(*pitem);
		assert(i == (*pitem)->m_Value.m_Int);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(key, "foo%d", i);

        HbDictItem** pitem = dict->FindItem(key);
		assert(*pitem);
		dict->Clear(key);
        pitem = dict->FindItem(key);
		assert(!*pitem);
    }

    assert(0 == dict->Count());

    HbDict::Destroy(dict);
}

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

void
HbDictTest::TestIntString(const int numKeys)
{
    HbDict* dict = HbDict::Create();
    char value[32];

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(value, "foo%d", i);

        dict->Set((s64)i, value);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        sprintf(value, "foo%d", i);

        HbDictItem** pitem = dict->FindItem((s64)i);
		assert(*pitem);
		assert(0 == strcmp((*pitem)->m_Value.m_String, value));
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

///////////////////////////////////////////////////////////////////////////////
//  HbIndexItem
///////////////////////////////////////////////////////////////////////////////
HbIndexItem::HbIndexItem()
    : m_IntKey(0)
    , m_Node(NULL)
{
}

///////////////////////////////////////////////////////////////////////////////
//  HbIndexNode
///////////////////////////////////////////////////////////////////////////////
HbIndexNode::HbIndexNode()
    : m_NumItems(0)
{
    m_Next = m_Prev = this;
}

void
HbIndexNode::Dump(const bool leafOnly, const int curDepth, const int maxDepth) const
{
    static const char INDENT[] = {"                                "};
    char buf[256];
    const bool isLeaf = (curDepth == maxDepth);
    for(int i = 0; i < (int)ARRAYLEN(m_Items); ++i)
    {
        if(!leafOnly || isLeaf)
        {
            if(i >= m_NumItems)
            {
                snprintf(buf, sizeof(buf)-1, "%.*s_", curDepth, INDENT);
                printf("%s%s%p\n", buf, isLeaf?"L":"", this);
                continue;
            }
            else
            {
                snprintf(buf, sizeof(buf)-1, "%.*s%"PRId64"", curDepth, INDENT, (s64)m_Items[i].m_IntKey);
                printf("%s%s%p\n", buf, isLeaf?"L":"", this);
            }
        }

        if(!isLeaf && i < m_NumItems)
        {
            m_Items[i].m_Node->Dump(leafOnly, curDepth+1, maxDepth);
        }
    }
}

void
HbIndexNode::LinkBefore(HbIndexNode* node)
{
    m_Next = node;
    m_Prev = node->m_Prev;
    node->m_Prev = node->m_Prev->m_Next = this;
}

void
HbIndexNode::LinkAfter(HbIndexNode* node)
{
    m_Next = node->m_Next;
    m_Prev = node;
    node->m_Next = node->m_Next->m_Prev = this;
}

void
HbIndexNode::Unlink()
{
    m_Prev->m_Next = m_Next;
    m_Next->m_Prev = m_Prev;
    m_Next = m_Prev = this;
}

void
HbIndexNode::MoveItemsRight(HbIndexNode* src, HbIndexNode* dst, const int numItems)
{
    for(int i = dst->m_NumItems-1; i >= 0; --i)
    {
        dst->m_Items[dst->m_NumItems+numItems] = dst->m_Items[i];
    }

    for(int i = 0; i < numItems; ++i)
    {
        dst->m_Items[i] = src->m_Items[src->m_NumItems - (numItems-i)];
    }

    dst->m_NumItems += numItems;
    src->m_NumItems -= numItems;
}

void
HbIndexNode::MoveItemsLeft(HbIndexNode* src, HbIndexNode* dst, const int numItems)
{
    for(int i = 0; i < numItems; ++i)
    {
        dst->m_Items[dst->m_NumItems++] = src->m_Items[i];
    }

    src->m_NumItems -= numItems;
    for(int i = dst->m_NumItems-1; i >= 0; --i)
    {
        dst->m_Items[dst->m_NumItems+numItems] = dst->m_Items[i];
    }

    for(int i = 0; i < numItems; ++i)
    {
        dst->m_Items[i] = src->m_Items[src->m_NumItems - (numItems-i)];
    }

    dst->m_NumItems += numItems;
    src->m_NumItems -= numItems;
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
    m_First = m_Cur = NULL;
    m_ItemIndex = 0;
}

bool
HbIterator::HasNext() const
{
    if(!m_Cur)
    {
        return false;
    }
    else if(m_Cur == m_First->m_Prev)
    {
        return m_ItemIndex < m_Cur->m_NumItems-1;
    }

    return true;
}

bool
HbIterator::HasPrev() const
{
    if(!m_Cur)
    {
        return false;
    }
    else if(m_Cur == m_First)
    {
        return m_ItemIndex > 0;
    }

    return true;
}

bool
HbIterator::HasCurrent() const
{
    return NULL != m_Cur;
}

void
HbIterator::Next()
{
    if(HasNext())
    {
        ++m_ItemIndex;

        if(m_ItemIndex >= m_Cur->m_NumItems)
        {
            m_Cur = m_Cur->m_Next;
            m_ItemIndex = 0;
        }
    }
    else
    {
        m_Cur = NULL;
    }
}

void
HbIterator::Prev()
{
    if(HasPrev())
    {
        --m_ItemIndex;

        if(m_ItemIndex < 0)
        {
            m_Cur = m_Cur->m_Prev;
            m_ItemIndex = m_Cur->m_NumItems;
        }
    }
    else
    {
        m_Cur = NULL;
    }
}

s64
HbIterator::GetKey()
{
    return m_Cur->m_Items[m_ItemIndex].m_IntKey;
}

int
HbIterator::GetValue()
{
    return m_Cur->m_Items[m_ItemIndex].m_Value;
}

///////////////////////////////////////////////////////////////////////////////
//  HbIndex
///////////////////////////////////////////////////////////////////////////////
static int LowerBound(const int key, HbIndexItem* first, HbIndexItem* end)
{
    size_t count = end - first;
    HbIndexItem* cur = first;
    while(count > 0)
    {
        HbIndexItem* item = cur;
        size_t step = count >> 1;
        item += step;
        if(item->m_IntKey < key)
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

static int UpperBound(const int key, HbIndexItem* first, HbIndexItem* end)
{
    size_t count = end - first;
    HbIndexItem* cur = first;
    while(count > 0)
    {
        HbIndexItem* item = cur;
        size_t step = count >> 1;
        item += step;
        if(!(key < item->m_IntKey))
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

HbIndex::HbIndex()
    : m_Nodes(NULL)
    , m_Count(0)
    , m_Capacity(0)
    , m_Depth(0)
    , m_KeySize(0)
{
}

void
HbIndex::DumpStats() const
{
    printf("Fill: %"PRIu64"/%"PRIu64" %g\n", m_Count, m_Capacity, (double)m_Count/m_Capacity);
}

void
HbIndex::Dump(const bool leafOnly) const
{
    m_Nodes->Dump(leafOnly, 0, m_Depth-1);

    DumpStats();
}

bool
HbIndex::Insert(const int key, const int value)
{
    if(NULL == m_Nodes)
    {
        m_Nodes = s_Heap.AllocIndexNode();
        if(!m_Nodes)
        {
            return false;
        }

        m_Capacity += ARRAYLEN(m_Nodes->m_Items);
        ++m_Depth;
    }

    HbIndexNode* node = m_Nodes;
    HbIndexNode* parent = NULL;
    int itemIdx = 0;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        if(ARRAYLEN(node->m_Items) == node->m_NumItems)
        {
            HbIndexNode* rightSibling;
            HbIndexNode* leftSibling;

            //If there's room then move one item from the current
            //node to one of the siblings.
            rightSibling = (parent && itemIdx < parent->m_NumItems-1) ? parent->m_Items[itemIdx+1].m_Node : NULL;
            if(!rightSibling || rightSibling->m_NumItems >= (int)ARRAYLEN(rightSibling->m_Items)-1)
            {
                rightSibling = NULL;
                leftSibling = (parent && itemIdx > 0) ? parent->m_Items[itemIdx-1].m_Node : NULL;

                if(leftSibling && leftSibling->m_NumItems >= (int)ARRAYLEN(leftSibling->m_Items)-1)
                {
                    leftSibling = NULL;
                }
            }
            else
            {
                leftSibling = NULL;
            }

            if(rightSibling)
            {
                for(int i = rightSibling->m_NumItems; i > 0; --i)
                {
                    rightSibling->m_Items[i] = rightSibling->m_Items[i-1];
                }

                rightSibling->m_Items[0] = node->m_Items[node->m_NumItems-1];
                ++rightSibling->m_NumItems;
                --node->m_NumItems;
                parent->m_Items[itemIdx].m_IntKey = node->m_Items[node->m_NumItems-1].m_IntKey;

                if(key > node->m_Items[node->m_NumItems-1].m_IntKey)
                {
                    ++itemIdx;
                    assert(itemIdx < parent->m_NumItems);
                    node = rightSibling;
                }
            }
            else if(leftSibling)
            {
                leftSibling->m_Items[leftSibling->m_NumItems] = node->m_Items[0];

                for(int i = 0; i < node->m_NumItems-1; ++i)
                {
                    node->m_Items[i] = node->m_Items[i+1];
                }

                ++leftSibling->m_NumItems;
                --node->m_NumItems;
                parent->m_Items[itemIdx-1].m_IntKey = leftSibling->m_Items[leftSibling->m_NumItems-1].m_IntKey;

                if(key <= leftSibling->m_Items[leftSibling->m_NumItems-1].m_IntKey)
                {
                    --itemIdx;
                    assert(itemIdx >= 0);
                    node = leftSibling;
                }
            }
            else
            {
                //Split the node

                HbIndexNode* sibling = s_Heap.AllocIndexNode();
                if(!sibling)
                {
                    return false;
                }

                if(!parent)
                {
                    parent = s_Heap.AllocIndexNode();
                    if(!parent)
                    {
                        s_Heap.FreeIndexNode(sibling);
                        return false;
                    }

                    parent->m_Items[parent->m_NumItems++].m_Node = node;
                    m_Nodes = parent;
                    m_Capacity += ARRAYLEN(parent->m_Items);

                    ++m_Depth;
                    ++depth;
                }

                m_Capacity += ARRAYLEN(sibling->m_Items);

                const int splitLoc = ARRAYLEN(node->m_Items) / 2;

                for(int i = splitLoc; i < (int)ARRAYLEN(node->m_Items); ++i, --node->m_NumItems, ++sibling->m_NumItems)
                {
                    sibling->m_Items[sibling->m_NumItems] = node->m_Items[i];
                }

                for(int i = parent->m_NumItems; i > itemIdx+1; --i)
                {
                    parent->m_Items[i] = parent->m_Items[i-1];
                }

                parent->m_Items[itemIdx].m_IntKey = node->m_Items[node->m_NumItems-1].m_IntKey;
                parent->m_Items[itemIdx+1].m_IntKey = sibling->m_Items[sibling->m_NumItems-1].m_IntKey;
                parent->m_Items[itemIdx+1].m_Node = sibling;
                ++parent->m_NumItems;

                sibling->LinkAfter(node);

                //if(key >= sibling->m_Items[0].m_IntKey)
                if(key > node->m_Items[node->m_NumItems-1].m_IntKey)
                {
                    ++itemIdx;
                    assert(itemIdx < parent->m_NumItems);
                    node = sibling;
                }
            }
        }

        //Is it an inner node (non-leaf)?
        if(depth < m_Depth-1)
        {
            int newItemIdx = ::UpperBound(key, &node->m_Items[0], &node->m_Items[node->m_NumItems]);
            if(newItemIdx == node->m_NumItems)
            {
                --newItemIdx;
                if(parent)
                {
                    parent->m_Items[itemIdx].m_IntKey = key;
                }
            }

            parent = node;
            node = node->m_Items[newItemIdx].m_Node;
            itemIdx = newItemIdx;
        }
    }

    const int newItemIdx = ::UpperBound(key, &node->m_Items[0], &node->m_Items[node->m_NumItems]);

    if(newItemIdx < node->m_NumItems)
    {
        for(int i = node->m_NumItems; i > newItemIdx; --i)
        {
            node->m_Items[i] = node->m_Items[i-1];
        }
        node->m_Items[newItemIdx].m_IntKey = key;
        node->m_Items[newItemIdx].m_Value = value;
        ++node->m_NumItems;
    }
    else
    {
        if(parent)
        {
            parent->m_Items[itemIdx].m_IntKey = key;
        }

        node->m_Items[newItemIdx].m_IntKey = key;
        node->m_Items[newItemIdx].m_Value = value;
        ++node->m_NumItems;
    }

    /*//Find the first item with a larger key and insert the new item before it.
    const int newItemIdx = ::UpperBound(key, &node->m_Items[0], &node->m_Items[node->m_NumItems]);

    if(newItemIdx < node->m_NumItems)
    {
        for(int i = node->m_NumItems; i > newItemIdx; --i)
        {
            node->m_Items[i] = node->m_Items[i-1];
        }
    }
    else if(parent)
    {
        parent->m_Items[itemIdx].m_IntKey = key;
    }

    node->m_Items[newItemIdx].m_IntKey = key;
    node->m_Items[newItemIdx].m_Value = value;
    ++node->m_NumItems;*/

    ++m_Count;

    return true;
}

bool
HbIndex::Delete(const int key)
{
    HbIndexNode* parent = NULL;
    HbIndexNode* node = m_Nodes;
    int itemIdx = 0;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        if(parent && node->m_NumItems < (int)ARRAYLEN(node->m_Items)/2)
        {
            HbIndexNode* rightSibling = itemIdx < parent->m_NumItems-1 ? parent->m_Items[itemIdx+1].m_Node : NULL;
            HbIndexNode* leftSibling = itemIdx > 0 ? parent->m_Items[itemIdx-1].m_Node : NULL;
            HbIndexNode* sibling = NULL;

            if(rightSibling && (node->m_NumItems + rightSibling->m_NumItems) < (int)ARRAYLEN(node->m_Items))
            {
                sibling = rightSibling;
            }
            else if(leftSibling && (node->m_NumItems + leftSibling->m_NumItems) < (int)ARRAYLEN(node->m_Items))
            {
                sibling = node;
                node = leftSibling;
                --itemIdx;
            }
            else if(rightSibling)
            {
                node->m_Items[node->m_NumItems] = rightSibling->m_Items[0];
                parent->m_Items[itemIdx].m_IntKey = node->m_Items[node->m_NumItems].m_IntKey;
                ++node->m_NumItems;

                for(int i = 0; i < rightSibling->m_NumItems-1; ++i)
                {
                    rightSibling->m_Items[i] = rightSibling->m_Items[i+1];
                }

                --rightSibling->m_NumItems;
            }
            else if(leftSibling)
            {
                node->m_Items[node->m_NumItems] = node->m_Items[0];
                node->m_Items[0] = leftSibling->m_Items[leftSibling->m_NumItems-1];
                ++node->m_NumItems;
                parent->m_Items[itemIdx-1].m_IntKey = leftSibling->m_Items[node->m_NumItems-1].m_IntKey;
                --leftSibling->m_NumItems;
            }

            if(sibling)
            {
                //Merge

                for(int i = 0; i < sibling->m_NumItems; ++i)
                {
                    node->m_Items[node->m_NumItems++] = sibling->m_Items[i];
                }

                parent->m_Items[itemIdx].m_IntKey = parent->m_Items[itemIdx+1].m_IntKey;

                for(int i = itemIdx+1; i < parent->m_NumItems-1; ++i)
                {
                    parent->m_Items[i] = parent->m_Items[i+1];
                }

                sibling->Unlink();
                m_Capacity -= ARRAYLEN(sibling->m_Items);
                s_Heap.FreeIndexNode(sibling);

                --parent->m_NumItems;

                if(1 == parent->m_NumItems && 1 == depth)
                {
                    m_Nodes = node;
                    m_Capacity -= ARRAYLEN(parent->m_Items);
                    s_Heap.FreeIndexNode(parent);
                    --m_Depth;
                    --depth;
                }
            }
        }

        const int newItemIdx = ::LowerBound(key, &node->m_Items[0], &node->m_Items[node->m_NumItems]);
        if(newItemIdx >= node->m_NumItems)
        {
            //Doesnt exist
            return false;
        }

        if(depth == m_Depth-1)  //Is it a leaf?
        {
            if(key != node->m_Items[newItemIdx].m_IntKey)
            {
                return false;
            }
            else if(node->m_NumItems > 1)
            {
                for(int i = newItemIdx; i < node->m_NumItems-1; ++i)
                {
                    node->m_Items[i] = node->m_Items[i+1];
                }

                --node->m_NumItems;
            }
            else if(parent)
            {
                for(int i = itemIdx; i < parent->m_NumItems-1; ++i)
                {
                    parent->m_Items[i] = parent->m_Items[i+1];
                }

                --parent->m_NumItems;

                node->Unlink();
                m_Capacity -= ARRAYLEN(node->m_Items);
                s_Heap.FreeIndexNode(node);

                assert(parent->m_NumItems > 0);
            }
            else
            {
                assert(node == m_Nodes);
                m_Capacity -= ARRAYLEN(node->m_Items);
                s_Heap.FreeIndexNode(node);
                m_Nodes = node = NULL;
                m_Depth = 0;
                break;
            }
        }

        parent = node;
        node = node->m_Items[newItemIdx].m_Node;
        itemIdx = newItemIdx;
    }

    --m_Count;
    return true;
}

bool
HbIndex::GetFirst(HbIterator* it) const
{
    it->Clear();

    if(m_Nodes)
    {
        HbIndexNode* node = m_Nodes;

        for(int depth = 0; depth < m_Depth-1; ++depth)
        {
            node = node->m_Items[0].m_Node;
        }

        it->m_Cur = it->m_First = node;
        return true;
    }

    return false;
}

bool
HbIndex::Find(const int key, int* value) const
{
    HbIterator it;

    if(Find(key, &it))
    {
        *value = it.GetValue();
        return true;
    }

    return false;
}

bool
HbIndex::Find(const int key, HbIterator* it) const
{
    if(m_Nodes)
    {
        HbIndexNode* node = m_Nodes;

        for(int depth = 0; depth < m_Depth-1 && node; ++depth)
        {
            int itemIdx = ::LowerBound(key, &node->m_Items[0], &node->m_Items[node->m_NumItems]);
            if(itemIdx < node->m_NumItems)
            {
                node = node->m_Items[itemIdx].m_Node;
            }
            else
            {
                node = NULL;
            }
        }

        if(node)
        {
            int itemIdx = ::LowerBound(key, &node->m_Items[0], &node->m_Items[node->m_NumItems]);

            if(itemIdx < node->m_NumItems && key == node->m_Items[itemIdx].m_IntKey)
            {
                it->Clear();
                it->m_Cur = it->m_First = node;
                it->m_ItemIndex = itemIdx;
                return true;
            }
        }
    }

    return false;
}

unsigned
HbIndex::Count(const int key) const
{
    HbIterator it;
    if(Find(key, &it))
    {
        unsigned count = 0;
        for(; it.HasCurrent() && key == it.GetKey(); it.Next())
        {
            ++count;
        }

        return count;
    }

    return 0;
}

bool
HbIndex::Validate() const
{
    if(m_Nodes)
    {
        const HbIndexNode* parent = m_Nodes;
        for(int i = 0; i < m_Depth-1; ++i)
        {
            const HbIndexNode* first = parent->m_Items[0].m_Node;
            const HbIndexNode* cur = first;
            const HbIndexNode* prev = NULL;

            const HbIndexNode* p = parent;
            do
            {
                for(int j = 0; j < p->m_NumItems; ++j)
                {
                    const HbIndexNode* child = p->m_Items[j].m_Node;
                    if(p->m_Items[j].m_IntKey != child->m_Items[child->m_NumItems-1].m_IntKey)
                    {
                        assert(false);
                        return false;
                    }
                }
            }
            while(parent != p);

            do
            {
                for(int j = 1; j < cur->m_NumItems; ++j)
                {
                    if(cur->m_Items[j].m_IntKey < cur->m_Items[j-1].m_IntKey)
                    {
                        assert(false);
                        return false;
                    }
                }
                prev = cur;
                cur = cur->m_Next;

                if(cur != first && cur->m_Items[0].m_IntKey < prev->m_Items[prev->m_NumItems-1].m_IntKey)
                {
                    assert(false);
                    return false;
                }
            }
            while(cur != first);

            parent = first;
        }
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//  HbIndexTest
///////////////////////////////////////////////////////////////////////////////

struct TextKv
{
    int m_Key;
    int m_Value;

    bool operator<(const TextKv& a) const
    {
        return m_Key < a.m_Key;
    }
};

static ptrdiff_t myrandom (ptrdiff_t i)
{
    return HbRand()%i;
}

void
HbIndexTest::AddRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbIndex index;
    int value;

    int* keys = NULL;

    if(unique)
    {
        keys = new int[numKeys];
        for(int i = 0; i < numKeys; ++i)
        {
            keys[i] = i;
        }
        std::random_shuffle(&keys[0], &keys[numKeys], myrandom);
    }

    TextKv* kv = new TextKv[numKeys];
    int i;
    for(i = 0; i < numKeys; ++i)
    {
        kv[i].m_Key = unique ? keys[i] : HbRand() % range;
        kv[i].m_Value = i;
        index.Insert(kv[i].m_Key, kv[i].m_Value);
        assert(index.Find(kv[i].m_Key, &value) && (!unique || value == kv[i].m_Value));
        //index.Validate();
    }

    index.DumpStats();

    index.Validate();

    //index.Dump(true);

    std::sort(&kv[0], &kv[numKeys]);

    for(i = 0; i < numKeys; ++i)
    {
        assert(index.Find(kv[i].m_Key, &value) && (!unique || value == kv[i].m_Value));
    }

    for(i = 0; i < numKeys; ++i)
    {
        int j = HbRand() % numKeys;
        assert(index.Find(kv[j].m_Key, &value) && (!unique || value == kv[j].m_Value));
    }

    for(i = 0; i < numKeys; ++i)
    {
        assert(index.Delete(kv[i].m_Key));
    }

    index.Validate();
}

void
HbIndexTest::AddSortedKeys(const int numKeys)
{
    HbIndex index;

    TextKv* kv = new TextKv[numKeys];
    int i;
    for(i = 0; i < numKeys; ++i)
    {
        kv[i].m_Key = HbRand() % 32767;
    }

    std::sort(&kv[0], &kv[numKeys]);

    for(i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        index.Insert(kv[i].m_Key, kv[i].m_Value);
    }

    index.Validate();

    int count = 0;
    HbIterator it;
    index.GetFirst(&it);
    for(; it.HasCurrent(); it.Next())
    {
        ++count;
    }
    assert(count == numKeys);
}

int main(int /*argc*/, char** /*argv*/)
{
    HbStringTest::Test();
    HbDictTest::TestStringString(1024);
    HbDictTest::TestStringInt(1024);
    HbDictTest::TestIntInt(1024);
    HbDictTest::TestIntString(1024);

    HbIndexTest::AddRandomKeys(1024*1024, false, 32767);
    HbIndexTest::AddRandomKeys(1024*1024, true, 0);

    HbIndexTest::AddSortedKeys(1024*1024);

}
