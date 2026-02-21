#include "ui_menus.h"
#include "dashboard.h"
#include "theme.h"
#include "raylib.h"
#include <cmath>

// ── State ─────────────────────────────────────────────────────────────────────
bool showOnboarding = false;
int  onboardingStep = 0;
bool showColorMenu  = false;
int  selectedTheme  = 0;

// ── Onboarding ────────────────────────────────────────────────────────────────
void DrawOnboarding() {
    if (!showOnboarding) return;

    const int BW = 800, BH = 500;
    const int BX = WINDOW_WIDTH / 2 - BW / 2;
    const int BY = WINDOW_HEIGHT / 2 - BH / 2;

    // Neutral colors - not affected by current theme
    const Color NT = {255, 255, 255, 255};  // title
    const Color NS = {180, 180, 180, 255};  // subtitle
    const Color CU = {255, 255, 100, 255};  // cursor

    DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, ColorAlpha(COLOR_BLACK, 0.95f));
    DrawRectangle(BX, BY, BW, BH, {12, 12, 12, 255});
    DrawRectangleLines(BX, BY, BW, BH, NT);

    if (onboardingStep == 0) {
        DrawText("WELCOME TO RETROFORGE", BX + 80,  BY + 50,  28, NT);
        DrawText("A customizable system monitoring terminal",  BX + 150, BY + 95,  16, NS);
        DrawLine(BX + 20, BY + 125, BX + BW - 20, BY + 125, {60, 60, 60, 255});
        DrawText("- Real-time system monitoring (CPU, RAM, Disk)", BX + 60, BY + 145, 16, NS);
        DrawText("- Customizable widgets",                         BX + 60, BY + 172, 16, NS);
        DrawText("- Multiple color themes",                        BX + 60, BY + 199, 16, NS);
        DrawText("- Embedded terminal",                            BX + 60, BY + 226, 16, NS);
        DrawText("- Authentic CRT effects",                        BX + 60, BY + 253, 16, NS);
        DrawLine(BX + 20, BY + 290, BX + BW - 20, BY + 290, {60, 60, 60, 255});
        DrawText("ENTER: customize your dashboard", BX + 240, BY + 320, 18, CU);
        DrawText("ESC: skip and use defaults",       BX + 270, BY + 355, 15, NS);
    }
    else if (onboardingStep == 1) {
        DrawText("SELECT YOUR COLOR THEME", BX + 220, BY + 40, 26, NT);
        DrawText("Choose the CRT phosphor color that suits you", BX + 180, BY + 82, 15, NS);
        DrawLine(BX + 20, BY + 112, BX + BW - 20, BY + 112, {60, 60, 60, 255});

        int ty = BY + 128;
        for (int i = 0; i < THEME_COUNT; i++) {
            bool sel = (i == selectedTheme);
            if (sel) DrawRectangle(BX + 4, ty - 4, BW - 8, 42, {35, 35, 35, 255});
            if (sel && fmod(menuBlinkTimer, 1.f) < 0.5f)
                DrawText(">", BX + 14, ty + 8, 18, CU);
            DrawText(THEME_NAMES[i], BX + 40, ty + 6, 20, PALETTES[i].primary);
            DrawRectangle(BX + 490, ty + 4, 28, 28, PALETTES[i].primary);
            DrawRectangle(BX + 526, ty + 4, 28, 28, PALETTES[i].secondary);
            DrawRectangle(BX + 562, ty + 4, 28, 28, PALETTES[i].accent);
            ty += 50;
        }

        DrawLine(BX + 20, BY + BH - 50, BX + BW - 20, BY + BH - 50, {60, 60, 60, 255});
        DrawText("UP/DOWN navigate   ENTER select   ESC skip", BX + 190, BY + BH - 36, 14, NS);
    }
}

// ── Color theme selector ──────────────────────────────────────────────────────
void DrawColorMenu() {
    if (!showColorMenu) return;

    DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, ColorAlpha(COLOR_BLACK, 0.80f));

    const int MW = 640, MH = 420;
    const int MX = WINDOW_WIDTH / 2 - MW / 2;
    const int MY = WINDOW_HEIGHT / 2 - MH / 2;

    const Color BD = {200, 200, 200, 255};
    const Color TT = {255, 255, 255, 255};
    const Color TX = {180, 180, 180, 255};
    const Color CU = {255, 255, 100, 255};

    DrawRectangle(MX, MY, MW, MH, {10, 10, 10, 255});
    DrawRectangleLines(MX, MY, MW, MH, BD);
    DrawText("COLOR THEME SELECTOR", MX + 140, MY + 18, 24, TT);
    DrawLine(MX + 10, MY + 52, MX + MW - 10, MY + 52, {70, 70, 70, 255});
    DrawText("UP/DOWN navigate   ENTER apply   ESC cancel", MX + 80, MY + MH - 28, 14, TX);

    int ty = MY + 66;
    for (int i = 0; i < THEME_COUNT; i++) {
        bool sel = (i == selectedTheme);
        if (sel) DrawRectangle(MX + 4, ty - 4, MW - 8, 42, {38, 38, 38, 255});
        if (sel && fmod(menuBlinkTimer, 1.f) < 0.5f)
            DrawText(">", MX + 14, ty + 8, 18, CU);
        DrawText(THEME_NAMES[i], MX + 40, ty + 6, 20, PALETTES[i].primary);
        DrawRectangle(MX + 370, ty + 4, 28, 28, PALETTES[i].primary);
        DrawRectangle(MX + 406, ty + 4, 28, 28, PALETTES[i].secondary);
        DrawRectangle(MX + 442, ty + 4, 28, 28, PALETTES[i].accent);
        DrawRectangleLines(MX + 370, ty + 4, 28, 28, BD);
        DrawRectangleLines(MX + 406, ty + 4, 28, 28, BD);
        DrawRectangleLines(MX + 442, ty + 4, 28, 28, BD);
        if (i == currentTheme)
            DrawText("ACTIVE", MX + 490, ty + 10, 14, CU);
        ty += 56;
    }
}

// ── Widget customization ──────────────────────────────────────────────────────
void DrawWidgetMenu() {
    if (!showWidgetMenu) return;

    DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, ColorAlpha(COLOR_BLACK, 0.80f));

    const int MW = 520, MH = WIDGET_COUNT * 38 + 110;
    const int MX = WINDOW_WIDTH / 2 - MW / 2;
    const int MY = WINDOW_HEIGHT / 2 - MH / 2;

    const Color BD = {200, 200, 200, 255};
    const Color TT = {255, 255, 255, 255};
    const Color TX = {180, 180, 180, 255};
    const Color CU = {255, 255, 100, 255};
    const Color ON = {80,  255,  80, 255};
    const Color OF = {120, 120, 120, 255};

    DrawRectangle(MX, MY, MW, MH, {10, 10, 10, 255});
    DrawRectangleLines(MX, MY, MW, MH, BD);
    DrawText("WIDGET CUSTOMIZATION", MX + 20, MY + 16, 22, TT);
    DrawLine(MX + 10, MY + 48, MX + MW - 10, MY + 48, {70, 70, 70, 255});
    DrawText("UP/DOWN navigate   SPACE toggle   ESC close", MX + 20, MY + MH - 28, 13, TX);

    bool* ws[WIDGET_COUNT];
    GetWidgetStates(ws);

    int oy = MY + 56;
    for (int i = 0; i < WIDGET_COUNT; i++) {
        bool sel = (i == selectedWidget);
        if (sel) DrawRectangle(MX + 4, oy - 2, MW - 8, 32, {38, 38, 38, 255});
        if (sel && fmod(menuBlinkTimer, 1.f) < 0.5f)
            DrawText(">", MX + 12, oy + 6, 16, CU);
        DrawText(WIDGET_NAMES[i], MX + 36, oy + 4, 17, sel ? TT : TX);
        DrawText(*ws[i] ? "ON" : "OFF", MX + MW - 55, oy + 6, 16, *ws[i] ? ON : OF);
        oy += 36;
    }
}
