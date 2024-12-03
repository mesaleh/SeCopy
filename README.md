# SeCopy

A Windows file copy utility for pentesters that leverages backup/restore privileges to copy any file on the system - without requiring administrator rights. Perfect for security assessments and CTF challenges.

## Features
- Read any file regardless of NTFS permissions
- Access protected files like proof.txt and registry hives
- Write to protected locations like System32
- Replace locked system files

## Why SeCopy?
Having SeBackup/SeRestore privileges alone isn't enough - standard Windows tools like `copy` or `xcopy` don't use these privileges properly. SeCopy is specifically designed to leverage these privileges by:
- Using `CreateFileA` with `FILE_FLAG_BACKUP_SEMANTICS`
- Setting proper flags like `ACCESS_SYSTEM_SECURITY`
- Implementing the correct Windows API calls

Think of it like this: just like having admin rights doesn't make every program run as admin, having backup privileges doesn't make every copy tool use them. You need a tool specifically written to use these privileges.

## Usage
```
secopy.exe <source_file> <destination_file>
```

## Build
```
cl.exe /W4 /O2 /DWIN32 /D_WINDOWS secopy.cpp /link advapi32.lib user32.lib kernel32.lib
```

## Disclaimer
This tool is intended for authorized security testing only. Users are solely responsible for ensuring compliance with applicable laws and regulations. Unauthorized use is strictly prohibited.