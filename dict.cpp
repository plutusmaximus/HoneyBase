#include "dict.h"

#include <new.h>
#include <string.h>

namespace honeybase
{

static Log s_Log("dict");

#define FNV1_32_INIT ((u32)0x811c9dc5)
//#define NO_FNV_GCC_OPTIMIZATION
//#define FNV_32_PRIME 16777619

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

//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby

// Note - This code makes a few assumptions about how your machine behaves -

// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4

// And it has a few limitations -

// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.

unsigned int MurmurHash2(const void * key, int len, unsigned int seed)
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

    hb_static_assert(sizeof(int) == 4);

	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	unsigned int h = seed ^ len;

	// Mix 4 bytes at a time into the hash

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;

		k *= m; 
		k ^= k >> r; 
		k *= m; 
		
		h *= m; 
		h ^= k;

		data += 4;
		len -= 4;
	}
	
	// Handle the last few bytes of the input array

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
} 

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
//  HtItem
///////////////////////////////////////////////////////////////////////////////
HtItem*
HtItem::Create(const Key key, const KeyType keyType,
                const Value value, const ValueType valueType)
{
    HtItem* item = Create();
    if(item)
    {
        item->m_KeyType = keyType;
        item->m_ValType = valueType;

        switch(keyType)
        {
            case KEYTYPE_INT:
                item->m_Key.m_Int = key.m_Int;
                break;
            case KEYTYPE_DOUBLE:
                item->m_Key.m_Double = key.m_Double;
                break;
            case KEYTYPE_BLOB:
                item->m_Key.m_Blob = key.m_Blob->Dup();
                if(NULL == item->m_Key.m_Blob)
                {
                    Destroy(item);
                    return NULL;
                }
                break;
            default:
                Destroy(item);
                return NULL;
        }

        switch(valueType)
        {
            case VALUETYPE_INT:
                item->m_Value.m_Int = value.m_Int;
                break;
            case VALUETYPE_DOUBLE:
                item->m_Value.m_Double = value.m_Double;
                break;
            case VALUETYPE_BLOB:
                item->m_Value.m_Blob = value.m_Blob->Dup();
                if(NULL == item->m_Value.m_Blob)
                {
                    Destroy(item);
                    return NULL;
                }
                break;
            default:
                Destroy(item);
                return NULL;
        }
    }

    return item;
}

HtItem*
HtItem::CreateEmpty(const Key key, const KeyType keyType, const size_t len)
{
    HtItem* item = Create();
    if(item)
    {
        item->m_KeyType = keyType;
        item->m_ValType = VALUETYPE_BLOB;

        if(NULL == (item->m_Value.m_Blob = Blob::Create(len)))
        {
            Destroy(item);
            item = NULL;
        }
        else if(KEYTYPE_BLOB == keyType)
        {
            if(NULL == (item->m_Key.m_Blob = key.m_Blob->Dup()))
            {
                Destroy(item);
                item = NULL;
            }
        }
        else
        {
            item->m_Key = key;
        }
    }

    return item;
}

void
HtItem::Destroy(HtItem* item)
{
    if(item)
    {
        if(VALUETYPE_BLOB == item->m_KeyType)
        {
            Blob::Destroy(item->m_Key.m_Blob);
            item->m_Key.m_Blob = NULL;
        }

        if(VALUETYPE_BLOB == item->m_ValType)
        {
            Blob::Destroy(item->m_Value.m_Blob);
            item->m_Value.m_Blob = NULL;
        }

        item->~HtItem();
        Heap::Free(item);
        --s_NumDictItems;
    }
}

//private:

HtItem*
HtItem::Create()
{
    HtItem* item = (HtItem*) Heap::ZAlloc(sizeof(HtItem));

    if(item)
    {
        new(item) HtItem();
        ++s_NumDictItems;
    }

    return item;
}

HtItem::HtItem()
    : m_Next(0)
    , m_KeyType(KEYTYPE_INT)
    , m_ValType(VALUETYPE_INT)
{
}

HtItem::~HtItem()
{
}

///////////////////////////////////////////////////////////////////////////////
//  HashTable
///////////////////////////////////////////////////////////////////////////////
HashTable::HashTable()
    : m_Count(0)
    , m_NumSlots(0)
    , m_HashSalt(FNV1_32_INIT)
{
    m_Slots[0] = m_Slots[1] = NULL;
}

HashTable::~HashTable()
{
}

HashTable*
HashTable::Create()
{
    HashTable* dict = (HashTable*) Heap::ZAlloc(sizeof(HashTable));
    if(dict)
    {
        new (dict) HashTable();
        dict->m_Slots[0] = (Slot*) Heap::ZAlloc(INITIAL_NUM_SLOTS * sizeof(Slot));
        if(dict->m_Slots[0])
        {
            dict->m_NumSlots = INITIAL_NUM_SLOTS;
        }
        else
        {
            HashTable::Destroy(dict);
            dict = NULL;
        }
    }

    return dict;
}

void
HashTable::Destroy(HashTable* dict)
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
                HtItem* item = slots[j].m_Item;
                while(item)
                {
                    HtItem* next = item->m_Next;
                    HtItem::Destroy(item);
                    item = next;
                }
            }

            Heap::Free(slots);
            dict->m_Slots[i] = NULL;
        }

        dict->~HashTable();
        Heap::Free(dict);
    }
}

bool
HashTable::Set(const Key key, const KeyType keyType,
                const Value value, const ValueType valueType)
{
    HtItem* item = HtItem::Create(key, keyType, value, valueType);
    if(item)
    {
        Set(item);
        return true;
    }

    return false;
}

bool
HashTable::Clear(const Key key, const KeyType keyType)
{
    Slot* slot;
    u32 hash;
    HtItem** item = Find(key, keyType, &slot, &hash);
    if(*item)
    {
        HtItem* next = (*item)->m_Next;
        HtItem::Destroy(*item);
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
HashTable::Find(const Key key, const KeyType keyType,
                Value* value, ValueType* valueType)
{
    Slot* slot;
    u32 hash;
    HtItem** item = const_cast<HashTable*>(this)->Find(key, keyType, &slot, &hash);

    if(*item)
    {
        *value = (*item)->m_Value;
        *valueType = (*item)->m_ValType;
        return true;
    }

    return false;
}

bool
HashTable::Patch(const Key key, const KeyType keyType,
                const size_t numPatches,
                const Blob** patches,
                const size_t* offsets)
{
    Slot* slot;
    u32 hash;
    HtItem* pOldItem = *Find(key, keyType, &slot, &hash);

    size_t patchNum = 0;

again:

    if(pOldItem)
    {
        if(hbverify(VALUETYPE_BLOB == pOldItem->m_ValType))
        {
            byte* oldData;
            const byte* patchData;
            const size_t oldLen = pOldItem->m_Value.m_Blob->GetData(&oldData);

            for(; patchNum < numPatches; ++patchNum)
            {
                const Blob* patch = patches[patchNum];
                const size_t offset = offsets[patchNum];

                const size_t patchLen = patch->GetData(&patchData);
                if(offset + patchLen <= oldLen)
                {
                    //New data will be patched within the bounds of
                    //the old data.

                    //Use memmove because value and oldItem could be
                    //the same item.
                    memmove(&oldData[offset], patchData, patchLen);
                }
                else
                {
                    //New data extends past the end of the old data.

                    const size_t newLen = offset + patchLen;
                    Blob* newStr = Blob::Create(newLen);
                    if(newStr)
                    {
                        byte* newData;
                        newStr->GetData(&newData);
                        if(offset < oldLen)
                        {
                            memcpy(newData, oldData, offset);
                        }
                        else
                        {
                            memcpy(newData, oldData, oldLen);
                        }

                        memcpy(&newData[offset], patchData, patchLen);

                        Blob::Destroy(pOldItem->m_Value.m_Blob);
                        pOldItem->m_Value.m_Blob = newStr;
                    }
                }

                return true;
            }
        }
    }
    else
    {
        //Item doesn't exist yet, create it.

        const byte* patchData;
        const size_t patchLen = patches[patchNum]->GetData(&patchData);
        const size_t newLen = offsets[patchNum] + patchLen;

        HtItem* newItem = pOldItem = HtItem::CreateEmpty(key, keyType, newLen);

        if(newItem)
        {
            byte* newData;
            newItem->m_Value.m_Blob->GetData(&newData);
            memcpy(&newData[offsets[patchNum]], patchData, patchLen);
            Set(newItem);
            patchNum = 1;
            goto again;
        }
        else
        {
            return false;
        }
    }

    return false;
}

size_t
HashTable::Count() const
{
    return m_Count;
}

//private:

u32
HashTable::HashBytes(const byte* bytes, const size_t len) const
{
    //return FnvHashBufInitVal(bytes, len, m_HashSalt);

    return MurmurHash2(bytes, len, m_HashSalt);
}

void
HashTable::Set(HtItem* newItem)
{
    if(m_Count >= m_NumSlots*2)
    {
        Slot* newSlots = (Slot*) Heap::ZAlloc(m_NumSlots * 2 * sizeof(Slot));
        if(newSlots)
        {
            Slot* oldSlots = m_Slots[0];
            m_Slots[0] = newSlots;
            const unsigned oldNumSlots = m_NumSlots;
            m_NumSlots *= 2;

            for(int i = 0; i < (int)oldNumSlots; ++i)
            {
                HtItem* item = oldSlots[i].m_Item;
                while(item)
                {
                    HtItem* next = item->m_Next;
                    item->m_Next = NULL;

                    bool replaced;
                    this->Set(item, &replaced);
                    hbassert(!replaced);

                    item = next;
                }
            }

            Heap::Free(oldSlots);
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
HashTable::Set(HtItem* newItem, bool* replaced)
{
    Slot* slot;
    u32 hash;
    HtItem** pitem = Find(newItem->m_Key, newItem->m_KeyType, &slot, &hash);

    hbassert(pitem);

    if(!*pitem)
    {
        ++slot->m_Count;
        *replaced = false;
    }
    else
    {
        HtItem::Destroy(*pitem);
        *replaced = true;
    }

    *pitem = newItem;
    (*pitem)->m_Hash = hash;
}

HtItem**
HashTable::Find(const Key key, const KeyType keyType, Slot** slot, u32* hash)
{
    switch(keyType)
    {
    case KEYTYPE_INT:
        *hash = HashBytes((const byte*)&key.m_Int, sizeof(key.m_Int));
        break;
    case KEYTYPE_DOUBLE:
        *hash = HashBytes((const byte*)&key.m_Double, sizeof(key.m_Double));
        break;
    case KEYTYPE_BLOB:
        {
            const byte* keyData;
            const size_t keylen = key.m_Blob->GetData(&keyData);
            *hash = HashBytes(keyData, keylen);
        }
        break;
    }

    HtItem** item = NULL;
    for(int i = 0; i < 2 && m_Slots[i]; ++i)
    {
        const unsigned idx = *hash & (m_NumSlots-1);
        *slot = &m_Slots[i][idx];
        item = &(*slot)->m_Item;

        for(; *item; item = &(*item)->m_Next)
        {
            if(*hash == (*item)->m_Hash
                && keyType == (*item)->m_KeyType
                 && (*item)->m_Key.EQ(keyType, key))
            {
                return item;
            }
        }
    }

    return item;
}

}   //namespace honeybase