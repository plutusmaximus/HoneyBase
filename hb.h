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

#define HB_ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))

#define STATIC_ASSERT(cond) typedef char static_assertion_##__LINE__[(cond)?1:-1]

#define hbVerify(cond) ((cond) || (__debugbreak(),false))

unsigned HbRand();
unsigned HbRand(const unsigned min, const unsigned max);

enum HbItemType
{
    HB_ITEMTYPE_INVALID,
    HB_ITEMTYPE_INT,
    HB_ITEMTYPE_DOUBLE,
    HB_ITEMTYPE_STRING,
    HB_ITEMTYPE_DICT
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
    static void Destroy(HbString* hbs);

    size_t GetData(const byte** data) const;
    size_t GetData(byte** data);

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
