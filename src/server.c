#include <stdio.h>
#include <windows.h>
#include <winsock.h>

#define WSA_VERSION_CODE 0x202
SOCKET server_socket, client_socket;

DWORD WINAPI clienthandler(LPVOID);

int main(int argc, char** argv){
    
    WSADATA wsadata;
    HANDLE thread_handle;
    DWORD t_id, t_exit_code, bind_result;
    SOCKADDR_IN server_addr;

    //INITIALIZE WIN SOCKET API
    if(WSAStartup(WSA_VERSION_CODE, &wsadata) == 0){
        printf("WSA initialized succesfully \n");
    }else{
        printf("WSA initialization failed. Err code:%d", getLastError());
        WSACleanup();
        return -1;
    }

    //CREATE SERVER SOCKET TO BIND
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server_socket == INVALID_SOCKET){
        printf("");
    }

    return 0;
}

DWORD WINAPI clientHandler(LPVOID params){

}