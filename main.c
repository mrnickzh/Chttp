#define WIN32_LEAN_AND_MEAN
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define DEFAULT_BUFLEN 4096
#define DEFAULT_PORT "27015"

struct Request
{
	SOCKET socket;
	char* path;
	char* type;
};

void remove_element(struct Request *arr, int size, int index) {
    // 1. Validate the bounds of the index
    if (index < 0 || index >= size) {
        printf("Index out of bounds.\n");
        return;
    }

    // 2. Calculate how many elements need to be shifted left
    int elements_to_move = size - index - 1;

    // 3. Shift the elements left by one position if necessary
    if (elements_to_move > 0) {
        memmove(&arr[index], &arr[index + 1], elements_to_move * sizeof(arr[0]));
    }
}

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

struct Status
{
	int done;
	char* data; 
};

struct Endpoint
{
	char path[128];
	struct Status (*func)(); 
};

// ENDPOINTS

struct Status test()
{
	struct Status status;
	static int counter = 0;
	if (counter == 0) { counter = time(NULL); }
	
	if (counter + 10 < time(NULL)) {
		char* data;
		data = malloc(128);
		int random_num = rand();
		sprintf(data, "Random Number: %d\n", random_num);
		counter = 0;
		
		status.done = 1;
		status.data = data;
		return status;
	}
	else {
		status.done = 0;
		status.data = NULL;
		return status;
	}
}

struct Status snek()
{	
	struct Status status;

	char path[] = "index.html";
	FILE *file = fopen(path, "r");
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* filebuf = malloc(file_size + 1);
	size_t bytes_read = fread(filebuf, 1, file_size, file);
	filebuf[bytes_read] = '\0';
	fclose(file);
	
	status.done = 1;
	status.data = filebuf;
	return status;
}

// ENDPOINTS

struct Endpoint endpoints[] = { {"/test", test}, {"/snek", snek} };

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
	
	int socket_idx = 0;
	struct Request SocketArray[16];

    struct addrinfo *result = NULL;
    struct addrinfo hints;

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
	
	struct Request ListenResponse = {ListenSocket, NULL, NULL};
	SocketArray[socket_idx] = ListenResponse;
	socket_idx++;

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
	    fd_set reading, writing;
	    struct timeval timeout;
	    int rc;
	 
	    /* initialize the bit sets */
	    FD_ZERO( &reading );
		FD_ZERO( &writing );
	 
	    /* add r, w, and e to the appropriate bit set */
		for (int s = 0; s < socket_idx; s++) {
			// printf( "socket added %d\n", s);
			FD_SET( SocketArray[s].socket, &reading );
			FD_SET( SocketArray[s].socket, &writing );
		}
	 
	    memset( &timeout, 0, sizeof(timeout) );
	 
	    /* poll */
	    rc = select( socket_idx, &reading, &writing, NULL, &timeout );
	 
	    if ( rc < 0 ) {
			/* an error occurred during the select() */
		 	printf( "select error %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
	    }
	    else if ( rc == 0 ) {
			/* none of the sockets were ready in our little poll */
			// printf( "nobody is home.\n" );
	    } else {
			if (FD_ISSET(SocketArray[0].socket, &reading))
			{
				printf( "SOCKET SET\n" );
				struct sockaddr_in clientAddr = {0};
				int clientAddrSize = sizeof(clientAddr);
				
				// Accept a client socket
				SOCKET ClientSocket = INVALID_SOCKET;
				ClientSocket = accept(ListenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
				printf("%d socket\n", ClientSocket);
				if (ClientSocket == INVALID_SOCKET) {
					printf("accept failed with error: %d\n", WSAGetLastError());
					closesocket(ListenSocket);
					WSACleanup();
					return 1;
				}
				
				char ipStr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
				printf("%s connected\n", ipStr);
				
				struct Request ClientResponse = {ClientSocket, NULL, NULL};
				SocketArray[socket_idx] = ClientResponse;
				socket_idx++;
			}
			for (int i = 1; i < socket_idx; i++) 
			{
				printf("%d %d status %d id\n", FD_ISSET(SocketArray[i].socket, &reading), FD_ISSET(SocketArray[i].socket, &writing), i);
				if (FD_ISSET(SocketArray[i].socket, &reading))
				{
					SOCKET ClientSocket = INVALID_SOCKET;
					ClientSocket = SocketArray[i].socket;
					do {
						iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
						printf("%d %d\n", iResult, i);
						
						if (iResult <= 0) { break; }
						
						printf("Bytes received: %d\n", iResult);
						printf("Content: %s\n", recvbuf);
						
						if (SocketArray[i].path == NULL) {
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
								
								SocketArray[i].path = malloc(128);
								SocketArray[i].type = malloc(128);
								sprintf(SocketArray[i].path, "%s\0", path);
								sprintf(SocketArray[i].type, "%s\0", type);
							}
						}
						
						if (iResult < DEFAULT_BUFLEN) {				
							break;
						}
					} while (1);
					
					if (iResult == 0) {
						int sResult = shutdown(ClientSocket, SD_SEND);
						printf("disconnected with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						free(SocketArray[socket_idx].path);
						free(SocketArray[socket_idx].type);
						remove_element(SocketArray, socket_idx, i);
						socket_idx--;
					}
					else if (iResult < 0) {
						printf("recv failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						WSACleanup();
						return 1;
					}
				}
					
				else if (FD_ISSET(SocketArray[i].socket, &writing))
				{
					SOCKET ClientSocket = INVALID_SOCKET;
					ClientSocket = SocketArray[i].socket;
					
					if (SocketArray[i].path == NULL) { continue; }
					
					struct Endpoint* ep = NULL;
						
					for (int j = 0; j < sizeof(endpoints) / sizeof(struct Endpoint); j++)
					{
						printf("%s %s\n", SocketArray[i].path, endpoints[j].path);
						if (strcmp(endpoints[j].path, SocketArray[i].path) == 0)
						{
							ep = &endpoints[j];
							break;
						}
					}
					
					// FILE *file = fopen(path+1, "r");
					char* response;
					
					int finished = 0;
					
					if (ep == NULL) {
						printf("endpoint not found\n");
						response = malloc(DEFAULT_BUFLEN + strlen(response_err));
						sprintf(response, response_str, 404, "Not Found", strlen(response_err), response_err);
						finished = 1;
					}
					else {
						struct Status status = ep->func();
						printf("%d\n", status);
						if (status.done) {
							response = malloc(DEFAULT_BUFLEN + strlen(status.data));
							sprintf(response, response_str, 200, "OK", strlen(status.data), status.data);
							free(status.data);
							finished = 1;
						}
					}
					
					if (finished) {
						int iSendResult = send( ClientSocket, response, strlen(response), 0 );
						if (iSendResult == SOCKET_ERROR) {
							printf("send failed with error: %d\n", WSAGetLastError());
							closesocket(ClientSocket);
							WSACleanup();
							return 1;
						}
						printf("Bytes sent: %d\n", iSendResult);
						
						free(response);
						
						int sResult = shutdown(ClientSocket, SD_SEND);
						printf("response sent with error: %d\n", WSAGetLastError());
						closesocket(ClientSocket);
						free(SocketArray[socket_idx].path);
						free(SocketArray[socket_idx].type);
						remove_element(SocketArray, socket_idx, i);
						socket_idx--;
						iResult == 0;
					}
				}
			}
	    }
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