#pragma once
#include "raylib.h"
#include "theme.h"
#include <string>
#include <vector>

// ── Window constants ──────────────────────────────────────────────────────────
static const int WINDOW_WIDTH  = 1280;
static const int WINDOW_HEIGHT = 720;
static const char* WINDOW_TITLE = "RetroForge";

// ── Menu options ──────────────────────────────────────────────────────────────
enum MenuOption {
    MENU_DASHBOARD = 0,
    MENU_REAL_MONITORING,
    MENU_NETWORK_TEST,
    MENU_SYSTEM_INFO,
    MENU_CUSTOMIZE_WIDGETS,
    MENU_COLOR_THEMES,
    MENU_TERMINAL,
    MENU_COUNT
};

inline const char* MENU_OPTIONS[] = {
    "DASHBOARD VIEW",
    "REAL-TIME MONITORING",
    "NETWORK DIAGNOSTICS",
    "SYSTEM INFORMATION",
    "CUSTOMIZE WIDGETS",
    "COLOR THEMES",
    "TERMINAL"
};

// ── Widget system ─────────────────────────────────────────────────────────────
struct DashboardWidgets {
    bool showCPU         = true;
    bool showRAM         = true;
    bool showDisk        = true;
    bool showNetwork     = true;
    bool showAnomaly     = true;
    bool showSystemLog   = true;
    bool showSystemTime  = true;
    bool showProcesses   = true;
    bool showUptime      = true;
    bool showComputerName= true;
};

enum WidgetOption {
    WIDGET_CPU = 0,
    WIDGET_RAM,
    WIDGET_DISK,
    WIDGET_NETWORK,
    WIDGET_ANOMALY,
    WIDGET_LOG,
    WIDGET_TIME,
    WIDGET_PROCESSES,
    WIDGET_UPTIME,
    WIDGET_COMPUTERNAME,
    WIDGET_COUNT
};

inline const char* WIDGET_NAMES[] = {
    "CPU Monitor", "RAM Monitor", "Disk Monitor", "Network Stats",
    "Anomaly Detector", "System Log", "System Time",
    "Process Count", "System Uptime", "Computer Name"
};

// ── System stats ──────────────────────────────────────────────────────────────
struct SystemStats {
    float cpu = 45.f, ram = 60.f, disk = 70.f;
    float netDown = 15.f, netUp = 3.f;
    float targetCpu = 45.f, targetRam = 60.f, targetDisk = 70.f;
    float targetNetDown = 15.f, targetNetUp = 3.f;
    bool  useRealData = false;
    int   processCount = 120;
    unsigned long long uptimeSeconds = 3600;
    char  computerName[64] = {};
};

// ── Anomaly state ───────────────────────────────────────────────────────────
struct AnomalyState {
    bool        triggered  = false;
    std::string reason     = "NONE DETECTED";
    float       flashTimer = 0.f;
    // Rolling net baseline (exponential moving average)
    float       netBaseline = -1.f;
};

// ── Log entry ─────────────────────────────────────────────────────────────────
struct LogEntry {
    std::string message;
    double      time;
    Color       color;
};

// ── Globals (defined in dashboard.cpp) ───────────────────────────────────────
extern SystemStats      stats;
extern std::vector<LogEntry> logEntries;
extern float            timeAccumulator;
extern Font             retroFont;
extern bool             hasCustomFont;
extern int              currentMenu;
extern int              selectedOption;
extern bool             showMenu;
extern float            menuBlinkTimer;
extern DashboardWidgets widgets;
extern bool             showWidgetMenu;
extern int              selectedWidget;
extern bool             isFirstRun;
extern AnomalyState     anomaly;

// ── Functions ─────────────────────────────────────────────────────────────────
void GetWidgetStates(bool* states[WIDGET_COUNT]);
void AddLogEntry(const std::string& message, Color entryColor);
void InitializeStats();
void UpdateStats(float deltaTime);
void GenerateRandomLog();
void DrawProgressBar(int x, int y, int w, int h, float pct, Color col);
void DrawPanel(int x, int y, int w, int h, const char* title);
void DrawDashboard();
void DrawMenu();
void DrawNetworkDiagnostics();
void DrawSystemInfo();
void HandleMenuSelection();
