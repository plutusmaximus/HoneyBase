#ifndef __HB_H__
#define __HB_H__

#include <stdint.h>
#include <stddef.h>

#define __STDC_FORMAT_MACROS

#ifdef __GNUC__
#include <inttypes.h>
#endif

#if _MSC_VER
#define snprintf _snprintf
#endif

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;
typedef u8          byte;

#ifdef _MSC_VER
#ifndef PRIu64
#define PRId64 "I64d"
#define PRIu64 "I64u"
#endif
#endif

#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))

#define hbVerify(cond) ((cond) || (__debugbreak(),false))

enum HbItemType
{
    HB_ITEMTYPE_INVALID,
    HB_ITEMTYPE_INT,
    HB_ITEMTYPE_DOUBLE,
    HB_ITEMTYPE_STRING,
    HB_ITEMTYPE_DICT
};

class HbHeap;
class HbDict;
class HbIterator;
class HbIndex;
class HbIndexNode;

class HbString
{
public:

    static HbString* Create(const byte* string, const size_t stringLen);
    static void Destroy(HbString* hbs);

    const byte* Data() const;

    size_t GetData(const byte** data) const;

    size_t Length() const;

    size_t Size() const;

    static size_t Size(const size_t stringLen);

    bool EQ(const HbString& that) const;
    bool EQ(const byte* string, const size_t stringLen) const;

private:

    HbString(){};
    ~HbString(){}
    HbString(const HbString&);
    HbString& operator=(const HbString&);
};

class HbStringTest
{
public:

    static void Test();
};

class HbStopWatch
{
public:
    HbStopWatch();

    void Start();
    void Stop();

    double GetElapsed() const;

private:
    u64 m_Freq;
    u64 m_Start;
    u64 m_Stop;

    bool m_Running  : 1;
};

union HbValue
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

    HbValue m_Key;
    HbValue m_Value;
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
	friend class HbHeap;
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

class HbIterator
{
    friend class HbIndex;
public:
    HbIterator();
    ~HbIterator();

    void Clear();

    bool HasNext() const;
    bool HasCurrent() const;

    bool Next();

    s64 GetKey();
    s64 GetValue();

private:

    HbIndexNode* m_Cur;
    int m_ItemIndex;
};

class HbIndexNode;

class HbIndexItem
{
public:

    HbIndexItem()
        : m_Value(0)
    {
    }

    union
    {
        s64 m_Value;
        HbIndexNode* m_Node;
    };
};

class HbIndexNode
{
public:

    static const int NUM_KEYS = 256;

    HbIndexNode();

    bool HasDups() const;

    s64 m_Keys[NUM_KEYS];
    HbIndexItem m_Items[NUM_KEYS+1];

    int m_NumKeys;
};

class HbIndex
{
public:

    HbIndex();

    bool Insert(const s64 key, const s64 value);

    bool Delete(const s64 key);

    bool Find(const s64 key, s64* value) const;

    bool GetFirst(HbIterator* it);

    u64 Count() const;

    void Validate();

private:

    bool Find(const s64 key,
            const HbIndexNode** outNode,
            int* outKeyIdx,
            const HbIndexNode** outParent,
            int* outParentKeyIdx) const;
    bool Find(const s64 key,
            HbIndexNode** outNode,
            int* outKeyIdx,
            HbIndexNode** outParent,
            int* outParentKeyIdx);

    void MergeLeft(HbIndexNode* parent, const int keyIdx, const int count, const int depth);
    void MergeRight(HbIndexNode* parent, const int keyIdx, const int count, const int depth);

    void TrimNode(HbIndexNode* node, const int depth);

    void ValidateNode(const int depth, HbIndexNode* node);

    static int UpperBound(const s64 key, const s64* first, const s64* end);

    HbIndexNode* m_Nodes;

    HbIndexNode* m_Leaves;

    u64 m_Count;
    u64 m_Capacity;
    int m_Depth;
};

class HbIndexTest
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

    static void CreateRandomKeys(KV* kv, const int numKeys, const bool unique, const int range);
    static void AddRandomKeys(const int numKeys, const bool unique, const int range);
    static void AddDeleteRandomKeys(const int numKeys, const bool unique, const int range);
    static void AddSortedKeys(const int numKeys, const bool unique, const int range, const bool ascending);
    static void AddDups(const int numKeys, const int min, const int max);
};

#endif  //__HB_H__
