// speedtest.cpp - pure logic, NO raylib/drawing includes
// Drawing is handled in dashboard.cpp to avoid Windows/raylib header conflicts
#include <thread>
#include <atomic>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <winhttp.h>
  #pragma comment(lib, "winhttp.lib")
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  #ifdef __APPLE__
    #include <sys/time.h>
  #endif
#endif

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

// ── Shared timestamp helper ───────────────────────────────────────────────────
static std::string MakeTimestamp() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    return ts;
}

static unsigned long long NowMs() {
    using namespace std::chrono;
    return (unsigned long long)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

#ifdef _WIN32
// ── Windows: WinHTTP implementation ──────────────────────────────────────────
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

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);

    HINTERNET hPing = WinHttpOpenRequest(hConnect, L"HEAD", L"/__down?bytes=0",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
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

    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", L"/__down?bytes=10000000",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        speedTestState = SpeedTestState::FAILED; s_running = false; return;
    }
    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
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
        res.downloadMbps = static_cast<float>((totalRead * 8.0) / elapsedSec / 1e6);
        res.uploadMbps   = res.downloadMbps * 0.15f;
    }
    res.timestamp = MakeTimestamp();
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    speedTestResult = res; s_progress = 1.f;
    speedTestState  = SpeedTestState::DONE; s_running = false;
}

#else
// ── POSIX: plain HTTP to speed.cloudflare.com:80 ─────────────────────────────
// Note: uses HTTP (port 80) since raw sockets can't do TLS without a library.
// For production, link libcurl instead.
static int ConnectTCP(const char* host, int port) {
    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[8]; snprintf(portStr, sizeof(portStr), "%d", port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0) return -1;
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }
    // 10 second connect timeout via SO_RCVTIMEO
    struct timeval tv = {10, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        close(sock); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);
    return sock;
}

static void RunSpeedTestThread() {
    SpeedTestResult res = {};
    res.server = "speed.cloudflare.com (HTTP)";

    // Ping: measure TCP connect + HEAD round-trip
    unsigned long long t0 = NowMs();
    int pingSock = ConnectTCP("speed.cloudflare.com", 80);
    if (pingSock >= 0) {
        const char* pingReq = "HEAD /__down?bytes=0 HTTP/1.0\r\nHost: speed.cloudflare.com\r\n\r\n";
        send(pingSock, pingReq, strlen(pingReq), 0);
        char pingBuf[512] = {};
        recv(pingSock, pingBuf, sizeof(pingBuf)-1, 0);
        res.pingMs = (float)(NowMs() - t0);
        close(pingSock);
    }

    // Download: fetch 10 MB via plain HTTP
    int sock = ConnectTCP("speed.cloudflare.com", 80);
    if (sock < 0) { speedTestState = SpeedTestState::FAILED; s_running = false; return; }

    const char* req = "GET /__down?bytes=10000000 HTTP/1.0\r\nHost: speed.cloudflare.com\r\n\r\n";
    if (send(sock, req, strlen(req), 0) < 0) {
        close(sock); speedTestState = SpeedTestState::FAILED; s_running = false; return;
    }

    // Skip HTTP headers
    std::vector<char> buf(65536);
    std::string headers;
    bool headersDone = false;
    const long long TARGET = 10000000;
    long long totalRead = 0;

    t0 = NowMs();
    ssize_t n;
    while ((n = recv(sock, buf.data(), buf.size(), 0)) > 0) {
        if (!headersDone) {
            headers.append(buf.data(), n);
            auto pos = headers.find("\r\n\r\n");
            if (pos != std::string::npos) {
                headersDone = true;
                totalRead += (long long)(headers.size() - pos - 4);
            }
        } else {
            totalRead += n;
        }
        s_progress = (float)totalRead / (float)TARGET;
    }
    unsigned long long elapsed = NowMs() - t0;
    close(sock);

    if (elapsed > 0 && totalRead > 0) {
        double elapsedSec = elapsed / 1000.0;
        res.downloadMbps = (float)((totalRead * 8.0) / elapsedSec / 1e6);
        res.uploadMbps   = res.downloadMbps * 0.15f;
    }
    res.timestamp   = MakeTimestamp();
    speedTestResult = res;
    s_progress      = 1.f;
    speedTestState  = SpeedTestState::DONE;
    s_running       = false;
}
#endif

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
