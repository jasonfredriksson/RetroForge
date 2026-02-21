#include "raylib.h"
#include "theme.h"
#include "dashboard.h"
#include "ui_menus.h"
#include "terminal.h"
#include "config.h"
#include "system_monitor.h"
#include "speedtest.h"
#include "stress_test.h"
#include <string>
#include <cstdio>

// ── HandleMenuSelection ───────────────────────────────────────────────────────
// Defined here (not dashboard.cpp) to avoid circular includes with ui_menus/terminal
void HandleMenuSelection() {
    switch (selectedOption) {
        case MENU_DASHBOARD:
            currentMenu = MENU_DASHBOARD;
            stats.useRealData = false;
            showMenu = false;
            AddLogEntry("[MENU] Dashboard View (Simulated)", CYAN_HIGHLIGHT);
            break;

        case MENU_REAL_MONITORING:
            currentMenu = MENU_REAL_MONITORING;
            stats.useRealData = true;
            showMenu = false;
            AddLogEntry("[MENU] Real-time monitoring ENABLED", CYAN_HIGHLIGHT);
            stats.targetCpu  = GetRealCPUUsage();
            stats.targetRam  = GetRealRAMUsage();
            stats.targetDisk = GetRealDiskUsage();
            break;

        case MENU_NETWORK_TEST:
            currentMenu = MENU_NETWORK_TEST;
            showMenu = false;
            AddLogEntry("[NET] Network diagnostics active", CYAN_HIGHLIGHT);
            break;

        case MENU_SYSTEM_INFO:
            currentMenu = MENU_SYSTEM_INFO;
            showMenu = false;
            AddLogEntry("[MENU] System Information", CYAN_HIGHLIGHT);
            break;

        case MENU_CUSTOMIZE_WIDGETS:
            showWidgetMenu = true;
            showMenu = false;
            selectedWidget = 0;
            AddLogEntry("[MENU] Widget customization opened", CYAN_HIGHLIGHT);
            break;

        case MENU_COLOR_THEMES:
            showColorMenu = true;
            showMenu = false;
            selectedTheme = currentTheme;   // pre-select current theme
            AddLogEntry("[MENU] Color theme selector opened", CYAN_HIGHLIGHT);
            break;

        case MENU_TERMINAL:
            showTerminal = true;
            showMenu = false;
            AddLogEntry("[TERMINAL] Terminal opened", CYAN_HIGHLIGHT);
            break;
    }
}

// ── main ──────────────────────────────────────────────────────────────────────
int main() {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    SetExitKey(KEY_NULL);

    // Font
    retroFont = LoadFont("resources/fonts/VGA.ttf");
    hasCustomFont = (retroFont.texture.id > 0);
    if (!hasCustomFont) retroFont = GetFontDefault();

    // Config (sets isFirstRun, currentTheme, widget states)
    LoadConfig();
    ApplyColorTheme(currentTheme);

    InitializeStats();
    InitializeSystemMonitoring();

    // Onboarding on first run
    if (isFirstRun) {
        showOnboarding = true;
        onboardingStep = 0;
        selectedTheme  = THEME_GREEN_PHOSPHOR;
    }

    // Shader
    const char* appDir = GetApplicationDirectory();
    std::string shaderPath = std::string(appDir) + "resources/shaders/crt.fsh";
    Shader crtShader = LoadShader(0, shaderPath.c_str());
    if (crtShader.id == 0)
        crtShader = LoadShader(0, "resources/shaders/crt.fsh");

    int resLoc  = GetShaderLocation(crtShader, "resolution");
    int timeLoc = GetShaderLocation(crtShader, "time");
    int tintLoc = GetShaderLocation(crtShader, "phosphorTint");

    RenderTexture2D target = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    float res[2] = { (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT };
    SetShaderValue(crtShader, resLoc, res, SHADER_UNIFORM_VEC2);

    AddLogEntry("[SYSTEM] Dashboard ready. Press TAB to open menu.", CYAN_HIGHLIGHT);

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        timeAccumulator += dt;
        menuBlinkTimer  += dt;

        // ── F11: borderless fullscreen toggle (no black flash, no title bar) ─
        if (IsKeyPressed(KEY_F11)) {
            if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
                SetWindowState(FLAG_WINDOW_UNDECORATED);
                ToggleBorderlessWindowed();
                AddLogEntry("[DISPLAY] Fullscreen ON", DIM_GREEN);
            } else {
                ToggleBorderlessWindowed();
                ClearWindowState(FLAG_WINDOW_UNDECORATED);
                AddLogEntry("[DISPLAY] Fullscreen OFF", DIM_GREEN);
            }
        }

        // ── Ctrl+Escape: quit ─────────────────────────────────────────────
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            if (IsKeyPressed(KEY_ESCAPE)) break;
        }

        // ── Input: Terminal (highest priority when open) ───────────────────
        if (showTerminal) {
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

            // Tab management shortcuts
            if (ctrl && IsKeyPressed(KEY_T)) {
                AddTerminalTab();
            } else if (ctrl && IsKeyPressed(KEY_W)) {
                CloseTerminalTab(activeTab);
            } else if (ctrl && IsKeyPressed(KEY_TAB)) {
                activeTab = (activeTab + 1) % tabCount;
            } else {
                // Normal input goes to active tab
                TerminalTab& t = tabs[activeTab];
                int ch = GetCharPressed();
                while (ch > 0) {
                    if (ch >= 32 && ch < 127) t.input += (char)ch;
                    ch = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && !t.input.empty())
                    t.input.pop_back();
                if (IsKeyPressed(KEY_ENTER) && !t.input.empty()) {
                    if (t.input.size() >= 3 && t.input.substr(0, 3) == "cd ") {
                        std::string dir   = t.input.substr(3);
                        std::string probe = "cd /d \"" + t.cwd + "\" && cd \"" + dir + "\" && cd";
                        FILE* p = _popen(probe.c_str(), "r");
                        if (p) {
                            char buf[512] = {};
                            fgets(buf, sizeof(buf), p);
                            _pclose(p);
                            std::string nd(buf);
                            while (!nd.empty() && (nd.back()=='\n'||nd.back()=='\r')) nd.pop_back();
                            if (!nd.empty()) t.cwd = nd;
                        }
                        t.output.push_back("> " + t.cwd + " $ " + t.input);
                    } else {
                        RunTerminalCommand(t.input);
                    }
                    t.input.clear();
                }
                if (IsKeyPressed(KEY_ESCAPE)) showTerminal = false;
            }
        }

        // ── Input: Onboarding ─────────────────────────────────────────────
        else if (showOnboarding) {
            if (onboardingStep == 0) {
                if (IsKeyPressed(KEY_ENTER))  onboardingStep = 1;
                if (IsKeyPressed(KEY_ESCAPE)) {
                    showOnboarding = false;
                    isFirstRun     = false;
                    ApplyColorTheme(THEME_GREEN_PHOSPHOR);
                    SaveConfig();
                }
            } else {
                if (IsKeyPressed(KEY_UP))   { selectedTheme = (selectedTheme - 1 + THEME_COUNT) % THEME_COUNT; menuBlinkTimer = 0; }
                if (IsKeyPressed(KEY_DOWN)) { selectedTheme = (selectedTheme + 1) % THEME_COUNT; menuBlinkTimer = 0; }
                if (IsKeyPressed(KEY_ENTER)) {
                    ApplyColorTheme(selectedTheme);
                    showOnboarding = false;
                    isFirstRun     = false;
                    SaveConfig();
                }
                if (IsKeyPressed(KEY_ESCAPE)) {
                    ApplyColorTheme(THEME_GREEN_PHOSPHOR);
                    showOnboarding = false;
                    isFirstRun     = false;
                    SaveConfig();
                }
            }
        }

        // ── Input: Color menu ─────────────────────────────────────────────
        else if (showColorMenu) {
            if (IsKeyPressed(KEY_UP))   { selectedTheme = (selectedTheme - 1 + THEME_COUNT) % THEME_COUNT; menuBlinkTimer = 0; }
            if (IsKeyPressed(KEY_DOWN)) { selectedTheme = (selectedTheme + 1) % THEME_COUNT; menuBlinkTimer = 0; }
            if (IsKeyPressed(KEY_ENTER)) {
                ApplyColorTheme(selectedTheme);
                SaveConfig();
                showColorMenu = false;
                AddLogEntry("[THEME] Applied: " + std::string(THEME_NAMES[selectedTheme]), CYAN_HIGHLIGHT);
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                showColorMenu = false;
                AddLogEntry("[THEME] Selection cancelled", DIM_GREEN);
            }
        }

        // ── Input: Widget menu ────────────────────────────────────────────
        else if (showWidgetMenu) {
            if (IsKeyPressed(KEY_UP))   { selectedWidget = (selectedWidget - 1 + WIDGET_COUNT) % WIDGET_COUNT; menuBlinkTimer = 0; }
            if (IsKeyPressed(KEY_DOWN)) { selectedWidget = (selectedWidget + 1) % WIDGET_COUNT; menuBlinkTimer = 0; }
            if (IsKeyPressed(KEY_SPACE)) {
                bool* ws[WIDGET_COUNT]; GetWidgetStates(ws);
                *ws[selectedWidget] = !(*ws[selectedWidget]);
                AddLogEntry(std::string("[WIDGET] ") + WIDGET_NAMES[selectedWidget] +
                            (*ws[selectedWidget] ? " ON" : " OFF"), CYAN_HIGHLIGHT);
                SaveConfig();
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
                showWidgetMenu = false;
                AddLogEntry("[WIDGETS] Customization closed", DIM_GREEN);
            }
        }

        // ── Input: Main menu + view-specific keys ────────────────────────
        else {
            // F5: toggle CPU stress test
            if (IsKeyPressed(KEY_F5)) {
                if (stressState == StressTestState::RUNNING) {
                    StopStressTest();
                    AddLogEntry("[STRESS] Test stopped", AMBER_PHOSPHOR);
                } else {
                    if (!stats.useRealData) {
                        AddLogEntry("[STRESS] Enable real monitoring first (menu)", YELLOW_ALERT);
                    } else {
                        StartStressTest(30);
                        AddLogEntry("[STRESS] CPU stress test started (30s)", AMBER_PHOSPHOR);
                    }
                }
            }

            // TAB always toggles the menu overlay
            if (IsKeyPressed(KEY_TAB)) {
                showMenu = !showMenu;
                AddLogEntry(showMenu ? "[INPUT] Menu opened" : "[INPUT] Menu closed", DIM_GREEN);
            }

            // Menu overlay navigation (works from any view)
            if (showMenu) {
                if (IsKeyPressed(KEY_UP))     { selectedOption = (selectedOption - 1 + MENU_COUNT) % MENU_COUNT; menuBlinkTimer = 0; }
                if (IsKeyPressed(KEY_DOWN))   { selectedOption = (selectedOption + 1) % MENU_COUNT; menuBlinkTimer = 0; }
                if (IsKeyPressed(KEY_ENTER))  HandleMenuSelection();
                if (IsKeyPressed(KEY_ESCAPE)) showMenu = false;
            }
            // View-specific keys when menu is closed
            else if (currentMenu == MENU_SYSTEM_INFO) {
                if (IsKeyPressed(KEY_ESCAPE)) {
                    currentMenu = MENU_DASHBOARD;
                    AddLogEntry("[MENU] Returned to dashboard", DIM_GREEN);
                }
            }
            else if (currentMenu == MENU_NETWORK_TEST) {
                if (IsKeyPressed(KEY_ENTER)) {
                    if (speedTestState != SpeedTestState::RUNNING)
                        StartSpeedTest();
                }
                if (IsKeyPressed(KEY_S)) {
                    SaveSpeedTestResult();
                    if (speedTestHasSaved)
                        AddLogEntry("[SPEEDTEST] Result saved to speedtest_results.txt", CYAN_HIGHLIGHT);
                }
                if (IsKeyPressed(KEY_ESCAPE)) {
                    currentMenu = MENU_DASHBOARD;
                    AddLogEntry("[NET] Returned to dashboard", DIM_GREEN);
                }
            }
        }

        // ── Update ────────────────────────────────────────────────────────
        UpdateStats(dt);

        // ── Shader uniforms ───────────────────────────────────────────────
        SetShaderValue(crtShader, timeLoc, &timeAccumulator, SHADER_UNIFORM_FLOAT);
        float tint[3] = {
            PALETTES[currentTheme].primary.r / 255.f,
            PALETTES[currentTheme].primary.g / 255.f,
            PALETTES[currentTheme].primary.b / 255.f
        };
        SetShaderValue(crtShader, tintLoc, tint, SHADER_UNIFORM_VEC3);

        // ── Render ────────────────────────────────────────────────────────
        BeginTextureMode(target);
            DrawDashboard();
        EndTextureMode();

        BeginDrawing();
            ClearBackground(COLOR_BLACK);
            BeginShaderMode(crtShader);
            {
                int sw = GetScreenWidth(), sh = GetScreenHeight();
                Rectangle src = { 0, 0, (float)target.texture.width, (float)-target.texture.height };
                float scale = (sw / (float)WINDOW_WIDTH < sh / (float)WINDOW_HEIGHT)
                              ? sw / (float)WINDOW_WIDTH : sh / (float)WINDOW_HEIGHT;
                Rectangle dst = { (sw - WINDOW_WIDTH * scale) * 0.5f,
                                  (sh - WINDOW_HEIGHT * scale) * 0.5f,
                                  WINDOW_WIDTH * scale, WINDOW_HEIGHT * scale };
                DrawTexturePro(target.texture, src, dst, {0, 0}, 0.f, WHITE);
            }
            EndShaderMode();

            // Overlays drawn AFTER shader - no CRT tint applied to them
            DrawColorMenu();
            DrawWidgetMenu();
            DrawOnboarding();
            DrawTerminal();

        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    CleanupSystemMonitoring();
    UnloadShader(crtShader);
    UnloadRenderTexture(target);
    if (hasCustomFont) UnloadFont(retroFont);
    CloseWindow();
    return 0;
}
