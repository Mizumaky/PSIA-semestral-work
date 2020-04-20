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
	std::string computed_crc = crc32(&packet[CRC_LEN], len).c_str();
	memcpy(&packet, &computed_crc, CRC_LEN);
}

void pack_data(int packet_num, char * &buf, int len) {
	char type = (char)DATA;
	memcpy(&buf[CRC_LEN], &type, sizeof(char));
	memcpy(&buf[CRC_LEN + 1], &packet_num, sizeof(int));
	add_crc(buf, len + sizeof(int) + sizeof(char));
}

void send_size_of_file(int file_size, int s, int flags, const sockaddr * to, int tolen) {
	char char_size[1024];
	sprintf(char_size, "%d", file_size);
	int len = sizeof(char_size) + CRC_LEN;
	char* buf = (char*)malloc(len);
	char type = (char)LEN;
		memcpy(&buf[CRC_LEN], &type, sizeof(char));
	memcpy(&buf[CRC_LEN + 1], &file_size, len);
	add_crc(buf, sizeof(char_size));
	send_or_fail(s, buf, len, flags, to, tolen);
}
void send_filename(const char *  filename, int s, int flags, const sockaddr * to, int tolen) {
	int len = strlen(filename) + CRC_LEN;
	char* buf = (char*)malloc(len);
	char type = (char)NAME;
	memcpy(&buf[CRC_LEN], &type, sizeof(char));
	memcpy(&buf[CRC_LEN + 1], &filename, len);
	add_crc(buf, sizeof(char));
	send_or_fail(s, buf, len, flags, to, tolen);
}

void send_just_type(type_t  packet_type, int s, int flags, const sockaddr * to, int tolen) {
	char* buf = (char*)calloc(BUFLEN,0);
	char type = (char)packet_type;
	memcpy(&buf[CRC_LEN], &type, sizeof(char));
	add_crc(buf, sizeof(BUFLEN - CRC_LEN));
	send_or_fail(s, buf, BUFLEN, flags, to, tolen);
}


void send_or_fail(int s, const char * buf, int len, int flags, const sockaddr * to, int tolen) {

	if ((sendto(s, buf, len, flags, to, tolen)) == SOCKET_ERROR)
		{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		getchar();
		exit(EXIT_FAILURE);
		}
}

void recv_or_fail(int s, char * buf, int len, const sockaddr * si_other, int slen) {

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
	int socket_num, slen = sizeof(si_other);

	WSADATA wsa;


	char* filename = "test.PNG";

	std::cout << strlen(filename);
	int delay = 5;
	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//create socket
	if ((socket_num = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
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
	send_filename(filename, socket_num, 0, (struct sockaddr *) &si_other, slen);

		//send size of file
	send_size_of_file(size_of_file, socket_num, 0, (struct sockaddr *) &si_other, slen);


	int packetsNumber = ceil((double)size_of_file / (double)DATALEN);
	printf("filesize %d pnum %d\n", size_of_file, packetsNumber);
	char** buffer = (char**)malloc(packetsNumber * sizeof(char*));


	for (int i = 0; i < packetsNumber; i++) {
		buffer[i] = (char*)calloc(BUFLEN * sizeof(char), 0);
	}
	for (int i = 0; i < packetsNumber; i++)
	{
		if (size_of_file > DATALEN) {
			fread(&buffer[i][CRC_LEN + sizeof(int)], sizeof(char), DATALEN, f);
			pack_data(i, buffer[i], DATALEN);
			Sleep(delay);
		}
		else {
			fread(&buffer[i][CRC_LEN + sizeof(int)], sizeof(char), size_of_file, f);
			pack_data(i, buffer[i], size_of_file);
			Sleep(delay);
		}
		printf("Bytes left %d\n", size_of_file);
		//printf("Buff: %s\n", buffer[i]);

		bool success = false;
		//send the file packet
		while (!success) {
			send_or_fail(socket_num, buffer[i], DATALEN, 0, (struct sockaddr *) &si_other, slen);
			char tmp[BUFLEN];
			recv_or_fail(socket_num, tmp, BUFLEN, (struct sockaddr *) &si_other, slen);
			success = (type_t)tmp[0] == OK;
		}
		size_of_file = size_of_file - DATALEN;
	}


	closesocket(socket_num);
	WSACleanup();

	return 0;
}