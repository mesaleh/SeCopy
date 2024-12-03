#include <windows.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

BOOL WriteToConsole(DWORD stdHandle, LPCSTR message) {
    HANDLE hConsole = GetStdHandle(stdHandle);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    DWORD written;
    size_t len = lstrlenA(message);
    if (len > MAXDWORD) return FALSE;  // Prevent overflow

    return WriteConsoleA(hConsole, message, (DWORD)len, &written, NULL) &&
           written == len;
}

void PrintError(LPCSTR message, DWORD error) {
    if (!message) return;

    CHAR errorBuffer[256] = {0};
    CHAR outputBuffer[512] = {0};
    DWORD length;
    
    length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errorBuffer,
        sizeof(errorBuffer),
        NULL
    );

    if (length > 0) {
        // Remove trailing newlines that FormatMessage adds
        while (length > 0 && (errorBuffer[length-1] == '\n' || errorBuffer[length-1] == '\r')) {
            errorBuffer[--length] = 0;
        }

        wsprintfA(outputBuffer, "%s Error: %lu - %s\r\n", message, error, errorBuffer);
        WriteToConsole(STD_ERROR_HANDLE, outputBuffer);
    } else {
        // Fallback if FormatMessage fails
        wsprintfA(outputBuffer, "%s Error: %lu\r\n", message, error);
        WriteToConsole(STD_ERROR_HANDLE, outputBuffer);
    }
}

BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege) 
{
    if (!hToken || !lpszPrivilege) return FALSE;

    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
        PrintError("LookupPrivilegeValue", GetLastError());
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = bEnablePrivilege ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        PrintError("AdjustTokenPrivileges", GetLastError());
        return FALSE;
    }

    // Check if the privilege was actually adjusted
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        PrintError("The process does not have the specified privilege", ERROR_NOT_ALL_ASSIGNED);
        return FALSE;
    }

    return TRUE;
}

BOOL CopyFileWithPrivileges(LPCSTR srcPath, LPCSTR dstPath) 
{
    if (!srcPath || !dstPath) return FALSE;

    HANDLE hSrc = INVALID_HANDLE_VALUE;
    HANDLE hDst = INVALID_HANDLE_VALUE;
    BOOL success = FALSE;

    __try {
        // Open source file with backup semantics
        hSrc = CreateFileA(srcPath, 
                          GENERIC_READ | ACCESS_SYSTEM_SECURITY,
                          FILE_SHARE_READ,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                          NULL);

        if (hSrc == INVALID_HANDLE_VALUE) {
            PrintError("Failed to open source file", GetLastError());
            __leave;
        }

        // Open destination with restore semantics
        hDst = CreateFileA(dstPath, 
                          GENERIC_WRITE | WRITE_DAC | WRITE_OWNER | ACCESS_SYSTEM_SECURITY,
                          0,
                          NULL,
                          CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                          NULL);

        if (hDst == INVALID_HANDLE_VALUE) {
            PrintError("Failed to open destination file", GetLastError());
            __leave;
        }

        // Copy the file contents
        BYTE buffer[8192] = {0};
        DWORD bytesRead, bytesWritten;

        while (ReadFile(hSrc, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            if (!WriteFile(hDst, buffer, bytesRead, &bytesWritten, NULL) ||
                bytesRead != bytesWritten) {
                PrintError("Write failed", GetLastError());
                __leave;
            }
        }

        // Check for read error
        if (GetLastError() != ERROR_HANDLE_EOF) {
            PrintError("Read failed", GetLastError());
            __leave;
        }

        success = TRUE;
    }
    __finally {
        if (hSrc != INVALID_HANDLE_VALUE) CloseHandle(hSrc);
        if (hDst != INVALID_HANDLE_VALUE) CloseHandle(hDst);
    }

    return success;
}

int main(int argc, CHAR* argv[]) 
{
    // Verify we're running in console mode
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) {
        return 1;
    }

    // Check arguments
    if (argc != 3) {
        WriteToConsole(STD_ERROR_HANDLE, "Usage: SeCopy.exe <source_file> <destination_file>\r\n");
        return 1;
    }

    // Get process token
    HANDLE hToken = NULL;
    BOOL success = FALSE;

    __try {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            PrintError("OpenProcessToken", GetLastError());
            __leave;
        }

        // Enable both privileges
        if (!SetPrivilege(hToken, SE_BACKUP_NAME, TRUE)) {
            WriteToConsole(STD_ERROR_HANDLE, "Failed to enable SeBackupPrivilege\r\n");
            __leave;
        }

        if (!SetPrivilege(hToken, SE_RESTORE_NAME, TRUE)) {
            WriteToConsole(STD_ERROR_HANDLE, "Failed to enable SeRestorePrivilege\r\n");
            __leave;
        }

        // Try to copy the file with privileges
        if (CopyFileWithPrivileges(argv[1], argv[2])) {
            WriteToConsole(STD_OUTPUT_HANDLE, "File copied successfully\r\n");
            success = TRUE;
        } else {
            WriteToConsole(STD_ERROR_HANDLE, "Failed to copy file\r\n");
        }
    }
    __finally {
        if (hToken) CloseHandle(hToken);
    }

    return success ? 0 : 1;
}