#ifndef __HB_DICT_H__
#define __HB_DICT_H__

#include "hb.h"

namespace honeybase
{

class SortedSet;

class HtItem
{
    friend class HashTable;
    friend class SortedSet;
private:

    static HtItem* Create(const Value& key,
                            const ValueType keyType,
                            const u32 hash);
    static HtItem* Create(const Value& key, const ValueType keyType,
                            const Value& value, const ValueType valueType,
                            const u32 hash);
    static HtItem* CreateBlob(const Value& key,
                                const ValueType keyType,
                                const size_t len,
                                const u32 hash);
    static void Destroy(HtItem* item);

    Value m_Key;
    Value m_Value;
    HtItem* m_Next;
    u32 m_Hash;

    ValueType m_KeyType     : 4;
    ValueType m_ValueType   : 4;

private:

    HtItem();
    ~HtItem();
    HtItem(const HtItem&);
    HtItem& operator=(const HtItem&);
};

class HashTable
{
    friend class SortedSet;
    friend class HtItem;

public:

    static HashTable* Create();

    bool Set(const Value& key, const ValueType keyType,
            const Value& value, const ValueType valueType);

    bool Clear(const Value& key, const ValueType keyType);

    bool Find(const Value& key, const ValueType keyType,
                Value* value, ValueType* valueType);

    bool Patch(const Value& key, const ValueType keyType,
                const size_t numPatches,
                const Blob** patches,
                const size_t* offsets);

    size_t Count() const;

    void Ref() const;
    void Unref();

private:

	class Slot
	{
	public:
		Slot()
			: m_Item(NULL)
			, m_Count(0)
		{
		}

		HtItem* m_Item;
		int m_Count;
	};

    static void Destroy(HashTable* dict);

	u32 HashBytes(const byte* bytes, const size_t len) const ;
	u32 HashKey(const Value& key, const ValueType keyType) const;

    void Set(HtItem* item);
    void Set(HtItem* item, bool* replaced);
    void Set(HtItem* item, HtItem**pitem, Slot* slot, bool* replaced);

    void Rehash();
    
    HtItem** Find(const Value& key, const ValueType keyType, Slot** slot, u32* hash);

    HtItem** Find(const Value& key, const ValueType keyType, const u32 hash, Slot** slot);

	static const int INITIAL_NUM_SLOTS	= (1<<8);

    Slot* m_Slots[2];

    size_t m_SlotToMove;

    size_t m_Count;
    size_t m_NumSlots;
    mutable int m_RefCount;
	u32 m_HashSalt;

    HashTable();
    ~HashTable();
    HashTable(const HashTable&);
    HashTable& operator=(const HashTable&);
};

}   //namespace honeybase

#endif  //__HB_DICT_H__
