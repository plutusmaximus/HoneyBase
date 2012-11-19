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

unsigned HbRand();
unsigned HbRand(const unsigned min, const unsigned max);

enum HbValueType
{
    HB_VALUETYPE_INVALID,
    HB_VALUETYPE_INT,
    HB_VALUETYPE_DOUBLE,
    HB_VALUETYPE_STRING,
    HB_VALUETYPE_DICT,
    HB_VALUETYPE_SKIPLIST
};

class HbString;
class HbDict;
class HbSkipList;

union HbValue
{
    s64 m_Int;
    double m_Double;
    HbString* m_String;
    HbDict* m_Dict;
    HbSkipList* m_SkipList;
};

///////////////////////////////////////////////////////////////////////////////
//  HbLog
///////////////////////////////////////////////////////////////////////////////
class HbLog
{
public:

    HbLog();
    explicit HbLog(const char* channel);

    void Debug(const char* fmt, ...);
    void Error(const char* fmt, ...);

private:

    char m_Channel[64];
};

///////////////////////////////////////////////////////////////////////////////
//  HbHeap
///////////////////////////////////////////////////////////////////////////////
class HbHeap
{
public:

	static void* ZAlloc(size_t size);

	static void Free(void* mem);
};

///////////////////////////////////////////////////////////////////////////////
//  HbString
///////////////////////////////////////////////////////////////////////////////
class HbString
{
public:

    static HbString* Create(const size_t stringLen);
    static HbString* Create(const byte* string, const size_t stringLen);
    static HbString* Dup(const HbString* string);
    static void Destroy(HbString* hbs);

    static HbString* Encode(const byte* src, const size_t srcLen,
                            byte* dst, const size_t dstSize);

    static size_t GetData(const byte* hbs, const byte** data);
    static size_t GetData(byte* hbs, byte** data);

    static size_t Length(const byte* hbs);

    static size_t Size(const byte* hbs);

    static size_t Size(const size_t stringLen);

    size_t GetData(const byte** data) const;
    size_t GetData(byte** data);

    size_t Length() const;

    size_t Size() const;

    HbString* Dup() const;

    bool EQ(const HbString* that) const;
    bool EQ(const byte* string, const size_t stringLen) const;

protected:

    static void Init(HbString* hbs, const byte* bytes, const size_t size);

private:

    HbString(){};
    ~HbString(){}
    HbString(const HbString&);
    HbString& operator=(const HbString&);
};

///////////////////////////////////////////////////////////////////////////////
//  HbFixedString32
///////////////////////////////////////////////////////////////////////////////
class HbFixedString32 : HbString
{
public:

    explicit HbFixedString32(const byte* string, const size_t stringLen);

    ~HbFixedString32();

private:

    HbFixedString32();

    byte m_Buf[32];
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

    void Restart();
    void Stop();

    double GetElapsed() const;

private:
    u64 m_Freq;
    u64 m_Start;
    u64 m_Stop;

    bool m_Running  : 1;
};


#endif  //__HB_H__
