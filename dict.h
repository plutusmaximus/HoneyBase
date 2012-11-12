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

    bool Find(const s64 key, s64* value) const;

    HbDictItem** Find(const byte* key, const size_t keylen);
    HbDictItem** Find(const s64 key);
    HbDictItem** Find(const HbString* key);

    void Set(HbDictItem* item);

    bool Merge(HbDictItem* mergeItem, const size_t mergeOffset);

	bool Clear(const byte* key, const size_t keylen);
	bool Clear(const s64 key);

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
    
    HbDictItem** Find(const byte* key, const size_t keylen, Slot** slot);
    HbDictItem** Find(const s64 key, Slot** slot);
    HbDictItem** Find(const HbString* key, Slot** slot);

    void Set(HbDictItem* item, bool* replaced);

	static const int INITIAL_NUM_SLOTS	= (1<<8);

    Slot* m_Slots[2];

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
    struct KV
    {
        int m_Key;
        int m_Value;
        bool m_Added : 1;

        KV()
        : m_Added(false)
        {
        }

        bool operator<(const KV& a) const
        {
            return m_Key < a.m_Key;
        }
    };

    static void TestStringString(const int numKeys);
    static void TestStringInt(const int numKeys);
    static void TestIntInt(const int numKeys);
    static void TestIntString(const int numKeys);

    static void TestMergeIntKeys(const int numKeys);

    static void CreateRandomKeys(KV* kv, const int numKeys);

    static void AddRandomKeys(const int numKeys);

    static void AddDeleteRandomKeys(const int numKeys);
};

#endif  //__HB_DICT_H__
