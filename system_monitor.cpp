#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <cstdio>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")

#include "system_monitor.h"

// Windows system monitoring handles
static PDH_HQUERY cpuQuery;
static PDH_HCOUNTER cpuCounter;
static bool pdhInitialized = false;

// Network monitoring state
static ULONGLONG netPrevBytesIn  = 0;
static ULONGLONG netPrevBytesOut = 0;
static ULONGLONG netPrevTime     = 0;
static float     netDownKBps     = 0.f;
static float     netUpKBps       = 0.f;

// Sums bytes across all non-loopback adapters using the classic GetIfTable API
static void SampleNetworkBytes(ULONGLONG& bytesIn, ULONGLONG& bytesOut) {
    bytesIn = bytesOut = 0;

    DWORD size = 0;
    GetIfTable(nullptr, &size, FALSE);
    if (size == 0) return;

    MIB_IFTABLE* table = reinterpret_cast<MIB_IFTABLE*>(new BYTE[size]);
    if (GetIfTable(table, &size, FALSE) == NO_ERROR) {
        for (DWORD i = 0; i < table->dwNumEntries; i++) {
            MIB_IFROW& row = table->table[i];
            if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (row.dwOperStatus != IF_OPER_STATUS_OPERATIONAL &&
                row.dwOperStatus != IF_OPER_STATUS_CONNECTED) continue;
            bytesIn  += row.dwInOctets;
            bytesOut += row.dwOutOctets;
        }
    }
    delete[] reinterpret_cast<BYTE*>(table);
}

void UpdateNetworkStats() {
    ULONGLONG now = GetTickCount64();
    ULONGLONG bytesIn = 0, bytesOut = 0;
    SampleNetworkBytes(bytesIn, bytesOut);

    if (netPrevTime > 0 && now > netPrevTime) {
        double elapsedSec = (now - netPrevTime) / 1000.0;
        netDownKBps = static_cast<float>((bytesIn  - netPrevBytesIn)  / elapsedSec / 1024.0);
        netUpKBps   = static_cast<float>((bytesOut - netPrevBytesOut) / elapsedSec / 1024.0);
        // Clamp negatives (counter wrap or adapter reset)
        if (netDownKBps < 0.f) netDownKBps = 0.f;
        if (netUpKBps   < 0.f) netUpKBps   = 0.f;
    }

    netPrevBytesIn  = bytesIn;
    netPrevBytesOut = bytesOut;
    netPrevTime     = now;
}

float GetNetDownKBps() { return netDownKBps; }
float GetNetUpKBps()   { return netUpKBps;   }

void InitializeSystemMonitoring() {
    // Baseline network sample so first delta is meaningful
    SampleNetworkBytes(netPrevBytesIn, netPrevBytesOut);
    netPrevTime = GetTickCount64();

    PDH_STATUS status = PdhOpenQuery(NULL, 0, &cpuQuery);
    if (status == ERROR_SUCCESS) {
        status = PdhAddEnglishCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuCounter);
        if (status == ERROR_SUCCESS) {
            // Collect initial baseline data
            PdhCollectQueryData(cpuQuery);
            Sleep(100); // Wait for initial sample
            PdhCollectQueryData(cpuQuery);
            pdhInitialized = true;
        }
    }
}

void CleanupSystemMonitoring() {
    if (pdhInitialized) {
        PdhCloseQuery(cpuQuery);
        pdhInitialized = false;
    }
    netPrevTime = 0;
    netDownKBps = netUpKBps = 0.f;
}

float GetRealCPUUsage() {
    if (!pdhInitialized) return 0.0f;
    
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
    return static_cast<float>(counterVal.doubleValue);
}

float GetRealRAMUsage() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return static_cast<float>(memInfo.dwMemoryLoad);
}

float GetRealDiskUsage() {
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        double usedBytes = static_cast<double>(totalNumberOfBytes.QuadPart - totalNumberOfFreeBytes.QuadPart);
        double totalBytes = static_cast<double>(totalNumberOfBytes.QuadPart);
        return static_cast<float>((usedBytes / totalBytes) * 100.0);
    }
    return 0.0f;
}

unsigned long long GetTotalRAM_MB() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return memInfo.ullTotalPhys / (1024 * 1024);
}

unsigned long long GetUsedRAM_MB() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024);
}

unsigned long long GetTotalDisk_GB() {
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        return totalNumberOfBytes.QuadPart / (1024 * 1024 * 1024);
    }
    return 0;
}

unsigned long long GetUsedDisk_GB() {
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA("C:\\", &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        return (totalNumberOfBytes.QuadPart - totalNumberOfFreeBytes.QuadPart) / (1024 * 1024 * 1024);
    }
    return 0;
}

int GetProcessCount() {
    DWORD processes[1024], bytesNeeded, processCount;
    if (EnumProcesses(processes, sizeof(processes), &bytesNeeded)) {
        processCount = bytesNeeded / sizeof(DWORD);
        return static_cast<int>(processCount);
    }
    return 0;
}

unsigned long long GetSystemUptimeSeconds() {
    return GetTickCount64() / 1000;
}

void GetHostName(char* buffer, int bufferSize) {
    DWORD size = bufferSize;
    if (!GetComputerNameA(buffer, &size)) {
        buffer[0] = '\0';
    }
}

// ── Hardware info (CPU name, GPU name, GPU temp) ──────────────────────────────
static std::string WmicQuery(const char* wmiClass, const char* field) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "wmic %s get %s /value 2>nul", wmiClass, field);
    FILE* p = _popen(cmd, "r");
    if (!p) return "";
    char buf[512] = {};
    std::string result;
    while (fgets(buf, sizeof(buf), p)) {
        std::string line(buf);
        // Line format: "Field=Value\r\n"
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            std::string val = line.substr(eq + 1);
            // Trim whitespace / CR / LF
            while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' '))
                val.pop_back();
            if (!val.empty()) { result = val; break; }
        }
    }
    _pclose(p);
    return result;
}

HardwareInfo GetHardwareInfo() {
    HardwareInfo info;
    info.gpuTempCelsius = -1.f;
    info.gpuTempValid   = false;

    // CPU name via wmic
    info.cpuName = WmicQuery("cpu", "Name");
    // Trim common padding like "                    " that wmic adds
    auto trim = [](std::string& s) {
        size_t start = s.find_first_not_of(' ');
        size_t end   = s.find_last_not_of(' ');
        if (start == std::string::npos) s = "";
        else s = s.substr(start, end - start + 1);
    };
    trim(info.cpuName);

    // GPU name via wmic (first discrete adapter)
    // Run wmic path Win32_VideoController get Name to get all GPUs
    FILE* p = _popen("wmic path Win32_VideoController get Name /value 2>nul", "r");
    if (p) {
        char buf[256] = {};
        while (fgets(buf, sizeof(buf), p)) {
            std::string line(buf);
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                std::string val = line.substr(eq + 1);
                while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' '))
                    val.pop_back();
                if (!val.empty()) {
                    // Prefer discrete GPU (skip "Microsoft Basic", "Remote Desktop")
                    if (val.find("Microsoft") == std::string::npos &&
                        val.find("Remote")    == std::string::npos &&
                        val.find("Virtual")   == std::string::npos) {
                        info.gpuName = val;
                        break;
                    }
                    if (info.gpuName.empty()) info.gpuName = val; // fallback
                }
            }
        }
        _pclose(p);
    }
    trim(info.gpuName);

    // OS version via wmic
    info.osVersion = WmicQuery("os", "Version");
    trim(info.osVersion);

    // GPU driver version via wmic
    info.gpuDriverVersion = WmicQuery("path Win32_VideoController", "DriverVersion");
    trim(info.gpuDriverVersion);

    // GPU temp: try wmic thermal zones - some systems expose GPU zone
    // This is best-effort; works on some AMD/Intel systems via ACPI
    FILE* tp = _popen(
        "wmic /namespace:\\\\root\\wmi path MSAcpi_ThermalZoneTemperature "
        "get CurrentTemperature,InstanceName /value 2>nul", "r");
    if (tp) {
        char buf[256] = {};
        float lastTemp = -1.f;
        std::string lastName;
        while (fgets(buf, sizeof(buf), tp)) {
            std::string line(buf);
            auto eq = line.find('=');
            if (eq == std::string::npos) {
                // Blank line = end of object; check if it's a GPU zone
                if (lastTemp > 0.f) {
                    std::string ln = lastName;
                    for (auto& ch : ln) ch = (char)tolower((unsigned char)ch);
                    if (ln.find("gpu") != std::string::npos ||
                        ln.find("vga") != std::string::npos ||
                        ln.find("gfx") != std::string::npos) {
                        info.gpuTempCelsius = lastTemp;
                        info.gpuTempValid   = true;
                        break;
                    }
                }
                lastTemp = -1.f; lastName = "";
                continue;
            }
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            while (!val.empty() && (val.back()=='\r'||val.back()=='\n'||val.back()==' ')) val.pop_back();
            if (key == "CurrentTemperature" && !val.empty()) {
                try { lastTemp = (std::stof(val) / 10.f) - 273.15f; } catch (...) {}
            } else if (key == "InstanceName") {
                lastName = val;
            }
        }
        _pclose(tp);
    }

    return info;
}

std::vector<DiskInfo> GetAllDrives() {
    std::vector<DiskInfo> result;
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (!(drives & (1 << i))) continue;
        char root[4] = { (char)('A' + i), ':', '\\', '\0' };
        UINT type = GetDriveTypeA(root);
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) continue;

        DiskInfo d;
        d.letter = 'A' + i;
        d.ready  = false;
        d.totalGB = d.usedGB = 0;
        d.usedPct = 0.f;

        ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
        if (GetDiskFreeSpaceExA(root, &freeBytes, &totalBytes, &totalFreeBytes)) {
            d.totalGB = totalBytes.QuadPart / (1024ULL * 1024 * 1024);
            d.usedGB  = (totalBytes.QuadPart - totalFreeBytes.QuadPart) / (1024ULL * 1024 * 1024);
            d.usedPct = d.totalGB > 0
                ? static_cast<float>(d.usedGB) / static_cast<float>(d.totalGB) * 100.f
                : 0.f;
            d.ready = true;
        }
        result.push_back(d);
    }
    return result;
}

std::vector<AdapterInfo> GetAdapterList() {
    std::vector<AdapterInfo> result;

    // Build traffic lookup from GetIfTable (keyed by adapter index)
    DWORD ifBufSize = 0;
    GetIfTable(nullptr, &ifBufSize, FALSE);
    std::vector<BYTE> ifBuf(ifBufSize > 0 ? ifBufSize : 1);
    MIB_IFTABLE* table = reinterpret_cast<MIB_IFTABLE*>(ifBuf.data());
    bool hasIfTable = (ifBufSize > 0 && GetIfTable(table, &ifBufSize, FALSE) == NO_ERROR);

    // Use GetAdaptersInfo as primary source - gives friendly Description + IP
    ULONG ipBufSize = 0;
    GetAdaptersInfo(nullptr, &ipBufSize);
    if (ipBufSize == 0) return result;

    std::vector<BYTE> ipBuf(ipBufSize);
    IP_ADAPTER_INFO* adapterList = reinterpret_cast<IP_ADAPTER_INFO*>(ipBuf.data());
    if (GetAdaptersInfo(adapterList, &ipBufSize) != NO_ERROR) return result;

    for (IP_ADAPTER_INFO* ai = adapterList; ai != nullptr; ai = ai->Next) {
        AdapterInfo info;

        // Use Description as the friendly name (e.g. "Intel(R) Wi-Fi 6 AX201")
        info.name      = ai->Description[0] ? ai->Description : ai->AdapterName;
        info.ipAddress = ai->IpAddressList.IpAddress.String;
        info.bytesIn   = 0;
        info.bytesOut  = 0;
        info.speed     = 0;
        info.connected = false;

        // Enrich with traffic data from GetIfTable by matching index
        if (hasIfTable) {
            for (DWORD i = 0; i < table->dwNumEntries; i++) {
                MIB_IFROW& row = table->table[i];
                if (row.dwIndex == ai->Index) {
                    info.bytesIn  = row.dwInOctets;
                    info.bytesOut = row.dwOutOctets;
                    info.speed    = row.dwSpeed;
                    info.connected = (row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL ||
                                      row.dwOperStatus == IF_OPER_STATUS_CONNECTED);
                    break;
                }
            }
        }

        result.push_back(info);
    }
    return result;
}

#endif // _WIN32
