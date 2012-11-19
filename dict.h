#ifndef __HB_DICT_H__
#define __HB_DICT_H__

#include "hb.h"

class HbDictItem
{
    friend class HbDict;
private:

    static HbDictItem* Create(const HbString* key, const HbString* value);
    static HbDictItem* Create(const HbString* key, const s64 value);
    static HbDictItem* Create(const HbString* key, const double value);
    static HbDictItem* Create(const s64 key, const HbString* value);
    static HbDictItem* Create(const s64 key, const s64 value);
    static HbDictItem* Create(const s64 key, const double value);
    static HbDictItem* CreateEmpty(const HbString* key, const size_t len);
    static HbDictItem* CreateEmpty(const s64 key, const size_t len);
    static void Destroy(HbDictItem* item);

    HbValue m_Key;
    HbValue m_Value;
    HbDictItem* m_Next;

    HbValueType m_KeyType : 4;
    HbValueType m_ValType : 4;

private:

    static HbDictItem* Create();

    HbDictItem();
    ~HbDictItem();
    HbDictItem(const HbDictItem&);
    HbDictItem& operator=(const HbDictItem&);
};

class HbDict
{
public:

    static HbDict* Create();
    static void Destroy(HbDict* dict);

    bool Set(const HbString* key, const HbString* value);
    bool Set(const HbString* key, const s64 value);
    bool Set(const s64 key, const HbString* value);
    bool Set(const s64 key, const s64 value);

	bool Clear(const s64 key);
	bool Clear(const HbString* key);

    bool Find(const HbString* key, const HbString** value) const;
    bool Find(const HbString* key, s64* value) const;
    bool Find(const s64 key, const HbString** value) const;
    bool Find(const s64 key, s64* value) const;

    bool Merge(const HbString* key, const HbString* value, const size_t mergeOffset);
    bool Merge(const s64 key, const HbString* value, const size_t mergeOffset);

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

		HbDictItem* m_Item;
		int m_Count;
	};

	u32 HashString(const byte* string, const size_t stringLen) const;

    void Set(HbDictItem* item);
    void Set(HbDictItem* item, bool* replaced);
    
    HbDictItem** Find(const HbString* key, Slot** slot);
    HbDictItem** Find(const s64 key, Slot** slot);

	static const int INITIAL_NUM_SLOTS	= (1<<8);

    Slot* m_Slots[2];

    size_t m_Count;
    size_t m_NumSlots;
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

    static void TestMergeIntKeys(const int numKeys, const int numIterations);

    static void CreateRandomKeys(KV* kv, const int numKeys);

    static void AddRandomKeys(const int numKeys);

    static void AddDeleteRandomKeys(const int numKeys);
};

#endif  //__HB_DICT_H__
