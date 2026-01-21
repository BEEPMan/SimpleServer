#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "8888"
#define BUFFER_LEN 512

using namespace std;

DWORD WINAPI IOCPWorkerThread(LPVOID lpParam);

enum OperationType
{
    OP_ACCEPT,
	OP_READ,
	OP_WRITE
};

struct IOCPData
{
    OVERLAPPED overlapped;
    OperationType operationType;
    SOCKET socket;
    char buffer[BUFFER_LEN];
};

int main()
{
	WSADATA wsaData;
	int iResult;

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	struct addrinfo* result = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, PORT, &hints, &result);
    if (iResult != 0)
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    int id;
    HANDLE hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    HANDLE port = CreateIoCompletionPort(socket, hPort, (ULONG_PTR)id, 0);

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("Listen failed with error: %ld\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("서버가 시작되었습니다. 클라이언트 연결을 기다립니다...\n");

    const int ioThreadCount = thread::hardware_concurrency() * 2;
    for (int i = 0; i < ioThreadCount; i++)
    {
        DWORD ThreadId;
        HANDLE hThread = CreateThread(NULL, 0, IOCPWorkerThread, hPort, 0, &ThreadId);
    }

    SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET)
    {
        printf("accept failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("클라이언트가 연결되었습니다.\n");



    char recvbuf[BUFFER_LEN];
    int recvbuflen = BUFFER_LEN;
    int iSendResult;

    do
    {
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
        {
            printf("수신된 바이트: %d\n", iResult);

            iSendResult = send(ClientSocket, recvbuf, iResult, 0);
            if (iSendResult == SOCKET_ERROR)
            {
                printf("send failed: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }
            printf("전송된 바이트: %d\n", iSendResult);
        }
        else if (iResult == 0)
            printf("연결 종료\n");
        else
        {
            printf("recv failed: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
            WSACleanup();
            return 1;
        }
    } while (iResult > 0);

    closesocket(ClientSocket);
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}

DWORD WINAPI IOCPWorkerThread(LPVOID lpParam)
{
    DWORD bytesTransferred;
    PULONG_PTR lpCompletionKey;
    IOCPData pOverlapped;
    int iResult, iSendResult;

    do
    {
        if (!GetQueuedCompletionStatus((HANDLE)lpParam, &bytesTransferred, lpCompletionKey, (LPOVERLAPPED*)&pOverlapped, 0))
        {
            continue;
        }
        iResult = recv(pOverlapped.socket, pOverlapped.buffer, bytesTransferred, 0);
        if (iResult > 0)
        {
            printf("수신된 바이트: %d\n", iResult);

            iSendResult = send(pOverlapped.socket, pOverlapped.buffer, iResult, 0);
            if (iSendResult == SOCKET_ERROR)
            {
                printf("send failed: %d\n", WSAGetLastError());
                closesocket(pOverlapped.socket);
                WSACleanup();
                return 1;
            }
            printf("전송된 바이트: %d\n", iSendResult);
        }
        else if (iResult == 0)
            printf("연결 종료\n");
        else
        {
            printf("recv failed: %d\n", WSAGetLastError());
            closesocket(pOverlapped.socket);
            WSACleanup();
            return 1;
        }
    } while (true);
}