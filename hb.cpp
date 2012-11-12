#define _CRT_SECURE_NO_WARNINGS

#include "hb.h"

#include <assert.h>
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
    const size_t size = Size(stringLen);
    HbString* hbs = (HbString*) HbHeap::ZAlloc(size);
    if(hbs)
    {
        byte* bytes = (byte*) hbs;

        int offset = 0;
        size_t tmpLen = stringLen;
        while(tmpLen > 0x7F)
        {
            bytes[offset++] = 0x80 | (tmpLen & 0x7F);
            tmpLen >>= 7;
        }

        bytes[offset++] = tmpLen & 0x7F;
    }

    return hbs;
}

HbString*
HbString::Create(const byte* string, const size_t stringLen)
{
    HbString* hbs = Create(stringLen);
    if(hbs && string)
    {
        byte* data;
        hbs->GetData(&data);
        memcpy(data, string, stringLen);
    }

    return hbs;
}

void
HbString::Destroy(HbString* hbs)
{
    hbs->~HbString();
    HbHeap::Free(hbs);
}

size_t
HbString::GetData(const byte** data) const
{
    size_t len = 0;
    const byte* p = (byte*)this;
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
HbString::GetData(byte** data)
{
    const byte* cdata;
    const size_t len = GetData(&cdata);
    *data = (byte*)cdata;
    return len;
}

size_t
HbString::Length() const
{
    size_t len = 0;
    const byte* p = (byte*)this;
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
HbString::Size() const
{
    size_t len = 0;
    const byte* p = (byte*)this;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    return len + (p - (byte*)this + 1);
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

bool
HbString::EQ(const HbString& that) const
{
    const size_t mysize = Size();
    return (that.Size() == mysize
            && (0 == mysize || 0 == memcmp(this, &that, mysize)));
}

bool
HbString::EQ(const byte* string, const size_t stringLen) const
{
    const byte* data;
    const size_t mylen = GetData(&data);
    return (stringLen == mylen
            && (0 == mylen || 0 == memcmp(data, string, mylen)));
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
    assert(len == 3);
    assert(0 == memcmp("Foo", data, strlen("Foo")));
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
    assert(hbs->Length() == len);
    assert(0 == memcmp(str, data, len));
    HbString::Destroy(hbs);
    delete [] str;

    len = 0x7FFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    assert(hbs->Length() == len);
    hbs->GetData(&data);
    assert(0 == memcmp(str, data, len));
    HbString::Destroy(hbs);
    delete [] str;

    len = 0x7FFFFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    assert(hbs->Length() == len);
    hbs->GetData(&data);
    assert(0 == memcmp(str, data, len));
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

