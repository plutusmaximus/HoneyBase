#define _CRT_SECURE_NO_WARNINGS

#include "hb.h"

#include <assert.h>
#include <malloc.h>

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

///////////////////////////////////////////////////////////////////////////////
//  HbHeap
///////////////////////////////////////////////////////////////////////////////
void*
HbHeap::Alloc(size_t size)
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
HbString::Create(const byte* string, const size_t stringLen)
{
    const size_t size = Size(stringLen);
    HbString* hbs = (HbString*) HbHeap::Alloc(size);
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

        memcpy(&bytes[offset], string, stringLen);
    }

    return hbs;
}

void
HbString::Destroy(HbString* hbs)
{
    hbs->~HbString();
    HbHeap::Free(hbs);
}

const byte*
HbString::Data() const
{
    const byte* p = (byte*)this;
    for(; *p & 0x80; ++p)
    {
    }

    return ++p;
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

    while(tmpLen)
    {
        tmpLen >>= 7;
        ++size;
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
    const size_t mylen = Length();
    return (stringLen == mylen
            && (0 == mylen || 0 == memcmp(this->Data(), string, mylen)));
}

///////////////////////////////////////////////////////////////////////////////
//  HbStringTest
///////////////////////////////////////////////////////////////////////////////
void
HbStringTest::Test()
{
    HbString* hbs = HbString::Create((const byte*)"Foo", strlen("Foo"));
    assert(hbs->Length() == 3);
    assert(0 == memcmp("Foo", hbs->Data(), strlen("Foo")));
    HbString::Destroy(hbs);

    size_t len;
    byte* str;

    len = 0x7F + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = HbString::Create(str, len);
    assert(hbs->Length() == len);
    assert(0 == memcmp(str, hbs->Data(), len));
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
    assert(0 == memcmp(str, hbs->Data(), len));
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
    assert(0 == memcmp(str, hbs->Data(), len));
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

