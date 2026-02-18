#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <string>
#include <vector>

struct DiskInfo {
    char letter;        // e.g. 'C'
    unsigned long long totalGB;
    unsigned long long usedGB;
    float usedPct;
    bool ready;
};

struct HardwareInfo {
    std::string cpuName;
    std::string gpuName;
    std::string osVersion;       // e.g. "10.0.22631"
    std::string gpuDriverVersion;
    float       gpuTempCelsius;  // -1 if unavailable
    bool        gpuTempValid;
};

struct AdapterInfo {
    std::string name;
    std::string ipAddress;
    unsigned long long bytesIn;
    unsigned long long bytesOut;
    unsigned long speed;  // bps
    bool connected;
};

// Windows system monitoring functions
void InitializeSystemMonitoring();
void CleanupSystemMonitoring();
void UpdateNetworkStats();
float GetRealCPUUsage();
float GetRealRAMUsage();
float GetRealDiskUsage();
unsigned long long GetTotalRAM_MB();
unsigned long long GetUsedRAM_MB();
unsigned long long GetTotalDisk_GB();
unsigned long long GetUsedDisk_GB();
int GetProcessCount();
unsigned long long GetSystemUptimeSeconds();
void GetHostName(char* buffer, int bufferSize);
float GetNetDownKBps();
float GetNetUpKBps();
std::vector<AdapterInfo> GetAdapterList();
std::vector<DiskInfo>    GetAllDrives();
HardwareInfo             GetHardwareInfo();

#endif // SYSTEM_MONITOR_H
