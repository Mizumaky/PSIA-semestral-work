#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"

#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include "ws2tcpip.h"
#include "crc32.h"
#include "sha256.h"

// SETTINGS
#define TARGET_IP	"127.0.0.1"
#define BUFFERS_LEN 1024
#define CRC_LEN 8
#define TYPE_LEN 1
#define NUM_LEN 4
#define DATA_LEN (BUFFERS_LEN - CRC_LEN - TYPE_LEN - NUM_LEN)
#define SHA_LEN 64
#define TARGET_PORT 8888
#define LOCAL_PORT 5555

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

struct com
{
    char buffer_rx[BUFFERS_LEN];
    char buffer_tx[BUFFERS_LEN];
    SOCKET socket;
    sockaddr_in local_address;
    sockaddr_in target_address;
} com;

void Error(std::string text)
{
    std::string error_text = "ERROR: " + text + " :C\n" + "INTERNAL ERRNO: ";
    perror(error_text.c_str());
    std::cerr << "\n";
}

void Warning(std::string text)
{
    std::cerr << "WARNING: " << text << "\n";
}

void Info(std::string text)
{
    std::cout << "INFO: " << text << "\n";
}


void InitWinsock()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void InitApp()
{
    com.local_address.sin_family = AF_INET;
    com.local_address.sin_port = htons(LOCAL_PORT);
    com.local_address.sin_addr.s_addr = INADDR_ANY;

    com.target_address.sin_family = AF_INET;
    com.target_address.sin_port = htons(TARGET_PORT);
    InetPton(AF_INET, _T(TARGET_IP), &com.target_address.sin_addr.s_addr);

    com.socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (bind(com.socket, reinterpret_cast<sockaddr*>(&com.local_address), sizeof(com.local_address)) != 0) {
        Error("socket binding unsuccessful");
    }
}

char* RecievePacket()
{
    sockaddr_in from_adress;
    int from_addr_len;
    const auto data = new char[BUFFERS_LEN];
    //memset(com.buffer_rx, 0, BUFFERS_LEN);
    if (recvfrom(com.socket, com.buffer_rx, BUFFERS_LEN, 0, reinterpret_cast<sockaddr*>(&from_adress), &from_addr_len) == SOCKET_ERROR) {
        Error("could not recieve data - socket error or timeout");
    }
    memcpy(data, com.buffer_rx, BUFFERS_LEN);
    return data;
}

void SendPacket(char* data, int len)
{
    memset(com.buffer_tx, 0, BUFFERS_LEN);
    memcpy(com.buffer_tx, data, len);
    sendto(com.socket, com.buffer_tx, BUFFERS_LEN, 0, (sockaddr*)&com.target_address, sizeof(com.target_address));
}

void SendOK()
{
    char ok = OK;
    SendPacket(&ok, 1);
}
void SendERR()
{
    char err = ERR;
    SendPacket(&err, 1);
}

//**********************************************************************
int main()
{
    InitWinsock();
    InitApp();

    // RECIEVE FILES
    while (true) {

        // Start le state machine
        Info("RECIEVER v2 initiated, waiting for START packet...");
        type_t state = END;
        int32_t expected_num = 0;
        char filename[BUFFERS_LEN] = "";
        int file_size = 0;
        int recieved_data_size = 0;
        char* data = nullptr;
        char* packet = nullptr;

        // RECIEVE PACKETS
        while (true) {
            // Recieve somthin
            delete[] packet;
            packet = RecievePacket();

            // Check CRC
            char crc_tmp[CRC_LEN + 1] = "";
            std::string recieved_crc = strncpy(crc_tmp, packet, CRC_LEN);
            CRC32 crc32;
            std::string computed_crc = crc32(&packet[CRC_LEN], BUFFERS_LEN - CRC_LEN);
            if (computed_crc != recieved_crc) {
                Warning("CRC check FAILED, recieved: " + recieved_crc + ", computed: " + computed_crc);
                SendERR();
                Info("ERR packet sent back");
                continue;
            }

            // Get packet type
            if (packet[CRC_LEN] >= TYPE_SIZE || packet[CRC_LEN] < 0) {
                Error("recieved unsupported packet type " + std::to_string(+packet[CRC_LEN]));
                break;
            }
            const type_t type = type_t(packet[CRC_LEN]);

            // Get packet number
            int32_t recieved_num;
            memcpy(&recieved_num, &packet[CRC_LEN + TYPE_LEN], 4);
            if (recieved_num == expected_num - 1) {
                Warning("recieved the same packet as before");
                SendOK();
                continue;
            }
            else if (recieved_num != expected_num) {
                Error("recieved packet out of order, expected #" + std::to_string(expected_num) + ", recieved #" + std::to_string(recieved_num));
                break;
            }
            expected_num++;
            Info("recieved packet #" + std::to_string(recieved_num) + " of type " + type_to_str[type]);

            // --- Decide what to do depending on type ---
            // START
            if (state == END && type == START) {
            }
            // FILENAME
            else if (state == START && type == NAME) {
                strcpy(filename, &packet[CRC_LEN + TYPE_LEN + NUM_LEN]);
                Info("filename set to \"" + std::string(filename) + "\"");
            }
            // FILE SIZE
            else if (state == NAME && type == LEN) {
                if (sscanf(&packet[CRC_LEN + TYPE_LEN + NUM_LEN], "%d", &file_size) < 0) {
                    Error("cannot convert characters to integer");
                    break;
                }
                data = new char[file_size];
                Info("data buffer of size " + std::to_string(file_size) + " bytes created");
            }
            // DATA
            else if ((state == LEN || state == DATA) && type == DATA) {
                const int data_size_to_copy = file_size - recieved_data_size < DATA_LEN ? file_size - recieved_data_size : DATA_LEN;
                memcpy(&data[recieved_data_size], &packet[CRC_LEN + TYPE_LEN + NUM_LEN], data_size_to_copy);
                recieved_data_size += data_size_to_copy;
                Info("recieved " + std::to_string(recieved_data_size) + " of " + std::to_string(file_size) + " bytes");
            }
            // SHA256
            else if (state == DATA && type == CHKSUM) {
                char sha_tmp[SHA_LEN + 1] = "";
                std::string recieved_sha = strncpy(sha_tmp, &packet[CRC_LEN + TYPE_LEN + NUM_LEN], SHA_LEN);
                SHA256 sha256;
                std::string computed_sha = sha256(data, file_size);
                if (computed_sha != recieved_sha) {
                    Warning("SHA256 check FAILED, recieved: " + recieved_sha + ", computed: " + computed_sha);
                    Info("trying to save file anyway");
                }
            }
            // SAVE FILE
            else if (state == CHKSUM && type == END) {
                FILE* file = fopen(filename, "wb");
                if (!file) {
                    Error("cannot open file for writing");
                    break;
                }
                fwrite(data, sizeof(char), file_size, file);
                fclose(file);
                Info("File saved");
            }
            else {
                Error("recieved packet of type " + type_to_str[type] + " while in state " + type_to_str[state] + ", that was unexpected");
                break;
            }

            // Confirm recieving
            SendOK();

            // Update to new state
            state = type;

            // BREAK
            if (state == END) {
                break;
            }
        }

        // Ask for next file
        delete[] data;
        std::cout << "Communication finished, press Enter to recieve next file or 'q' + Enter to quit ...\n";
        if (getchar() == 'q') {
            std::cout << "Goodbye...\n";
            break;
        } else {
            std::cout << "\n\n\n\n\n\n\n\n\n";
        }
    }

    // Close communication
    closesocket(com.socket);
    return 0;
}

// TODO possibly not waste copying from buffer_rx and _tx, and directly pass them