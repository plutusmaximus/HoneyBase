#ifndef __HB_DICT_H__
#define __HB_DICT_H__

#include "hb.h"

namespace honeybase
{

class HtItem
{
    friend class HashTable;
private:

    static HtItem* Create(const Blob* key, const Blob* value);
    static HtItem* Create(const Blob* key, const s64 value);
    static HtItem* Create(const Blob* key, const double value);
    static HtItem* Create(const s64 key, const Blob* value);
    static HtItem* Create(const s64 key, const s64 value);
    static HtItem* Create(const s64 key, const double value);
    static HtItem* CreateEmpty(const Blob* key, const size_t len);
    static HtItem* CreateEmpty(const s64 key, const size_t len);
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
    bool Set(const Blob* key, const Blob* value);
    bool Set(const Blob* key, const s64 value);
    bool Set(const s64 key, const Blob* value);
    bool Set(const s64 key, const s64 value);

    bool Clear(const Key key, const KeyType keyType);
	bool Clear(const s64 key);
	bool Clear(const Blob* key);

    bool Find(const Key key, const KeyType keyType,
                Value* value, ValueType* valueType);
    bool Find(const Blob* key, const Blob** value) const;
    bool Find(const Blob* key, s64* value) const;
    bool Find(const s64 key, const Blob** value) const;
    bool Find(const s64 key, s64* value) const;

    bool Patch(const Key key, const KeyType keyType,
                const Blob* value, const size_t offset);
    bool Patch(const Blob* key, const Blob* value, const size_t offset);
    bool Patch(const s64 key, const Blob* value, const size_t offset);

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
    
    HtItem** Find(const Blob* key, Slot** slot, u32* hash);
    HtItem** Find(const s64 key, Slot** slot, u32* hash);

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
