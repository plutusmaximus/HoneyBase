#ifndef __HB_H__
#define __HB_H__

#include <stdint.h>
#include <stddef.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;
typedef u8          byte;

#ifndef PRIu64
#if defined(_MSC_VER)
#define PRId64 "I64d"
#define PRIu64 "I64u"
#elif defined(__GNUC__)
#define PRId64 "lld"
#define PRIu64 "llu"
#endif
#endif

#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))

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

union HbValue
{
    s64 m_Int;
    double m_Double;
    const char* m_String;
    HbDict* m_Dict;
};

class HbDictItem
{
    friend class HbHeap;
public:

    static HbDictItem* Create(const char* key, const char* value);
    static HbDictItem* Create(const char* key, const s64 value);
    static HbDictItem* Create(const char* key, const double value);
    static HbDictItem* CreateDict(const char* key);
    static void Destroy(HbDictItem* item);

    HbItemType m_KeyType : 3;
    HbItemType m_ValType : 3;

    HbDictItem* m_Next;

    HbValue m_Key;
    HbValue m_Value;

private:

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

    HbDictItem** FindItem(const char* key);

    void Set(HbDictItem* item);
    void Set(const char* key, const char* value);
    void Set(const char* key, const s64 value);
    void Set(const char* key, const double value);
    HbDict* SetDict(const char* key);

private:

    HbDictItem* m_Slots[256];

    s64 m_Count;
    int m_NumSlots;

    HbDict();
    ~HbDict();
    HbDict(const HbDict&);
    HbDict& operator=(const HbDict&);
};

class HbIndexItem
{
    friend class HbIndexNode;

public:

    int m_Key;
    union
    {
        int m_Value;
        HbIndexNode* m_Node;
    };

private:

    HbIndexItem();
};

class HbIndexNode
{
public:

    HbIndexNode();

    void Dump(const bool leafOnly, const int curDepth, const int maxDepth) const;

    HbIndexItem m_Items[128];

    int m_NumItems;

    void LinkBefore(HbIndexNode* node);
    void LinkAfter(HbIndexNode* node);
    void Unlink();

    static void MoveItemsRight(HbIndexNode* src, HbIndexNode* dst, const int numItems);
    static void MoveItemsLeft(HbIndexNode* src, HbIndexNode* dst, const int numItems);

    HbIndexNode* m_Next;
    HbIndexNode* m_Prev;
};

class HbIterator
{
    friend class HbIndex;
public:
    HbIterator();
    ~HbIterator();

    void Clear();

    bool HasNext() const;
    bool HasPrev() const;
    bool HasCurrent() const;

    void Next();
    void Prev();

    int GetKey();
    int GetValue();

private:

    HbIndexNode* m_First;
    HbIndexNode* m_Cur;
    int m_ItemIndex;
};

class HbIndex
{
public:

    HbIndex();

    void DumpStats() const;

    void Dump(const bool leafOnly) const;

    bool Insert(const int key, const int value);

    bool Delete(const int key);

    bool GetFirst(HbIterator* it) const;

    bool Find(const int key, int* value) const;

    bool Find(const int key, HbIterator* it) const;

    unsigned Count(const int key) const;

    bool Validate() const;

private:

    static bool ValidateNode(const HbIndexNode* node);

    HbIndexNode* m_Nodes;

    u64 m_Count;
    u64 m_Capacity;
    int m_Depth;
    int m_KeySize;
};

class HbIndexTest
{
public:
    static void AddRandomKeys(const int numKeys, const bool unique, const int range);
    static void AddSortedKeys(const int numKeys);
};

#endif  //__HB_H__
