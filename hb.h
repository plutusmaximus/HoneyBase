#ifndef __HB_H__
#define __HB_H__

#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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
#define HB_DEBUGONLY(a) a
#else
#define HB_ASSERT 0
#define HB_ASSERTONLY(a)
#define HB_DEBUGONLY(a)
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

enum ValueType
{
    VALUETYPE_INT,
    VALUETYPE_DOUBLE,
    VALUETYPE_BLOB
};

///////////////////////////////////////////////////////////////////////////////
//  StopWatch
///////////////////////////////////////////////////////////////////////////////
class StopWatch
{
public:
    StopWatch();

    void Clear();
    void Start();
    void Restart();
    void Stop();

    double GetElapsed() const;

private:
    u64 m_Freq;
    u64 m_Start;
    u64 m_Elapsed;

    bool m_Running  : 1;
};

///////////////////////////////////////////////////////////////////////////////
//  Blob
///////////////////////////////////////////////////////////////////////////////
class Blob
{
public:

    static Blob* Create(const size_t stringLen);
    static Blob* Create(const byte* string, const size_t stringLen);
    static size_t Size(const size_t len);

    /*static Blob* Encode(const byte* src, const size_t srcLen,
                        byte* dst, const size_t dstSize);*/

    size_t GetData(const byte** data) const;
    size_t GetData(byte** data);

    size_t CopyData(byte* dst, const size_t dstSize) const;

    size_t Length() const;

    //Blob* Dup() const;

    void Ref() const;
    void Unref();
    int NumRefs() const;

    int Compare(const byte* thatData, const size_t thatLen) const;

    int Compare(const Blob* that) const
    {
        const byte* thatData;
        const size_t thatLen = that->GetData(&thatData);
        return Compare(thatData, thatLen);
    }

    static size_t GlobalBlobCount();

    static StopWatch sm_StopWatch;

protected:

    static void Init(Blob* hbs, const byte* bytes, const size_t size);

private:

    //static Blob* Dup(const Blob* string);
    static void Destroy(Blob* blob);

    mutable s8 m_RefCount;
    byte bytes[1];

    Blob()
    : m_RefCount(1)
    {}
    ~Blob(){}
    Blob(const Blob&);
    Blob& operator=(const Blob&);
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
    };

    int Compare(const ValueType valueType, const Value& that) const
    {
        hb_static_assert(sizeof(m_Int) == sizeof(m_Double));

        switch(valueType)
        {
            case VALUETYPE_INT:
                return int(m_Int - that.m_Int);
                break;
            case VALUETYPE_DOUBLE:
                return (m_Double < that.m_Double) ? -1 : (m_Double == that.m_Double) ? 0 : 1;
                break;
            case VALUETYPE_BLOB:
                return m_Blob->Compare(that.m_Blob);
                break;
        }

        return -1;
    }

    bool EQ(const ValueType valueType, const Value& that) const
    {
        return Compare(valueType, that) == 0;
    }

    bool LT(const ValueType valueType, const Value& that) const
    {
        return Compare(valueType, that) < 0;
    }

    bool GT(const ValueType valueType, const Value& that) const
    {
        return Compare(valueType, that) > 0;
    }

    bool LE(const ValueType valueType, const Value& that) const
    {
        return Compare(valueType, that) <= 0;
    }

    bool GE(const ValueType valueType, const Value& that) const
    {
        return Compare(valueType, that) >= 0;
    }
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

	static void* Alloc(size_t size);

	static void Free(void* mem);
};

class BlobTest
{
public:

    static void Test();
};

}   //namespace honeybase

#endif  //__HB_H__
