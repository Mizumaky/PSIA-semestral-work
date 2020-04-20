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
#include "crc32.h"
#include "sha256.h"
#include <Windows.h>

#define SERVER "192.168.30.27"  //ip address of udp server
#define BUFLEN 1024  //Max length of buffer[i]
#define DATALEN 1010 //Max length of data in data packet
#define CRC_LEN 8

#define PORT 5555   //The port on which to listen for incoming data
enum type_t
{
	START = 0,
	END,
	OK,
	ERR,
	NAME,
	LEN,
	DATA,
	CHKSUM,
	TYPE_SIZE
};

std::string type_to_str[] = {
	"START", "END", "OK", "ERR", "NAME", "LEN", "DATA", "CHKSUM"
};



void add_crc(char * packet, int len) {
	CRC32 crc32;
	std::string computed_crc = crc32(packet[CRC_LEN], len);
	memcpy(&packet, computed_crc, CRC_LEN);
}

void pack_data(int packet_num, char * buf, int len) {
	memcpy(buf[CRC_LEN], (char)DATA, sizeof(char));
	memcpy(buf[CRC_LEN+1], &packet_num, sizeof(int));
	add_crc(buf, len + sizeof(int) + sizeof(char));
}

void send_size_of_file(int file_size, int s, int flags, const sockaddr * to, int tolen) {
	char char_size[1024];
	sprintf(char_size, "%d", size_of_file);
	int len = sizeof(char_size) + CRC_LEN;
	char* buf = malloc(len)
	memcpy(buf[CRC_LEN], (char)LEN, sizeof(char));
	memcpy(buf[CRC_LEN + 1], &file_size, len);
	add_crc(buf, sizeof(char));
	send_or_fail(s, buf, len, flags, to, tolen);
}
void send_filename(const char *  filename, int len, int s, int flags, const sockaddr * to, int tolen) {
	int len = len + CRC_LEN;
	char* buf = malloc(len)
	memcpy(buf[CRC_LEN], (char)NAME, sizeof(char));
	memcpy(buf[CRC_LEN + 1], &filename, len);
	add_crc(buf, sizeof(char));
	send_or_fail(s, buf, len, flags, to, tolen);
}

void send_just_type(type_t  packet_type, int s, int flags, const sockaddr * to, int tolen) {
	int len = sizeof(char) + CRC_LEN;
	char* buf = malloc(len)
	memcpy(buf[CRC_LEN], (char)packet_type, sizeof(char));
	add_crc(buf, sizeof(char));
	send_or_fail(s, buf, len, flags, to, tolen);
}


void send_or_fail(int s, const char * buf, int len, int flags, const sockaddr * to, int tolen) {
		
		if (sendto(s, buf, len, flags, to, tolen)) == SOCKET_ERROR)
		{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		getchar();
		exit(EXIT_FAILURE);
		}
}

void recv_or_fail(int s, const char * buf, int len , const sockaddr * si_other, int slen) {
		
	if ((recvfrom(s, buf, len, 0, (struct sockaddr *) &si_other, &slen)) == SOCKET_ERROR)
	{
		printf("recvfrom() failed with error code : %d", WSAGetLastError());
		getchar();
		exit(EXIT_FAILURE);
	}
}



int main()
{
	struct sockaddr_in si_other;
	int socket, slen = sizeof(si_other);
	
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
	if ((socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		printf("socket() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	//setup address structure
	memset((char *)&si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT);
	si_other.sin_addr.S_un.S_addr = inet_addr(SERVER);

	FILE *f = fopen(filename, "rb");
	if (!f) {
		perror("Error opening file");
		return 1;
	}
	fseek(f, 0, SEEK_END);
	int size_of_file = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	//start communication
	
	//send filename
	send_filename(const char *  filename, int len, int s, int flags, const sockaddr * to, int tolen)
	
	//send size of file
	send_size_of_file(size_of_file,s, 0, (struct sockaddr *) &si_other, slen);


	int packetsNumber = ceil((double)size_of_file / (double)DATALEN);
	printf("filesize %d pnum %d\n", size_of_file, packetsNumber);
	char** buffer = (char**)malloc(packetsNumber * sizeof(char*));


	for (int i = 0; i < packetsNumber; i++) {
		buffer[i] = (char*)malloc(BUFLEN * sizeof(char));
	}
	for (int i = 0; i < packetsNumber; i++)
	{
		if (size_of_file > DATALEN) {
			fread(buffer[i][CRC_LEN+sizeof(int)], sizeof(char), DATALEN, f);
			pack_data(i, &buffer[i], DATALEN);
			Sleep(delay);
		}
		else {
			fread(buffer[i][CRC_LEN + sizeof(int)], sizeof(char), size_of_file, f);
			pack_data(i, &buffer[i], size_of_file);
			Sleep(delay);
		}
		printf("Bytes left %d\n", size_of_file);
		//printf("Buff: %s\n", buffer[i]);

		bool success = false;
		//send the file packet
		while (!success) {
			send_or_fail(socket, buffer[i], DATALEN, 0, (struct sockaddr *) &si_other, slen);
			char tmp[BUFLEN];
			recv_or_fail(socket, tmp, BUFLEN, &si_other, slen);
			success = (type_t)tmp[0] == OK;
		}
		size_of_file = size_of_file - DATALEN;
	}

	
	closesocket(socket);
	WSACleanup();

	return 0;
}