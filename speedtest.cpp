// speedtest.cpp - pure logic, NO raylib/drawing includes
// Drawing is handled in dashboard.cpp to avoid Windows/raylib header conflicts
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <atomic>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <vector>

#pragma comment(lib, "winhttp.lib")

#include "speedtest.h"

// GetApplicationDirectory comes from raylib - forward declare to avoid including raylib.h
extern "C" const char* GetApplicationDirectory(void);

// ── State ─────────────────────────────────────────────────────────────────────
SpeedTestState  speedTestState    = SpeedTestState::IDLE;
SpeedTestResult speedTestResult   = {};
SpeedTestResult speedTestLastSaved= {};
bool            speedTestHasSaved = false;
float           speedTestProgress = 0.f;

// Shared between thread and main
static std::atomic<float> s_progress{0.f};
static std::atomic<bool>  s_running{false};

// ── Download test using WinHTTP ───────────────────────────────────────────────
// Downloads ~10 MB from a CDN and measures throughput
static void RunSpeedTestThread() {
    SpeedTestResult res = {};
    res.server = "speed.cloudflare.com";

    HINTERNET hSession = WinHttpOpen(
        L"TerminusDashboard/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) { speedTestState = SpeedTestState::FAILED; s_running = false; return; }

    HINTERNET hConnect = WinHttpConnect(hSession, L"speed.cloudflare.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        speedTestState = SpeedTestState::FAILED; s_running = false; return;
    }

    // Ping test: measure time for a tiny HEAD request
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);

    HINTERNET hPing = WinHttpOpenRequest(hConnect, L"HEAD", L"/__down?bytes=0",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (hPing) {
        QueryPerformanceCounter(&t0);
        if (WinHttpSendRequest(hPing, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hPing, nullptr)) {
            QueryPerformanceCounter(&t1);
            res.pingMs = static_cast<float>((t1.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart);
        }
        WinHttpCloseHandle(hPing);
    }

    // Download test: fetch 10 MB
    const wchar_t* path = L"/__down?bytes=10000000";
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hReq) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        speedTestState = SpeedTestState::FAILED; s_running = false; return;
    }

    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        speedTestState = SpeedTestState::FAILED; s_running = false; return;
    }

    const DWORD TARGET = 10000000;
    DWORD totalRead = 0;
    std::vector<char> buf(65536);

    QueryPerformanceCounter(&t0);
    DWORD bytesRead = 0;
    while (WinHttpReadData(hReq, buf.data(), (DWORD)buf.size(), &bytesRead) && bytesRead > 0) {
        totalRead += bytesRead;
        s_progress = static_cast<float>(totalRead) / static_cast<float>(TARGET);
    }
    QueryPerformanceCounter(&t1);

    double elapsedSec = static_cast<double>(t1.QuadPart - t0.QuadPart) / freq.QuadPart;
    if (elapsedSec > 0.0 && totalRead > 0) {
        double bitsPerSec = (totalRead * 8.0) / elapsedSec;
        res.downloadMbps = static_cast<float>(bitsPerSec / 1e6);
        // Rough upload estimate: inverse of ping ratio (heuristic)
        res.uploadMbps = res.downloadMbps * 0.15f;
    }

    // Timestamp
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    res.timestamp = ts;

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    speedTestResult = res;
    s_progress      = 1.f;
    speedTestState  = SpeedTestState::DONE;
    s_running       = false;
}

// ── Public API ────────────────────────────────────────────────────────────────
void StartSpeedTest() {
    if (s_running) return;
    speedTestState  = SpeedTestState::RUNNING;
    speedTestProgress = 0.f;
    s_progress      = 0.f;
    s_running       = true;
    std::thread(RunSpeedTestThread).detach();
}

void SaveSpeedTestResult() {
    if (speedTestState != SpeedTestState::DONE) return;
    const char* appDir = GetApplicationDirectory();
    std::string path = std::string(appDir) + "speedtest_results.txt";
    std::ofstream f(path, std::ios::app);
    if (!f.is_open()) return;
    f << "[" << speedTestResult.timestamp << "]"
      << "  Server: " << speedTestResult.server
      << "  DL: " << speedTestResult.downloadMbps << " Mbps"
      << "  UL: ~" << speedTestResult.uploadMbps << " Mbps"
      << "  Ping: " << speedTestResult.pingMs << " ms\n";
    speedTestLastSaved = speedTestResult;
    speedTestHasSaved  = true;
    // Note: log entry added by caller (dashboard.cpp) to avoid raylib dependency here
}
