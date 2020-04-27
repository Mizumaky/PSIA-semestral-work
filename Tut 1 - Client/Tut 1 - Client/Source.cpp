// Client.cpp : Defines the entry point for the console application.
//

#pragma comment(lib,"ws2_32.lib")
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <iostream>
#include <vector>
#include <fstream> 
#include <string>  
#include <cmath>
#include <vector>
#include "crc32.h"
#include "sha256.h"
#include <Windows.h>

#define SERVER "127.0.0.1"  //ip address of udp server
#define BUFLEN 1024  //Max length of buffer[i]
#define DATALEN 1011 //Max length of data in data packet
#define CRC_LEN 8
#define DELAY 20

#define TARGET_PORT 5555  
#define LOCAL_PORT 4444  
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


void send_or_fail(int s, const char * buf, int len, int flags, const sockaddr * to, int tolen) {
	bool success = false;
	while (!success) {
		if ((sendto(s, buf, len, flags, to, tolen)) == SOCKET_ERROR)
		{
			printf("sendto() failed with error code : %d", WSAGetLastError());
			getchar();
			exit(EXIT_FAILURE);
		}
		char tmp[BUFLEN];
		Sleep(DELAY);
		if ((recvfrom(s, tmp, len, 0, NULL, NULL)) == SOCKET_ERROR)
		{
			printf("recvfrom() failed with error code : %d", WSAGetLastError());
			getchar();
			exit(EXIT_FAILURE);
		}
		success = (type_t)tmp[0] == OK;
		if (!success) { std::cout << "FAIL\n"; getchar(); }
		else { std::cout << "OKK\n"; }
	}
}

std::string string_to_hex(const std::string& input)
{
	static const char hex_digits[] = "0123456789ABCDEF";

	std::string output;
	output.reserve(input.length() * 2);
	for (int i = 0; i < BUFLEN; i++)
	{
		output.push_back(hex_digits[input[i] >> 4]);
		output.push_back(hex_digits[input[i] & 15]);
	}
	return output;
}

void add_crc(char * packet, int len) {
	CRC32 crc32;
	std::string  computed_crc = crc32(&packet[CRC_LEN], len);

	memcpy(packet, &computed_crc, CRC_LEN);
	//std::cout << string_to_hex(packet) << " Packet\n";
}

void pack_data(int packet_num, char * &buf, int len) {
	// TODO: Fix this
	buf[CRC_LEN] = DATA;
	memcpy(&buf[CRC_LEN + 1], &packet_num, sizeof(int));
	add_crc(buf, BUFLEN - CRC_LEN);
}

void send_size_of_file(int file_size, int s, int flags, const sockaddr * to, int tolen) {
	char char_size[BUFLEN-CRC_LEN-1];
	sprintf(char_size, "%d", file_size);

	char* buf = (char*)calloc(BUFLEN, sizeof(char));
	buf[CRC_LEN] = (char)LEN;
	strcpy(&buf[CRC_LEN + 1], char_size);
	add_crc(buf, BUFLEN - CRC_LEN);
	send_or_fail(s, buf, BUFLEN, flags, to, tolen);
}
void send_filename(const char *  filename, int s, int flags, const sockaddr * to, int tolen) {
	char* buf = (char*)calloc(BUFLEN, sizeof(char));
	buf[CRC_LEN] = (char)NAME;
	strcpy(&buf[CRC_LEN + 1], filename);
	std::cout << buf;
	add_crc(buf, BUFLEN - CRC_LEN);
	send_or_fail(s, buf, BUFLEN, flags, to, tolen);
}

void send_just_type(type_t  packet_type, int s, int flags, const sockaddr * to, int tolen) {
	char* buf = (char*)calloc(BUFLEN, sizeof(char));
	buf[CRC_LEN] = packet_type;
	add_crc(buf, BUFLEN - CRC_LEN);
	send_or_fail(s, buf, BUFLEN, flags, to, tolen);
	free(buf);
}



int main()
{
	struct sockaddr_in si_other;
	struct sockaddr_in local_address;
	int socket_num, slen = sizeof(si_other);

	WSADATA wsa;



	char* filename = "test.PNG";

	std::cout << strlen(filename);
	//Initialise winsock
	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
	printf("Initialised.\n");

	//create socket
	local_address.sin_family = AF_INET;
	local_address.sin_port = htons(LOCAL_PORT);
	local_address.sin_addr.s_addr = INADDR_ANY;

	if ((socket_num = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		printf("socket() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	if (bind(socket_num, reinterpret_cast<sockaddr*>(&local_address), sizeof(local_address)) != 0) {
		std::cout << "socket binding unsuccessful";
		exit(EXIT_FAILURE);
	}

	BOOL bNewBehavior = FALSE;
	DWORD dwBytesReturned = 0;
	WSAIoctl(socket_num, SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);


	//setup address structure
	memset((char *)&si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(TARGET_PORT);
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

	
	send_just_type(START, socket_num, 0, (struct sockaddr *) &si_other, slen);
	//send filename
	send_filename(filename, socket_num, 0, (struct sockaddr *) &si_other, slen);

		//send size of file
	send_size_of_file(size_of_file, socket_num, 0, (struct sockaddr *) &si_other, slen);


	int packetsNumber = ceil((double)size_of_file / (double)DATALEN);
	printf("filesize %d pnum %d\n", size_of_file, packetsNumber);
	std::vector<char*> buffer(packetsNumber);


	for (int i = 0; i < packetsNumber; i++) {
		buffer[i] = (char*)calloc(BUFLEN, 1);
	}
	for (int i = 0; i < packetsNumber; i++)
	{
		if (size_of_file > DATALEN) {
			fread(&buffer[i][CRC_LEN + sizeof(int)+1], sizeof(char), DATALEN, f);
			Sleep(DELAY);
			pack_data(i, buffer[i], DATALEN);
		}
		else {
			fread(&buffer[i][CRC_LEN + sizeof(int) + 1], sizeof(char), size_of_file, f);
			Sleep(DELAY);
			pack_data(i, buffer[i], size_of_file);
			
		}
		printf("Current packet %d Bytes left %d\n",i ,size_of_file);
		//printf("Buff: %s\n", buffer[i]);

		
		//fwrite(&buffer[i][, sizeof(char), size_of_file>=DATALEN?DATALEN:size_of_file, file);
		


		send_or_fail(socket_num, buffer[i], DATALEN, 0, (struct sockaddr *) &si_other, slen);
		size_of_file = size_of_file - DATALEN;
	}

	fclose(f);

	closesocket(socket_num);
	WSACleanup();

	return 0;
}