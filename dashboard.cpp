#include "dashboard.h"
#include "theme.h"
#include "system_monitor.h"
#include "speedtest.h"
#include "raylib.h"
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <thread>
#include <atomic>

// ── Global definitions ────────────────────────────────────────────────────────
SystemStats      stats;
std::vector<LogEntry> logEntries;
float            timeAccumulator = 0.0f;
Font             retroFont;
bool             hasCustomFont   = false;
int              currentMenu     = MENU_DASHBOARD;
int              selectedOption  = 0;
bool             showMenu        = false;
float            menuBlinkTimer  = 0.0f;
DashboardWidgets widgets;
bool             showWidgetMenu  = false;
int              selectedWidget  = 0;
bool             isFirstRun      = true;

// ── Widget state helper ───────────────────────────────────────────────────────
void GetWidgetStates(bool* states[WIDGET_COUNT]) {
    states[WIDGET_CPU]          = &widgets.showCPU;
    states[WIDGET_RAM]          = &widgets.showRAM;
    states[WIDGET_DISK]         = &widgets.showDisk;
    states[WIDGET_NETWORK]      = &widgets.showNetwork;
    states[WIDGET_ANOMALY]      = &widgets.showAnomaly;
    states[WIDGET_LOG]          = &widgets.showSystemLog;
    states[WIDGET_TIME]         = &widgets.showSystemTime;
    states[WIDGET_PROCESSES]    = &widgets.showProcesses;
    states[WIDGET_UPTIME]       = &widgets.showUptime;
    states[WIDGET_COMPUTERNAME] = &widgets.showComputerName;
}

// ── Log ───────────────────────────────────────────────────────────────────────
void AddLogEntry(const std::string& message, Color entryColor) {
    logEntries.push_back({ message, GetTime(), entryColor });
    if (logEntries.size() > 40)
        logEntries.erase(logEntries.begin());
}

// ── Stats ─────────────────────────────────────────────────────────────────────
void InitializeStats() {
    srand(static_cast<unsigned>(time(nullptr)));
    stats = SystemStats{};
    stats.cpu      = stats.targetCpu      = 45.f;
    stats.ram      = stats.targetRam      = 60.f;
    stats.disk     = stats.targetDisk     = 70.f;
    stats.netDown  = stats.targetNetDown  = 15.f;
    stats.netUp    = stats.targetNetUp    = 3.f;
    stats.useRealData = false;
    stats.processCount  = 120;
    stats.uptimeSeconds = 3600;
    GetHostName(stats.computerName, sizeof(stats.computerName));
}

void UpdateStats(float deltaTime) {
    // Network is always real regardless of mode
    static float netTimer = 0.f;
    netTimer += deltaTime;
    if (netTimer >= 1.f) {
        UpdateNetworkStats();
        stats.targetNetDown = GetNetDownKBps();
        stats.targetNetUp   = GetNetUpKBps();
        netTimer = 0.f;
    }

    if (stats.useRealData) {
        stats.targetCpu  = GetRealCPUUsage();
        stats.targetRam  = GetRealRAMUsage();
        stats.targetDisk = GetRealDiskUsage();

        static float infoTimer = 0.f;
        infoTimer += deltaTime;
        if (infoTimer >= 2.f) {
            stats.processCount  = GetProcessCount();
            stats.uptimeSeconds = GetSystemUptimeSeconds();
            infoTimer = 0.f;
        }
    } else {
        if (GetRandomValue(0, 100) < 2) {
            stats.targetCpu  = static_cast<float>(GetRandomValue(20, 80));
            stats.targetRam  = static_cast<float>(GetRandomValue(40, 90));
            stats.targetDisk = static_cast<float>(GetRandomValue(60, 95));
        }
        stats.processCount  = 120 + GetRandomValue(-5, 5);
        stats.uptimeSeconds = static_cast<unsigned long long>(GetTime()) + 3600;
    }
    const float S = stats.useRealData ? 5.f : 2.f;
    stats.cpu     += (stats.targetCpu     - stats.cpu)     * deltaTime * S;
    stats.ram     += (stats.targetRam     - stats.ram)     * deltaTime * S;
    stats.disk    += (stats.targetDisk    - stats.disk)    * deltaTime * S;
    stats.netDown += (stats.targetNetDown - stats.netDown) * deltaTime * S;
    stats.netUp   += (stats.targetNetUp   - stats.netUp)   * deltaTime * S;
}

void GenerateRandomLog() {
    static const char* msgs[] = {
        "[INFO] Boot sequence complete",
        "[NET] Packet received from 192.168.1.1",
        "[CPU] Core 0 temperature normal",
        "[MEM] Cache flushed successfully",
        "[DISK] Defragmentation complete",
        "[SECURITY] Firewall active",
        "[NET] Connection established",
        "[SYSTEM] Heartbeat signal received",
        "[PROC] Task scheduler running",
        "[IO] Buffer cleared",
        "[NET] Data transmission complete",
        "[CPU] Frequency scaling active",
        "[TEMP] Thermal sensors calibrated",
        "[POWER] UPS status: ONLINE",
        "[LOG] Rotation complete"
    };
    AddLogEntry(msgs[GetRandomValue(0, 14)], GREEN_PHOSPHOR);
}

// ── Drawing helpers ───────────────────────────────────────────────────────────
void DrawProgressBar(int x, int y, int w, int h, float value, Color barColor) {
    DrawRectangle(x, y, w, h, COLOR_BLACK);
    DrawRectangleLines(x, y, w, h, DIM_GREEN);
    int fill = static_cast<int>((w - 4) * (value / 100.f));
    DrawRectangle(x + 2, y + 2, fill, h - 4, barColor);
    std::string txt = std::to_string(static_cast<int>(value)) + "%";
    int tw = MeasureText(txt.c_str(), 16);
    DrawText(txt.c_str(), x + w / 2 - tw / 2, y + h / 2 - 8, 16, COLOR_BLACK);
}

void DrawPanel(int x, int y, int w, int h, const char* title) {
    DrawRectangle(x, y, w, h, ColorAlpha(COLOR_BLACK, 0.6f));
    DrawRectangleLines(x, y, w, h, DIM_GREEN);
    if (title && title[0]) {
        int tw = MeasureText(title, 14);
        DrawRectangle(x + 10, y - 8, tw + 8, 16, COLOR_BLACK);
        DrawText(title, x + 14, y - 7, 14, GREEN_PHOSPHOR);
    }
}

// ── Main menu overlay (inside shader) ────────────────────────────────────────
void DrawMenu() {
    if (!showMenu) return;

    int mX = WINDOW_WIDTH - 440, mY = 80, mW = 400, mH = MENU_COUNT * 38 + 130;
    DrawRectangle(mX, mY, mW, mH, ColorAlpha(COLOR_BLACK, 0.9f));
    DrawRectangleLines(mX, mY, mW, mH, GREEN_PHOSPHOR);
    DrawText("CONTROL PANEL", mX + 20, mY + 16, 22, GREEN_PHOSPHOR);
    DrawLine(mX + 10, mY + 46, mX + mW - 10, mY + 46, DIM_GREEN);
    DrawText("UP/DOWN  ENTER select  TAB close", mX + 20, mY + mH - 28, 13, DIM_GREEN);

    int oY = mY + 56;
    for (int i = 0; i < MENU_COUNT; i++) {
        Color c = (i == selectedOption) ? CYAN_HIGHLIGHT : GREEN_PHOSPHOR;
        if (i == selectedOption && fmod(menuBlinkTimer, 1.f) < 0.5f)
            DrawText(">", mX + 14, oY + 2, 18, CYAN_HIGHLIGHT);
        DrawText(MENU_OPTIONS[i], mX + 36, oY + 2, 18, c);
        if (i == currentMenu)
            DrawText("[ACTIVE]", mX + mW - 90, oY + 4, 13, AMBER_PHOSPHOR);
        oY += 38;
    }
}

// ── Hardware info (fetched once on background thread) ─────────────────────────
static HardwareInfo s_hwInfo;
static std::atomic<bool> s_hwReady{false};
static bool s_hwFetchStarted = false;

static void FetchHardwareInfoThread() {
    s_hwInfo  = GetHardwareInfo();
    s_hwReady = true;
}

static void EnsureHardwareInfo() {
    if (!s_hwFetchStarted) {
        s_hwFetchStarted = true;
        std::thread(FetchHardwareInfoThread).detach();
    }
}

// ── Dashboard ─────────────────────────────────────────────────────────────────
void DrawDashboard() {
    ClearBackground(COLOR_BLACK);
    EnsureHardwareInfo();

    const int PAD  = 10;
    const int HDR  = 55;
    const int BOT  = 30;
    const int COLW = 620;
    const int LX   = PAD;
    const int RX   = COLW + PAD * 3;
    const int RW   = WINDOW_WIDTH - RX - PAD;
    const int CT   = HDR + PAD * 2;
    const int CB   = WINDOW_HEIGHT - BOT - PAD * 2;
    const int CH   = CB - CT;

    // Header
    DrawRectangle(0, 0, WINDOW_WIDTH, HDR, ColorAlpha(COLOR_BLACK, 0.85f));
    DrawLine(0, HDR, WINDOW_WIDTH, HDR, DIM_GREEN);
    const char* title = stats.useRealData ? "MAINFRAME ONLINE [LIVE]" : "MAINFRAME ONLINE [SIM]";
    int tw = MeasureText(title, 36);
    DrawText(title, WINDOW_WIDTH / 2 - tw / 2, 10, 36, GREEN_PHOSPHOR);

    if (widgets.showComputerName && stats.computerName[0]) {
        std::string h = "HOST: " + std::string(stats.computerName);
        DrawText(h.c_str(), LX + PAD, 8, 14, DIM_GREEN);
    }
    if (s_hwReady.load()) {
        if (!s_hwInfo.cpuName.empty()) {
            std::string c = "CPU: " + s_hwInfo.cpuName;
            DrawText(c.c_str(), LX + PAD, 24, 13, DIM_GREEN);
        }
        if (!s_hwInfo.gpuName.empty()) {
            std::string g = "GPU: " + s_hwInfo.gpuName;
            DrawText(g.c_str(), LX + PAD, 38, 13, DIM_GREEN);
        }
    } else {
        DrawText("CPU: detecting...", LX + PAD, 24, 13, DIM_GREEN);
        DrawText("GPU: detecting...", LX + PAD, 38, 13, DIM_GREEN);
    }
    if (widgets.showSystemTime) {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char buf[32]; strftime(buf, sizeof(buf), "%H:%M:%S", t);
        std::string ts = std::string("TIME: ") + buf;
        int tsw = MeasureText(ts.c_str(), 16);
        DrawText(ts.c_str(), WINDOW_WIDTH - tsw - PAD * 2, 20, 16, DIM_GREEN);
    }

    // Left panel - metrics
    DrawPanel(LX, CT, COLW, CH, "SYSTEM METRICS");
    int rowH = 38, barX = LX + 110, barW = 280, barH = 22;
    int detX = barX + barW + 10, rowY = CT + 20;

    if (widgets.showCPU) {
        if (stats.useRealData) {
            Color cpuCol = stats.cpu > 90.f ? YELLOW_ALERT : stats.cpu > 70.f ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
            DrawText("CPU", LX + 14, rowY + 2, 18, cpuCol);
            DrawProgressBar(barX, rowY, barW, barH, stats.cpu, cpuCol);
        } else {
            DrawText("CPU", LX + 14, rowY + 2, 18, DIM_GREEN);
            DrawText("-- enable real monitoring --", barX + 4, rowY + 4, 13, DIM_GREEN);
        }
        rowY += rowH;
    }
    if (widgets.showRAM) {
        if (stats.useRealData) {
            Color ramCol = stats.ram > 90.f ? YELLOW_ALERT : stats.ram > 75.f ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
            DrawText("RAM", LX + 14, rowY + 2, 18, ramCol);
            DrawProgressBar(barX, rowY, barW, barH, stats.ram, ramCol);
            std::string d = std::to_string(GetUsedRAM_MB()) + "/" + std::to_string(GetTotalRAM_MB()) + " MB";
            DrawText(d.c_str(), detX, rowY + 4, 14, DIM_GREEN);
        } else {
            DrawText("RAM", LX + 14, rowY + 2, 18, DIM_GREEN);
            DrawText("-- enable real monitoring --", barX + 4, rowY + 4, 13, DIM_GREEN);
        }
        rowY += rowH;
    }
    if (widgets.showDisk) {
        static std::vector<DiskInfo> drives;
        static float driveTimer = 5.f;
        driveTimer += GetFrameTime();
        if (driveTimer >= 5.f) { drives = GetAllDrives(); driveTimer = 0.f; }

        for (const auto& drv : drives) {
            char label[8]; snprintf(label, sizeof(label), "%c:", drv.letter);
            DrawText(label, LX + 14, rowY + 2, 18, GREEN_PHOSPHOR);
            Color dCol = drv.usedPct > 90.f ? YELLOW_ALERT :
                         drv.usedPct > 70.f ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
            DrawProgressBar(barX, rowY, barW, barH, drv.usedPct, dCol);
            if (drv.ready) {
                char detail[32];
                snprintf(detail, sizeof(detail), "%llu/%llu GB", drv.usedGB, drv.totalGB);
                DrawText(detail, detX, rowY + 4, 14, DIM_GREEN);
            }
            rowY += rowH;
        }
    }

    bool hasText = widgets.showNetwork || widgets.showProcesses || widgets.showUptime || widgets.showAnomaly;
    if (hasText && rowY > CT + 20) {
        DrawLine(LX + 10, rowY, LX + COLW - 10, rowY, DIM_GREEN);
        rowY += 10;
    }
    if (widgets.showNetwork) {
        char downBuf[32], upBuf[32], netBuf[80];
        // Auto-scale: show KB/s below 1024, MB/s above
        if (stats.netDown >= 1024.f)
            snprintf(downBuf, sizeof(downBuf), "%.2f MB/s", stats.netDown / 1024.f);
        else
            snprintf(downBuf, sizeof(downBuf), "%.1f KB/s", stats.netDown);
        if (stats.netUp >= 1024.f)
            snprintf(upBuf, sizeof(upBuf), "%.2f MB/s", stats.netUp / 1024.f);
        else
            snprintf(upBuf, sizeof(upBuf), "%.1f KB/s", stats.netUp);
        snprintf(netBuf, sizeof(netBuf), "NET  %s DOWN   %s UP", downBuf, upBuf);
        Color netCol = (stats.netDown > 512.f || stats.netUp > 512.f) ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
        DrawText(netBuf, LX + 14, rowY, 18, netCol);
        if (!stats.useRealData) {
            DrawText("[SIM]", LX + COLW - 60, rowY + 2, 12, DIM_GREEN);
        }
        rowY += rowH;
    }
    if (widgets.showProcesses) {
        std::string s = "PROC  " + std::to_string(stats.processCount) + " running";
        DrawText(s.c_str(), LX + 14, rowY, 18, GREEN_PHOSPHOR);
        rowY += rowH;
    }
    if (widgets.showUptime) {
        unsigned long long up = stats.uptimeSeconds;
        char buf[64];
        snprintf(buf, sizeof(buf), "UP    %llud %02lluh %02llum", up/86400, (up%86400)/3600, (up%3600)/60);
        DrawText(buf, LX + 14, rowY, 18, GREEN_PHOSPHOR);
        rowY += rowH;
    }
    if (widgets.showAnomaly) {
        static float flash = 0.f;
        flash += GetFrameTime() * 3.f;
        Color ac = YELLOW_ALERT;
        ac.a = static_cast<unsigned char>(160 + 95 * sinf(flash));
        DrawText("ANOMALY  NONE DETECTED", LX + 14, rowY, 18, ac);
        rowY += rowH;
    }

    // Right panel - log
    if (widgets.showSystemLog) {
        DrawPanel(RX, CT, RW, CH, "SYSTEM LOG");
        int logY = CT + 18, logMax = CT + CH - 10;
        for (int i = (int)logEntries.size() - 1; i >= 0 && logY + 18 < logMax; i--) {
            float age = GetTime() - logEntries[i].time;
            if (age < 60.f) {
                Color c = logEntries[i].color;
                c.a = static_cast<unsigned char>(255 * (1.f - age / 60.f));
                DrawText(logEntries[i].message.c_str(), RX + 10, logY, 14, c);
                logY += 18;
            }
        }
    }

    // Bottom bar
    DrawLine(0, WINDOW_HEIGHT - BOT, WINDOW_WIDTH, WINDOW_HEIGHT - BOT, DIM_GREEN);
    DrawText("TAB: Menu", PAD, WINDOW_HEIGHT - BOT + 8, 14, DIM_GREEN);
    const char* mode = stats.useRealData ? "MODE: LIVE" : "MODE: SIM";
    DrawText(mode, WINDOW_WIDTH / 2 - 40, WINDOW_HEIGHT - BOT + 8, 14, DIM_GREEN);
    int tnw = MeasureText(THEME_NAMES[currentTheme], 14);
    DrawText(THEME_NAMES[currentTheme], WINDOW_WIDTH - tnw - PAD, WINDOW_HEIGHT - BOT + 8, 14, DIM_GREEN);

    // View overlays (replace right panel / full area when active)
    if (currentMenu == MENU_NETWORK_TEST) {
        DrawNetworkDiagnostics();
    }
    if (currentMenu == MENU_SYSTEM_INFO) {
        DrawSystemInfo();
    }

    DrawMenu();
}

// ── Speed test panel (drawn here to avoid Windows/raylib header conflicts) ────
static void DrawSpeedTestPanel(int x, int y, int w, int h) {
    speedTestProgress = speedTestProgress; // sync done in main loop
    const Color NT  = {255, 255, 255, 255};
    const int   PAD = 12;

    const char* stateStr = "IDLE - PRESS ENTER TO START";
    Color stateCol = DIM_GREEN;
    if      (speedTestState == SpeedTestState::RUNNING) { stateStr = "RUNNING..."; stateCol = AMBER_PHOSPHOR; }
    else if (speedTestState == SpeedTestState::DONE)    { stateStr = "COMPLETE";   stateCol = GREEN_PHOSPHOR; }
    else if (speedTestState == SpeedTestState::FAILED)  { stateStr = "FAILED - CHECK CONNECTION"; stateCol = YELLOW_ALERT; }

    DrawText(stateStr, x + PAD, y + PAD + 2, 13, stateCol);
    DrawLine(x + PAD, y + 30, x + w - PAD, y + 30, DIM_GREEN);

    int cy = y + 36;

    if (speedTestState == SpeedTestState::RUNNING) {
        int bw = w - PAD * 2;
        DrawRectangle(x + PAD, cy, bw, 14, COLOR_BLACK);
        DrawRectangleLines(x + PAD, cy, bw, 14, DIM_GREEN);
        int fill = static_cast<int>(bw * speedTestProgress);
        if (fill > 2) DrawRectangle(x + PAD + 1, cy + 1, fill - 2, 12, GREEN_PHOSPHOR);
        char pct[16]; snprintf(pct, sizeof(pct), "%.0f%%", speedTestProgress * 100.f);
        DrawText(pct, x + PAD + bw / 2 - 12, cy, 13, NT);
        cy += 20;
        DrawText("speed.cloudflare.com", x + PAD, cy, 12, DIM_GREEN);
    }

    if (speedTestState == SpeedTestState::DONE) {
        char buf[64];
        snprintf(buf, sizeof(buf), "DL  %.1f Mbps", speedTestResult.downloadMbps);
        DrawText(buf, x + PAD, cy, 15, GREEN_PHOSPHOR); cy += 20;

        snprintf(buf, sizeof(buf), "UL  ~%.1f Mbps", speedTestResult.uploadMbps);
        DrawText(buf, x + PAD, cy, 15, AMBER_PHOSPHOR); cy += 20;

        Color pingCol = speedTestResult.pingMs < 50.f ? GREEN_PHOSPHOR :
                        speedTestResult.pingMs < 100.f ? AMBER_PHOSPHOR : YELLOW_ALERT;
        snprintf(buf, sizeof(buf), "PING  %.0f ms", speedTestResult.pingMs);
        DrawText(buf, x + PAD, cy, 15, pingCol); cy += 20;

        DrawLine(x + PAD, cy, x + w - PAD, cy, {40, 40, 40, 255}); cy += 6;
        DrawText(speedTestResult.timestamp.c_str(), x + PAD, cy, 11, DIM_GREEN); cy += 16;
        DrawText("ENTER: rerun   S: save", x + PAD, cy, 12, DIM_GREEN);
    }

    if (speedTestHasSaved) {
        char saved[80];
        snprintf(saved, sizeof(saved), "Saved: %.1f Mbps  %s",
            speedTestLastSaved.downloadMbps, speedTestLastSaved.timestamp.c_str());
        DrawText(saved, x + PAD, y + h - 18, 11, DIM_GREEN);
    }
}

// ── Network Diagnostics view ──────────────────────────────────────────────────
void DrawNetworkDiagnostics() {
    const int PAD = 10;
    const int HDR = 55;
    const int BOT = 30;
    const int RX  = 640 + PAD * 3;
    const int RW  = WINDOW_WIDTH - RX - PAD;
    const int CT  = HDR + PAD * 2;
    const int CH  = WINDOW_HEIGHT - BOT - PAD * 2 - CT;

    // Split: top 55% adapters, bottom 45% speed test
    const int ADAPTER_H  = static_cast<int>(CH * 0.52f);
    const int SPEEDTEST_H = CH - ADAPTER_H - PAD;
    const int SPEEDTEST_Y = CT + ADAPTER_H + PAD;

    // ── Adapter panel ─────────────────────────────────────────────────────────
    static std::vector<AdapterInfo> adapters;
    static float refreshTimer = 2.f;
    refreshTimer += GetFrameTime();
    if (refreshTimer >= 2.f) { adapters = GetAdapterList(); refreshTimer = 0.f; }

    DrawPanel(RX, CT, RW, ADAPTER_H, "NETWORK ADAPTERS");

    // Live throughput bar
    float down = stats.netDown, up = stats.netUp;
    char downStr[24], upStr[24], throughput[80];
    auto fmtKB = [](float kbps, char* buf, int sz) {
        if (kbps >= 1024.f) snprintf(buf, sz, "%.2f MB/s", kbps / 1024.f);
        else                 snprintf(buf, sz, "%.1f KB/s", kbps);
    };
    fmtKB(down, downStr, sizeof(downStr));
    fmtKB(up,   upStr,   sizeof(upStr));
    snprintf(throughput, sizeof(throughput), "LIVE  %s DOWN   %s UP", downStr, upStr);
    Color tCol = (down > 512.f || up > 512.f) ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
    DrawText(throughput, RX + 10, CT + 16, 14, tCol);
    DrawLine(RX + 10, CT + 34, RX + RW - 10, CT + 34, DIM_GREEN);

    int ay = CT + 40;
    const int lineH = 16;
    if (adapters.empty()) {
        DrawText("No adapters found", RX + 10, ay, 13, DIM_GREEN);
    }
    for (const auto& a : adapters) {
        if (ay + lineH * 4 > CT + ADAPTER_H - 6) break;
        Color sc = a.connected ? GREEN_PHOSPHOR : DIM_GREEN;
        std::string name = a.name.size() > 30 ? a.name.substr(0, 27) + "..." : a.name;
        DrawText(name.c_str(), RX + 10, ay, 13, sc);
        DrawText(a.connected ? "[UP]" : "[DN]", RX + RW - 46, ay, 12, sc);
        ay += lineH;

        std::string ip = "  " + a.ipAddress;
        DrawText(ip.c_str(), RX + 10, ay, 12, DIM_GREEN);

        char spd[32];
        if      (a.speed >= 1000000000UL) snprintf(spd, sizeof(spd), "%.0fGbps", a.speed/1e9);
        else if (a.speed >= 1000000UL)    snprintf(spd, sizeof(spd), "%.0fMbps", a.speed/1e6);
        else if (a.speed > 0)             snprintf(spd, sizeof(spd), "%.0fKbps", a.speed/1e3);
        else                              snprintf(spd, sizeof(spd), "N/A");
        DrawText(spd, RX + RW - MeasureText(spd, 12) - 6, ay, 12, DIM_GREEN);
        ay += lineH;

        char rx[40];
        snprintf(rx, sizeof(rx), "  RX %.1fMB  TX %.1fMB",
            a.bytesIn/(1024.0*1024.0), a.bytesOut/(1024.0*1024.0));
        DrawText(rx, RX + 10, ay, 12, DIM_GREEN);
        ay += lineH;

        DrawLine(RX + 10, ay + 1, RX + RW - 10, ay + 1, {40, 40, 40, 255});
        ay += 6;
    }

    // ── Speed test panel ──────────────────────────────────────────────────────
    DrawPanel(RX, SPEEDTEST_Y, RW, SPEEDTEST_H, "SPEED TEST");
    DrawSpeedTestPanel(RX, SPEEDTEST_Y, RW, SPEEDTEST_H);
}

// ── System Information view ───────────────────────────────────────────────────
void DrawSystemInfo() {
    const int PAD  = 10;
    const int HDR  = 55;
    const int BOT  = 30;
    const int CT   = HDR + PAD * 2;
    const int CB   = WINDOW_HEIGHT - BOT - PAD;
    const int CH   = CB - CT;
    const int HALF = (WINDOW_WIDTH - PAD * 3) / 2;
    const int LX   = PAD;
    const int RX   = LX + HALF + PAD;
    const int LH   = 20;   // line height
    const int FS   = 15;   // font size body
    const int FS_H = 13;   // font size sub-header

    // ── Left panel: Processor & Memory ───────────────────────────────────────
    DrawPanel(LX, CT, HALF, CH, "PROCESSOR & MEMORY");
    int y = CT + 20;

    // CPU
    DrawText("PROCESSOR", LX + 14, y, FS_H, AMBER_PHOSPHOR); y += LH;
    DrawLine(LX + 14, y, LX + HALF - 14, y, DIM_GREEN); y += 6;

    if (s_hwReady.load() && !s_hwInfo.cpuName.empty()) {
        // Word-wrap the CPU name across two lines if needed
        std::string cn = s_hwInfo.cpuName;
        if ((int)MeasureText(cn.c_str(), FS) > HALF - 28) {
            // Split at last space before midpoint
            size_t mid = cn.size() / 2;
            size_t sp  = cn.rfind(' ', mid + 10);
            if (sp != std::string::npos) {
                DrawText(cn.substr(0, sp).c_str(), LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
                DrawText(cn.substr(sp + 1).c_str(), LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
            } else {
                DrawText(cn.c_str(), LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
            }
        } else {
            DrawText(cn.c_str(), LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
        }
    } else {
        DrawText(s_hwReady.load() ? "Unknown" : "Detecting...", LX + 14, y, FS, DIM_GREEN); y += LH;
    }

    // Live CPU usage
    if (stats.useRealData) {
        char cpuBuf[48];
        snprintf(cpuBuf, sizeof(cpuBuf), "Usage: %.1f%%", stats.cpu);
        Color cpuCol = stats.cpu > 90.f ? YELLOW_ALERT : stats.cpu > 70.f ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
        DrawText(cpuBuf, LX + 14, y, FS, cpuCol);
    } else {
        DrawText("Usage: -- (enable real monitoring)", LX + 14, y, FS, DIM_GREEN);
    }
    y += LH + 8;

    // RAM
    DrawText("MEMORY", LX + 14, y, FS_H, AMBER_PHOSPHOR); y += LH;
    DrawLine(LX + 14, y, LX + HALF - 14, y, DIM_GREEN); y += 6;

    unsigned long long totalRam = GetTotalRAM_MB();
    unsigned long long usedRam  = GetUsedRAM_MB();
    char ramTotal[32], ramUsed[32];
    snprintf(ramTotal, sizeof(ramTotal), "Installed: %llu MB (%.1f GB)", totalRam, totalRam / 1024.f);
    DrawText(ramTotal, LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;

    if (stats.useRealData) {
        snprintf(ramUsed, sizeof(ramUsed), "In use:    %llu MB", usedRam);
        float ramPct = totalRam > 0 ? (usedRam * 100.f / totalRam) : 0.f;
        Color ramCol = ramPct > 90.f ? YELLOW_ALERT : ramPct > 75.f ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
        DrawText(ramUsed, LX + 14, y, FS, ramCol); y += LH;
        // Mini bar
        int bw = HALF - 28;
        DrawRectangle(LX + 14, y, bw, 10, COLOR_BLACK);
        DrawRectangleLines(LX + 14, y, bw, 10, DIM_GREEN);
        int fill = (int)(bw * ramPct / 100.f);
        if (fill > 2) DrawRectangle(LX + 15, y + 1, fill - 2, 8, ramCol);
        y += 16;
    } else {
        DrawText("In use:    -- (enable real monitoring)", LX + 14, y, FS, DIM_GREEN); y += LH;
    }
    y += 8;

    // OS / System
    DrawText("SYSTEM", LX + 14, y, FS_H, AMBER_PHOSPHOR); y += LH;
    DrawLine(LX + 14, y, LX + HALF - 14, y, DIM_GREEN); y += 6;

    char hostBuf[80];
    snprintf(hostBuf, sizeof(hostBuf), "Hostname:  %s",
             stats.computerName[0] ? stats.computerName : "Unknown");
    DrawText(hostBuf, LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;

    // OS version from HardwareInfo (fetched via wmic on background thread)
    if (s_hwReady.load() && !s_hwInfo.osVersion.empty()) {
        std::string osLine = "Windows:   " + s_hwInfo.osVersion;
        DrawText(osLine.c_str(), LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
    }

    // Uptime
    unsigned long long up = GetSystemUptimeSeconds();
    char upBuf[64];
    snprintf(upBuf, sizeof(upBuf), "Uptime:    %llud %02lluh %02llum %02llus",
             up/86400, (up%86400)/3600, (up%3600)/60, up%60);
    DrawText(upBuf, LX + 14, y, FS, GREEN_PHOSPHOR); y += LH;

    // Process count
    if (stats.useRealData) {
        char procBuf[40];
        snprintf(procBuf, sizeof(procBuf), "Processes: %d running", stats.processCount);
        DrawText(procBuf, LX + 14, y, FS, GREEN_PHOSPHOR);
    } else {
        DrawText("Processes: -- (enable real monitoring)", LX + 14, y, FS, DIM_GREEN);
    }

    // ── Right panel: GPU & Storage ────────────────────────────────────────────
    DrawPanel(RX, CT, HALF, CH, "GPU & STORAGE");
    y = CT + 20;

    // GPU
    DrawText("GRAPHICS", RX + 14, y, FS_H, AMBER_PHOSPHOR); y += LH;
    DrawLine(RX + 14, y, RX + HALF - 14, y, DIM_GREEN); y += 6;

    if (s_hwReady.load() && !s_hwInfo.gpuName.empty()) {
        std::string gn = s_hwInfo.gpuName;
        if ((int)MeasureText(gn.c_str(), FS) > HALF - 28) {
            size_t mid = gn.size() / 2;
            size_t sp  = gn.rfind(' ', mid + 10);
            if (sp != std::string::npos) {
                DrawText(gn.substr(0, sp).c_str(), RX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
                DrawText(gn.substr(sp + 1).c_str(), RX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
            } else {
                DrawText(gn.c_str(), RX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
            }
        } else {
            DrawText(gn.c_str(), RX + 14, y, FS, GREEN_PHOSPHOR); y += LH;
        }
    } else {
        DrawText(s_hwReady.load() ? "Unknown" : "Detecting...", RX + 14, y, FS, DIM_GREEN); y += LH;
    }

    if (s_hwReady.load() && !s_hwInfo.gpuDriverVersion.empty()) {
        std::string drv = "Driver:    " + s_hwInfo.gpuDriverVersion;
        DrawText(drv.c_str(), RX + 14, y, FS, DIM_GREEN); y += LH;
    }
    y += 8;

    // Storage
    DrawText("STORAGE", RX + 14, y, FS_H, AMBER_PHOSPHOR); y += LH;
    DrawLine(RX + 14, y, RX + HALF - 14, y, DIM_GREEN); y += 6;

    static std::vector<DiskInfo> siDrives;
    static float siDriveTimer = 5.f;
    siDriveTimer += GetFrameTime();
    if (siDriveTimer >= 5.f) { siDrives = GetAllDrives(); siDriveTimer = 0.f; }

    for (const auto& drv : siDrives) {
        if (y + LH * 2 + 16 > CB) break;
        char drvBuf[64];
        snprintf(drvBuf, sizeof(drvBuf), "%c:  %llu / %llu GB",
                 drv.letter, drv.usedGB, drv.totalGB);
        Color dCol = drv.usedPct > 90.f ? YELLOW_ALERT :
                     drv.usedPct > 70.f ? AMBER_PHOSPHOR : GREEN_PHOSPHOR;
        DrawText(drvBuf, RX + 14, y, FS, dCol); y += LH;
        // Bar
        int bw = HALF - 28;
        DrawRectangle(RX + 14, y, bw, 8, COLOR_BLACK);
        DrawRectangleLines(RX + 14, y, bw, 8, DIM_GREEN);
        int fill = (int)(bw * drv.usedPct / 100.f);
        if (fill > 2) DrawRectangle(RX + 15, y + 1, fill - 2, 6, dCol);
        y += 14;
    }
    y += 8;

    // Bottom hint
    DrawText("ESC  return to dashboard", LX + 14, CB - 2, 12, DIM_GREEN);
}
