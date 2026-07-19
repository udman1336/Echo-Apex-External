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

typedef struct _ReadWrite {
    INT32 Security;
    INT32 ProcessID;
    ULONGLONG Address;
    ULONGLONG Buffer;
    ULONGLONG Size;
    BOOLEAN Write;
    BOOLEAN EAC;
} RW, * PRW;

typedef struct _MU {
    long X;
    long Y;
    unsigned short MouseFlags;
    unsigned short ButtonFlags;
} MU, * PMU;

typedef struct INITMOUSE
{
    bool Init;
}INITMOUSEREQUEST, * INITMOUSE_;

typedef struct _DTB {
    INT32 Security;
    INT32 ProcessID;
    bool* Operation;
} DTB, * PDTB;

typedef struct _BA {
    INT32 Security;
    INT32 ProcessID;
    ULONGLONG* Address;
} BA, * PBA;

namespace Driver
{
    extern HANDLE DriverHandle;
    extern INT32 ProcessID;
    extern uintptr_t base;
    extern bool EAC;

    DWORD GetCPUIDHash();
    const char* GetCPUVendorPrefix();
    void GetDynamicLinkName(char* buffer, size_t bufferSize);

    bool SetupDriver();

    template<typename T>
    bool SendCommand(DWORD ioctl, T* inBuffer);

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
        if (address <= 0x400000 || address == 0xCCCCCCCCCCCCCCCC || reinterpret_cast<void*>(address) == nullptr || address >
            0x7FFFFFFFFFFFFFFF) {
            return false;
        }
        return true;
    }

    template <typename T>
    inline T read(uint64_t address) {
        T value = read_<T>(address);
        return value;
    }

    void NewFrame() {

    }

    template <typename T>
    T ReadBit(uint64_t address, uint8_t bitPosition) {
        return (read<T>(address) >> bitPosition);
    }

    template <typename T>
    void WriteBit(uint64_t address, uint8_t bitPosition, T value) {
        uint8_t byteAtOffset = read<uint8_t>(address);
        byteAtOffset |= (value << bitPosition);
        write<uint8_t>(address, byteAtOffset);
    }

    template <typename T>
    inline T write(uint64_t address, T buffer)
    {
        if (!IsValidPointerAddress<uintptr_t>(address)) return {};
        Driver::WritePhysicalMemory((PVOID)address, &buffer, sizeof(T));
        return buffer;
    }

    inline bool read_array(const std::uintptr_t address, void* buffer, const std::size_t size)
    {
        if (buffer == nullptr || size == 0) {
            return false;
        }

        Driver::ReadPhysicalMemory(reinterpret_cast<PVOID>(address), buffer, static_cast<DWORD>(size));
    }

    template<typename T>
    bool read_large_array(uint64_t address, T out[], size_t len)
    {
        size_t real_size = sizeof(T) * len;
        size_t read_size = 0;
        T* temp = out;

        while (read_size < real_size) {
            BYTE* buffer = new BYTE[512];

            size_t diff = real_size - read_size;
            if (diff > 512)
                diff = 512;

            Driver::ReadPhysicalMemory(PVOID(address + read_size), buffer, diff);

            memcpy(temp, buffer, diff);

            read_size += diff;
            temp += (diff / sizeof(T));

            delete[] buffer;
        }

        return true;
    }

    template<typename T>
    bool batch_read(uint64_t address, T* out, size_t count)
    {
        return read_large_array(address, out, count);
    }

    bool batch_read_bytes(uint64_t address, void* buffer, size_t size)
    {
        return read_array(address, buffer, size);
    }

    inline uintptr_t GetBaseAddress()
    {
        return Driver::base;
    }

    std::vector<int> PatternToByte(const char* pattern)
    {
        std::vector<int> bytes;
        char* start = const_cast<char*>(pattern);
        char* end = const_cast<char*>(pattern) + strlen(pattern);

        for (char* current = start; current < end; ++current)
        {
            if (*current == '?')
            {
                ++current;
                if (*current == '?') ++current;
                bytes.push_back(-1);
            }
            else
            {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    }

    unsigned long long FindSignature(uintptr_t moduleBase, size_t moduleSize, const char* pattern)
    {
        std::vector<int> signature = PatternToByte(pattern);
        size_t signatureSize = signature.size();

        for (uintptr_t i = 0; i < moduleSize - signatureSize; ++i)
        {
            bool found = true;
            for (size_t j = 0; j < signatureSize; ++j)
            {
                if (signature[j] != -1 && signature[j] != this->read<unsigned char>(moduleBase + i + j))
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                return moduleBase + i;
            }
        }

        return 0;
    }
private:
    template <typename T>
    inline T read_(uint64_t address)
    {
        T buffer{ };
        Driver::ReadPhysicalMemory((PVOID)address, &buffer, sizeof(T));
        return buffer;
    }
};

inline CKernel Kernel;

HANDLE Driver::DriverHandle = nullptr;
INT32 Driver::ProcessID = 0;
uintptr_t Driver::base = 0;
bool Driver::EAC = true;

class CCodes {
private:
    ULONG GetPML4Code;
    ULONG ReadWriteOperationCode;
    ULONG MouseMovementCode;
    ULONG InitMouseMovementCode;
    ULONG GetBaseAddressCode;
    ULONG SecurityCode;
    ULONG MouseCorrection;

public:
    inline void SetGetPML4Code(ULONG NewCode) { GetPML4Code = NewCode; }
    inline void SetReadWriteOperationCode(ULONG NewCode) { ReadWriteOperationCode = NewCode; }
    inline void SetMouseMovementCode(ULONG NewCode) { MouseMovementCode = NewCode; }
    inline void SetInitMouseMovementCode(ULONG NewCode) { InitMouseMovementCode = NewCode; }
    inline void SetGetBaseAddressCode(ULONG NewCode) { GetBaseAddressCode = NewCode; }
    inline void SetSecurityCode(ULONG NewCode) { SecurityCode = NewCode; }
    inline void SetMouseCorrection(ULONG NewCorrection) { MouseCorrection = NewCorrection; }

    inline ULONG GetGetPML4Code() const { return GetPML4Code; }
    inline ULONG GetReadWriteOperationCode() const { return ReadWriteOperationCode; }
    inline ULONG GetMouseMovementCode() const { return MouseMovementCode; }
    inline ULONG GetInitMouseMovementCode() const { return InitMouseMovementCode; }

    inline ULONG GetGetBaseAddressCode() const { return GetBaseAddressCode; }
    inline ULONG GetSecurityCode() const { return SecurityCode; }
    inline ULONG GetMouseCorrection() const { return MouseCorrection; }
};
inline CCodes Codes;

DWORD Driver::GetCPUIDHash() {
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 0);
    return cpuInfo[0] ^ cpuInfo[1] ^ cpuInfo[2] ^ cpuInfo[3] * 0x8329;
}

const char* Driver::GetCPUVendorPrefix() {
    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 0);

    static char vendor[13] = { 0 };
    *(int*)&vendor[0] = cpuInfo[1];
    *(int*)&vendor[4] = cpuInfo[3];
    *(int*)&vendor[8] = cpuInfo[2];
    return vendor;
}

void Driver::GetDynamicLinkName(char* buffer, size_t bufferSize) {
    auto prefix = GetCPUVendorPrefix();
    DWORD hash = GetCPUIDHash();

    snprintf(buffer, bufferSize, "\\\\.\\%s_%08X", prefix, hash);
}

bool Driver::SetupDriver() {
    char DynamicPath[256];
    GetDynamicLinkName(DynamicPath, sizeof(DynamicPath));

    ULONG BaseHash = GetCPUIDHash();

    Codes.SetGetPML4Code(BaseHash * 0x100);
    Codes.SetReadWriteOperationCode(BaseHash * 0x200);
    Codes.SetMouseMovementCode(BaseHash * 0x300);
    Codes.SetGetBaseAddressCode(BaseHash * 0x400);
    Codes.SetSecurityCode(BaseHash * 0x500);
    Codes.SetMouseCorrection(BaseHash * 0x6);

    DriverHandle = CreateFileA(DynamicPath, GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (!DriverHandle || (DriverHandle == INVALID_HANDLE_VALUE))
        return false;

    return true;
}

template<typename T>
bool Driver::SendCommand(DWORD ioctl, T* inBuffer) {
    return DeviceIoControl(
        DriverHandle,
        ioctl,
        inBuffer,
        sizeof(T),
        nullptr,
        0,
        nullptr,
        nullptr
    );
}

bool Driver::ReadPhysicalMemory(PVOID address, PVOID buffer, DWORD size) {
    _ReadWrite Arguments = { 0 };
    Arguments.Security = Codes.GetSecurityCode();
    Arguments.Address = (ULONGLONG)address;
    Arguments.Buffer = (ULONGLONG)buffer;
    Arguments.Size = size;
    Arguments.ProcessID = ProcessID;
    Arguments.Write = FALSE;
    Arguments.EAC = EAC;

    return SendCommand(Codes.GetReadWriteOperationCode(), &Arguments);
}

bool Driver::WritePhysicalMemory(PVOID address, PVOID buffer, DWORD size) {
    _ReadWrite Arguments = { 0 };
    Arguments.Security = Codes.GetSecurityCode();
    Arguments.Address = (ULONGLONG)address;
    Arguments.Buffer = (ULONGLONG)buffer;
    Arguments.Size = size;
    Arguments.ProcessID = ProcessID;
    Arguments.Write = TRUE;
    Arguments.EAC = EAC;

    return SendCommand(Codes.GetReadWriteOperationCode(), &Arguments);
}

bool Driver::BypassCR3() {
    bool Ret = false;
    _DTB Arguments = { 0 };
    Arguments.Security = Codes.GetSecurityCode();
    Arguments.ProcessID = ProcessID;
    Arguments.Operation = (bool*)&Ret;

    if (EAC)
        return SendCommand(Codes.GetGetPML4Code(), &Arguments);

    return true;
}

void Driver::Attach(int PID) {
    ProcessID = PID;
}

int Driver::GetPID() {
    return ProcessID;
}

uintptr_t Driver::GetBase() {
    uintptr_t image_address = 0;
    _BA Arguments = { 0 };

    Arguments.Security = Codes.GetSecurityCode();
    Arguments.ProcessID = ProcessID;
    Arguments.Address = (ULONGLONG*)&image_address;
    SendCommand(Codes.GetGetBaseAddressCode(), &Arguments);

    base = image_address;
    return image_address;
}

void Driver::MoveMouse(long X, long Y, unsigned short MouseFlags, unsigned short ButtonFlags) {
    _MU Arguments = { 0 };
    Arguments.X = X + Codes.GetMouseCorrection();
    Arguments.Y = Y + Codes.GetMouseCorrection();
    Arguments.MouseFlags = MouseFlags;
    Arguments.ButtonFlags = ButtonFlags;
    SendCommand(Codes.GetMouseMovementCode(), &Arguments);
}

bool Driver::InitMouse() {
    INITMOUSE Arguments = { 0 };
    Arguments.Init = true;
    return SendCommand(Codes.GetInitMouseMovementCode(), &Arguments);
}

INT32 Driver::FindProcess(LPCTSTR process_name) {
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
