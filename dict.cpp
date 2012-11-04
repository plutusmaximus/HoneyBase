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
static size_t s_NumDictItems;
class HbHeap
{
public:

	void* Alloc(size_t size)
	{
		return calloc(1, size);
	}

	void Free(void* mem)
	{
		free(mem);
	}
};

static HbHeap s_Heap;

///////////////////////////////////////////////////////////////////////////////
//  HbString
///////////////////////////////////////////////////////////////////////////////
HbString*
HbString::Create(const byte* string, const size_t stringLen)
{
    const size_t size = Size(stringLen);
    HbString* hbs = (HbString*) s_Heap.Alloc(size);
    if(hbs)
    {
        byte* bytes = (byte*) hbs;

        int offset = 0;
        size_t tmpLen = stringLen;
        while(tmpLen > 0x7F)
        {
            bytes[offset++] = 0x80 | (tmpLen & 0x7F);
            tmpLen >>= 7;
        }

        bytes[offset++] = tmpLen & 0x7F;

        memcpy(&bytes[offset], string, stringLen);
    }

    return hbs;
}

void
HbString::Destroy(HbString* hbs)
{
    hbs->~HbString();
    s_Heap.Free(hbs);
}

const byte*
HbString::Data() const
{
    const byte* p = (byte*)this;
    for(; *p & 0x80; ++p)
    {
    }

    return ++p;
}

size_t
HbString::GetData(const byte** data) const
{
    size_t len = 0;
    const byte* p = (byte*)this;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    *data = ++p;

    return len;
}

size_t
HbString::Length() const
{
    size_t len = 0;
    const byte* p = (byte*)this;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    return len;
}

size_t
HbString::Size() const
{
    size_t len = 0;
    const byte* p = (byte*)this;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    return len + (p - (byte*)this + 1);
}

size_t
HbString::Size(const size_t stringLen)
{
    size_t size = stringLen;
    size_t tmpLen = stringLen;

    while(tmpLen)
    {
        tmpLen >>= 7;
        ++size;
    }

    return size;
}

bool
HbString::EQ(const HbString& that) const
{
    const size_t mysize = Size();
    return (that.Size() == mysize
            && (0 == mysize || 0 == memcmp(this, &that, mysize)));
}

bool
HbString::EQ(const byte* string, const size_t stringLen) const
{
    const size_t mylen = Length();
    return (stringLen == mylen
            && (0 == mylen || 0 == memcmp(this->Data(), string, mylen)));
}

///////////////////////////////////////////////////////////////////////////////
//  HbStringTest
///////////////////////////////////////////////////////////////////////////////
void
HbStringTest::Test()
{
    HbString* hbs = HbString::Create((const byte*)"Foo", strlen("Foo"));
    assert(hbs->Length() == 3);
    assert(0 == memcmp("Foo", hbs->Data(), strlen("Foo")));
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
    assert(0 == memcmp(str, hbs->Data(), len));
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
    assert(0 == memcmp(str, hbs->Data(), len));
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
    assert(0 == memcmp(str, hbs->Data(), len));
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
        s_Heap.Free(item);
        --s_NumDictItems;
    }
}

//private:

HbDictItem*
HbDictItem::Create()
{
    HbDictItem* item = (HbDictItem*) s_Heap.Alloc(sizeof(HbDictItem));

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
    HbDict* dict = (HbDict*) s_Heap.Alloc(sizeof(HbDict));
    if(dict)
    {
        new (dict) HbDict();
		dict->m_Slots = (Slot*) s_Heap.Alloc(INITIAL_NUM_SLOTS * sizeof(Slot));
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

            s_Heap.Free(dict->m_Slots);
            dict->m_Slots = NULL;
        }

        dict->~HbDict();
        s_Heap.Free(dict);
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
//  HbIndexLeaf
///////////////////////////////////////////////////////////////////////////////
HbIndexLeaf::HbIndexLeaf()
: m_NumKeys(0)
, m_Next(NULL)
{
    memset(m_Keys, 0, sizeof(m_Keys));
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
//  HbIndex
///////////////////////////////////////////////////////////////////////////////
#define TRIM_NODE   0
#define AUTO_DEFRAG 1

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
        m_Nodes = m_Leaves = AllocNode();
        if(!m_Nodes)
        {
            return false;
        }

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

#if AUTO_DEFRAG
        if(HbIndexNode::NUM_KEYS == node->m_NumKeys && parent)
        {
            HbIndexNode* sibling = NULL;
            if(keyIdx > 0)
            {
                //Left sibling
                sibling = parent->m_Items[keyIdx-1].m_Node;
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1
                    //Don't split dups
                    && node->m_Keys[0] != node->m_Keys[1])
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
                if(sibling->m_NumKeys < HbIndexNode::NUM_KEYS-1
                    //Don't split dups
                    && node->m_Keys[node->m_NumKeys-1] != node->m_Keys[node->m_NumKeys-2])
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

        if(HbIndexNode::NUM_KEYS == node->m_NumKeys)
        {
            //Split the node.

            //Guarantee the splitLoc will always be >= 2;
            //See comments below about numToCopy.
            STATIC_ASSERT(HbIndexNode::NUM_KEYS >= 4);

            int splitLoc = HbIndexNode::NUM_KEYS / 2;

            //If we have dups, don't split the dups.
            /*if(splitLoc < HbIndexNode::NUM_KEYS-1)
            {
                if(isLeaf)
                {
                    if(node->m_Keys[splitLoc] == node->m_Keys[splitLoc-1])
                    {
                        splitLoc += Bound(node->m_Keys[splitLoc],
                                            &node->m_Keys[splitLoc],
                                            &node->m_Keys[HbIndexNode::NUM_KEYS]);
                        assert(splitLoc <= HbIndexNode::NUM_KEYS);

                        //If the dups extend to the end of the node it's ok to split
                        //on the last dup because the item currently being inserted
                        //will fill the hole.
                        if(splitLoc == HbIndexNode::NUM_KEYS)
                        {
                            --splitLoc;
                        }
                    }
                }
                else
                {
                    if(node->m_Keys[splitLoc] == node->m_Keys[splitLoc+1])
                    {
                        splitLoc += Bound(node->m_Keys[splitLoc],
                                            &node->m_Keys[splitLoc],
                                            &node->m_Keys[HbIndexNode::NUM_KEYS]);
                        assert(splitLoc <= HbIndexNode::NUM_KEYS);

                        //If the dups extend to the end of the node it's ok to split
                        //on the last dup because the item currently being inserted
                        //will fill the hole.
                        if(splitLoc == HbIndexNode::NUM_KEYS)
                        {
                            --splitLoc;
                        }
                    }
                }
            }*/

            HbIndexNode* newNode = AllocNode();
            const int numToCopy = HbIndexNode::NUM_KEYS-splitLoc;
            assert(numToCopy > 0);

            if(isLeaf)
            {
                //k0 k1 k2 k3 k4 k5 k6 k7
                //v0 v1 v2 v3 v4 v5 v6 v7

                //k0 k1 k2 k3    k4 k5 k6 k7
                //v0 v1 v2 v3    v4 v5 v6 v7

                memcpy(newNode->m_Keys, &node->m_Keys[splitLoc], numToCopy * sizeof(node->m_Keys[0]));
                memcpy(newNode->m_Items, &node->m_Items[splitLoc], numToCopy * sizeof(node->m_Items[0]));
                newNode->m_NumKeys = numToCopy;
                node->m_NumKeys -= numToCopy;
            }
            else
            {
                // k0 k1 k2 k3 k4 k5 k6 k7
                //v0 v1 v2 v3 v4 v5 v6 v7 v8

                // k0 k1 k2 k3    k4 k5 k6 k7
                //v0 v1 v2 v3    v4 v5 v6 v7 v8

                memcpy(newNode->m_Keys, &node->m_Keys[splitLoc], (numToCopy) * sizeof(node->m_Keys[0]));
                memcpy(newNode->m_Items, &node->m_Items[splitLoc], (numToCopy+1) * sizeof(node->m_Items[0]));
                newNode->m_NumKeys = numToCopy;
                //Subtract an extra one from m_NumKeys because we'll
                //rotate the last key from node up into parent.
                //This is also why NUM_KEYS must be >= 4.  It guarantees numToCopy+1
                //will always be >= 1.
                node->m_NumKeys -= numToCopy+1;
            }

            if(!parent)
            {
                parent = m_Nodes = AllocNode();
                parent->m_Items[0].m_Node = node;
                ++m_Depth;
                ++depth;
            }

            assert(parent->m_NumKeys <= HbIndexNode::NUM_KEYS);
            Move(&parent->m_Keys[keyIdx+1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx+2], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);

            if(isLeaf)
            {
                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc];

                newNode->m_Items[HbIndexNode::NUM_KEYS].m_Node =
                    node->m_Items[HbIndexNode::NUM_KEYS].m_Node;
                node->m_Items[HbIndexNode::NUM_KEYS].m_Node = newNode;
            }
            else
            {
                parent->m_Keys[keyIdx] = node->m_Keys[splitLoc-1];
            }

            parent->m_Items[keyIdx+1].m_Node = newNode;
            ++parent->m_NumKeys;

#if TRIM_NODE
            TrimNode(node, depth);
#endif

            if(key > parent->m_Keys[keyIdx])
            {
                node = newNode;
            }
        }

        keyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        assert(keyIdx >= 0);

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
        if(parent)// && parent->m_NumKeys > 1)
        {
            HbIndexNode* sibling = NULL;
            HbIndexNode* leftSibling = parentKeyIdx > 0 ? parent->m_Items[parentKeyIdx-1].m_Node : NULL;
            HbIndexNode* rightSibling = parentKeyIdx < parent->m_NumKeys ? parent->m_Items[parentKeyIdx+1].m_Node : NULL;
            if(leftSibling && (node->m_NumKeys + leftSibling->m_NumKeys) < HbIndexNode::NUM_KEYS)
            {
                //Merge with the left sibling
                sibling = leftSibling;
                if(node->m_NumKeys < leftSibling->m_NumKeys)
                {
                    MergeLeft(parent, parentKeyIdx, node->m_NumKeys, depth);
                }
                else if(parentKeyIdx > 0)
                {
                    MergeRight(parent, parentKeyIdx-1, sibling->m_NumKeys, depth);
                }
            }
            else if(leftSibling && 1 == node->m_NumKeys)
            {
                //Borrow one from the left sibling
                sibling = leftSibling;
                //DO NOT SUBMIT
                if(leftSibling->m_NumKeys >= 2
                    && leftSibling->m_Keys[leftSibling->m_NumKeys-1] == leftSibling->m_Keys[leftSibling->m_NumKeys-2])
                {
                    //splitting dups
                    OutputDebugString("HERE\n");
                }
                MergeRight(parent, parentKeyIdx-1, 1, depth);
            }
            else if(rightSibling && (node->m_NumKeys + rightSibling->m_NumKeys) < HbIndexNode::NUM_KEYS)
            {
                //Merge with the right sibling
                sibling = rightSibling;
                if(node->m_NumKeys < rightSibling->m_NumKeys)
                {
                    MergeRight(parent, parentKeyIdx, node->m_NumKeys, depth);
                }
                else if(parentKeyIdx < parent->m_NumKeys)
                {
                    MergeLeft(parent, parentKeyIdx+1, sibling->m_NumKeys, depth);
                }
            }
            else if(rightSibling && 1 == node->m_NumKeys)
            {
                //Borrow one from the right sibling
                sibling = rightSibling;
                //DO NOT SUBMIT
                if(rightSibling->m_NumKeys >= 2
                    && rightSibling->m_Keys[0] == rightSibling->m_Keys[1])
                {
                    //splitting dups
                    OutputDebugString("HERE\n");
                }
                MergeLeft(parent, parentKeyIdx+1, 1, depth);
            }

            if(0 == node->m_NumKeys)
            {
                FreeNode(node);

                if(0 == sibling->m_NumKeys)
                {
                    FreeNode(sibling);
                    node = parent;
                    --depth;
                }
                else
                {
                    node = sibling;
                }
            }
            else if(sibling && 0 == sibling->m_NumKeys)
            {
                FreeNode(sibling);
            }
        }

        parentKeyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
        parent = node;
        node = parent->m_Items[parentKeyIdx].m_Node;
    }

    int keyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);

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

#if TRIM_NODE
                TrimNode(node, m_Depth-1);
#endif
                //DO NOT SUBMIT
                /*if(parent)
                {
                    ValidateNode(m_Depth-2, parent);
                }*/
            }
            else
            {
                if(parent)
                {
                    if(parentKeyIdx <= parent->m_NumKeys)
                    {
                        parent->m_Items[parentKeyIdx].m_Node = parent->m_Items[parentKeyIdx+1].m_Node;
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
                        FreeNode(child);
                        --m_Depth;

#if TRIM_NODE
                        TrimNode(parent, m_Depth-1);
#endif
                        //DO NOT SUBMIT
                        //ValidateNode(m_Depth-1, parent);
                    }
                    else
                    {
#if TRIM_NODE
                        TrimNode(parent, m_Depth-2);
#endif
                        //DO NOT SUBMIT
                        //ValidateNode(m_Depth-2, parent);
                    }
                }

                FreeNode(node);
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

u64
HbIndex::Count() const
{
    return m_Count;
}

void
HbIndex::Validate()
{
    if(m_Nodes)
    {
        ValidateNode(0, m_Nodes);

        //Trace down the right edge of the tree and make sure
        //we reach the last node
        HbIndexNode* node = m_Nodes;
        for(int depth = 0; depth < m_Depth-1; ++depth)
        {
            node = node->m_Items[node->m_NumKeys].m_Node;
        }

        assert(!node->m_Items[HbIndexNode::NUM_KEYS].m_Node);
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
        keyIdx = Bound(key, node->m_Keys, &node->m_Keys[node->m_NumKeys]);
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
            sibling->m_Keys[sibling->m_NumKeys] = parent->m_Keys[keyIdx-1];
            sibling->m_Items[sibling->m_NumKeys+1] = node->m_Items[0];
            ++sibling->m_NumKeys;
            Move(&parent->m_Keys[keyIdx-1], &parent->m_Keys[keyIdx], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Move count items from the node to the left sibling
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

#if TRIM_NODE
    TrimNode(sibling, depth);
    TrimNode(node, depth);
    TrimNode(parent, depth-1);
#endif
    //DO NOT SUBMIT
    //ValidateNode(depth-1, parent);
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
            ++sibling->m_NumKeys;
            Move(&parent->m_Keys[keyIdx], &parent->m_Keys[keyIdx+1], parent->m_NumKeys-keyIdx);
            Move(&parent->m_Items[keyIdx], &parent->m_Items[keyIdx+1], parent->m_NumKeys-keyIdx);
            --parent->m_NumKeys;
        }
    }
    else
    {
        //Make room in the right sibling for items from the node
        Move(&sibling->m_Keys[count], &sibling->m_Keys[0], sibling->m_NumKeys);
        Move(&sibling->m_Items[count], &sibling->m_Items[0], sibling->m_NumKeys);

        //Move count items from the node to the right sibling
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

#if TRIM_NODE
    TrimNode(sibling, depth);
    TrimNode(node, depth);
    TrimNode(parent, depth-1);
#endif
    //DO NOT SUBMIT
    //ValidateNode(depth-1, parent);
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

#if TRIM_NODE
    for(int i = node->m_NumKeys; i < HbIndexNode::NUM_KEYS; ++i)
    {
        assert(0 == node->m_Keys[i]);
        assert(node->m_Items[i].m_Node != node->m_Items[HbIndexNode::NUM_KEYS].m_Node
                || 0 == node->m_Items[i].m_Node);
    }
#endif

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

        //Make sure all the keys in the right sibling are >= keys in the left sibling.
        const s64 key = node->m_Keys[node->m_NumKeys-1];
        const HbIndexNode* child = node->m_Items[node->m_NumKeys].m_Node;
        for(int j = 0; j < child->m_NumKeys; ++j)
        {
            assert(child->m_Keys[j] >= key);
        }

        /*for(int i = 0; i < node->m_NumKeys; ++i)
        {
            //Make sure we haven't split duplicates
            const HbIndexNode* a = node->m_Items[i].m_Node;
            const HbIndexNode* b = node->m_Items[i+1].m_Node;
            if(a->m_NumKeys < HbIndexNode::NUM_KEYS)
            {
                assert(a->m_Keys[a->m_NumKeys-1] != b->m_Keys[0]);
            }
        }*/

        for(int i = 0; i < node->m_NumKeys+1; ++i)
        {
            ValidateNode(depth+1, node->m_Items[i].m_Node);
        }
    }
}

HbIndexLeaf*
HbIndex::AllocLeaf()
{
    HbIndexLeaf* leaf = new HbIndexLeaf();
    if(leaf)
    {
        m_Capacity += HbIndexLeaf::NUM_KEYS;
    }

    return leaf;
}

void
HbIndex::FreeLeaf(HbIndexLeaf* leaf)
{
    if(leaf)
    {
        m_Capacity -= HbIndexLeaf::NUM_KEYS+1;
        delete leaf;
    }
}

HbIndexNode*
HbIndex::AllocNode()
{
    HbIndexNode* node = new HbIndexNode();
    if(node)
    {
        m_Capacity += HbIndexNode::NUM_KEYS+1;
    }

    return node;
}

void
HbIndex::FreeNode(HbIndexNode* node)
{
    if(node)
    {
        m_Capacity -= HbIndexNode::NUM_KEYS+1;
        delete node;
    }
}

int
HbIndex::Bound(const s64 key, const s64* first, const s64* end)
{
    return UpperBound(key, first, end);
}

int
HbIndex::LowerBound(const s64 key, const s64* first, const s64* end)
{
    if(end > first)
    {
        const s64* cur = first;
        size_t count = end - first;
        while(count > 0)
        {
            const s64* item = cur;
            size_t step = count >> 1;
            item += step;
            if(key < *item)
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
HbIndex::UpperBound(const s64 key, const s64* first, const s64* end)
{
    if(end > first)
    {
        const s64* cur = first;
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
    static int iii;
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        index.Insert(kv[i].m_Key, kv[i].m_Value);
        assert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
             assert(value == kv[i].m_Value);
        }
        //index.Validate();
    }

    //index.DumpStats();

    index.Validate();

    //index.Dump(true);

    std::sort(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Find(kv[i].m_Key, &value));
        if(unique)
        {
             assert(value == kv[i].m_Value);
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        int j = HbRand() % numKeys;
        assert(index.Find(kv[j].m_Key, &value));
        if(unique)
        {
            assert(value == kv[j].m_Value);
        }
    }

    for(int i = 0; i < numKeys; ++i)
    {
        assert(index.Delete(kv[i].m_Key));
        //index.Validate();
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
            assert(index.Find(kv[idx].m_Key, &value));
            if(unique)
            {
                 assert(value == kv[idx].m_Value);
            }
        }
        else
        {
            index.Delete(kv[idx].m_Key);
            kv[idx].m_Added = false;
            assert(!index.Find(kv[idx].m_Key, &value));
        }

        //index.Validate();
    }

    index.Validate();

    for(int i = 0; i < numKeys; ++i)
    {
        if(kv[i].m_Added)
        {
            assert(index.Find(kv[i].m_Key, &value));
            if(unique)
            {
                 assert(value == kv[i].m_Value);
            }
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
HbIndexTest::AddSortedKeys(const int numKeys, const bool unique, const int range, const bool ascending)
{
    HbIndex index;

    KV* kv = new KV[numKeys];
    CreateRandomKeys(kv, numKeys, unique, range);
    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Value = i;
        if(!ascending)
        {
            kv[i].m_Key = -kv[i].m_Key;
        }
    }

    std::sort(&kv[0], &kv[numKeys]);

    for(int i = 0; i < numKeys; ++i)
    {
        kv[i].m_Key = -kv[i].m_Key;
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

void
HbIndexTest::AddDups(const int numKeys, const int min, const int max)
{
    HbIndex index;
    int range = max - min;
    if(0 == range)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            index.Insert(min, i);
        }
    }
    else
    {
        for(int i = 0; i < numKeys; ++i)
        {
            index.Insert((i%range)+min, i);
        }
    }

    index.Validate();

    if(0 == range)
    {
        for(int i = 0; i < numKeys; ++i)
        {
            assert(index.Delete(min));
        }
    }
    else
    {
        for(int i = 0; i < numKeys; ++i)
        {
            assert(index.Delete((i%range)+min));
        }
    }

    index.Validate();

    assert(0 == index.Count());
}

int main(int /*argc*/, char** /*argv*/)
{
    HbStringTest::Test();
    HbDictTest::TestStringString(1024);
    HbDictTest::TestStringInt(1024);
    HbDictTest::TestIntInt(1024);
    HbDictTest::TestIntString(1024);

    //HbIndexTest::AddDeleteRandomKeys(1024*1024, true, 0);
    //HbIndexTest::AddRandomKeys(1024, true, 32767);
    //HbIndexTest::AddRandomKeys(10, false, 1);
    //HbIndexTest::AddRandomKeys(1024*1024, false, 32767);
    //HbIndexTest::AddRandomKeys(1024*1024, false, 1);
    HbIndexTest::AddRandomKeys(1024*1024, true, 0);

    //HbIndexTest::AddSortedKeys(1024*1024, true, 0, true);
    //HbIndexTest::AddSortedKeys(1024*1024, true, 0, false);

    //HbIndexTest::AddDups(1024*1024, 1, 1);
    //HbIndexTest::AddDups(1024*1024, 1, 2);
    //HbIndexTest::AddDups(1024*1024, 1, 4);
    //HbIndexTest::AddDups(9, 1, 3);
}
