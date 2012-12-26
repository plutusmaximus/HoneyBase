#include "error.h"

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdarg.h>

namespace honeybase
{

void
Error::SetSucceeded(const char* fmt, ...)
{
    va_list args; 
    va_start(args, fmt);
    vsnprintf(m_Message, sizeof(m_Message)-1, fmt, args);
    va_end(args);

    m_Code = ERROR_NONE;
    m_Succeeded = true;
}

void
Error::SetFailed(const ErrorCode code, const char* fmt, ...)
{
    va_list args; 
    va_start(args, fmt);
    vsnprintf(m_Message, sizeof(m_Message)-1, fmt, args);
    va_end(args);

    m_Code = code;
    m_Succeeded = false;
}

}   //namespace honeybase