#ifndef __HB_DICT_H__
#define __HB_DICT_H__

#include "hb.h"

namespace honeybase
{

class HtItem
{
    friend class HashTable;
private:

    static HtItem* Create(const Key key, const KeyType keyType,
                            const Value value, const ValueType valueType);
    static HtItem* CreateEmpty(const Key key, const KeyType keyType, const size_t len);
    static void Destroy(HtItem* item);

    Key m_Key;
    Value m_Value;
    HtItem* m_Next;
    u32 m_Hash;

    KeyType m_KeyType : 4;
    ValueType m_ValType : 4;

private:

    static HtItem* Create();

    HtItem();
    ~HtItem();
    HtItem(const HtItem&);
    HtItem& operator=(const HtItem&);
};

class HashTable
{
public:

    static HashTable* Create();
    static void Destroy(HashTable* dict);

    bool Set(const Key key, const KeyType keyType,
            const Value value, const ValueType valueType);

    bool Clear(const Key key, const KeyType keyType);

    bool Find(const Key key, const KeyType keyType,
                Value* value, ValueType* valueType);

    bool Patch(const Key key, const KeyType keyType,
                const size_t numPatches,
                const Blob** patches,
                const size_t* offsets);

    size_t Count() const;

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

	u32 HashBytes(const byte* bytes, const size_t len) const;

    void Set(HtItem* item);
    void Set(HtItem* item, bool* replaced);
    
    HtItem** Find(const Key key, const KeyType keyType, Slot** slot, u32* hash);

	static const int INITIAL_NUM_SLOTS	= (1<<8);

    Slot* m_Slots[2];

    size_t m_Count;
    size_t m_NumSlots;
	u32 m_HashSalt;

    HashTable();
    ~HashTable();
    HashTable(const HashTable&);
    HashTable& operator=(const HashTable&);
};

}   //namespace honeybase

#endif  //__HB_DICT_H__
