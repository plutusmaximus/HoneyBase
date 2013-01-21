#ifndef __HB_COMMAND_H__
#define __HB_COMMAND_H__

#include "hb.h"

namespace honeybase
{

class Error;
class HashTable;

enum CommandExecResult
{
    EXECRESULT_OK,
    EXECRESULT_ERROR,
    EXECRESULT_INTEGER,
    EXECRESULT_BULK,
    EXECRESULT_MULTIBULK
};

class Command
{
public:
    enum State
    {
        STATE_CR,
        STATE_LF,
        STATE_ASTERISK,
        STATE_NUM_ARGS,
        STATE_DOLLAR,
        STATE_ARG_LEN,
        STATE_ARG,
        STATE_COMPLETE
    };

    Command();

    ~Command();

    void Reset();

    size_t Parse(const u8* cmdStr,
                const size_t len,
                Error* err);

    bool IsComplete() const
    {
        return STATE_COMPLETE == m_State;
    }

    CommandExecResult Exec(HashTable* dict, Error* err);

    State m_State;
    State m_NextState;
    int m_ArgC;
    int m_CurArg;
    int m_ArgLen;
    int m_ArgOffset;
    Blob** m_ArgV;
    Blob* m_CurBlob;
    byte* m_BlobData;

    int m_ResultC;
    Value* m_ResultV;
    ValueType* m_ResultT;

    Value m_SingleResultValue;
    ValueType m_SingleResultType;
};

}   //namespace honeybase

#endif  //__HB_COMMAND_H__