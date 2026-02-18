#include "terminal.h"
#include "theme.h"
#include "dashboard.h"
#include "raylib.h"
#include <cstdio>
#include <cmath>
#include <string>

bool showTerminal = false;
int  activeTab    = 0;
int  tabCount     = 1;
TerminalTab tabs[MAX_TERMINAL_TABS];

static float termCursorBlink = 0.f;

static std::string DefaultCwd() {
#ifdef _WIN32
    return "C:\\";
#else
    const char* home = getenv("HOME");
    return home ? home : "/";
#endif
}

static void InitTab(int idx) {
    tabs[idx].input  = "";
    tabs[idx].output.clear();
    tabs[idx].cwd    = DefaultCwd();
    tabs[idx].name   = "Tab " + std::to_string(idx + 1);
    tabs[idx].output.push_back("CRT Dashboard Terminal  -  type commands and press ENTER");
    tabs[idx].output.push_back("Ctrl+T: new tab   Ctrl+W: close tab   Ctrl+Tab: switch");
}

void RunTerminalCommand(const std::string& cmd) {
    TerminalTab& t = tabs[activeTab];
    t.output.push_back("> " + t.cwd + " $ " + cmd);
#ifdef _WIN32
    std::string full = "cd /d \"" + t.cwd + "\" && " + cmd + " 2>&1";
#else
    std::string full = "cd \"" + t.cwd + "\" && " + cmd + " 2>&1";
#endif
#ifdef _WIN32
    FILE* pipe = _popen(full.c_str(), "r");
#else
    FILE* pipe = popen(full.c_str(), "r");
#endif
    if (!pipe) { t.output.push_back("[ERROR] Failed to run command"); return; }
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        t.output.push_back(line);
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    if (t.output.size() > 200)
        t.output.erase(t.output.begin(), t.output.begin() + (t.output.size() - 200));
}

void AddTerminalTab() {
    if (tabCount >= MAX_TERMINAL_TABS) return;
    InitTab(tabCount);
    activeTab = tabCount;
    tabCount++;
}

void CloseTerminalTab(int idx) {
    if (tabCount <= 1) { showTerminal = false; return; }
    for (int i = idx; i < tabCount - 1; i++) tabs[i] = tabs[i + 1];
    tabCount--;
    if (activeTab >= tabCount) activeTab = tabCount - 1;
    // Rename remaining tabs
    for (int i = 0; i < tabCount; i++)
        tabs[i].name = "Tab " + std::to_string(i + 1);
}

void DrawTerminal() {
    if (!showTerminal) return;

    // Ensure tab 0 is always initialised
    static bool inited = false;
    if (!inited) { InitTab(0); inited = true; }

    TerminalTab& t = tabs[activeTab];

    DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, ColorAlpha(COLOR_BLACK, 0.88f));

    const int TX      = 40;
    const int TY      = 40;
    const int TW      = WINDOW_WIDTH - 80;
    const int TH      = WINDOW_HEIGHT - 100;
    const int TAB_H   = 28;
    const int INPUT_H = 34;
    const int BODY_Y  = TY + TAB_H;
    const int BODY_H  = TH - TAB_H;
    const int OUT_H   = BODY_H - INPUT_H;

    DrawRectangle(TX, TY, TW, TH, {8, 8, 8, 255});
    DrawRectangleLines(TX, TY, TW, TH, GREEN_PHOSPHOR);

    // ── Tab bar ───────────────────────────────────────────────────────────────
    DrawRectangle(TX, TY, TW, TAB_H, {18, 18, 18, 255});
    DrawLine(TX, TY + TAB_H, TX + TW, TY + TAB_H, DIM_GREEN);

    const int TAB_W = 120;
    for (int i = 0; i < tabCount; i++) {
        int tx = TX + i * (TAB_W + 2);
        bool active = (i == activeTab);
        DrawRectangle(tx, TY, TAB_W, TAB_H - 1, active ? Color{28, 28, 28, 255} : Color{12, 12, 12, 255});
        if (active) DrawRectangle(tx, TY, TAB_W, 2, GREEN_PHOSPHOR);
        DrawText(tabs[i].name.c_str(), tx + 8, TY + 7, 13, active ? GREEN_PHOSPHOR : DIM_GREEN);
        // Close button [x]
        if (tabCount > 1) {
            DrawText("x", tx + TAB_W - 16, TY + 7, 13, active ? AMBER_PHOSPHOR : DIM_GREEN);
        }
    }

    // Hint on right side of tab bar
    DrawText("Ctrl+T: new  Ctrl+W: close  Ctrl+Tab: switch  ESC: hide",
             TX + tabCount * (TAB_W + 2) + 10, TY + 8, 11, DIM_GREEN);

    // CWD on far right
    int cwdW = MeasureText(t.cwd.c_str(), 12);
    DrawText(t.cwd.c_str(), TX + TW - cwdW - 8, TY + 8, 12, DIM_GREEN);

    // ── Output area ───────────────────────────────────────────────────────────
    int lineH    = 16;
    int maxLines = OUT_H / lineH;
    int outY     = BODY_Y + 4;
    int start    = (int)t.output.size() > maxLines
                   ? (int)t.output.size() - maxLines : 0;
    for (int i = start; i < (int)t.output.size(); i++) {
        Color c = GREEN_PHOSPHOR;
        if (!t.output[i].empty() && t.output[i][0] == '>')
            c = CYAN_HIGHLIGHT;
        else if (t.output[i].find("[ERROR]") != std::string::npos)
            c = YELLOW_ALERT;
        else if (i < 2)
            c = DIM_GREEN;
        DrawText(t.output[i].c_str(), TX + 8, outY, 14, c);
        outY += lineH;
    }

    // ── Input bar ─────────────────────────────────────────────────────────────
    int inputY = TY + TH - INPUT_H;
    DrawRectangle(TX, inputY, TW, INPUT_H, {15, 15, 15, 255});
    DrawLine(TX, inputY, TX + TW, inputY, DIM_GREEN);
    std::string prompt = t.cwd + " $ " + t.input;
    DrawText(prompt.c_str(), TX + 8, inputY + 8, 16, GREEN_PHOSPHOR);

    // Blinking cursor
    termCursorBlink += GetFrameTime() * 2.f;
    if (fmod(termCursorBlink, 1.f) < 0.5f) {
        int cx = TX + 8 + MeasureText(prompt.c_str(), 16);
        DrawRectangle(cx, inputY + 6, 10, 20, GREEN_PHOSPHOR);
    }
}
