#define _CRT_SECURE_NO_WARNINGS

#include "hb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <algorithm>
#include <assert.h>

#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__GNUC__)
#include <time.h>
#endif

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

static unsigned HbRand()
{
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;  /* 32-bit result */
}

template<typename T>
static inline void Move(T* dst, const T* src, const size_t count)
{
    memmove(dst, src, count * sizeof(T));
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
    size_t memLen = sizeof(HbString) + len;
    assert(memLen > len);   //throw
    size_t tmpLen = len;

    while(tmpLen)
    {
        tmpLen >>= 7;
        ++memLen;
    }

    HbString* hbs = (HbString*) s_Heap.AllocMem(memLen);
    if(hbs)
    {
        new (hbs) HbString();

        hbs->m_Bytes = (byte*) &hbs[1];

        int offset = 0;
        tmpLen = len;
        while(tmpLen > 0x7F)
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
//  HbStopWatch
///////////////////////////////////////////////////////////////////////////////
HbStopWatch::HbStopWatch()
: m_Start(0)
, m_Stop(0)
, m_Running(false)
{
#if _MSC_VER
    QueryPerformanceFrequency((LARGE_INTEGER*)&m_Freq);
#elif defined(__GNUC__)
    m_Freq = 1000000000;
#endif
}

void
HbStopWatch::Start()
{
#if _MSC_VER
    QueryPerformanceCounter((LARGE_INTEGER*)&m_Start);
#elif defined(__GNUC__)
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    m_Start = (ts.tv_sec*m_Freq) + ts.tv_nsec;
#endif
    m_Running = true;
}

void
HbStopWatch::Stop()
{
#if _MSC_VER
    QueryPerformanceCounter((LARGE_INTEGER*)&m_Stop);
#elif defined(__GNUC__)
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    m_Stop = (ts.tv_sec*m_Freq) + ts.tv_nsec;
#endif
    m_Running = false;
}

double
HbStopWatch::GetElapsed() const
{
    u64 stop;
    if(m_Running)
    {
#if _MSC_VER
        QueryPerformanceCounter((LARGE_INTEGER*)&stop);
#elif defined(__GNUC__)
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        stop = (ts.tv_sec*m_Freq) + ts.tv_nsec;
#endif
    }
    else
    {
        stop = m_Stop;
    }

    return ((double)(stop - m_Start)) / m_Freq;
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
    m_Cur = NULL;
    m_ItemIndex = 0;
}

bool
HbIterator::HasNext() const
{
    if(m_Cur)
    {
        return (m_ItemIndex < m_Cur->m_NumKeys-1)
                || (NULL != m_Cur->m_Items[HbIndexNode::NUM_KEYS].m_Node);
    }

    return false;
}

bool
HbIterator::HasCurrent() const
{
    return NULL != m_Cur;
}

bool
HbIterator::Next()
{
    if(m_Cur)
    {
        ++m_ItemIndex;

        if(m_ItemIndex >= m_Cur->m_NumKeys)
        {
            m_Cur = m_Cur->m_Items[HbIndexNode::NUM_KEYS].m_Node;
            m_ItemIndex = 0;
        }
    }

    return (NULL != m_Cur);
}

s64
HbIterator::GetKey()
{
    return m_Cur->m_Keys[m_ItemIndex];
}

s64
HbIterator::GetValue()
{
    return m_Cur->m_Items[m_ItemIndex].m_Value;
}

///////////////////////////////////////////////////////////////////////////////
//  HbIndexNode
///////////////////////////////////////////////////////////////////////////////
HbIndexNode::HbIndexNode()
: m_NumKeys(0)
{
    memset(m_Keys, 0, sizeof(m_Keys));
}

bool
HbIndexNode::HasDups() const
{
    return m_NumKeys >= 2
            && m_Keys[m_NumKeys-1] == m_Keys[m_NumKeys-2];
}


///////////////////////////////////////////////////////////////////////////////
//  HbIndex2
///////////////////////////////////////////////////////////////////////////////
HbIndex::HbIndex()
: m_Nodes(NULL)
, m_Leaves(NULL)
, m_Count(0)
, m_Capacity(0)
, m_Depth(0)
{
}

bool
HbIndex::Insert(const s64 key, const s64 value)
{
    if(!m_Nodes)
    {
        m_Leaves = m_Nodes = new HbIndexNode();
        if(!m_Nodes)
        {
            return false;
        }

        m_Capacity += HbIndexNode::NUM_KEYS+1;
        ++m_Depth;

        m_Nodes->m_Keys[0] = key;
        m_Nodes->m_Items[0].m_Value = value;
        ++m_Nodes->m_NumKeys;

        ++m_Count;

        return true;
    }

    HbIndexNode* node = m_Nodes;
    HbIndexNode* parent = NULL;
    int keyIdx = 0;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        const bool isLeaf = (depth == m_Depth-1);

#define AUTO_DEFRAG_A 1
#if AUTO_DEFRAG_A
        if(HbIndexNode::NUM_KEYS == node->m_NumKeys && parent)
        {
            HbIndexNode* sibling = NULL;
            if(keyIdx > 0)
            {
                //Left sibling
                sibling = parent->m_Items[keyIdx-1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1)
                {
                    MergeLeft(parent, keyIdx, 1, depth);

                    if(key < parent->m_Keys[keyIdx-1])
                    {
                        --keyIdx;
                        node = sibling;
                    }
                }
                else
                {
                    sibling = NULL;
                }
            }

            if(!sibling && keyIdx < parent->m_NumKeys)
            {
                //right sibling
                sibling = parent->m_Items[keyIdx+1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1)
                {
                    MergeRight(parent, keyIdx, 1, depth);

                    if(key >= parent->m_Keys[keyIdx])
                    {
                        ++keyIdx;
                        node = sibling;
                    }
                }
                else
                {
                    sibling = NULL;
                }
            }
        }
#endif  //AUTO_DEFRAG

#define AUTO_DEFRAG 0
#if AUTO_DEFRAG
        if(HbIndexNode::NUM_KEYS == node->m_NumKeys && parent)
        {
            HbIndexNode* sibling = NULL;
            if(keyIdx > 0)
            {
                //Left sibling
                sibling = parent->m_Items[keyIdx-1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1)
                {
                    if(isLeaf)
                    {
                        sibling->m_Keys[sibling->m_NumKeys] = node->m_Keys[0];
                        sibling->m_Items[sibling->m_NumKeys] = node->m_Items[0];

                        Move(&node->m_Items[0], &node->m_Items[1], node->m_NumKeys-1);

                        parent->m_Keys[keyIdx-1] = node->m_Keys[1];
                    }
                    else
                    {
                        sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
                        sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];

                        Move(&node->m_Items[0], &node->m_Items[1], node->m_NumKeys);

                        parent->m_Keys[keyIdx-1] = node->m_Keys[0];
                    }

                    Move(&node->m_Keys[0], &node->m_Keys[1], node->m_NumKeys-1);

                    ++sibling->m_NumKeys;
                    --node->m_NumKeys;

                    //DO NOT SUBMIT
                    //TrimNode(sibling, depth);
                    //TrimNode(node, depth);
                    //ValidateNode(depth-1, parent);

                    if(key < parent->m_Keys[keyIdx-1])
                    {
                        --keyIdx;
                        node = sibling;
                    }
                }
                else
                {
                    sibling = NULL;
                }
            }

            if(!sibling && keyIdx < parent->m_NumKeys)
            {
                //right sibling
                sibling = parent->m_Items[keyIdx+1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1)
                {
                    Move(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);

                    if(isLeaf)
                    {
                        Move(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys);

                        sibling->m_Keys[0] = node->m_Keys[node->m_NumKeys-1];
                        sibling->m_Items[0] = node->m_Items[node->m_NumKeys-1];
                        parent->m_Keys[keyIdx] = sibling->m_Keys[0];
                    }
                    else
                    {
                        Move(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);

                        sibling->m_Keys[0] = parent->m_Keys[keyIdx];
                        sibling->m_Items[0] = node->m_Items[node->m_NumKeys];
                        parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-1];
                    }

                    ++sibling->m_NumKeys;
                    --node->m_NumKeys;

                    //DO NOT SUBMIT
                    //TrimNode(sibling, depth);
                    //TrimNode(node, depth);
                    //ValidateNode(depth-1, parent);

                    if(key >= parent->m_Keys[keyIdx])
                    {
                        ++keyIdx;
                        node = sibling;
                    }
                }
                else
                {
                    sibling = NULL;
                }
            }
        }
#endif  //AUTO_DEFRAG

        if(HbIndexNode::NUM_KEYS == node->m_NumKeys)
        {
            /*if(isLeaf && node->HasDups())
            {
                HbIndexNode* newNode = node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
                if(!newNode
                    || HbIndexNode::NUM_KEYS == newNode->m_NumKeys
                    || newNode->m_Keys[0] != node->m_Keys[node->m_NumKeys-1])
                {
                    newNode = new HbIndexNode();
                    m_Capacity += HbIndexNode::NUM_KEYS+1;

                    newNode->m_Items[HbIndexNode::NUM_KEYS].m_Node =
                        node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
                    node->m_Items[HbIndexNode::NUM_KEYS].m_Node = newNode;
                }

                node = newNode;
            }
            else*/
            {
                //Split the node.

                //Guarantee the splitLoc will always be >= 1;
                static_assert(HbIndexNode::NUM_KEYS >= 3, "Invalid NUM_KEYS");

                int splitLoc = HbIndexNode::NUM_KEYS / 2;

                //If we have dups, don't split the dups.
                if(splitLoc < HbIndexNode::NUM_KEYS-1
                    && node->m_Keys[splitLoc] == node->m_Keys[splitLoc+1])
                {
                    splitLoc += UpperBound(node->m_Keys[splitLoc],
                                            &node->m_Keys[splitLoc],
                                            &node->m_Keys[HbIndexNode::NUM_KEYS]);
                    assert(splitLoc <= HbIndexNode::NUM_KEYS);
                }

                HbIndexNode* newNode = new HbIndexNode();
                m_Capacity += HbIndexNode::NUM_KEYS+1;
                const int numToCopy = HbIndexNode::NUM_KEYS-splitLoc;
                if(numToCopy > 0)
                {
                    memcpy(newNode->m_Keys, &node->m_Keys[splitLoc+1], (numToCopy-1) * sizeof(node->m_Keys[0]));
                    if(isLeaf)
                    {
                        memcpy(newNode->m_Keys, &node->m_Keys[splitLoc], numToCopy * sizeof(node->m_Keys[0]));
                        memcpy(newNode->m_Items, &node->m_Items[splitLoc], numToCopy * sizeof(node->m_Items[0]));
                        newNode->m_NumKeys = numToCopy;
                        node->m_NumKeys -= numToCopy;
                    }
                    else
                    {
                        memcpy(newNode->m_Keys, &node->m_Keys[splitLoc+1], (numToCopy-1) * sizeof(node->m_Keys[0]));
                        memcpy(newNode->m_Items, &node->m_Items[splitLoc+1], numToCopy * sizeof(node->m_Items[0]));
                        newNode->m_NumKeys = numToCopy-1;
                        node->m_NumKeys -= numToCopy;
                    }
                }

                if(!parent)
                {
                    parent = m_Nodes = new HbIndexNode();
                    m_Capacity += HbIndexNode::NUM_KEYS+1;
                    parent->m_Items[0].m_Node = node;
                    ++m_Depth;
                    ++depth;
                }

                Move(&parent->m_Keys[keyIdx+1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
                Move(&parent->m_Items[keyIdx+2], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);

                if(splitLoc < HbIndexNode::NUM_KEYS)
                {
                    parent->m_Keys[keyIdx] = node->m_Keys[splitLoc];
                }

                parent->m_Items[keyIdx+1].m_Node = newNode;
                ++parent->m_NumKeys;

                if(isLeaf)
                {
                    newNode->m_Items[HbIndexNode::NUM_KEYS].m_Node =
                        node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
                    node->m_Items[HbIndexNode::NUM_KEYS].m_Node = newNode;
                }

                //DO NOT SUBMIT
                TrimNode(node, depth);

                if(key >= parent->m_Keys[keyIdx])
                {
                    node = newNode;
                }
            }
        }

        keyIdx = UpperBound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);

        if(!isLeaf)
        {
            parent = node;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

    Move(&node->m_Keys[keyIdx+1], &node->m_Keys[keyIdx], node->m_NumKeys-keyIdx);
    Move(&node->m_Items[keyIdx+1], &node->m_Items[keyIdx], node->m_NumKeys-keyIdx);

    node->m_Keys[keyIdx] = key;
    node->m_Items[keyIdx].m_Value = value;
    ++node->m_NumKeys;

    ++m_Count;

    return true;
}

bool
HbIndex::Delete(const s64 key)
{
    HbIndexNode* node = m_Nodes;
    HbIndexNode* parent = NULL;
    int parentKeyIdx = -1;

    for(int depth = 0; depth < m_Depth-1; ++depth)
    {
        if(1 == node->m_NumKeys && parent)
        {
            HbIndexNode* sibling = NULL;
            if(parentKeyIdx < parent->m_NumKeys)
            {
                //right sibling
                sibling = parent->m_Items[parentKeyIdx+1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-2)
                {
                    //Move the current node's items to the sibling.
                    MergeRight(parent, parentKeyIdx, node->m_NumKeys, depth);
                }
                else
                {
                    //Move an item from the sibling to the current node.
                    MergeLeft(parent, parentKeyIdx+1, 1, depth);
                }

                if(0 == node->m_NumKeys)
                {
                    delete node;

                    if(0 == sibling->m_NumKeys)
                    {
                        delete sibling;
                        node = parent;
                        --depth;
                    }
                    else
                    {
                        node = sibling;
                    }
                }
                else if(0 == sibling->m_NumKeys)
                {
                    delete sibling;
                }

                /*sibling = parent->m_Items[parentKeyIdx+1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-2)
                {
                    //Move the current node's items to the sibling and
                    //delete the current node.
                    Move(&sibling->m_Keys[2], &sibling->m_Keys[0], sibling->m_NumKeys);
                    Move(&sibling->m_Items[2], &sibling->m_Items[0], sibling->m_NumKeys+1);
                    sibling->m_Keys[1] = parent->m_Keys[parentKeyIdx];
                    sibling->m_Keys[0] = node->m_Keys[0];
                    sibling->m_Items[1] = node->m_Items[1];
                    sibling->m_Items[0] = node->m_Items[0];
                    Move(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx-1], parentKeyIdx-1);
                    Move(&parent->m_Items[parentKeyIdx], &parent->m_Items[parentKeyIdx-1], parentKeyIdx-1);
                    --parent->m_NumKeys;
                    sibling->m_NumKeys += 2;
                    delete node;
                    node = sibling;
                }
                else if(1 == sibling->m_NumKeys)
                {
                    //Move the last item from the sibling to the current node
                    //and delete the sibling
                    node->m_Keys[sibling->m_NumKeys] = parent->m_Keys[parentKeyIdx];
                    node->m_Keys[sibling->m_NumKeys+1] = sibling->m_Keys[0];
                    node->m_Items[sibling->m_NumKeys+1] = sibling->m_Items[0];
                    node->m_Items[sibling->m_NumKeys+2] = sibling->m_Items[1];
                    Move(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);
                    Move(&parent->m_Items[parentKeyIdx+1], &parent->m_Items[parentKeyIdx+2], parent->m_NumKeys-parentKeyIdx);
                    --parent->m_NumKeys;
                    node->m_NumKeys += 2;
                    delete sibling;
                }
                else
                {
                    //Move an item from the sibling to the current node.
                    node->m_Keys[1] = parent->m_Keys[parentKeyIdx];
                    parent->m_Keys[parentKeyIdx] = sibling->m_Keys[0];
                    node->m_Items[2] = sibling->m_Items[0];

                    Move(&sibling->m_Keys[0], &sibling->m_Keys[1], sibling->m_NumKeys-1);
                    Move(&sibling->m_Items[0], &sibling->m_Items[1], sibling->m_NumKeys);
                    --sibling->m_NumKeys;
                    ++node->m_NumKeys;
                }*/
            }
            else if(parentKeyIdx > 0)
            {
                //Left sibling
                sibling = parent->m_Items[parentKeyIdx-1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-2)
                {
                    //Move the current node's items to the sibling.
                    MergeLeft(parent, parentKeyIdx, node->m_NumKeys, depth);
                }
                else
                {
                    //Move an item from the sibling to the current node.
                    MergeLeft(parent, parentKeyIdx-1, 1, depth);
                }

                if(0 == node->m_NumKeys)
                {
                    delete node;

                    if(0 == sibling->m_NumKeys)
                    {
                        delete sibling;
                        node = parent;
                        --depth;
                    }
                    else
                    {
                        node = sibling;
                    }
                }
                else if(0 == sibling->m_NumKeys)
                {
                    delete sibling;
                }

                /*sibling = parent->m_Items[parentKeyIdx-1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-2)
                {
                    //Move the current node's items to the sibling and
                    //delete the current node.
                    sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[parentKeyIdx];
                    sibling->m_Keys[sibling->m_NumKeys+1] = node->m_Keys[0];
                    sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
                    sibling->m_Items[sibling->m_NumKeys+2] = node->m_Items[1];
                    Move(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);
                    Move(&parent->m_Items[parentKeyIdx+1], &parent->m_Items[parentKeyIdx+2], parent->m_NumKeys-parentKeyIdx);
                    --parent->m_NumKeys;
                    sibling->m_NumKeys += 2;
                    delete node;
                    --parentKeyIdx;
                    node = sibling;
                }
                else if(1 == sibling->m_NumKeys)
                {
                    //Move the last item from the sibling to the current node
                    //and delete the sibling
                    node->m_Keys[2] = node->m_Keys[0];
                    node->m_Keys[1] = parent->m_Keys[parentKeyIdx];
                    node->m_Keys[0] = sibling->m_Keys[0];
                    node->m_Items[3] = node->m_Items[1];
                    node->m_Items[2] = node->m_Items[0];
                    node->m_Items[1] = sibling->m_Items[1];
                    node->m_Items[0] = sibling->m_Items[0];
                    Move(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx-1], parentKeyIdx-1);
                    Move(&parent->m_Items[parentKeyIdx], &parent->m_Items[parentKeyIdx-1], parentKeyIdx-1);
                    --parent->m_NumKeys;
                    node->m_NumKeys += 2;
                    delete sibling;
                }
                else
                {
                    //Move an item from the sibling to the current node.
                    node->m_Keys[1] = node->m_Keys[0];
                    node->m_Items[2] = node->m_Items[1];
                    node->m_Items[1] = node->m_Items[0];

                    node->m_Keys[0] = parent->m_Keys[parentKeyIdx];
                    node->m_Items[0] = sibling->m_Items[sibling->m_NumKeys];
                    parent->m_Keys[parentKeyIdx] = sibling->m_Keys[sibling->m_NumKeys-1];
                    --sibling->m_NumKeys;
                    ++node->m_NumKeys;
                }*/
            }
        }

        parentKeyIdx = UpperBound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        parent = node;
        node = parent->m_Items[parentKeyIdx].m_Node;
    }

    int keyIdx = UpperBound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);

    if(keyIdx > 0 && keyIdx <= node->m_NumKeys)
    {
        --keyIdx;
        if(key == node->m_Keys[keyIdx])
        {
            if(node->m_NumKeys > 1)
            {
                Move(&node->m_Keys[keyIdx], &node->m_Keys[keyIdx+1], node->m_NumKeys-keyIdx);
                Move(&node->m_Items[keyIdx], &node->m_Items[keyIdx+1], node->m_NumKeys-keyIdx);

                --node->m_NumKeys;

                //DO NOT SUBMIT
                TrimNode(node, m_Depth-1);
                if(parent)
                {
                    ValidateNode(m_Depth-2, parent);
                }
            }
            else
            {
                if(parent)
                {
                    if(parentKeyIdx > 0)
                    {
                        parent->m_Items[parentKeyIdx-1].m_Node = parent->m_Items[parentKeyIdx].m_Node;
                    }

                    Move(&parent->m_Keys[parentKeyIdx], &parent->m_Keys[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);
                    Move(&parent->m_Items[parentKeyIdx], &parent->m_Items[parentKeyIdx+1], parent->m_NumKeys-parentKeyIdx);

                    --parent->m_NumKeys;

                    if(0 == parent->m_NumKeys)
                    {
                        assert(parent == m_Nodes);

                        HbIndexNode* child = parent->m_Items[0].m_Node;
                        Move(parent->m_Keys, child->m_Keys, child->m_NumKeys);
                        Move(parent->m_Items, child->m_Items, child->m_NumKeys);

                        parent->m_NumKeys = child->m_NumKeys;
                        child->m_NumKeys = 0;
                        delete child;
                        --m_Depth;

                        //DO NOT SUBMIT
                        TrimNode(parent, m_Depth-1);
                        ValidateNode(m_Depth-1, parent);
                    }
                    else
                    {
                        //DO NOT SUBMIT
                        TrimNode(parent, m_Depth-2);
                        ValidateNode(m_Depth-2, parent);
                    }
                }

                delete node;
            }

            --m_Count;

            if(0 == m_Count)
            {
                assert(node == m_Nodes);
                m_Nodes = NULL;
            }
            return true;
        }
    }

    return false;
}

bool
HbIndex::Find(const s64 key, s64* value) const
{
    const HbIndexNode* node;
    const HbIndexNode* parent;
    int keyIdx, parentKeyIdx;
    if(Find(key, &node, &keyIdx, &parent, &parentKeyIdx))
    {
        *value = node->m_Items[keyIdx].m_Value;
        return true;
    }

    return false;
}

bool
HbIndex::GetFirst(HbIterator* it)
{
    if(m_Leaves)
    {
        it->m_Cur = m_Leaves;
        return true;
    }
    else
    {
        it->Clear();
    }

    return false;
}

void
HbIndex::Validate()
{
    if(m_Nodes)
    {
        ValidateNode(0, m_Nodes);
    }
}

//private:

bool
HbIndex::Find(const s64 key,
                const HbIndexNode** outNode,
                int* outKeyIdx,
                const HbIndexNode** outParent,
                int* outParentKeyIdx) const
{
    HbIndexNode* tmpNode;
    HbIndexNode* tmpParent;
    const bool success =
        const_cast<HbIndex*>(this)->Find(key, &tmpNode, outKeyIdx, &tmpParent, outParentKeyIdx);
    *outNode = tmpNode;
    *outParent = tmpParent;
    return success;
}

bool
HbIndex::Find(const s64 key,
                HbIndexNode** outNode,
                int* outKeyIdx,
                HbIndexNode** outParent,
                int* outParentKeyIdx)
{
    HbIndexNode* node = m_Nodes;
    HbIndexNode* parent = NULL;
    int keyIdx = -1;
    int parentKeyIdx = -1;

    for(int depth = 0; depth < m_Depth; ++depth)
    {
        keyIdx = UpperBound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        if(depth < m_Depth-1)
        {
            parent = node;
            parentKeyIdx = keyIdx;
            node = parent->m_Items[keyIdx].m_Node;
        }
    }

    if(keyIdx > 0 && keyIdx <= node->m_NumKeys)
    {
        --keyIdx;
        if(key == node->m_Keys[keyIdx])
        {
            *outNode = node;
            *outParent = parent;
            *outKeyIdx = keyIdx;
            *outParentKeyIdx = parentKeyIdx;
            return true;
        }
    }

    *outNode = *outParent = NULL;
    *outKeyIdx = *outParentKeyIdx = -1;
    return false;
}

void
HbIndex::MergeLeft(HbIndexNode* parent, const int keyIdx, const int count, const int depth)
{
    assert(keyIdx > 0);

    HbIndexNode* node = parent->m_Items[keyIdx].m_Node;
    //Left sibling
    HbIndexNode* sibling = parent->m_Items[keyIdx-1].m_Node;
    assert(node->m_NumKeys >= count);
    assert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

    if(node->m_NumKeys == count)
    {
        if(!hbVerify(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1))
        {
            return;
        }
    }

    const bool isLeaf = (depth == m_Depth-1);

    if(!isLeaf)
    {
        for(int i = 0; i < count; ++i)
        {
            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            parent->m_Keys[keyIdx-1] = node->m_Keys[0];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            Move(&node->m_Keys[0], &node->m_Keys[1], node->m_NumKeys-1);
            Move(&node->m_Items[0], &node->m_Items[1], node->m_NumKeys);
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }

        if(0 == node->m_NumKeys)
        {
            sibling->m_Keys[sibling->m_NumKeys++] = parent->m_Keys[keyIdx-1];
            sibling->m_Items[sibling->m_NumKeys] = node->m_Items[0];
            Move(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        Move(&sibling->m_Keys[sibling->m_NumKeys], &node->m_Keys[0], count);
        Move(&sibling->m_Items[sibling->m_NumKeys], &node->m_Items[0], count);

        if(count < node->m_NumKeys)
        {
            Move(&node->m_Keys[0], &node->m_Keys[count], node->m_NumKeys-count);
            Move(&node->m_Items[0], &node->m_Items[count], node->m_NumKeys-count);

            parent->m_Keys[keyIdx-1] = node->m_Keys[0];
        }
        else
        {
            Move(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }

        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;
    }

    if(0 == parent->m_NumKeys)
    {
        assert(parent == m_Nodes);

        HbIndexNode* child = parent->m_Items[0].m_Node;
        Move(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys+1);
        }

        parent->m_NumKeys = child->m_NumKeys;
        child->m_NumKeys = 0;
        --m_Depth;
    }

    //DO NOT SUBMIT
    TrimNode(sibling, depth);
    TrimNode(node, depth);
    TrimNode(parent, depth-1);
    ValidateNode(depth-1, parent);
}

void
HbIndex::MergeRight(HbIndexNode* parent, const int keyIdx, const int count, const int depth)
{
    assert(keyIdx < parent->m_NumKeys);

    HbIndexNode* node = parent->m_Items[keyIdx].m_Node;
    //Right sibling
    HbIndexNode* sibling = parent->m_Items[keyIdx+1].m_Node;
    assert(node->m_NumKeys >= count);
    assert(sibling->m_NumKeys < HbIndexNode::NUM_KEYS);

    if(node->m_NumKeys == count)
    {
        if(!hbVerify(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1))
        {
            return;
        }
    }

    const bool isLeaf = (depth == m_Depth-1);

    if(!isLeaf)
    {
        for(int i = 0; i < count; ++i)
        {
            Move(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            Move(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            parent->m_Keys[keyIdx] = node->m_Keys[node->m_NumKeys-1];
            sibling->m_Items[0] = node->m_Items[node->m_NumKeys];
            ++sibling->m_NumKeys;
            --node->m_NumKeys;
        }

        if(0 == node->m_NumKeys)
        {
            Move(&sibling->m_Keys[1], &sibling->m_Keys[0], sibling->m_NumKeys);
            Move(&sibling->m_Items[1], &sibling->m_Items[0], sibling->m_NumKeys+1);
            sibling->m_Keys[0] = parent->m_Keys[keyIdx];
            sibling->m_Items[0] = node->m_Items[0];
            Move(&parent->m_Keys[keyIdx], &parent->m_Keys[keyIdx+1], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        Move(&sibling->m_Keys[count], &sibling->m_Keys[0], count);
        Move(&sibling->m_Items[count], &sibling->m_Items[0], count);

        Move(&sibling->m_Keys[0], &node->m_Keys[node->m_NumKeys-count], count);
        Move(&sibling->m_Items[0], &node->m_Items[node->m_NumKeys-count], count);

        sibling->m_NumKeys += count;
        node->m_NumKeys -= count;

        if(node->m_NumKeys > 0)
        {
            parent->m_Keys[keyIdx] = sibling->m_Keys[0];
        }
        else
        {
            Move(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx-1);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }

    if(0 == parent->m_NumKeys)
    {
        assert(parent == m_Nodes);

        HbIndexNode* child = parent->m_Items[0].m_Node;
        Move(parent->m_Keys, child->m_Keys, child->m_NumKeys);
        if(isLeaf)
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys);
        }
        else
        {
            Move(parent->m_Items, child->m_Items, child->m_NumKeys+1);
        }

        parent->m_NumKeys = child->m_NumKeys;
        child->m_NumKeys = 0;
        --m_Depth;
    }

    //DO NOT SUBMIT
    TrimNode(sibling, depth);
    TrimNode(node, depth);
    TrimNode(parent, depth-1);
    ValidateNode(depth-1, parent);
}

void
HbIndex::TrimNode(HbIndexNode* node, const int depth)
{
    const bool isLeaf = (depth == m_Depth-1);

    for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
    {
        node->m_Keys[i] = 0;
    }

    if(!isLeaf)
    {
        for(int i = node->m_NumKeys+1; i < HbIndexNode::NUM_KEYS+1; ++i)
        {
            node->m_Items[i].m_Value = 0;
        }
    }
    else
    {
        for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
        {
            node->m_Items[i].m_Value = 0;
        }
    }
}

#define ALLOW_DUPS  1

void
HbIndex::ValidateNode(const int depth, HbIndexNode* node)
{
    const bool isLeaf = ((m_Depth-1) == depth);

    if(!isLeaf)
    {
        for(int i = 1; i < node->m_NumKeys; ++i)
        {
#if ALLOW_DUPS
            assert(node->m_Keys[i] >= node->m_Keys[i-1]);
#else
            assert(node->m_Keys[i] > node->m_Keys[i-1]);
#endif
        }
    }
    else
    {
        //Allow duplicates in the leaves
        for(int i = 1; i < node->m_NumKeys; ++i)
        {
            assert(node->m_Keys[i] >= node->m_Keys[i-1]);
        }
    }

    //DO NOT SUBMIT
    for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
    {
        assert(0 == node->m_Keys[i]);
        assert(node->m_Items[i].m_Node != node->m_Items[HbIndexNode::NUM_KEYS].m_Node
                || 0 == node->m_Items[i].m_Node);
    }

    if(!isLeaf)
    {
        for(int i = 0; i < node->m_NumKeys; ++i)
        {
            const s64 key = node->m_Keys[i];
            const HbIndexNode* child = node->m_Items[i].m_Node;
            for(int j = 0; j < child->m_NumKeys; ++j)
            {
#if ALLOW_DUPS
                assert(child->m_Keys[j] <= key);
#else
                assert(child->m_Keys[j] < key);
#endif
            }
        }

        const s64 key = node->m_Keys[node->m_NumKeys-1];
        const HbIndexNode* child = node->m_Items[node->m_NumKeys].m_Node;
        for(int j = 0; j < child->m_NumKeys; ++j)
        {
            assert(child->m_Keys[j] >= key);
        }

        for(int i = 0; i < node->m_NumKeys+1; ++i)
        {
            ValidateNode(depth+1, node->m_Items[i].m_Node);
        }
    }
}

int
HbIndex::UpperBound(const s64 key, const s64* first, const s64* end)
{
    if(end > first)
    {
        const s64* cur = first;

        /*for(; cur < end; ++cur)
        {
            if(key < *cur)
            {
                break;
            }
        }*/

        size_t count = end - first;
        while(count > 0)
        {
            const s64* item = cur;
            size_t step = count >> 1;
            item += step;
            if(!(key < *item))
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

///////////////////////////////////////////////////////////////////////////////
//  HbIndexTest
///////////////////////////////////////////////////////////////////////////////

static ptrdiff_t myrandom (ptrdiff_t i)
{
    return HbRand()%i;
}

void
HbIndexTest::CreateRandomKeys(KV* kv, const int numKeys, const bool unique, const int range)
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
HbIndexTest::AddRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbIndex index;
    s64 value;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        index.Insert(kv[i].m_Key, kv[i].m_Value);
        assert(index.Find(kv[i].m_Key, &value) && value == kv[i].m_Value);
        index.Validate();
    }

    //index.DumpStats();

    index.Validate();

    //index.Dump(true);

    std::sort(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Find(kv[i].m_Key, &value) && value == kv[i].m_Value);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        int j = HbRand() % numKeys;
        assert(index.Find(kv[j].m_Key, &value) && value == kv[j].m_Value);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Delete(kv[i].m_Key));
        index.Validate();
    }

    index.Validate();
}

void
HbIndexTest::AddDeleteRandomKeys(const int numKeys, const bool unique, const int range)
{
    HbIndex index;
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
            index.Insert(kv[idx].m_Key, kv[idx].m_Value);
            kv[idx].m_Added = true;
            assert(index.Find(kv[i].m_Key, &value) && value == kv[i].m_Value);
        }
        else
        {
            index.Delete(kv[idx].m_Key);
            kv[idx].m_Added = false;
            assert(!index.Find(kv[idx].m_Key, &value));
        }

        index.Validate();
    }

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            assert(index.Find(kv[i].m_Key, &value) && value == kv[i].m_Value);
        }
        else
        {
            assert(!index.Find(kv[i].m_Key, &value));
        }
    }

    //index.DumpStats();

    index.Validate();

    //index.Dump(true);
}

void
HbIndexTest::AddSortedKeys(const int numKeys, const bool unique, const int range)
{
    HbIndex index;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
    }

    std::sort(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        index.Insert(kv[i].m_Key, kv[i].m_Value);
    }

    index.Validate();

    int count = 0;
    HbIterator it;
    for(bool b = index.GetFirst(&it); b; b = it.Next())
    {
        ++count;
    }
    assert(count == numKeys);
}

int main(int /*argc*/, char** /*argv*/)
{
    /*const int numKeys = 1000000;
    s64* keys = new s64[numKeys];
    HbIndex index;
    for(int i = 0; i < numKeys; ++i)
    {
        keys[i] = i+1;
    }

    std::random_shuffle(&keys[0], &keys[numKeys]);

    HbStopWatch sw;

    sw.Start();
    s64 value;
    for(int i = 0; i < numKeys; ++i)
    {
        index.Insert(keys[i], keys[i]);
        //index2.Validate();
        //assert(index2.Find(keys[i], &value) && value == keys[i]);
    }

    //index2.Validate();

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Find(i+1, &value) && value == i+1);
    }

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Find(keys[i], &value) && value == keys[i]);
    }

    sw.Stop();*/

    HbStringTest::Test();
    HbDictTest::TestStringString(1024);
    HbDictTest::TestStringInt(1024);
    HbDictTest::TestIntInt(1024);
    HbDictTest::TestIntString(1024);

    //HbIndexTest::AddDeleteRandomKeys(1024*1024, true, 0);
    HbIndexTest::AddRandomKeys(1024, true, 32767);
    //HbIndexTest::AddRandomKeys(10, false, 1);
    //HbIndexTest::AddRandomKeys(1024*1024, false, 32767);
    //HbIndexTest::AddRandomKeys(1024*1024, true, 0);

    //HbIndexTest::AddSortedKeys(1024*1024, true, 0);
}
