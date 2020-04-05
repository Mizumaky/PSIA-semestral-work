// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//

#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"

#include <fstream>
#include <winsock2.h>
#include "ws2tcpip.h"

#define TARGET_IP	"127.0.0.1"

#define BUFFERS_LEN 1024

//#define SENDER
#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 5555
#define LOCAL_PORT 8888
#endif // SENDER

#ifdef RECEIVER
#define TARGET_PORT 8888
#define LOCAL_PORT 5555
#endif // RECEIVER


void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void recievedata(SOCKET s, char* buf, int len, int flags, sockaddr* from, int* fromlen)
{
	if (recvfrom(s, buf, len, flags, from, fromlen) == SOCKET_ERROR) {
		printf("Socket error or timeout :C\n");
		getchar();
		exit(1);
	}
}

//**********************************************************************
int main()
{
	SOCKET socketS;
	
	InitWinsock();

	struct sockaddr_in local;
	struct sockaddr_in from;

	int fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;


	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0){
		printf("Binding error!\n");
	    getchar(); //wait for press Enter
		return 1;
	}
	//**********************************************************************
	char buffer_rx[BUFFERS_LEN];
	char buffer_tx[BUFFERS_LEN];

#ifdef SENDER
	
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	
	strncpy(buffer_tx, "Hello world payload!\n", BUFFERS_LEN); //put some data to buffer
	printf("Sending packet.\n");
	sendto(socketS, buffer_tx, strlen(buffer_tx), 0, (sockaddr*)&addrDest, sizeof(addrDest));	

	closesocket(socketS);

#endif // SENDER

#ifdef RECEIVER

	/// Recieve filename
	memset(buffer_rx, 0, BUFFERS_LEN);
	printf("Waiting for filename packet...\n");
	recievedata(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
	printf("Data recieved: %s\n", buffer_rx);
	char filename[BUFFERS_LEN];
	strncpy(filename, buffer_rx, BUFFERS_LEN);


	/// Recieve size of data
	memset(buffer_rx, 0, BUFFERS_LEN);
	printf("Waiting for data size packet...\n");
	recievedata(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
	int datalen;
	if (sscanf(buffer_rx, "%d", &datalen) < 0) {
		printf("Data to integer conversion error :C\n");
		getchar();
		return 1;
	}
	printf("Data recieved: %d\n", datalen);


	/// Recieve data
	FILE* file;
	file = fopen(filename, "wb");
	if (!file) {
		printf("Cannot open file for writing :c");
		getchar();
		return 1;
	}
	int const numofpackets = (datalen + BUFFERS_LEN-1) / BUFFERS_LEN;
	printf("Will be recieving %d packets\n", numofpackets);
	for (int i = 1; i < numofpackets + 1; i++) {
		memset(buffer_rx, 0, BUFFERS_LEN);
		printf("Waiting for data packet #%d...\n", i);
		recievedata(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
		if (i != numofpackets) {
			fwrite(buffer_rx, sizeof(char), 1024, file);
		}
		else {
			int remainingbytes = datalen % 1024;
			fwrite(buffer_rx, sizeof(char), remainingbytes, file);
		}
		//printf("Recieved: %s\n", buffer_rx);
	}
	fclose(file);
	printf("Communication successful\n");

	/// Close communication
	closesocket(socketS);
#endif

	/// End program
	getchar(); //wait for press Enter
	return 0;
}
