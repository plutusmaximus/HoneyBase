#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>

void TestMemMappedFile()
{
    int error;

    HANDLE hFile =
        CreateFile("FooFoo.dat",
                    //FILE_APPEND_DATA,
                    GENERIC_READ|GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
                    NULL);

    if(INVALID_HANDLE_VALUE == hFile)
    {
        error = GetLastError();
        return;
    }
    LARGE_INTEGER oldOffset;
    LARGE_INTEGER endOffset;
    LARGE_INTEGER newFp;

    endOffset.QuadPart = 0;

    bool success = false;

    if(SetFilePointerEx(hFile, endOffset, &oldOffset, FILE_CURRENT))
    {
        endOffset.QuadPart = 1L << 10;
        if(SetFilePointerEx(hFile, endOffset, &newFp, FILE_BEGIN)
            && SetEndOfFile(hFile))
        {
            success = true;
        }
        else
        {
            error = GetLastError();
        }
    }
    else
    {
        error = GetLastError();
    }

    if(success)
    {
        HANDLE hFileMapping =
            CreateFileMapping(hFile,
                                NULL,
                                PAGE_READWRITE,
                                0,
                                0,
                                NULL);

        void* mem = MapViewOfFile(hFileMapping,
                                    FILE_MAP_ALL_ACCESS,
                                    0,
                                    0,
                                    0);

        strcpy((char*)mem, "Hola dude");

        CloseHandle(hFileMapping);
    }

    CloseHandle(hFile);
}