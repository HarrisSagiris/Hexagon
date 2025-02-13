#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <commctrl.h>
#include <gdiplus.h>
#include <tchar.h>
#include <fstream>
#include <sstream>

// Link required libraries
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowContextMenu(HWND hwnd, POINT pt);
void CreateMainWindow();
void InitNotifyIcon(HWND hwnd);
void CleanupNotifyIcon(HWND hwnd);
void LoadModels();
std::string GenerateText(const std::string& model, const std::string& prompt);
void ShowError(const std::string& message);
void LoadConfig();
void SaveConfig();

// Global variables
NOTIFYICONDATA nid = {};
HWND g_hwnd = NULL;
std::vector<std::string> g_models;
const UINT WM_TRAYICON = WM_APP + 1;
const int IDM_MODEL_START = 1000;
const int IDM_GENERATE = 2000;
HICON g_hIcon = NULL;
std::string g_selectedModel;

// Config struct
struct Config {
    std::string apiToken = "hf_your_token_here";
    std::string selectedModel;
    bool darkMode;
} g_config;

// Main window class name
const TCHAR* CLASS_NAME = _T("HexagonTrayApp");

// Hugging Face API endpoint
const std::string API_ENDPOINT = "https://api-inference.huggingface.co/models/";

void LoadConfig() {
    std::ifstream configFile("config.json");
    if (configFile.is_open()) {
        std::string line;
        std::string content;
        while (std::getline(configFile, line)) {
            content += line;
        }
        configFile.close();

        size_t tokenPos = content.find("\"apiToken\"");
        if (tokenPos != std::string::npos) {
            size_t valueStart = content.find(":", tokenPos) + 1;
            size_t valueEnd = content.find(",", valueStart);
            std::string token = content.substr(valueStart, valueEnd - valueStart);
            // Remove whitespace and quotes
            token.erase(0, token.find_first_not_of(" \t\n\r\""));
            token.erase(token.find_last_not_of(" \t\n\r\"") + 1);
            g_config.apiToken = token;
        }

        size_t modelPos = content.find("\"selectedModel\"");
        if (modelPos != std::string::npos) {
            size_t valueStart = content.find(":", modelPos) + 1;
            size_t valueEnd = content.find(",", valueStart);
            std::string model = content.substr(valueStart, valueEnd - valueStart);
            // Remove whitespace and quotes
            model.erase(0, model.find_first_not_of(" \t\n\r\""));
            model.erase(model.find_last_not_of(" \t\n\r\"") + 1);
            g_config.selectedModel = model;
        }

        size_t darkModePos = content.find("\"darkMode\"");
        if (darkModePos != std::string::npos) {
            size_t valueStart = content.find(":", darkModePos) + 1;
            size_t valueEnd = content.find("}", valueStart);
            std::string darkMode = content.substr(valueStart, valueEnd - valueStart);
            // Remove whitespace
            darkMode.erase(0, darkMode.find_first_not_of(" \t\n\r"));
            darkMode.erase(darkMode.find_last_not_of(" \t\n\r") + 1);
            g_config.darkMode = (darkMode == "true");
        }
    }
}

void SaveConfig() {
    std::ofstream configFile("config.json");
    configFile << "{\n";
    configFile << "    \"apiToken\": \"" << g_config.apiToken << "\",\n";
    configFile << "    \"selectedModel\": \"" << g_config.selectedModel << "\",\n";
    configFile << "    \"darkMode\": " << (g_config.darkMode ? "true" : "false") << "\n";
    configFile << "}";
    configFile.close();
}

void ShowError(const std::string& message) {
    MessageBoxA(NULL, message.c_str(), "Error", MB_OK | MB_ICONERROR);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Load config
    LoadConfig();

    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassEx(&wc)) {
        ShowError("Failed to register window class");
        return 1;
    }

    // Create main window
    CreateMainWindow();
    if (!g_hwnd) {
        ShowError("Failed to create main window");
        return 1;
    }

    // Load available models
    LoadModels();

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    Gdiplus::GdiplusShutdown(gdiplusToken);
    if (g_hIcon) DestroyIcon(g_hIcon);
    SaveConfig();
    return 0;
}

void CreateMainWindow() {
    g_hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        _T("Hexagon AI Assistant"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (g_hwnd) {
        ShowWindow(g_hwnd, SW_HIDE);
        InitNotifyIcon(g_hwnd);
    }
}

void InitNotifyIcon(HWND hwnd) {
    const int iconSize = 16;
    Gdiplus::Bitmap bitmap(iconSize, iconSize);
    Gdiplus::Graphics graphics(&bitmap);
    
    Gdiplus::GraphicsPath path;
    const float radius = iconSize / 2.0f - 1;
    const float centerX = iconSize / 2.0f;
    const float centerY = iconSize / 2.0f;
    
    // Create hexagon path
    for (int i = 0; i < 6; i++) {
        float angle = i * 60.0f * 3.14159f / 180.0f;
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        if (i == 0)
            path.StartFigure();
        path.AddLine(x, y, x, y);
    }
    path.CloseFigure();

    // Create gradient brush
    Gdiplus::LinearGradientBrush gradientBrush(
        Gdiplus::Point(0, 0),
        Gdiplus::Point(iconSize, iconSize),
        Gdiplus::Color(255, 41, 128, 185),
        Gdiplus::Color(255, 52, 152, 219)
    );

    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.FillPath(&gradientBrush, &path);

    HICON hIcon = NULL;
    bitmap.GetHICON(&hIcon);
    g_hIcon = hIcon;

    // Setup notification icon
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_hIcon;
    lstrcpy(nid.szTip, _T("Hexagon AI Assistant"));
    
    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
        ShowError("Failed to create tray icon");
        return;
    }

    nid.uVersion = NOTIFYICON_VERSION;
    Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void CleanupNotifyIcon(HWND hwnd) {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void LoadModels() {
    g_models = {
        "gpt2",
        "bert-base-uncased", 
        "t5-base",
        "facebook/bart-large-cnn"
    };
    
    if (g_config.selectedModel.empty()) {
        g_config.selectedModel = g_models[0];
    }
}

void ShowContextMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    HMENU hModelMenu = CreatePopupMenu();

    for (size_t i = 0; i < g_models.size(); i++) {
        MENUITEMINFO mii = {sizeof(MENUITEMINFO)};
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
        mii.wID = IDM_MODEL_START + i;
        mii.fState = (g_models[i] == g_config.selectedModel) ? MFS_CHECKED : MFS_ENABLED;
        mii.dwTypeData = (LPSTR)g_models[i].c_str();
        InsertMenuItemA(hModelMenu, i, TRUE, &mii);
    }
    
    MENUINFO mi = {sizeof(MENUINFO)};
    mi.fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS;
    mi.dwStyle = MNS_NOTIFYBYPOS;
    SetMenuInfo(hMenu, &mi);

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hModelMenu, _T("Select Model"));
    AppendMenu(hMenu, MF_STRING, IDM_GENERATE, _T("Generate Text"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_GENERATE + 1, _T("Exit"));

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_VERNEGANIMATION,
                   pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

std::string GenerateText(const std::string& model, const std::string& prompt) {
    return "API functionality not implemented";
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            CleanupNotifyIcon(hwnd);
            PostQuitMessage(0);
            return 0;

        case WM_TRAYICON:
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                ShowContextMenu(hwnd, pt);
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) >= IDM_MODEL_START && LOWORD(wParam) < IDM_MODEL_START + g_models.size()) {
                int modelIndex = LOWORD(wParam) - IDM_MODEL_START;
                g_config.selectedModel = g_models[modelIndex];
                
                nid.szInfoTitle[0] = '\0';
                std::wstring infoText = std::wstring(_T("Now using: ")) + 
                    std::wstring(g_config.selectedModel.begin(), g_config.selectedModel.end());
                lstrcpy(nid.szInfo, infoText.c_str());
                nid.uFlags |= NIF_INFO;
                nid.dwInfoFlags = NIIF_INFO;
                Shell_NotifyIcon(NIM_MODIFY, &nid);
                SaveConfig();
            }
            else if (LOWORD(wParam) == IDM_GENERATE) {
                std::string result = GenerateText(g_config.selectedModel, "Hello, how are you?");
                MessageBoxA(hwnd, result.c_str(), "Generated Text", MB_OK | MB_ICONINFORMATION);
            }
            else if (LOWORD(wParam) == IDM_GENERATE + 1) {
                DestroyWindow(hwnd);
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
