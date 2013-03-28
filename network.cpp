#include "network.h"

#include "command.h"
#include "dict.h"
#include "error.h"

#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <WinSock2.h>
#include <mswsock.h>
#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "mswsock.lib")

namespace honeybase
{

static HANDLE s_hIOCP = NULL;
static SOCKET s_Listener = INVALID_SOCKET;
static LPFN_ACCEPTEX lpfnAcceptEx = NULL;

class Client
{
public:

    Client()
        : m_State(STATE_ACCEPT)
        , m_Skt(INVALID_SOCKET)
        , m_BufLen(0)
    {
    }

    bool Open()
    {
        if(!hbverify(INVALID_SOCKET == m_Skt))
        {
            return false;
        }

        m_Skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(INVALID_SOCKET == m_Skt)
        {
            return false;
        }

        DWORD bytesRecvd;
        memset(&m_Overlapped, 0, sizeof(m_Overlapped));

        if(!lpfnAcceptEx(s_Listener,
                        m_Skt,
                        m_LocalAndRemoteAddr,
                        0,
                        sizeof(sockaddr_in) + 16,
                        sizeof(sockaddr_in) + 16,
                        &bytesRecvd,
                        &m_Overlapped))
        {
            const int err = WSAGetLastError();
            if(ERROR_IO_PENDING != err)
            {
                return false;
            }
        }

        if(NULL == CreateIoCompletionPort((HANDLE)m_Skt,
                                            s_hIOCP,
                                            (ULONG_PTR)0,
                                            0))
        {
            const DWORD dwErr = GetLastError();
            if(ERROR_IO_PENDING != dwErr)
            {
                return false;
            }
        }

        return true;
    }

    void Close()
    {
        m_Cmd.Reset();
        if(INVALID_SOCKET != m_Skt)
        {
            int err = closesocket(m_Skt);
            m_Skt = INVALID_SOCKET;
        }

        m_State = STATE_ACCEPT;
        m_BufLen = 0;
    }

    bool Reopen()
    {
        Close();
        return Open();
    }

    enum State
    {
        STATE_ACCEPT,
        STATE_RECV
    };

    State m_State;
    SOCKET m_Skt;
    OVERLAPPED m_Overlapped;
    Command m_Cmd;
    byte m_LocalAndRemoteAddr[2*(sizeof(sockaddr_in)+16)];
    size_t m_BufLen;
    byte m_ReadBuf[256];
};

static const int MAX_CLIENTS    = 2;
static Client s_Clients[MAX_CLIENTS];

bool
Network::Startup(const unsigned listenPort)
{
    if(NULL == s_hIOCP)
    {
        WSADATA wsaData;

        int err = WSAStartup(MAKEWORD(2, 2), &wsaData);

        s_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                        NULL,
                                        (ULONG_PTR)0,
                                        0);

        if(NULL == s_hIOCP)
        {
            Shutdown();
        }

        s_Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(INVALID_SOCKET == s_Listener)
        {
            Shutdown();
            return false;
        }

        sockaddr_in listenAddr;
        listenAddr.sin_family = AF_INET;
        listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        listenAddr.sin_port = htons(listenPort);
        err = bind(s_Listener, (sockaddr*)&listenAddr, sizeof(listenAddr));
        err = listen(s_Listener, 200);

        if(NULL == CreateIoCompletionPort((HANDLE)s_Listener,
                                           s_hIOCP,
                                           (ULONG_PTR)0,
                                           0))
        {
            DWORD dwErr = GetLastError();
            if(ERROR_IO_PENDING != dwErr)
            {
                Shutdown();
                return false;
            }
        }

        GUID GuidAcceptEx = WSAID_ACCEPTEX;
        DWORD dwBytes;
        
        err = WSAIoctl(s_Listener, SIO_GET_EXTENSION_FUNCTION_POINTER,
                        &GuidAcceptEx, sizeof (GuidAcceptEx),
                        &lpfnAcceptEx, sizeof (lpfnAcceptEx),
                        &dwBytes, NULL, NULL);

        if(SOCKET_ERROR == err)
        {
            err = WSAGetLastError();
            Shutdown();
            return false;
        }

        for(int i = 0; i < MAX_CLIENTS; ++i)
        {
            if(!s_Clients[i].Open())
            {
                Shutdown();
                return false;
            }
        }

        HashTable* dict = HashTable::Create();

        /*{
            Key key;
            Value value;
            key.m_Blob = Blob::Create((byte*)"foo", strlen("foo"));
            value.m_Blob = Blob::Create((byte*)"bar", strlen("bar"));
            dict->Set(key, KEYTYPE_BLOB, value, VALUETYPE_BLOB);
            Blob::Destroy(key.m_Blob);
            Blob::Destroy(value.m_Blob);
        }*/

        while(true)
        {
            ULONG_PTR completionKey;
            DWORD bytesTransferred;
            OVERLAPPED* poverlapped;
            BOOL completed =
                GetQueuedCompletionStatus(s_hIOCP, &bytesTransferred, &completionKey, &poverlapped, INFINITE);

            Client* client = (Client*)((u8*)poverlapped - offsetof(Client, m_Overlapped));

            switch(client->m_State)
            {
            case Client::STATE_ACCEPT:
                if(completed)
                {
                    client->m_State = Client::STATE_RECV;
                }
                else
                {
                    client->Reopen();
                    continue;
                }
                break;
            case Client::STATE_RECV:
                if(!completed || (completed && 0 == bytesTransferred))
                {
                    client->Reopen();
                    continue;
                }

                Error err;
                DWORD numBytesSent;
                WSABUF wsaBufs[3];
                unsigned bufCount;
                client->m_BufLen += bytesTransferred;
                const size_t bytesConsumed = client->m_Cmd.Parse(client->m_ReadBuf, bytesTransferred, &err);
                if(0 == bytesConsumed)
                {
                    wsaBufs[0].buf = "-ERR ";
                    wsaBufs[0].len = sizeof("-ERR ")-1;
                    wsaBufs[1].buf = (char*)err.GetText();
                    wsaBufs[1].len = strlen(wsaBufs[1].buf);
                    wsaBufs[2].buf = "\r\n";
                    wsaBufs[2].len = 2;
                    bufCount = 3;

                    WSASend(client->m_Skt, wsaBufs, bufCount, &numBytesSent, 0, NULL, NULL);

                    client->Reopen();
                    continue;
                }
                else if(client->m_Cmd.IsComplete())
                {
                    const CommandExecResult result =
                        client->m_Cmd.Exec(dict, &err);
                    if(EXECRESULT_ERROR == result)
                    {
                        wsaBufs[0].buf = "-ERR";
                        wsaBufs[0].len = sizeof("-ERR")-1;
                        wsaBufs[1].buf = (char*)err.GetText();
                        wsaBufs[1].len = strlen(wsaBufs[1].buf);
                        wsaBufs[2].buf = "\r\n";
                        wsaBufs[2].len = 2;
                        bufCount = 3;
                    }
                    else if(EXECRESULT_OK == result)
                    {
                        wsaBufs[0].buf = "+OK\r\n";
                        wsaBufs[0].len = sizeof("+OK\r\n")-1;
                        bufCount = 1;
                    }
                    else if(EXECRESULT_BULK == result)
                    {
                        char lenBuf[64];
                        char valBuf[64];
                        const byte* blobData;
                        if(1 == client->m_Cmd.m_ResultC)
                        {
                            switch(client->m_Cmd.m_ResultT[0])
                            {
                            case VALUETYPE_INT:
                                sprintf(valBuf, "%"PRId64, client->m_Cmd.m_ResultV[0].m_Int);
                                wsaBufs[1].buf = valBuf;
                                wsaBufs[1].len = strlen(valBuf);
                                break;
                            case VALUETYPE_DOUBLE:
                                sprintf(valBuf, "%f", client->m_Cmd.m_ResultV[0].m_Double);
                                wsaBufs[1].buf = valBuf;
                                wsaBufs[1].len = strlen(valBuf);
                                break;
                            case VALUETYPE_BLOB:
                                wsaBufs[1].len = client->m_Cmd.m_ResultV[0].m_Blob->GetData(&blobData);
                                wsaBufs[1].buf = (char*)blobData;
                                break;
                            }
                            sprintf(lenBuf, "$%d\r\n", wsaBufs[1].len);
                            wsaBufs[0].buf = lenBuf;
                            wsaBufs[0].len = strlen(lenBuf);
                            wsaBufs[2].buf = "\r\n";
                            wsaBufs[2].len = 2;
                            bufCount = 3;
                        }
                        else
                        {
                            wsaBufs[0].buf = "$-1\r\n";
                            wsaBufs[0].len = strlen(wsaBufs[0].buf);
                            bufCount = 1;
                        }
                    }

                    WSASend(client->m_Skt, wsaBufs, bufCount, &numBytesSent, 0, NULL, NULL);

                    client->m_Cmd.Reset();
                }
                client->m_BufLen -= bytesConsumed;
                if(client->m_BufLen > 0)
                {
                    memmove(client->m_ReadBuf, &client->m_ReadBuf[bytesConsumed], client->m_BufLen);
                }
                break;
            }

            WSABUF wsaBuf;
            wsaBuf.buf = (char*)&client->m_ReadBuf[client->m_BufLen];
            wsaBuf.len = sizeof(client->m_ReadBuf) - client->m_BufLen;

            DWORD flags = 0;

            err = WSARecv(client->m_Skt, &wsaBuf, 1, NULL, &flags, &client->m_Overlapped, NULL);
        }
    }

    return true;
}

void
Network::Shutdown()
{
    if(NULL != s_hIOCP)
    {
        CloseHandle(s_hIOCP);
        s_hIOCP = NULL;
    }

    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
        if(INVALID_SOCKET != s_Clients[i].m_Skt)
        {
            closesocket(s_Clients[i].m_Skt);
            s_Clients[i].m_Skt = INVALID_SOCKET;
        }
    }

    if(INVALID_SOCKET != s_Listener)
    {
        closesocket(s_Listener);
        s_Listener = INVALID_SOCKET;
    }
}

}   //namespace honeybase
