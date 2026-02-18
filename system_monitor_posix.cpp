// system_monitor_posix.cpp
// Linux and macOS implementations of system monitoring functions.
// Compiled only on non-Windows platforms via CMakeLists.txt.
#if !defined(_WIN32)

#include "system_monitor.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <unistd.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(__APPLE__)
  #include <sys/sysctl.h>
  #include <sys/mount.h>
  #include <mach/mach.h>
  #include <mach/mach_host.h>
  #include <net/if_dl.h>
#elif defined(__linux__)
  #include <sys/sysinfo.h>
  #include <dirent.h>
#endif

// ── Shared state ──────────────────────────────────────────────────────────────
static float              netDownKBps    = 0.f;
static float              netUpKBps      = 0.f;
static unsigned long long netPrevBytesIn = 0;
static unsigned long long netPrevBytesOut= 0;
static unsigned long long netPrevTimeMs  = 0;

static unsigned long long NowMs() {
    using namespace std::chrono;
    return (unsigned long long)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

static void TrimStr(std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    s = (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::string ShellLine(const char* cmd) {
    FILE* p = popen(cmd, "r");
    if (!p) return "";
    char buf[512] = {};
    std::string r;
    while (fgets(buf, sizeof(buf), p)) {
        std::string l(buf); TrimStr(l);
        if (!l.empty()) { r = l; break; }
    }
    pclose(p);
    return r;
}

float GetNetDownKBps() { return netDownKBps; }
float GetNetUpKBps()   { return netUpKBps;   }

// ── Network byte sampling ─────────────────────────────────────────────────────
#if defined(__linux__)
static void SampleNetworkBytes(unsigned long long& bytesIn, unsigned long long& bytesOut) {
    bytesIn = bytesOut = 0;
    std::ifstream f("/proc/net/dev");
    std::string line;
    std::getline(f, line); std::getline(f, line); // skip 2 header lines
    while (std::getline(f, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string iface = line.substr(0, colon);
        TrimStr(iface);
        if (iface == "lo") continue;
        std::istringstream ss(line.substr(colon + 1));
        unsigned long long rb, rp, re, rr, rt, rm, rn, rc, tb;
        ss >> rb >> rp >> re >> rr >> rt >> rm >> rn >> rc >> tb;
        bytesIn  += rb;
        bytesOut += tb;
    }
}
#elif defined(__APPLE__)
static void SampleNetworkBytes(unsigned long long& bytesIn, unsigned long long& bytesOut) {
    bytesIn = bytesOut = 0;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return;
    for (struct ifaddrs* i = ifa; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_LINK) continue;
        if (i->ifa_flags & IFF_LOOPBACK) continue;
        struct if_data* ifd = (struct if_data*)i->ifa_data;
        if (!ifd) continue;
        bytesIn  += ifd->ifi_ibytes;
        bytesOut += ifd->ifi_obytes;
    }
    freeifaddrs(ifa);
}
#endif

void UpdateNetworkStats() {
    unsigned long long now = NowMs();
    unsigned long long bIn = 0, bOut = 0;
    SampleNetworkBytes(bIn, bOut);
    if (netPrevTimeMs > 0 && now > netPrevTimeMs) {
        double elapsed = (now - netPrevTimeMs) / 1000.0;
        netDownKBps = (float)((bIn  - netPrevBytesIn)  / elapsed / 1024.0);
        netUpKBps   = (float)((bOut - netPrevBytesOut) / elapsed / 1024.0);
        if (netDownKBps < 0.f) netDownKBps = 0.f;
        if (netUpKBps   < 0.f) netUpKBps   = 0.f;
    }
    netPrevBytesIn = bIn; netPrevBytesOut = bOut; netPrevTimeMs = now;
}

// ── CPU usage ─────────────────────────────────────────────────────────────────
#if defined(__linux__)
static unsigned long long cpuPrevIdle = 0, cpuPrevTotal = 0;

void InitializeSystemMonitoring() {
    SampleNetworkBytes(netPrevBytesIn, netPrevBytesOut);
    netPrevTimeMs = NowMs();
    // Prime CPU baseline
    std::ifstream f("/proc/stat");
    std::string line; std::getline(f, line);
    std::istringstream ss(line.substr(5));
    unsigned long long u, n, s, id, iow, irq, sirq;
    ss >> u >> n >> s >> id >> iow >> irq >> sirq;
    cpuPrevIdle  = id + iow;
    cpuPrevTotal = u + n + s + id + iow + irq + sirq;
}

void CleanupSystemMonitoring() { netPrevTimeMs = 0; netDownKBps = netUpKBps = 0.f; }

float GetRealCPUUsage() {
    std::ifstream f("/proc/stat");
    std::string line; std::getline(f, line);
    std::istringstream ss(line.substr(5));
    unsigned long long u, n, s, id, iow, irq, sirq;
    ss >> u >> n >> s >> id >> iow >> irq >> sirq;
    unsigned long long idle  = id + iow;
    unsigned long long total = u + n + s + id + iow + irq + sirq;
    unsigned long long dIdle  = idle  - cpuPrevIdle;
    unsigned long long dTotal = total - cpuPrevTotal;
    cpuPrevIdle = idle; cpuPrevTotal = total;
    return dTotal > 0 ? (float)(dTotal - dIdle) * 100.f / (float)dTotal : 0.f;
}

float GetRealRAMUsage() {
    std::ifstream f("/proc/meminfo");
    unsigned long long total = 0, avail = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("MemTotal:", 0) == 0)     sscanf(line.c_str(), "MemTotal: %llu", &total);
        if (line.rfind("MemAvailable:", 0) == 0) sscanf(line.c_str(), "MemAvailable: %llu", &avail);
    }
    return total > 0 ? (float)(total - avail) * 100.f / (float)total : 0.f;
}

unsigned long long GetTotalRAM_MB() {
    struct sysinfo si; sysinfo(&si);
    return (unsigned long long)si.totalram * si.mem_unit / (1024*1024);
}

unsigned long long GetUsedRAM_MB() {
    struct sysinfo si; sysinfo(&si);
    return (unsigned long long)(si.totalram - si.freeram - si.bufferram) * si.mem_unit / (1024*1024);
}

int GetProcessCount() {
    int count = 0;
    DIR* d = opendir("/proc");
    if (!d) return 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_type == DT_DIR && e->d_name[0] >= '1' && e->d_name[0] <= '9') count++;
    }
    closedir(d);
    return count;
}

unsigned long long GetSystemUptimeSeconds() {
    struct sysinfo si; sysinfo(&si);
    return (unsigned long long)si.uptime;
}

#elif defined(__APPLE__)

void InitializeSystemMonitoring() {
    SampleNetworkBytes(netPrevBytesIn, netPrevBytesOut);
    netPrevTimeMs = NowMs();
}
void CleanupSystemMonitoring() { netPrevTimeMs = 0; netDownKBps = netUpKBps = 0.f; }

float GetRealCPUUsage() {
    host_cpu_load_info_data_t info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    static unsigned long long prevUser=0, prevSys=0, prevIdle=0, prevNice=0;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        (host_info_t)&info, &count) != KERN_SUCCESS) return 0.f;
    unsigned long long user = info.cpu_ticks[CPU_STATE_USER];
    unsigned long long sys  = info.cpu_ticks[CPU_STATE_SYSTEM];
    unsigned long long idle = info.cpu_ticks[CPU_STATE_IDLE];
    unsigned long long nice = info.cpu_ticks[CPU_STATE_NICE];
    unsigned long long dUser = user-prevUser, dSys=sys-prevSys, dIdle=idle-prevIdle, dNice=nice-prevNice;
    prevUser=user; prevSys=sys; prevIdle=idle; prevNice=nice;
    unsigned long long total = dUser+dSys+dIdle+dNice;
    return total > 0 ? (float)(dUser+dSys+dNice)*100.f/(float)total : 0.f;
}

float GetRealRAMUsage() {
    vm_size_t pageSize; host_page_size(mach_host_self(), &pageSize);
    vm_statistics64_data_t vmStat; mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStat, &count);
    uint64_t total = GetTotalRAM_MB() * 1024ULL * 1024ULL;
    uint64_t free  = (uint64_t)(vmStat.free_count + vmStat.inactive_count) * pageSize;
    return total > 0 ? (float)(total - free) * 100.f / (float)total : 0.f;
}

unsigned long long GetTotalRAM_MB() {
    int64_t mem = 0; size_t sz = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &sz, nullptr, 0);
    return (unsigned long long)mem / (1024*1024);
}

unsigned long long GetUsedRAM_MB() {
    return (unsigned long long)(GetRealRAMUsage() * GetTotalRAM_MB() / 100.f);
}

int GetProcessCount() {
    int n = 0; size_t sz = sizeof(n);
    sysctlbyname("kern.nprocs", &n, &sz, nullptr, 0);
    return n;
}

unsigned long long GetSystemUptimeSeconds() {
    struct timeval tv; size_t sz = sizeof(tv);
    sysctlbyname("kern.boottime", &tv, &sz, nullptr, 0);
    return (unsigned long long)(time(nullptr) - tv.tv_sec);
}
#endif

// ── Shared POSIX implementations ─────────────────────────────────────────────
float GetRealDiskUsage() {
    struct statvfs st;
    if (statvfs("/", &st) == 0) {
        unsigned long long total = (unsigned long long)st.f_blocks * st.f_frsize;
        unsigned long long free  = (unsigned long long)st.f_bfree  * st.f_frsize;
        return total > 0 ? (float)(total - free) * 100.f / (float)total : 0.f;
    }
    return 0.f;
}

unsigned long long GetTotalDisk_GB() {
    struct statvfs st;
    if (statvfs("/", &st) == 0)
        return (unsigned long long)st.f_blocks * st.f_frsize / (1024ULL*1024*1024);
    return 0;
}

unsigned long long GetUsedDisk_GB() {
    struct statvfs st;
    if (statvfs("/", &st) == 0) {
        unsigned long long total = (unsigned long long)st.f_blocks * st.f_frsize;
        unsigned long long free  = (unsigned long long)st.f_bfree  * st.f_frsize;
        return (total - free) / (1024ULL*1024*1024);
    }
    return 0;
}

void GetHostName(char* buf, int sz) { gethostname(buf, sz); }

std::vector<DiskInfo> GetAllDrives() {
    std::vector<DiskInfo> result;
#if defined(__linux__)
    // Parse /proc/mounts for real filesystems
    std::ifstream f("/proc/mounts");
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string dev, mnt, fstype;
        ss >> dev >> mnt >> fstype;
        // Only show real block devices or common fs types
        if (fstype == "tmpfs" || fstype == "devtmpfs" || fstype == "sysfs" ||
            fstype == "proc"  || fstype == "cgroup"   || fstype == "devpts" ||
            fstype == "securityfs" || fstype == "pstore" || fstype == "efivarfs" ||
            fstype == "bpf"   || fstype == "tracefs"  || fstype == "debugfs" ||
            fstype == "hugetlbfs" || fstype == "mqueue" || fstype == "fusectl" ||
            fstype == "configfs" || fstype == "autofs" || fstype == "squashfs")
            continue;
        struct statvfs st;
        if (statvfs(mnt.c_str(), &st) != 0 || st.f_blocks == 0) continue;
        DiskInfo d;
        d.letter  = mnt[0];  // use first char of mount point as identifier
        d.totalGB = (unsigned long long)st.f_blocks * st.f_frsize / (1024ULL*1024*1024);
        unsigned long long freeB = (unsigned long long)st.f_bfree * st.f_frsize;
        unsigned long long totB  = (unsigned long long)st.f_blocks * st.f_frsize;
        d.usedGB  = (totB - freeB) / (1024ULL*1024*1024);
        d.usedPct = d.totalGB > 0 ? (float)d.usedGB / (float)d.totalGB * 100.f : 0.f;
        d.ready   = true;
        d.mountPoint = mnt;
        result.push_back(d);
        if (result.size() >= 8) break;
    }
#elif defined(__APPLE__)
    struct statfs* mounts; int n = getmntinfo(&mounts, MNT_NOWAIT);
    for (int i = 0; i < n; i++) {
        std::string fstype(mounts[i].f_fstypename);
        if (fstype == "devfs" || fstype == "autofs" || fstype == "map") continue;
        if (mounts[i].f_blocks == 0) continue;
        DiskInfo d;
        d.letter  = mounts[i].f_mntonname[0];
        d.totalGB = (unsigned long long)mounts[i].f_blocks * mounts[i].f_bsize / (1024ULL*1024*1024);
        unsigned long long freeB = (unsigned long long)mounts[i].f_bfree * mounts[i].f_bsize;
        unsigned long long totB  = (unsigned long long)mounts[i].f_blocks * mounts[i].f_bsize;
        d.usedGB  = (totB - freeB) / (1024ULL*1024*1024);
        d.usedPct = d.totalGB > 0 ? (float)d.usedGB / (float)d.totalGB * 100.f : 0.f;
        d.ready   = true;
        d.mountPoint = mounts[i].f_mntonname;
        result.push_back(d);
    }
#endif
    return result;
}

std::vector<AdapterInfo> GetAdapterList() {
    std::vector<AdapterInfo> result;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return result;
    for (struct ifaddrs* i = ifa; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        if (i->ifa_flags & IFF_LOOPBACK) continue;
        AdapterInfo info;
        info.name = i->ifa_name;
        char ipbuf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &((struct sockaddr_in*)i->ifa_addr)->sin_addr, ipbuf, sizeof(ipbuf));
        info.ipAddress = ipbuf;
        info.bytesIn = info.bytesOut = info.speed = 0;
        info.connected = !!(i->ifa_flags & IFF_RUNNING);
        result.push_back(info);
    }
    freeifaddrs(ifa);
    return result;
}

HardwareInfo GetHardwareInfo() {
    HardwareInfo info;
    info.gpuTempCelsius = -1.f;
    info.gpuTempValid   = false;

#if defined(__linux__)
    // CPU name from /proc/cpuinfo
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                info.cpuName = line.substr(colon + 2);
                TrimStr(info.cpuName);
                break;
            }
        }
    }
    // GPU name from lspci (if available)
    info.gpuName = ShellLine("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1 | sed 's/.*: //'");
    // OS version
    info.osVersion = ShellLine("cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2 | tr -d '\"'");
    if (info.osVersion.empty()) info.osVersion = ShellLine("uname -r");
    // GPU driver
    info.gpuDriverVersion = ShellLine("glxinfo 2>/dev/null | grep 'OpenGL version' | head -1 | awk '{print $NF}'");

#elif defined(__APPLE__)
    // CPU name via sysctl
    char cpuBuf[256] = {};
    size_t cpuSz = sizeof(cpuBuf);
    sysctlbyname("machdep.cpu.brand_string", cpuBuf, &cpuSz, nullptr, 0);
    info.cpuName = cpuBuf;
    TrimStr(info.cpuName);
    // GPU name via system_profiler
    info.gpuName = ShellLine("system_profiler SPDisplaysDataType 2>/dev/null | grep 'Chipset Model' | head -1 | sed 's/.*: //'");
    // OS version
    info.osVersion = ShellLine("sw_vers -productVersion 2>/dev/null");
    // GPU driver version
    info.gpuDriverVersion = ShellLine("system_profiler SPDisplaysDataType 2>/dev/null | grep 'Metal' | head -1 | sed 's/.*: //'");
#endif

    TrimStr(info.cpuName);
    TrimStr(info.gpuName);
    TrimStr(info.osVersion);
    TrimStr(info.gpuDriverVersion);
    return info;
}

#endif // !defined(_WIN32)
