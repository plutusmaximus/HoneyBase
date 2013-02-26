#include "hb.h"

#include <malloc.h>
#include <new.h>
#include <stdarg.h>
#include <stdio.h>

#if _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace honeybase
{

//Make sure our double and int are the same size.
hb_static_assert(sizeof(((Value*)0)->m_Double) == sizeof(((Value*)0)->m_Int));

static unsigned m_w = 77665;    /* must not be zero */
static unsigned m_z = 22559;    /* must not be zero */

unsigned Rand()
{
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;  /* 32-bit result */
}

unsigned Rand(const unsigned min, const unsigned max)
{
    return min + (Rand() % (max - min));
}

static const char BASE64_ALPHABET[] =
{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
};

size_t Base64Encode(const byte* bytes, const size_t numBytes, char* buf, const size_t bufSize)
{
    byte val;
    size_t strLen = 0;
    const size_t numBits = numBytes * 8;
    for(size_t i = 0; i < numBits && strLen < bufSize-1; i += 6)
    {
        const size_t byteIdx = i >> 3;
        switch(i & 0x07)
        {
            case 0:
                val = (bytes[byteIdx]>>2) & 63;
                break;
            case 1:
                val = (bytes[byteIdx]>>1) & 63;
                break;
            case 2:
                val = (bytes[byteIdx]>>0) & 63;
                break;
            case 3:
                val = (bytes[byteIdx]<<1);
                if(byteIdx+1 < numBytes)
                {
                    val |= (bytes[byteIdx+1]>>7);
                }
                val &= 63;
                break;
            case 4:
                val = (bytes[byteIdx]<<2);
                if(byteIdx+1 < numBytes)
                {
                    val |= (bytes[byteIdx+1]>>6);
                }
                val &= 63;
                break;
            case 5:
                val = (bytes[byteIdx]<<3);
                if(byteIdx+1 < numBytes)
                {
                    val |= (bytes[byteIdx+1]>>5);
                }
                val &= 63;
                break;
            case 6:
                val = (bytes[byteIdx]<<4);
                if(byteIdx+1 < numBytes)
                {
                    val |= (bytes[byteIdx+1]>>4);
                }
                val &= 63;
                break;
            case 7:
                val = (bytes[byteIdx]<<5);
                if(byteIdx+1 < numBytes)
                {
                    val |= (bytes[byteIdx+1]>>3);
                }
                val &= 63;
                break;
        }
        buf[strLen++] = BASE64_ALPHABET[val];
    }

    buf[strLen] = '\0';
    return strLen;
}

///////////////////////////////////////////////////////////////////////////////
//  Log
///////////////////////////////////////////////////////////////////////////////
Log::Log()
{
    m_Channel[0] = '\0';
}

Log::Log(const char* channel)
{
    snprintf(m_Channel, sizeof(m_Channel)-1, "%s", channel);
}

void
Log::Debug(const char* fmt, ...)
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
Log::Error(const char* fmt, ...)
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
//  Heap
///////////////////////////////////////////////////////////////////////////////

//static byte* s_Heap = (byte*)malloc(1024*1024*1024);
//static byte* s_Heap = (byte*)VirtualAlloc(NULL, 1024*1024*1024, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
//static size_t s_NextAlloc = 0;

/*static int InitHeap()
{
    memset(s_Heap, 0, 1024*1024*1024);
    return 1;
}*/

//static const int FOO = InitHeap();

void*
Heap::ZAlloc(size_t size)
{
    void* p = malloc(size);
    /*void* p = &s_Heap[s_NextAlloc];
    s_NextAlloc += size;*/
    if(p)
    {
        memset(p, 0, size);
    }

    return p;
}
void*
Heap::Alloc(size_t size)
{
    /*void* p = &s_Heap[s_NextAlloc];
    s_NextAlloc += size;
    return p;*/

    return malloc(size);
}

void
Heap::Free(void* mem)
{
	free(mem);
}

///////////////////////////////////////////////////////////////////////////////
//  Blob
///////////////////////////////////////////////////////////////////////////////
static size_t s_NumBlobs = 0;

StopWatch Blob::sm_StopWatch;

Blob*
Blob::Create(const size_t len)
{
    return Create(NULL, len);
}

Blob*
Blob::Create(const byte* bytes, const size_t len)
{
    const size_t size = Size(len);
    Blob* blob = (Blob*) Heap::Alloc(size);
    if(blob)
    {
        new(blob) Blob();
        Init(blob, bytes, len);
        ++s_NumBlobs;
    }

    return blob;
}

Blob*
Blob::Dup(const Blob* blob)
{
    Blob* newBlob = (Blob*) Heap::Alloc(Blob::Size(blob->Length()));
    if(newBlob)
    {
        new(newBlob) Blob();
        const byte* bytes;
        const size_t len = blob->GetData(&bytes);
        Init(newBlob, bytes, len);
    }

    return newBlob;
}

void
Blob::Destroy(Blob* blob)
{
    blob->~Blob();
    Heap::Free(blob);
    --s_NumBlobs;
}

/*Blob*
Blob::Encode(const byte* src, const size_t srcLen,
                byte* dst, const size_t dstSize)
{
    const size_t size = Size(srcLen);
    if(hbverify(size <= dstSize))
    {
        Init((Blob*)dst, src, srcLen);
        return (Blob*)dst;
    }

    return NULL;
}*/

size_t
Blob::Size(const size_t len)
{
    size_t size = len;
    size_t tmpLen = len;

    for(++size, tmpLen >>= 7; tmpLen; ++size, tmpLen >>= 7)
    {
    }

    return size + sizeof(Blob) - 1;
}

size_t
Blob::GetData(const byte** data) const
{
    size_t len = 0;
    const byte* p = bytes;
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
Blob::GetData(byte** data)
{
    return GetData((const byte**) data);
}

size_t
Blob::CopyData(byte* dst, const size_t dstSize) const
{
    const byte* data;
    const size_t len = GetData(&data);
    size_t cpyLen = (len > dstSize ? dstSize : len);
    memcpy(dst, data, cpyLen);
    return len;
}

size_t
Blob::Length() const
{
    size_t len = 0;
    const byte* p = bytes;
    int shift = 0;

    while(*p & 0x80)
    {
        len |= (*p++ & 0x7F) << shift;
        shift += 7;
    }

    len |= (*p & 0x7F) << shift;

    return len;
}

Blob*
Blob::Dup() const
{
    return Dup(this);
}

Blob*
Blob::Ref() const
{
    hbassert(m_RefCount < 127);
    ++m_RefCount;
    return const_cast<Blob*>(this);
}

void
Blob::Unref()
{
    hbassert(m_RefCount > 0);
    --m_RefCount;
    if(0 == m_RefCount)
    {
        Destroy(this);
    }
}

int
Blob::NumRefs() const
{
    return m_RefCount;
}

int
Blob::Compare(const byte* thatData, const size_t thatLen) const
{
    //DO NOT SUBMIT
    //sm_StopWatch.Start();

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

    //DO NOT SUBMIT
    //sm_StopWatch.Stop();
}

size_t
Blob::GlobalBlobCount()
{
    return s_NumBlobs;
}

//protected:

void
Blob::Init(Blob* blob, const byte* bytes, const size_t len)
{
    byte* p = blob->bytes;

    size_t tmpLen = len;
    while(tmpLen > 0x7F)
    {
        *p++ = 0x80 | (tmpLen & 0x7F);
        tmpLen >>= 7;
    }

    *p++ = tmpLen & 0x7F;

    if(bytes)
    {
        memcpy(p, bytes, len);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  BlobTest
///////////////////////////////////////////////////////////////////////////////
void
BlobTest::Test()
{
    Blob* hbs = Blob::Create((const byte*)"Foo", strlen("Foo"));
    const byte* data;
    size_t len = hbs->GetData(&data);
    hbassert(len == 3);
    hbassert(0 == memcmp("Foo", data, strlen("Foo")));
    Blob::Destroy(hbs);

    byte* str;

    len = 0x7F + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = Blob::Create(str, len);
    hbs->GetData(&data);
    hbassert(hbs->Length() == len);
    hbassert(0 == memcmp(str, data, len));
    Blob::Destroy(hbs);
    delete [] str;

    len = 0x7FFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = Blob::Create(str, len);
    hbassert(hbs->Length() == len);
    hbs->GetData(&data);
    hbassert(0 == memcmp(str, data, len));
    Blob::Destroy(hbs);
    delete [] str;

    len = 0x7FFFFF + 321;
    str = new byte[len];
    for(size_t i = 0; i < len; ++i)
    {
        str[i] = (byte)i;
    }

    hbs = Blob::Create(str, len);
    hbassert(hbs->Length() == len);
    hbs->GetData(&data);
    hbassert(0 == memcmp(str, data, len));
    Blob::Destroy(hbs);
    delete [] str;
}

///////////////////////////////////////////////////////////////////////////////
//  StopWatch
///////////////////////////////////////////////////////////////////////////////
StopWatch::StopWatch()
: m_Start(0)
, m_Elapsed(0)
, m_Running(false)
{
#if _MSC_VER
    QueryPerformanceFrequency((LARGE_INTEGER*)&m_Freq);
#elif defined(__GNUC__)
    m_Freq = 1000000000;
#endif
}

void
StopWatch::Clear()
{
    m_Start = m_Elapsed = 0;
    m_Running = false;
}

void
StopWatch::Start()
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
StopWatch::Restart()
{
    m_Elapsed = 0;
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
StopWatch::Stop()
{
    if(m_Running)
    {
        u64 stop;
#if _MSC_VER
        QueryPerformanceCounter((LARGE_INTEGER*)&stop);
#elif defined(__GNUC__)
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        stop = (ts.tv_sec*m_Freq) + ts.tv_nsec;
#endif
        m_Elapsed += stop - m_Start;
        m_Running = false;
    }
}

double
StopWatch::GetElapsed() const
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
        return ((double)(m_Elapsed + stop - m_Start)) / m_Freq;
    }
    else
    {
        return ((double)m_Elapsed) / m_Freq;
    }
}

}   //namespace honeybase