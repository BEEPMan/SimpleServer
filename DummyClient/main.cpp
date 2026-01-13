#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT "8888"
#define BUFFER_LEN 512
#define SERVER_ADDR "211.228.94.192"

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

    iResult = getaddrinfo(SERVER_ADDR, PORT, &hints, &result);
    if (iResult != 0)
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    SOCKET ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET)
    {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);
    if (iResult == SOCKET_ERROR)
    {
        printf("connect failed: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    printf("서버에 연결되었습니다. 메시지를 입력하세요. (exit 입력 시 종료)\n");

    char sendbuf[BUFFER_LEN];
    char recvbuf[BUFFER_LEN];

    while (true)
    {
        printf(">");
        if (!fgets(sendbuf, BUFFER_LEN, stdin))
            break;

        size_t len = strlen(sendbuf);
        if (len > 0 && sendbuf[len - 1] == '\n')
        {
            sendbuf[len - 1] = '\0';
            len--;
        }

        if (strcmp(sendbuf, "exit") == 0)
            break;

        if (len == 0)
            continue;

        int totalSent = 0;
        while (totalSent < (int)len)
        {
            int sent = send(ConnectSocket, sendbuf + totalSent, (int)len - totalSent, 0);
            if (sent == SOCKET_ERROR)
            {
                printf("send failed: %d\n", WSAGetLastError());
                closesocket(ConnectSocket);
                WSACleanup();
                return 1;
            }
            totalSent += sent;
        }

        int totalRecv = 0;
        while (totalRecv < (int)len)
        {
            int received = recv(ConnectSocket, recvbuf + totalRecv, (int)len - totalRecv, 0);
            if (received > 0)
            {
                totalRecv += received;
            }
            else if (received == 0)
            {
                printf("서버가 연결을 종료했습니다.\n");
                closesocket(ConnectSocket);
                WSACleanup();
                return 0;
            }
            else
            {
                printf("recv failed: %d\n", WSAGetLastError());
                closesocket(ConnectSocket);
                WSACleanup();
                return 1;
            }
        }

        recvbuf[totalRecv] = '\0';
        printf("에코 수신: %s (bytes=%d)\n", recvbuf, totalRecv);
    }

    shutdown(ConnectSocket, SD_SEND);
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}