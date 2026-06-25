#define WIN32_LEAN_AND_MEAN
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "27015"

struct Endpoint
{
	char path[128];
	char* (*func)(); 
};

// ENDPOINTS

char* test()
{
	char* data;
	data = malloc(128);
	int random_num = rand();
    sprintf(data, "Random Number: %d\n", random_num);
	return data;
}

char* snek()
{	
	char path[] = "index.html";
	FILE *file = fopen(path, "r");
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* filebuf = malloc(file_size + 1);
	size_t bytes_read = fread(filebuf, 1, file_size, file);
	filebuf[bytes_read] = '\0';
	fclose(file);
	
	return filebuf;
}

// ENDPOINTS

struct Endpoint endpoints[] = { {"/test", test}, {"/snek", snek} };

const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
    if (af == AF_INET)
    {
        struct sockaddr_in in;
        memset(&in, 0, sizeof(in));
        in.sin_family = AF_INET;
        memcpy(&in.sin_addr, src, sizeof(struct in_addr));
        getnameinfo((struct sockaddr *)&in, sizeof(struct
            sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
        return dst;
    }
    else if (af == AF_INET6)
    {
        struct sockaddr_in6 in;
        memset(&in, 0, sizeof(in));
        in.sin6_family = AF_INET6;
        memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
        getnameinfo((struct sockaddr *)&in, sizeof(struct
            sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
        return dst;
    }
    return NULL;
}

char response_str[] = "HTTP/1.1 %d %s\r\n" \
"Server: yarpopcat08 is gae\r\n" \
"Content-Type: text/html; charset=UTF-8\r\n" \
"Content-Length: %d\r\n"
"\r\n" \
"%s\0";

char response_err[] = "<head>" \
"<h1 style='color: red;'>Wrong Endpoint</h1>" \
"</head>";

int main(void) 
{	
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // No longer need server socket
    // closesocket(ListenSocket);

    // Receive until the peer shuts down the connection
    do {
		struct sockaddr_in clientAddr = {0};
		int clientAddrSize = sizeof(clientAddr);
		
		// Accept a client socket
		ClientSocket = accept(ListenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
		
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
		printf("%s connected\n", ipStr);

		do {
			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				printf("Bytes received: %d\n", iResult);
				// printf("Content: %s\n", recvbuf);
				
				const char delims1[] = "\r\n";
				const char delims2[] = " ";
				char *token1;
				char *token2;
				
				char type[128];
				char path[128];

				// Get the first token
				token1 = strtok(recvbuf, delims1);

				// Walk through the remaining tokens
				if (token1 != NULL) {
					token2 = strtok(token1, delims2);
					strcpy(type, token2);
					token2 = strtok(NULL, delims2);
					strcpy(path, token2);
					
					printf("%s\n", type);
					printf("%s\n", path);
				}
				
				if (iResult < DEFAULT_BUFLEN) {
					struct Endpoint* ep = NULL;
					
					for (int i = 0; i < sizeof(endpoints) / sizeof(struct Endpoint); i++)
					{
						if (strcmp(endpoints[i].path, path) == 0)
						{
							ep = &endpoints[i];
							break;
						}
					}
					
					// FILE *file = fopen(path+1, "r");
					char* response;
					
					if (ep == NULL) {
						printf("endpoint not found\n");
						response = malloc(DEFAULT_BUFLEN + strlen(response_err));
						sprintf(response, response_str, 404, "Not Found", strlen(response_err), response_err);
					}
					else {
						char* data = ep->func();
						response = malloc(DEFAULT_BUFLEN + strlen(data));
						sprintf(response, response_str, 200, "OK", strlen(data), data);
						free(data);
					}

					// if (file == NULL) {
						// printf("error opening file\n");
						// response = malloc(DEFAULT_BUFLEN + strlen(response_err));
						// sprintf(response, response_str, 404, "Not Found", strlen(response_err), response_err);
					// }
					// else {
						// fseek(file, 0, SEEK_END);
						// long file_size = ftell(file);
						// fseek(file, 0, SEEK_SET);
						// char* filebuf = malloc(file_size + 1);
						// size_t bytes_read = fread(filebuf, 1, file_size, file);
						// filebuf[bytes_read] = '\0';
						// fclose(file);
						// response = malloc(DEFAULT_BUFLEN + file_size);
						// sprintf(response, response_str, 200, "OK", strlen(filebuf), filebuf);
						// free(filebuf);
					// }

					// Echo the buffer back to the sender
					iSendResult = send( ClientSocket, response, strlen(response), 0 );
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}
					printf("Bytes sent: %d\n", iSendResult);
					
					free(response);
					
					int sResult = shutdown(ClientSocket, SD_SEND);
					printf("response sent to %s with error: %d\n", ipStr, WSAGetLastError());
					closesocket(ClientSocket);
					iResult = -1;
				}
			}
			else if (iResult == 0) {
				int sResult = shutdown(ClientSocket, SD_SEND);
				printf("%s disconnected with error: %d\n", ipStr, WSAGetLastError());
				closesocket(ClientSocket);
			}
			else  {
				printf("recv failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return 1;
			}
		} while (iResult > 0);

    } while (1);

    // shutdown the connection since we're done
    // iResult = shutdown(ClientSocket, SD_SEND);
    // if (iResult == SOCKET_ERROR) {
        // printf("shutdown failed with error: %d\n", WSAGetLastError());
        // closesocket(ClientSocket);
        // WSACleanup();
        // return 1;
    // }

    // cleanup
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}