#define _CRT_SECURE_NO_WARNINGS

#include "hb.h"

#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>

#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static unsigned m_w = 77665;    /* must not be zero */
static unsigned m_z = 22559;    /* must not be zero */

unsigned HbRand()
{
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;  /* 32-bit result */
}

unsigned HbRand(const unsigned min, const unsigned max)
{
    return min + (HbRand() % (max - min));
}

///////////////////////////////////////////////////////////////////////////////
//  HbLog
///////////////////////////////////////////////////////////////////////////////
HbLog::HbLog()
{
    m_Channel[0] = '\0';
}

HbLog::HbLog(const char* channel)
{
    snprintf(m_Channel, sizeof(m_Channel)-1, "%s", channel);
}

void
HbLog::Debug(const char* fmt, ...)
{
    char prefix[128];
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, args);
    snprintf(prefix, sizeof(prefix), "DBG[%s]: ", m_Channel);
    fprintf(stdout, "%s%s\n", prefix, buf);
    OutputDebugString(prefix);
    OutputDebugString(buf);
    OutputDebugString("\n");
}

void
HbLog::Error(const char* fmt, ...)
{
    char prefix[128];
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, args);
    snprintf(prefix, sizeof(prefix), "ERR[%s]: ", m_Channel);
    fprintf(stdout, "%s%s\n", prefix, buf);
    OutputDebugString(prefix);
    OutputDebugString(buf);
    OutputDebugString("\n");
}

///////////////////////////////////////////////////////////////////////////////
//  HbHeap
///////////////////////////////////////////////////////////////////////////////
void*
HbHeap::ZAlloc(size_t size)
{
	return calloc(1, size);
}

void
HbHeap::Free(void* mem)
{
	free(mem);
}

///////////////////////////////////////////////////////////////////////////////
//  HbString
///////////////////////////////////////////////////////////////////////////////
HbString*
HbString::Create(const size_t stringLen)
{
    return Create(NULL, stringLen);
}

HbString*
HbString::Create(const byte* string, const size_t stringLen)
{
    const size_t size = Size(stringLen);
    HbString* hbs = (HbString*) HbHeap::ZAlloc(size);
    if(hbs)
    {
        Init(hbs, string, stringLen);
    }

    return hbs;
}

HbString*
HbString::Dup(const HbString* string)
{
    HbString* hbs = (HbString*) HbHeap::ZAlloc(string->Size());
    if(hbs)
    {
        const byte* data;
        const size_t len = string->GetData(&data);
        Init(hbs, data, len);
    }

    return hbs;
}

void
HbString::Destroy(HbString* hbs)
{
    hbs->~HbString();
    HbHeap::Free(hbs);
}

HbString*
HbString::Encode(const byte* src, const size_t srcLen,
                byte* dst, const size_t dstSize)
{
    const size_t size = Size(srcLen);
    if(hbverify(size <= dstSize))
    {
        Init((HbString*)dst, src, srcLen);
        return (HbString*)dst;
    }

    return NULL;
}

size_t
HbString::GetData(const byte* hbs, const byte** data)
{
    size_t len = 0;
    const byte* p = hbs;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    *data = ++p;

    return len;
}

size_t
HbString::GetData(byte* hbs, byte** data)
{
    const byte* cdata;
    const size_t len = GetData(hbs, &cdata);
    *data = (byte*)cdata;
    return len;
}

size_t
HbString::Length(const byte* hbs)
{
    size_t len = 0;
    const byte* p = hbs;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    return len;
}

size_t
HbString::Size(const byte* hbs)
{
    size_t len = 0;
    const byte* p = hbs;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    return len + (p - hbs + 1);
}

size_t
HbString::Size(const size_t stringLen)
{
    size_t size = stringLen;
    size_t tmpLen = stringLen;

    for(++size, tmpLen >>= 7; tmpLen; ++size, tmpLen >>= 7)
    {
    }

    return size;
}

size_t
HbString::GetData(const byte** data) const
{
    return GetData((const byte*) this, data);
}

size_t
HbString::GetData(byte** data)
{
    return GetData((byte*) this, data);
}

size_t
HbString::Length() const
{
    return Length((const byte*) this);
}

size_t
HbString::Size() const
{
    return Size((const byte*) this);
}

HbString*
HbString::Dup() const
{
    return Dup(this);
}

int
HbString::Compare(const byte* thatData, const size_t thatLen) const
{
    const byte* myData;
    const size_t myLen = GetData(&myData);

    if(myLen < thatLen)
    {
        if(myLen > 0)
        {
            const int result = memcmp(myData, thatData, myLen);
            return (0 == result) ? -1 : result;
        }
        else
        {
            return -1;
        }
    }
    else if(thatLen < myLen)
    {
        if(thatLen > 0)
        {
            const int result = memcmp(myData, thatData, thatLen);
            return (0 == result) ? 1 : result;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return myLen > 0 ? memcmp(myData, thatData, myLen): 0;
    }
}

//protected:

void
HbString::Init(HbString* hbs, const byte* bytes, const size_t stringLen)
{
    byte* p = (byte*) hbs;

    size_t tmpLen = stringLen;
    while(tmpLen > 0x7F)
    {
        *p++ = 0x80 | (tmpLen & 0x7F);
        tmpLen >>= 7;
    }

    *p++ = tmpLen & 0x7F;

    if(bytes)
    {
        memcpy(p, bytes, stringLen);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  HbStringTest
///////////////////////////////////////////////////////////////////////////////
void
HbStringTest::Test()
{
    HbString* hbs = HbString::Create((const byte*)"Foo", strlen("Foo"));
    const byte* data;
    size_t len = hbs->GetData(&data);
    hbassert(len == 3);
    hbassert(0 == memcmp("Foo", data, strlen("Foo")));
    HbString::Destroy(hbs);

    byte* str;

    len = 0x7F + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    hbs->GetData(&data);
    hbassert(hbs->Length() == len);
    hbassert(0 == memcmp(str, data, len));
    HbString::Destroy(hbs);
    delete [] str;

    len = 0x7FFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    hbassert(hbs->Length() == len);
    hbs->GetData(&data);
    hbassert(0 == memcmp(str, data, len));
    HbString::Destroy(hbs);
    delete [] str;

    len = 0x7FFFFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    hbassert(hbs->Length() == len);
    hbs->GetData(&data);
    hbassert(0 == memcmp(str, data, len));
    HbString::Destroy(hbs);
    delete [] str;
}

///////////////////////////////////////////////////////////////////////////////
//  HbStopWatch
///////////////////////////////////////////////////////////////////////////////
HbStopWatch::HbStopWatch()
: m_Start(0)
, m_Stop(0)
, m_Running(false)
{
#if _MSC_VER
    QueryPerformanceFrequency((LARGE_INTEGER*)&m_Freq);
#elif defined(__GNUC__)
    m_Freq = 1000000000;
#endif
}

void
HbStopWatch::Restart()
{
#if _MSC_VER
    QueryPerformanceCounter((LARGE_INTEGER*)&m_Start);
#elif defined(__GNUC__)
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    m_Start = (ts.tv_sec*m_Freq) + ts.tv_nsec;
#endif
    m_Running = true;
}

void
HbStopWatch::Stop()
{
#if _MSC_VER
    QueryPerformanceCounter((LARGE_INTEGER*)&m_Stop);
#elif defined(__GNUC__)
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    m_Stop = (ts.tv_sec*m_Freq) + ts.tv_nsec;
#endif
    m_Running = false;
}

double
HbStopWatch::GetElapsed() const
{
    u64 stop;
    if(m_Running)
    {
#if _MSC_VER
        QueryPerformanceCounter((LARGE_INTEGER*)&stop);
#elif defined(__GNUC__)
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        stop = (ts.tv_sec*m_Freq) + ts.tv_nsec;
#endif
    }
    else
    {
        stop = m_Stop;
    }

    return ((double)(stop - m_Start)) / m_Freq;
}

