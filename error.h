#ifndef __HB_ERROR__
#define __HB_ERROR__

namespace honeybase
{

enum ErrorCode
{
    ERROR_NONE,
    ERROR_OUT_OF_MEMORY,
    ERROR_UNRECOGNIZED_COMMAND,
    ERROR_UNEXPECTED_TOKEN,
    ERROR_UNKNOWN
};

class Error
{
public:

    void SetSucceeded();

    void SetSucceeded(const char* fmt, ...);

    void SetFailed(const ErrorCode code, const char* fmt, ...);

    bool Succeeded() const;

    ErrorCode GetCode() const;

    const char* GetText() const;

private:

    ErrorCode m_Code;
    char m_Message[256];
    bool m_Succeeded;
};

}   //honeybase

#endif  //__HB_ERROR__