#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include "ws2tcpip.h"
#include "crc32.h"
#include "sha256.h"

// SETTINGS
#define TARGET_IP	"192.168.30.24"

#define START_PACKET_LEN 256
#define DATA_PACKET_LEN 1024
#define WINDOW 8

#define CRC_LEN 4
#define TYPE_LEN 1
#define NUMBER_LEN 4
#define SHA_LEN 64
#define NAME_LEN (START_PACKET_LEN - CRC_LEN - TYPE_LEN - NUMBER_LEN - SHA_LEN)
#define DATA_LEN (DATA_PACKET_LEN - CRC_LEN - TYPE_LEN - NUMBER_LEN)
#define ACK_PACKET_LEN (CRC_LEN + TYPE_LEN + NUMBER_LEN)

#define TARGET_PORT 4444
#define LOCAL_PORT 5555
#define DEBUG false

#define Y_STATUS 1
#define Y_FILENAME 3
#define Y_NUMBER 6
#define Y_BAR 8
#define Y_FAILS_OFFSET 3
#define Y_INFO_OFFSET 7

// INITIAL DECLARATIONS
enum type_t
{
    START = 0,
    DATA,
    ACK,
    TYPE_SIZE
};

std::string type_to_str[] = {
    "START", "DATA", "ACK"
};

struct com
{
    char packet_rx[DATA_PACKET_LEN];
    char packet_tx[ACK_PACKET_LEN];
    SOCKET socket;
    sockaddr_in local_address;
    sockaddr_in target_address;
} com;

HANDLE h_std_out;
int screen_width;
int bar_height;
int Y_CRC_FAIL = Y_BAR + 3 + Y_FAILS_OFFSET;
int Y_WT_FAIL = Y_BAR + 3 + Y_FAILS_OFFSET + 1;
int Y_SP_FAIL = Y_BAR + 3 + Y_FAILS_OFFSET + 2;
int Y_INFO = Y_BAR + 3 + Y_INFO_OFFSET;

// PRINT FUNCTIONS
void SetPrintPos(int x, int y)
{
    const COORD position = { short(x), short(y) };
    SetConsoleCursorPosition(h_std_out, position);
}
void SetPrintPos(COORD position)
{
    SetConsoleCursorPosition(h_std_out, position);
}
void Error(std::string text)
{
    SetPrintPos(0, Y_INFO);
    std::wstring wtext = std::wstring(text.begin(), text.end());
    wprintf(L"ERROR: %s :C                                            \n", wtext.c_str());
    getchar();
}
void Warning(std::string text)
{
    SetPrintPos(0, Y_INFO);
    std::wstring wtext = std::wstring(text.begin(), text.end());
    wprintf(L"WARNING: %s                                             \n", wtext.c_str());
}
void Info(std::string text)
{
    SetPrintPos(0, Y_INFO);
    std::wstring wtext = std::wstring(text.begin(), text.end());
    wprintf(L"%s                                                      \n", wtext.c_str());
}
COORD IndexToPos(int index)
{
    //Info("index = " + std::to_string(index) + "\n screen_width = " + std::to_string(screen_width) + "\n result =" + std::to_string(index % (screen_width - 2) + 1));
    return { short(index % (screen_width - 2) + 1), short(Y_BAR + index / (screen_width - 2)) };
}
void InitPrinting()
{
    _setmode(_fileno(stdout), _O_U16TEXT);
    CONSOLE_SCREEN_BUFFER_INFO scrnBufferInfo;
    GetConsoleScreenBufferInfo(h_std_out, &scrnBufferInfo);
    screen_width = scrnBufferInfo.srWindow.Right - scrnBufferInfo.srWindow.Left + 1;
    if (!SetConsoleScreenBufferSize(h_std_out, { short(screen_width), 100 })) {
        Error("could not prepare screen buffer");
    }
}

// CORE FUNCTIONS
void InitWinsock()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}
void InitApp()
{
    h_std_out = GetStdHandle(STD_OUTPUT_HANDLE);

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
void RecievePacket()
{
    //memset(com.buffer_rx, 0, DATA_PACKET_LEN);
    if (recvfrom(com.socket, com.packet_rx, DATA_PACKET_LEN, 0, nullptr, nullptr) == SOCKET_ERROR) {
        Error("could not recieve data - socket error or timeout");
    }
}
void SendPacket()
{
    sendto(com.socket, com.packet_tx, ACK_PACKET_LEN, 0, (sockaddr*)&com.target_address, sizeof(com.target_address));
}
void SendACK(int32_t number)
{
    // TYPE
    type_t type = ACK;
    memcpy(&com.packet_tx[CRC_LEN], &type, TYPE_LEN);
    // NUMBER
    memcpy(&com.packet_tx[CRC_LEN + TYPE_LEN], &number, NUMBER_LEN);
    // CRC
    CRC32 crc32;
    std::string crc = crc32(&com.packet_tx[CRC_LEN], TYPE_LEN + NUMBER_LEN);
    memcpy(com.packet_tx, crc.c_str(), CRC_LEN);

    SendPacket();
}
bool isCrcOk(const char* data, size_t len)
{
    // load received
    char crc_tmp[CRC_LEN + 1] = "";
    std::string recieved_crc = strncpy(crc_tmp, data, CRC_LEN);
    // load rest of data and compute local
    CRC32 crc32;
    std::string computed_crc = crc32(&data[CRC_LEN], len - CRC_LEN);
    if (computed_crc != recieved_crc) {
        Warning("CRC check FAILED, recieved: " + recieved_crc + ", computed: " + computed_crc);
        return false;
    }
    return true;
}

//***************************** MAIN *****************************************
int main()
{
    InitWinsock();
    InitApp();
    InitPrinting();

    // PRINT INTRO
    SetPrintPos(0, 0);
    wprintf(L"|| Receiver v4.0 ||\n");
    SetPrintPos(0, Y_STATUS);
    wprintf(L"STATUS: waiting for START packet...\n");

    // RECIEVE FILES
    while (true) {

        int32_t ack_number = -2;
        int32_t packets_count;
        bool* is_received;
        unsigned int crc_fail_count = 0;
        unsigned int wrong_type_count = 0;
        unsigned int same_packet_received_count = 0;

        char filename[NAME_LEN + 1] = { '\0' };
        int32_t file_size = 0;
        char sha[SHA_LEN + 1] = { '\0' };
        char* data;

        // RECEIVE START PACKET
        while (true) {
            RecievePacket();
            // TYPE
            if (type_t(com.packet_rx[CRC_LEN]) == START) {
                // CRC
                if (isCrcOk(com.packet_rx, START_PACKET_LEN)) {
                    // FILE SIZE
                    memcpy(&file_size, &com.packet_rx[CRC_LEN + TYPE_LEN], NUMBER_LEN);
                    data = new char[file_size];
                    packets_count = (file_size / DATA_LEN) + 1;
                    is_received = new bool[packets_count]{false};
                    // SHA 256
                    memcpy(sha, &com.packet_rx[CRC_LEN + TYPE_LEN + NUMBER_LEN], SHA_LEN);
                    // FILENAME
                    strncpy(filename, &com.packet_rx[CRC_LEN + TYPE_LEN + NUMBER_LEN + SHA_LEN], NAME_LEN);

                    // SEND OK AND CONTINUE TO NEXT PART
                    ack_number++;
                    SendACK(ack_number);
                    break;
                }
                else {
                    SendACK(ack_number);
                    crc_fail_count++;
                }
            }
            else {
                SendACK(ack_number);
                wrong_type_count++;
                Warning("received packet type not START, waiting for START...");
            }
        }

        // PRINT START
        SetPrintPos(0, Y_STATUS);
        wprintf(L"STATUS: waiting for DATA packets...\n");
        SetPrintPos(0, Y_FILENAME);
        std::string filename_s = std::string(filename);
        wprintf(L"FILENAME: %s\n", std::wstring(filename_s.begin(), filename_s.end()).c_str());
        wprintf(L"FILE SIZE: %d bytes\n", file_size);
        // PREPARE DOWNLOAD BAR
        const int bar_height = ((packets_count - 1) / (screen_width - 2)) + 1;
        Y_CRC_FAIL = Y_BAR + bar_height + Y_FAILS_OFFSET;
        Y_WT_FAIL = Y_BAR + bar_height + Y_FAILS_OFFSET + 1;
        Y_SP_FAIL = Y_BAR + bar_height + Y_FAILS_OFFSET + 2;
        Y_INFO = Y_BAR + bar_height + Y_INFO_OFFSET;
        // --- rows
        SetPrintPos(0, Y_BAR - 1);
        wprintf(L"%s", std::wstring(screen_width, '-').c_str());
        SetPrintPos(0, Y_BAR + bar_height);
        wprintf(L"%s", std::wstring(screen_width, '-').c_str());
        // | | signs
        int remaining_tmp = packets_count;
        int bar_offset = 0;
        while (remaining_tmp > 0) {
            SetPrintPos(0, Y_BAR + bar_offset);
            wprintf(L"|");
            if (remaining_tmp >= screen_width - 2) {
                SetPrintPos(screen_width - 1, Y_BAR + bar_offset);
                wprintf(L"|");
            }
            else {
                SetPrintPos(remaining_tmp + 1, Y_BAR + bar_offset);
                wprintf(L"|");
            }
            remaining_tmp -= screen_width - 2;
            bar_offset++;
        }
        // fails
        SetPrintPos(0, Y_CRC_FAIL);
        wprintf(L"CRC fail count: %d\n", crc_fail_count);
        wprintf(L"Wrong packet type count: %d\n", wrong_type_count);
        wprintf(L"Same packet received count: %d\n", same_packet_received_count);


        // RECEIVE DATA PACKETS
        while (true) {
            RecievePacket();
            // TYPE GET/CHECK
            if (type_t(com.packet_rx[CRC_LEN]) != DATA) {
                SendACK(ack_number);
                SetPrintPos(0, Y_WT_FAIL);
                wprintf(L"Wrong packet type count: %d\n", ++wrong_type_count);
                Warning("received packet type not DATA");
                continue;
            }
            // NUMBER GET
			int32_t received_num;
			memcpy(&received_num, &com.packet_rx[CRC_LEN + TYPE_LEN], NUMBER_LEN);
            // CRC GET/CHECK
            if (!isCrcOk(com.packet_rx, DATA_PACKET_LEN)) {
                SendACK(ack_number);
                SetPrintPos(0, Y_CRC_FAIL);
                wprintf(L"CRC fail count: %d\n", ++crc_fail_count);
                if (received_num > ack_number && received_num < packets_count) {
                    SetPrintPos(IndexToPos(received_num));
                    wprintf(L"!");
                }
                continue;
            }
            // NUMBER CHECK
            if (received_num < 0 || received_num >= packets_count) {
                Error("received packet out of bounds, number = " + std::to_string(received_num));
                continue;
            }
            if (is_received[received_num]) {
                SendACK(ack_number);
                SetPrintPos(0, Y_SP_FAIL);
                wprintf(L"Same packet received count: %d\n", ++same_packet_received_count);
                Warning("already received packet number " + std::to_string(received_num));
                continue;
            }
            // DATA GET
            const int data_size_to_copy = (received_num == packets_count - 1) ? file_size % DATA_LEN : DATA_LEN;
            memcpy(&data[received_num * DATA_LEN], &com.packet_rx[CRC_LEN + TYPE_LEN + NUMBER_LEN], data_size_to_copy);
            is_received[received_num] = true;
            // UPDATE ACK NUMBER AND PRINT
            while (ack_number + 1 < packets_count && is_received[ack_number + 1]) { // look one ahead if received
                SetPrintPos(IndexToPos(ack_number)); // previous ack
                wprintf(L"▓");
                ack_number++; // update ack
            }
            SetPrintPos(IndexToPos(ack_number)); // current ack
            wprintf(L"█");
            // SEND ACK
            SendACK(ack_number);
            // IF RECEIVED PACKET IS STILL AHEAD OF ACK PRINT SOFT SQUARE
            if (received_num > ack_number) {
                SetPrintPos(IndexToPos(received_num));
                wprintf(L"▒");
            }
            // PRINT ACK NUMBER
            SetPrintPos(0, Y_NUMBER); // current ack
            wprintf(L"| ACK %d / %d |", ack_number, packets_count);
            // BREAK IF END
            if (ack_number == packets_count - 1) {
                break;
            }
        }

        // SHA256
        SHA256 sha256;
        std::string computed_sha = sha256(data, file_size);
        std::string received_sha = sha;
        if (computed_sha != received_sha) {
            Warning("SHA256 check FAILED\nrecieved: " + received_sha + "\ncomputed: " + computed_sha + "\nTrying to save file anyway...");
        }
        else {
            Info("SHA OK");
        }
        // SAVE FILE
        FILE* file = fopen(filename, "wb");
        if (!file) {
            Error("cannot open file for writing");
            break;
        }
        fwrite(data, sizeof(char), file_size, file);
        fclose(file);
        Info("File saved");
    
        // ASK FOR NEXT FILE
        delete[] data;
        delete[] is_received;
        Info("Communication finished, press Enter to recieve next file or 'q' + Enter to quit ...");
        if (getchar() == 'q') {
            Info("Goodbye...");
            break;
        } else {
            // CLEAR SCREEN
            std::wstring spaces = std::wstring(screen_width, ' ');
            for (int i = 0; i < 100; ++i) {
                SetPrintPos(0, i);
                wprintf(L"%s", spaces.c_str());
            }
        }
    }

    // Close communication
    closesocket(com.socket);
    return 0;
}




//// # TEST LOADING SCREEN #
//Sleep(500);
//int cursor = 0;
//for (int i = 0; i < packets_count; ++i) {
//    // NASTAV ACK NA NEJAKOU HODNOTU
//    ack_number++;
//    Info("cursor = " + std::to_string(cursor) + " x = " + std::to_string(IndexToPos(cursor).X) + " y = " + std::to_string(IndexToPos(cursor).Y));
//    // FUNKCE NA DOPLNENI semi transp a posledniho transp k poslednimu ack number
//    while (cursor < ack_number) {
//        SetPrintPos(IndexToPos(cursor));
//        wprintf(L"▒▓");
//        cursor++;
//        Info("cursor = " + std::to_string(cursor) + " x = " + std::to_string(IndexToPos(cursor).X) + " y = " + std::to_string(IndexToPos(cursor).Y));
//    }
//    SetPrintPos(IndexToPos(cursor));
//    wprintf(L"█");
//    Sleep(50);
//}