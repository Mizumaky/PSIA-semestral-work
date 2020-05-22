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
#define IN_AIR_COUNT 8
#define DELAY 20
#define STARTLEN 256
#define SHALEN 64
#define MAXFILENAME 179

#define TARGET_PORT  14000
#define LOCAL_PORT 15001  
enum type_t
{
	START = 0,
	DATA,
	ACK
};

std::string type_to_str[] = {
	"START", "END", "OK", "ERR", "NAME", "LEN", "DATA", "CHKSUM"
};


void send_or_fail(int s, const char * buf, int len, int flags, const sockaddr * to, int tolen) {

	if ((sendto(s, buf, len, flags, to, tolen)) == SOCKET_ERROR)
	{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
}

bool check_crc(char * packet, int len) {
	CRC32 crc32;
	std::string  computed_crc = crc32(&packet[CRC_LEN], len - CRC_LEN);
	std::string  actual_crc;
	memcpy(&actual_crc, &packet, CRC_LEN);
	return actual_crc == computed_crc;
}




int send_packets(int s, std::vector<char*> data, int flags, const sockaddr * to, int tolen) {

	int lastReceivedPacket = -1;
	int packetsToSendCount = IN_AIR_COUNT;


	while (lastReceivedPacket < data.size) {
		for (int i = lastReceivedPacket + 1; i <= lastReceivedPacket + 1 + packetsToSendCount; i++)
		{
			send_or_fail(s, data[i], strlen(data[i]), flags, to, tolen);
			printf("Send packet %d\n", i);
		}
		
		int newCount = 0;
		for (int i = 0; i < packetsToSendCount; i++)
		{
			int size;
			char tmp[BUFLEN];
			if ((size = recvfrom(s, tmp, BUFLEN, 0, NULL, NULL)) == SOCKET_ERROR)
			{
				printf("recvfrom() failed with error code : %d", WSAGetLastError());
				getchar();
				exit(EXIT_FAILURE);
			}
			if (check_crc(tmp, size) && tmp[CRC_LEN] == (char)ACK) {
				int tmpLast;
				memcpy(&tmpLast, &tmp[CRC_LEN + 1], sizeof(int));
				printf("ACK %d\n", tmpLast);
				if (tmpLast == lastReceivedPacket) {
					send_or_fail(s, data[lastReceivedPacket + 1], strlen(data[lastReceivedPacket + 1]), flags, to, tolen);
				}
				else if (tmpLast > lastReceivedPacket) {
					lastReceivedPacket = tmpLast;
					newCount++;
				}
			}
			else break;
		}
		packetsToSendCount = newCount;
	}
}


void add_crc(char * packet, int len) {
	CRC32 crc32;
	std::string  computed_crc = crc32(&packet[CRC_LEN], len);

	memcpy(packet, &computed_crc, CRC_LEN);
}


void pack_data(int packet_num, char * &buf, int len) {
	buf[CRC_LEN] = DATA;
	memcpy(&buf[CRC_LEN + 1], &packet_num, sizeof(int));
	add_crc(buf, len);
}

void send_start_packet(int file_size, std::string filename, std::string checksum, int s, int flags, const sockaddr * to, int tolen) {
	unsigned int len = filename.length;
	char* buf = (char*)calloc(STARTLEN, sizeof(char));
	buf[CRC_LEN] = (char)START;
	memcpy(&buf[CRC_LEN + 1], &file_size, sizeof(int));
	memcpy(&buf[CRC_LEN + 1 + sizeof(int)], &checksum, SHALEN);
	memcpy(&buf[CRC_LEN + 1 + sizeof(int) + SHALEN], &filename, SHALEN);
	add_crc(buf, 1 + sizeof(int) + SHALEN + filename.length);
	send_or_fail(s, buf, CRC_LEN + 1 + sizeof(int) + SHALEN + filename.length, flags, to, tolen);
}


int main()
{
	struct sockaddr_in si_other;
	struct sockaddr_in local_address;
	int socket_num, slen = sizeof(si_other);

	WSADATA wsa;



	char* filename = "64681_1574025586.webp";

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
	struct timeval timeout = { 1000,1000 }; //set timeout for 2 seconds

	/* set receive UDP message timeout */

	setsockopt(socket_num, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval));


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

	int packetsNumber = ceil((double)size_of_file / (double)DATALEN);
	printf("filesize %d pnum %d\n", size_of_file, packetsNumber);
	std::vector<char*> buffer(packetsNumber);


	//start communication


	send_just_type(START, socket_num, 0, (struct sockaddr *) &si_other, slen);
	//send filename
	send_filename(filename, socket_num, 0, (struct sockaddr *) &si_other, slen);

	//send size of file
	send_size_of_file(size_of_file, socket_num, 0, (struct sockaddr *) &si_other, slen);

	int tmp_size_of_file = size_of_file;
	for (int i = 0; i < packetsNumber; i++) {
		buffer[i] = (char*)calloc(BUFLEN, 1);

		if (tmp_size_of_file > DATALEN) {
			fread(&buffer[i][CRC_LEN + sizeof(int) + 1], sizeof(char), DATALEN, f);
			pack_data(i, buffer[i], DATALEN);
		}
		else {
			fread(&buffer[i][CRC_LEN + sizeof(int) + 1], sizeof(char), tmp_size_of_file, f);
			pack_data(i, buffer[i], tmp_size_of_file);

		}
		tmp_size_of_file = tmp_size_of_file - DATALEN;
	}


	for (int i = 0; i < packetsNumber; i++)
	{
		printf("Current packet %d\n", i);

		send_or_fail(socket_num, buffer[i], BUFLEN, 0, (struct sockaddr *) &si_other, slen);

	}

	//CHECK SUM
	SHA256 sha256;
	tmp_size_of_file = size_of_file;
	for (int i = 0; i < packetsNumber; i++) {

		if (tmp_size_of_file > DATALEN) {

			sha256.add(&buffer[i][CRC_LEN + sizeof(int) + 1], DATALEN);
		}
		else {
			sha256.add(&buffer[i][CRC_LEN + sizeof(int) + 1], tmp_size_of_file);
		}
		tmp_size_of_file = tmp_size_of_file - DATALEN;
	}

	//send filename
	send_check_sum(sha256.getHash(), socket_num, 0, (struct sockaddr *) &si_other, slen);

	send_just_type(END, socket_num, 0, (struct sockaddr *) &si_other, slen);
	fclose(f);

	closesocket(socket_num);
	WSACleanup();

	return 0;
}// Client.cpp : Defines the entry point for the console application.
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
#define IN_AIR_COUNT 8
#define DELAY 20
#define STARTLEN 256
#define SHALEN 64
#define MAXFILENAME 179

#define TARGET_PORT  14000
#define LOCAL_PORT 15001  
enum type_t
{
	START = 0,
	DATA,
	ACK
};

std::string type_to_str[] = {
	"START", "END", "OK", "ERR", "NAME", "LEN", "DATA", "CHKSUM"
};


void send_or_fail(int s, const char * buf, int len, int flags, const sockaddr * to, int tolen) {

	if ((sendto(s, buf, len, flags, to, tolen)) == SOCKET_ERROR)
	{
		printf("sendto() failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
}

bool check_crc(char * packet, int len) {
	CRC32 crc32;
	std::string  computed_crc = crc32(&packet[CRC_LEN], len - CRC_LEN);
	std::string  actual_crc;
	memcpy(&actual_crc, &packet, CRC_LEN);
	return actual_crc == computed_crc;
}




int send_packets(int s, std::vector<char*> data, int flags, const sockaddr * to, int tolen) {

	int lastReceivedPacket = -1;
	int packetsToSendCount = IN_AIR_COUNT;


	while (lastReceivedPacket < data.size) {
		for (int i = lastReceivedPacket + 1; i <= lastReceivedPacket + 1 + packetsToSendCount; i++)
		{
			send_or_fail(s, data[i], strlen(data[i]), flags, to, tolen);
		}
		
		int newCount = 0;
		for (int i = 0; i < packetsToSendCount; i++)
		{
			int size;
			char tmp[BUFLEN];
			if ((size = recvfrom(s, tmp, BUFLEN, 0, NULL, NULL)) == SOCKET_ERROR)
			{
				std::cout << "TIMEOUT\n";
				break;
			}else if (check_crc(tmp, size) && tmp[CRC_LEN] == (char)ACK) {
				int tmpLast;
				memcpy(&tmpLast, &tmp[CRC_LEN + 1], sizeof(int));
				if (tmpLast == lastReceivedPacket) {
					send_or_fail(s, data[lastReceivedPacket + 1], strlen(data[lastReceivedPacket + 1]), flags, to, tolen);
				}
				else if (tmpLast > lastReceivedPacket) {
					newCount += tmpLast - lastReceivedPacket;
					lastReceivedPacket = tmpLast;
				}
			}
		}
		packetsToSendCount = newCount;
	}
}


void add_crc(char * packet, int len) {
	CRC32 crc32;
	std::string  computed_crc = crc32(&packet[CRC_LEN], len);

	memcpy(packet, &computed_crc, CRC_LEN);
}


void pack_data(int packet_num, char * &buf, int len) {
	buf[CRC_LEN] = DATA;
	memcpy(&buf[CRC_LEN + 1], &packet_num, sizeof(int));
	add_crc(buf, len);
}

void send_start_packet(int file_size, std::string filename, std::string checksum, int s, int flags, const sockaddr * to, int tolen) {
	unsigned int len = filename.length;
	char* buf = (char*)calloc(STARTLEN, sizeof(char));
	buf[CRC_LEN] = (char)START;
	memcpy(&buf[CRC_LEN + 1], &file_size, sizeof(int));
	memcpy(&buf[CRC_LEN + 1 + sizeof(int)], &checksum, SHALEN);
	memcpy(&buf[CRC_LEN + 1 + sizeof(int) + SHALEN], &filename, SHALEN);
	add_crc(buf, 1 + sizeof(int) + SHALEN + filename.length);
	send_or_fail(s, buf, CRC_LEN + 1 + sizeof(int) + SHALEN + filename.length, flags, to, tolen);
}


int main()
{
	struct sockaddr_in si_other;
	struct sockaddr_in local_address;
	int socket_num, slen = sizeof(si_other);

	WSADATA wsa;



	char* filename = "64681_1574025586.webp";

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
	struct timeval timeout = { 1000,1000 }; //set timeout for 2 seconds

	/* set receive UDP message timeout */

	setsockopt(socket_num, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval));


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

	int packetsNumber = ceil((double)size_of_file / (double)DATALEN);
	printf("filesize %d pnum %d\n", size_of_file, packetsNumber);
	std::vector<char*> buffer(packetsNumber);


	int tmp_size_of_file = size_of_file;
	for (int i = 0; i < packetsNumber; i++) {
		buffer[i] = (char*)calloc(BUFLEN, 1);

		if (tmp_size_of_file > DATALEN) {
			fread(&buffer[i][CRC_LEN + sizeof(int) + 1], sizeof(char), DATALEN, f);
			pack_data(i, buffer[i], DATALEN);
		}
		else {
			fread(&buffer[i][CRC_LEN + sizeof(int) + 1], sizeof(char), tmp_size_of_file, f);
			pack_data(i, buffer[i], tmp_size_of_file);

		}
		tmp_size_of_file = tmp_size_of_file - DATALEN;
	}
	fclose(f);

		
	
	//CHECK SUM
	SHA256 sha256;
	tmp_size_of_file = size_of_file;
	for (int i = 0; i < packetsNumber; i++) {

		if (tmp_size_of_file > DATALEN) {

			sha256.add(&buffer[i][CRC_LEN + sizeof(int) + 1], DATALEN);
		}
		else {
			sha256.add(&buffer[i][CRC_LEN + sizeof(int) + 1], tmp_size_of_file);
		}
		tmp_size_of_file = tmp_size_of_file - DATALEN;
	}


	send_start_packet(size_of_file,filename,sha256.getHash(),socket_num, 0, (struct sockaddr *) &si_other, slen);
	send_packets(socket_num, buffer, 0, (struct sockaddr *) &si_other, slen);


	closesocket(socket_num);
	WSACleanup();

	return 0;
}