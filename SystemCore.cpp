#include "pch.h"
#include "SystemCore.h"
#include <string>
#include <TlHelp32.h>
#include <Psapi.h>

unsigned long long GetAvailableRAM_MB() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullAvailPhys / (1024 * 1024);
    }
    return 0;
}

unsigned int GetTotalCPUCores() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
}

void ExecuteSystemCommand(const char* command, char* outputBuffer, int bufferSize) {
    if (bufferSize <= 0 || outputBuffer == nullptr) return;

    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Створюємо анонімний канал (pipe) для STDOUT
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        strncpy_s(outputBuffer, bufferSize, "Error: CreatePipe failed.", _TRUNCATE);
        return;
    }

    // Забороняємо успадкування дескриптора читання новим процесом
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = hStdOutWrite;
    si.hStdOutput = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Ховаємо чорне вікно консолі

    // CreateProcess вимагає змінний буфер для команди
    char cmdBuffer[1024];
    strncpy_s(cmdBuffer, sizeof(cmdBuffer), command, _TRUNCATE);

    // Запускаємо процес
    BOOL bSuccess = CreateProcessA(
        NULL,           // Ім'я модуля (використовуємо command line)
        cmdBuffer,      // Командний рядок
        NULL,           // Атрибути безпеки процесу
        NULL,           // Атрибути безпеки потоку
        TRUE,           // Успадкування дескрипторів
        CREATE_NO_WINDOW, // Створення без вікна
        NULL,           // Блок середовища
        NULL,           // Поточний каталог
        &si,            // STARTUPINFO
        &pi             // PROCESS_INFORMATION
    );

    if (!bSuccess) {
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdOutRead);
        strncpy_s(outputBuffer, bufferSize, "Error: CreateProcess failed.", _TRUNCATE);
        return;
    }

    // Закриваємо дескриптор запису в нашому процесі, інакше ReadFile зависне
    CloseHandle(hStdOutWrite);

    // Читаємо результат з каналу
    DWORD dwRead;
    CHAR chBuf[4096];
    std::string result = "";
    BOOL bReadSuccess = FALSE;

    for (;;) {
        bReadSuccess = ReadFile(hStdOutRead, chBuf, sizeof(chBuf) - 1, &dwRead, NULL);
        if (!bReadSuccess || dwRead == 0) break;
        chBuf[dwRead] = '\0';
        result += chBuf;
    }

    // Очікуємо повного завершення дочірнього процесу
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);

    strncpy_s(outputBuffer, bufferSize, result.c_str(), _TRUNCATE);
}

unsigned long long GetTotalVBoxMemoryUsageMB() {
    unsigned long long totalMemoryUsageMB = 0;
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    // Створюємо знімок усіх процесів у системі
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return 0;
    }

    // Проходимо по всіх процесах
    do {
        // Шукаємо процеси, що відповідають за запущені віртуальні машини VirtualBox
        if (std::wstring(pe32.szExeFile) == L"VirtualBoxVM.exe") {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess != NULL) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    // WorkingSetSize - це обсяг фізичної пам'яті в байтах
                    totalMemoryUsageMB += (pmc.WorkingSetSize / (1024 * 1024));
                }
                CloseHandle(hProcess);
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return totalMemoryUsageMB;
}