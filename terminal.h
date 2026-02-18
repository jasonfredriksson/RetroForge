#pragma once
#include <string>
#include <vector>

static const int MAX_TERMINAL_TABS = 4;

struct TerminalTab {
    std::string              input;
    std::vector<std::string> output;
    std::string              cwd;
    std::string              name;
};

extern bool showTerminal;
extern int  activeTab;
extern int  tabCount;
extern TerminalTab tabs[MAX_TERMINAL_TABS];

void RunTerminalCommand(const std::string& cmd);
void AddTerminalTab();
void CloseTerminalTab(int idx);
void DrawTerminal();
