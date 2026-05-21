#pragma once
#include <windows.h>

extern "C" {
    // Отримання доступної фізичної оперативної пам'яті
    __declspec(dllexport) unsigned long long GetAvailableRAM_MB();

    // Отримання загальної кількості логічних ядер процесора
    __declspec(dllexport) unsigned int GetTotalCPUCores();

    // Виконання системної команди через WinAPI (CreateProcess) та перехоплення STDOUT
    __declspec(dllexport) void ExecuteSystemCommand(const char* command, char* outputBuffer, int bufferSize);

    // Отримання обсягу оперативної пам'яті, яку зараз споживають усі запущені ВМ (VirtualBoxVM.exe)
    __declspec(dllexport) unsigned long long GetTotalVBoxMemoryUsageMB();
}