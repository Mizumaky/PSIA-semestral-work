// Client.cpp : Defines the entry point for the console application.
//

#pragma comment(lib,"ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <iostream>
#include <fstream> 
#include <string>  
#include <cmath>
#include <vector>
#include "md5.h"
#include <Windows.h>

#define SERVER "192.168.30.27"  //ip address of udp server
#define BUFLEN 1024  //Max length of buffer[i]


#define PORT 5555   //The port on which to listen for incoming data

int main()
{
	struct sockaddr_in si_other;
	int s, slen = sizeof(si_other);
	char check[BUFLEN];
	
	MD5 md5;
	WSADATA wsa;
	char* filename = "test.PNG";

	std::cout << strlen(filename);
	int delay = 5;
	char *hash = md5.digestFile(filename);
	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//create socket
	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		printf("socket() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	//setup address structure
	memset((char *)&si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT);
	si_other.sin_addr.S_un.S_addr = inet_addr(SERVER);
start:
	FILE *f = fopen(filename, "rb");
	if (!f) {
		perror("Error opening file");
		return 1;
	}
	fseek(f, 0, SEEK_END);
	int size_of_file = ftell(f);
	fseek(f, 0, SEEK_SET);
	char char_size[1024];
	
	sprintf(char_size, "%d", size_of_file);
	//start communication
	//send filename


	if ((sendto(s, filename, strlen(filename), 0, (struct sockaddr *) &si_other, slen)) == SOCKET_ERROR)
	{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		getchar();
		exit(EXIT_FAILURE);
	}//send size of file
	if ((sendto(s, char_size, sizeof(char_size), 0, (struct sockaddr *) &si_other, slen)) == SOCKET_ERROR)
	{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		getchar();
		exit(EXIT_FAILURE);
	}
	int packetsNumber = ceil((double)size_of_file / (double)BUFLEN);
	printf("filesize %d pnum %d\n", size_of_file, packetsNumber);
	char** buffer = (char**)malloc(packetsNumber * sizeof(char*));


	for (int i = 0; i < packetsNumber; i++) {
		buffer[i] = (char*)malloc(BUFLEN * sizeof(char));
	}
	for (unsigned int i = 0; i < packetsNumber; i++)
	{
		//memset(buffer, '\0', BUFLEN);
		//send size of buffer[i]
		char* tmpBuffer = (char*)malloc(BUFLEN-4 * sizeof(char));
		char* tmpInt = (char*)malloc(4 * sizeof(char));

		memcpy(buffer[i], &i, 4);

		if (size_of_file > BUFLEN-4) {
			fread(tmpBuffer, sizeof(char), BUFLEN - 4, f);
			memcpy(&buffer[i][4], tmpBuffer, BUFLEN - 4);
			Sleep(delay);
		}
		else {
			fread(tmpBuffer, sizeof(char), size_of_file, f);
			memcpy(&buffer[i][4], tmpBuffer, size_of_file);
			Sleep(delay);
		}


		printf(" Bytes left %d\n", size_of_file);
		//printf("Buff: %s\n", buffer[i]);


		//send the file packet
		if ((sendto(s, buffer[i], BUFLEN, 0, (struct sockaddr *) &si_other, slen)) == SOCKET_ERROR)
		{
			printf("sendto() failed with error code : %d", WSAGetLastError());
			exit(EXIT_FAILURE);
		}
		//Sleep(delay * 1000);

		size_of_file = size_of_file - BUFLEN;
	}

	
	closesocket(s);
	WSACleanup();

	return 0;
}