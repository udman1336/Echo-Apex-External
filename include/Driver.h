#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <intrin.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "skcrypter.h"

#pragma once
#include <iostream>
#include <unordered_map>
#include <winternl.h>

#define MOUSE_LEFT_BUTTON_DOWN 0x0001
#define MOUSE_LEFT_BUTTON_UP   0x0002

namespace Driver
{
    extern HANDLE DriverHandle;
    extern INT32 ProcessID;
    extern uintptr_t base;
    extern bool EAC;

    bool SetupDriver();

    bool ReadPhysicalMemory(PVOID address, PVOID buffer, DWORD size);
    bool WritePhysicalMemory(PVOID address, PVOID buffer, DWORD size);

    bool BypassCR3();

    void Attach(int PID);
    int GetPID();
    uintptr_t GetBase();
    uint64_t PatternScan(const char* pattern, const std::string& mask);

    void MoveMouse(long X, long Y, unsigned short MouseFlags = 0, unsigned short ButtonFlags = 0);
    bool InitMouse();
    INT32 FindProcess(LPCTSTR process_name);
}

class CKernel
{
public:
    template <typename T>
    bool IsValidPointerAddress(T address)
    {
        return address > 0x400000 && address < 0x7FFFFFFFFFFFFFFF;
    }

    template <typename T>
    inline T read(uint64_t address) 
    {
        T value{};
        return value;
    }

    void NewFrame() {}

    template <typename T>
    T ReadBit(uint64_t address, uint8_t bitPosition) 
    {
        return 0;
    }

    template <typename T>
    void WriteBit(uint64_t address, uint8_t bitPosition, T value) {}

    template <typename T>
    inline T write(uint64_t address, T buffer)
    {
        return buffer;
    }

    inline bool read_array(const std::uintptr_t address, void* buffer, const std::size_t size)
    {
        if (buffer && size > 0)
        {
            memset(buffer, 0, size);
        }
        return false;
    }

    template<typename T>
    bool read_large_array(uint64_t address, T out[], size_t len)
    {
        if (out && len > 0)
        {
            memset(out, 0, sizeof(T) * len);
        }
        return false;
    }

    template<typename T>
    bool batch_read(uint64_t address, T* out, size_t count)
    {
        return false;
    }

    bool batch_read_bytes(uint64_t address, void* buffer, size_t size)
    {
        return false;
    }

    inline uintptr_t GetBaseAddress()
    {
        return Driver::base;
    }
};

inline CKernel Kernel;

HANDLE Driver::DriverHandle = nullptr;
INT32 Driver::ProcessID = 0;
uintptr_t Driver::base = 0;
bool Driver::EAC = true;

bool Driver::SetupDriver() 
{
    return false; 
}

bool Driver::ReadPhysicalMemory(PVOID address, PVOID buffer, DWORD size) 
{
    if (buffer && size > 0)
    {
        memset(buffer, 0, size);
    }
    return false;
}

bool Driver::WritePhysicalMemory(PVOID address, PVOID buffer, DWORD size) 
{
    return false;
}

bool Driver::BypassCR3() 
{
    return true;
}

void Driver::Attach(int PID) 
{
    ProcessID = PID;
}

int Driver::GetPID() 
{
    return ProcessID;
}

uintptr_t Driver::GetBase() 
{
    base = 0;
    return 0;
}

uint64_t Driver::PatternScan(const char* pattern, const std::string& mask)
{
    return 0;
}

void Driver::MoveMouse(long X, long Y, unsigned short MouseFlags, unsigned short ButtonFlags) {}

bool Driver::InitMouse() 
{
    return false;
}

INT32 Driver::FindProcess(LPCTSTR process_name) 
{
    PROCESSENTRY32 pt;
    HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    pt.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hsnap, &pt)) {
        do {
            if (!lstrcmpi(pt.szExeFile, process_name)) {
                CloseHandle(hsnap);
                ProcessID = pt.th32ProcessID;
                return pt.th32ProcessID;
            }
        } while (Process32Next(hsnap, &pt));
    }

    CloseHandle(hsnap);
    return ProcessID;
}

enum ConsoleColor {
    WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    RED = FOREGROUND_RED | FOREGROUND_INTENSITY,
    GREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    CYAN = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
};

void SetColor(HANDLE hConsole, ConsoleColor color) {
    SetConsoleTextAttribute(hConsole, color);
}