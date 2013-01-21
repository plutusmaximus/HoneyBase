#include "command.h"

#include "dict.h"
#include "error.h"

#include <string.h>

namespace honeybase
{

Command::Command()
: m_State(STATE_ASTERISK)
, m_NextState(STATE_ASTERISK)
, m_ArgC(0)
, m_CurArg(0)
, m_ArgLen(0)
, m_ArgOffset(0)
, m_ArgV(NULL)
, m_CurBlob(NULL)
, m_BlobData(NULL)
, m_ResultC(0)
, m_ResultV(NULL)
, m_ResultT(NULL)
{
}

Command::~Command()
{
    Reset();
}

void
Command::Reset()
{
    if(m_ArgV)
    {
        for(int i = 0; i < m_CurArg; ++i)
        {
            Blob::Destroy(m_ArgV[i]);
        }

        delete [] m_ArgV;
    }

    if(m_ResultV && &m_SingleResultValue != m_ResultV)
    {
        for(int i = 0; i < m_ResultC; ++i)
        {
            if(VALUETYPE_BLOB == m_ResultT[i])
            {
                Blob::Destroy(m_ResultV[i].m_Blob);
            }
        }

        delete [] m_ResultV;
        delete [] m_ResultT;
    }

    m_State = STATE_ASTERISK;
    m_NextState = STATE_ASTERISK;
    m_ArgC = 0;
    m_CurArg = 0;
    m_ArgLen = 0;
    m_ArgOffset = 0;
    m_ArgV = NULL;
    m_CurBlob = NULL;
    m_BlobData = NULL;

    m_ResultC = 0;
    m_ResultV = NULL;
    m_ResultT = NULL;
}

size_t
Command::Parse(const u8* cmdStr, const size_t len, Error* err)
{
    const u8* p = cmdStr;
    const u8* end = cmdStr + len;
    err->SetSucceeded();

    while(p < end)
    {
        switch(m_State)
        {
        case STATE_CR:
            if('\r' == *p)
            {
                ++p;
                if(p < end)
                {
                    if('\n' == *p)
                    {
                        m_State = m_NextState;
                        ++p;
                    }
                    else
                    {
                        err->SetFailed(ERROR_UNEXPECTED_TOKEN, "Expected '\\n'");
                        return 0;
                    }
                }
                else
                {
                    m_State = STATE_LF;
                }
            }
            else
            {
                err->SetFailed(ERROR_UNEXPECTED_TOKEN, "Expected '\\r'");
                return 0;
            }
            break;
        case STATE_LF:
            if('\n' == *p)
            {
                m_State = m_NextState;
                ++p;
            }
            else
            {
                err->SetFailed(ERROR_UNEXPECTED_TOKEN, "Expected '\\n'");
                return 0;
            }
            break;
        case STATE_ASTERISK:
            if('*' == *p)
            {
                m_ArgC = 0;
                m_State = STATE_NUM_ARGS;
                ++p;
            }
            else
            {
                err->SetFailed(ERROR_UNEXPECTED_TOKEN, "Expected '*'");
                return 0;
            }
            break;

        case STATE_NUM_ARGS:
            for(; p < end; ++p)
            {
                const int n = *p - '0';
                if(n < 0 || n > 9)
                {
                    m_State = STATE_LF;
                    m_NextState = STATE_DOLLAR;
                    m_CurArg = 0;
                    m_ArgV = new Blob*[m_ArgC];
                    ++p;
                    break;
                }
                else
                {
                    m_ArgC = (m_ArgC * 10) + n;
                }
            }
            break;

        case STATE_DOLLAR:
            if('$' == *p)
            {
                m_ArgLen = 0;
                m_State = STATE_ARG_LEN;
                ++p;
            }
            else if('\r' == *p)
            {
                m_State = STATE_LF;
                m_NextState = STATE_ARG_LEN;
                ++p;
            }
            else
            {
                err->SetFailed(ERROR_UNEXPECTED_TOKEN, "Expected '$'");
                return 0;
            }
            break;

        case STATE_ARG_LEN:
            for(; p < end; ++p)
            {
                const int n = *p - '0';
                if(n < 0 || n > 9)
                {
                    m_State = STATE_LF;
                    m_NextState = STATE_ARG;
                    m_ArgOffset = 0;
                    m_ArgV[m_CurArg] = m_CurBlob = Blob::Create(m_ArgLen);
                    m_CurBlob->GetData(&m_BlobData);
                    ++p;
                    break;
                }
                else
                {
                    m_ArgLen = (m_ArgLen * 10) + n;
                }
            }
            break;

        case STATE_ARG:
            {
                size_t len;
                if(m_ArgLen - m_ArgOffset < end - p)
                {
                    len = m_ArgLen - m_ArgOffset;
                }
                else
                {
                    len = end - p;
                }

                memcpy(&m_BlobData[m_ArgOffset], p, len);
                p += len;
                m_ArgOffset += len;
            }

            if(m_ArgOffset == m_ArgLen)
            {
                ++m_CurArg;
                if(m_CurArg < m_ArgC)
                {
                    m_ArgOffset = m_ArgLen = 0;
                    m_State = STATE_CR;
                    m_NextState = STATE_DOLLAR;
                }
                else
                {
                    m_State = STATE_CR;
                    m_NextState = STATE_COMPLETE;
                }
            }

            break;

        case STATE_COMPLETE:
            return p - cmdStr;
            break;
        }
    }

    return p - cmdStr;
}

CommandExecResult
Command::Exec(HashTable* dict, Error* err)
{
    if(STATE_COMPLETE == m_State)
    {
        const Blob* blob = m_ArgV[0];
        if(!blob->Compare((const byte*)"set", sizeof("set")-1))
        {
            if(3 == m_ArgC)
            {
                Key key; Value value;
                key.m_Blob = m_ArgV[1];
                value.m_Blob = m_ArgV[2];
                dict->Set(key, KEYTYPE_BLOB, value, VALUETYPE_BLOB);
                m_ResultV = &m_SingleResultValue;
                m_ResultT = &m_SingleResultType;
                m_ResultC = 1;
                m_ResultV[0].m_Int = 1;
                m_ResultT[0] = VALUETYPE_INT;
                err->SetSucceeded();
                return EXECRESULT_OK;
            }
        }
        else if(!blob->Compare((const byte*)"get", sizeof("get")-1))
        {
            if(m_ArgC == 2)
            {
                Key key;
                key.m_Blob = m_ArgV[1];
                if(dict->Find(key, KEYTYPE_BLOB, &m_SingleResultValue, &m_SingleResultType))
                {
                    m_ResultV = &m_SingleResultValue;
                    m_ResultT = &m_SingleResultType;
                    m_ResultC = 1;
                }
                else
                {
                    m_ResultC = 0;
                }

                err->SetSucceeded();

                return EXECRESULT_BULK;
            }
        }
        else
        {
            u8 command[128];
            const u8* tmp;
            size_t len = blob->GetData(&tmp);
            if(len >= sizeof(command)){len = sizeof(command-1);}
            memcpy(command, tmp, len);
            command[len] = '\0';
            err->SetFailed(ERROR_UNRECOGNIZED_COMMAND, "%s", command);
        }
    }

    return EXECRESULT_ERROR;
}

}   //namespace honeybase