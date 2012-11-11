#ifndef __HB_DICT_H__
#define __HB_DICT_H__

#include "hb.h"

class HbDict;

union HbDictValue
{
    s64 m_Int;
    double m_Double;
    HbString* m_String;
    HbDict* m_Dict;
};

class HbDictItem
{
    friend class HbHeap;
public:

    static HbDictItem* Create(const byte* key, const size_t keylen,
                                const byte* value, const size_t vallen);
    static HbDictItem* Create(const byte* key, const size_t keylen, const s64 value);
    static HbDictItem* Create(const byte* key, const size_t keylen, const double value);
    static HbDictItem* CreateDict(const byte* key, const size_t keylen);
    static HbDictItem* Create(const s64 key, const byte* value, const size_t vallen);
    static HbDictItem* Create(const s64 key, const s64 value);
    static HbDictItem* Create(const s64 key, const double value);
    static HbDictItem* CreateDict(const s64 key);
    static void Destroy(HbDictItem* item);

    HbDictValue m_Key;
    HbDictValue m_Value;
    HbDictItem* m_Next;

    HbItemType m_KeyType : 4;
    HbItemType m_ValType : 4;

private:

    static HbDictItem* Create();

    HbDictItem();
    ~HbDictItem();
    HbDictItem(const HbDictItem&);
    HbDictItem& operator=(const HbDictItem&);
};

class HbDict
{
	friend class HbDictItem;

public:

    static HbDict* Create();
    static void Destroy(HbDict* dict);

    HbDictItem** FindItem(const byte* key, const size_t keylen);
    HbDictItem** FindItem(const s64 key);

    void Set(HbDictItem* item);
    void Set(const byte* key, const size_t keylen,
            const byte* value, const size_t vallen);
    void Set(const byte* key, const size_t keylen, const s64 value);
    void Set(const byte* key, const size_t keylen, const double value);
    HbDict* SetDict(const byte* key, const size_t keylen);
    void Set(const s64 key, const byte* value, const size_t vallen);
    void Set(const s64 key, const s64 value);
    void Set(const s64 key, const double value);
    HbDict* SetDict(const s64 key);

	void Clear(const byte* key, const size_t keylen);
	void Clear(const s64 key);

    s64 Count() const;

private:

	class Slot
	{
	public:
		Slot()
			: m_Item(NULL)
			, m_Count(0)
		{
		}

		HbDictItem* m_Item;
		int m_Count;
	};

	u32 HashString(const byte* string, const size_t stringLen) const;

    HbDictItem** FindItem(const byte* key, const size_t keylen, Slot** slot);
    HbDictItem** FindItem(const s64 key, Slot** slot);

	static const int INITIAL_NUM_SLOTS	= (1<<8);

    Slot* m_Slots;

    s64 m_Count;
    int m_NumSlots;
	u32 m_HashSalt;

    HbDict();
    ~HbDict();
    HbDict(const HbDict&);
    HbDict& operator=(const HbDict&);
};

class HbDictTest
{
public:
    static void TestStringString(const int numKeys);
    static void TestStringInt(const int numKeys);
    static void TestIntInt(const int numKeys);
    static void TestIntString(const int numKeys);
};

#endif  //__HB_DICT_H__
