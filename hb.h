#ifndef __HB_H__
#define __HB_H__

#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <stddef.h>

#define __STDC_FORMAT_MACROS

#ifdef __GNUC__
#include <inttypes.h>
#endif

namespace honeybase
{

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

#ifndef NDEBUG
#define HB_ASSERT 1
#define HB_ASSERTONLY(a) a
#else
#define HB_ASSERT 0
#define HB_ASSERTONLY(a)
#endif

#define hbverify(cond) ((cond) || (__debugbreak(),false))

#if HB_ASSERT
#define hbassert(cond) (void)(hbverify(cond))
#else
#define hbassert(cond)
#endif

#define hbarraylen(a) (sizeof(a)/sizeof((a)[0]))

#define hb_static_assert(cond) typedef char static_assertion_##__LINE__[(cond)?1:-1]

unsigned Rand();
unsigned Rand(const unsigned min, const unsigned max);

size_t Base64Encode(const byte* bytes, const size_t numBytes, char* buf, const size_t bufSize);

enum KeyType
{
    KEYTYPE_INT,
    KEYTYPE_DOUBLE,
    KEYTYPE_BLOB
};

enum ValueType
{
    VALUETYPE_INT,
    VALUETYPE_DOUBLE,
    VALUETYPE_BLOB,
    VALUETYPE_CONTAINER
};

enum ContainerType
{
    CONTAINERTYPE_HASHTABLE,
    CONTAINERTYPE_BTREE,
    CONTAINERTYPE_SKIPLIST,
};

///////////////////////////////////////////////////////////////////////////////
//  Blob
///////////////////////////////////////////////////////////////////////////////
class Container
{
    ContainerType m_ContainerType;
};

///////////////////////////////////////////////////////////////////////////////
//  Blob
///////////////////////////////////////////////////////////////////////////////
class Blob
{
public:

    static Blob* Create(const size_t stringLen);
    static Blob* Create(const byte* string, const size_t stringLen);
    static Blob* Dup(const Blob* string);
    static void Destroy(Blob* hbs);

    static Blob* Encode(const byte* src, const size_t srcLen,
                            byte* dst, const size_t dstSize);

    static size_t Size(const size_t len);

    size_t GetData(const byte** data) const;
    size_t GetData(byte** data);

    size_t Length() const;

    size_t Size() const;

    Blob* Dup() const;

    int Compare(const Blob* that) const;

    bool EQ(const Blob* that) const
    {
        return Compare(that) == 0;
    }

    bool LT(const Blob* that) const
    {
        return Compare(that) < 0;
    }

    bool GT(const Blob* that) const
    {
        return Compare(that) > 0;
    }

    bool LE(const Blob* that) const
    {
        return Compare(that) <= 0;
    }

    bool GE(const Blob* that) const
    {
        return Compare(that) >= 0;
    }

protected:

    static void Init(Blob* hbs, const byte* bytes, const size_t size);

private:

    byte bytes[1];

    Blob(){};
    ~Blob(){}
    Blob(const Blob&);
    Blob& operator=(const Blob&);
};

///////////////////////////////////////////////////////////////////////////////
//  Key
///////////////////////////////////////////////////////////////////////////////
class Key
{
public:
    union
    {
        s64 m_Int;
        double m_Double;
        Blob* m_Blob;
    };

    void Set(const s64 value)
    {
        m_Int = value;
    }

    void Set(const double value)
    {
        m_Double = value;
    }

    void Set(const Blob* value)
    {
        m_Blob = value->Dup();
    }

    int Compare(const KeyType keyType, const Key& that) const
    {
        switch(keyType)
        {
            case KEYTYPE_INT:
            case KEYTYPE_DOUBLE:
                return int(m_Int - that.m_Int);
            break;
            case KEYTYPE_BLOB:
                return m_Blob->Compare(that.m_Blob);
            break;
        }

        return -1;
    }

    bool EQ(const KeyType keyType, const Key& that) const
    {
        return Compare(keyType, that) == 0;
    }

    bool LT(const KeyType keyType, const Key& that) const
    {
        return Compare(keyType, that) < 0;
    }

    bool GT(const KeyType keyType, const Key& that) const
    {
        return Compare(keyType, that) > 0;
    }

    bool LE(const KeyType keyType, const Key& that) const
    {
        return Compare(keyType, that) <= 0;
    }

    bool GE(const KeyType keyType, const Key& that) const
    {
        return Compare(keyType, that) >= 0;
    }
};

///////////////////////////////////////////////////////////////////////////////
//  Value
///////////////////////////////////////////////////////////////////////////////
class Value
{
public:
    union
    {
        s64 m_Int;
        double m_Double;
        Blob* m_Blob;
        Container* m_Container;
    };
};

///////////////////////////////////////////////////////////////////////////////
//  Log
///////////////////////////////////////////////////////////////////////////////
class Log
{
public:

    Log();
    explicit Log(const char* channel);

    void Debug(const char* fmt, ...);
    void Error(const char* fmt, ...);

private:

    char m_Channel[64];
};

///////////////////////////////////////////////////////////////////////////////
//  Heap
///////////////////////////////////////////////////////////////////////////////
class Heap
{
public:

	static void* ZAlloc(size_t size);

	static void Free(void* mem);
};

class BlobTest
{
public:

    static void Test();
};

class StopWatch
{
public:
    StopWatch();

    void Restart();
    void Stop();

    double GetElapsed() const;

private:
    u64 m_Freq;
    u64 m_Start;
    u64 m_Stop;

    bool m_Running  : 1;
};

}   //namespace honeybase

#endif  //__HB_H__
