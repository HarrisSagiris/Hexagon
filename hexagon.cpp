#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <commctrl.h>
#include <gdiplus.h>
#include <tchar.h>

//build === g++ hexagon.cpp -o hexagon.exe -lgdiplus -lshell32 -static-libgcc -static-libstdc++
// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowContextMenu(HWND hwnd, POINT pt);
void CreateMainWindow();
void InitNotifyIcon(HWND hwnd);
void CleanupNotifyIcon(HWND hwnd);
void LoadModels();
std::string GenerateText(const std::string& model, const std::string& prompt);

// Global variables
NOTIFYICONDATA nid = {};
HWND g_hwnd = NULL;
std::vector<std::string> g_models;
const UINT WM_TRAYICON = WM_APP + 1;
const int IDM_MODEL_START = 1000;
const int IDM_GENERATE = 2000;
HICON g_hIcon = NULL;

// Main window class name
const TCHAR* CLASS_NAME = _T("HexagonTrayApp");

// Hugging Face API endpoint and token
const std::string API_ENDPOINT = "https://api-inference.huggingface.co/models/";
const std::string API_TOKEN = "YOUR_HUGGINGFACE_TOKEN"; // Replace with your token

// Callback for HTTP requests
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class with custom icon
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); 
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassEx(&wc);

    // Create main window
    CreateMainWindow();

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
    return 0;
}

void CreateMainWindow() {
    g_hwnd = CreateWindow(
        CLASS_NAME,
        _T("Hexagon AI Assistant"),
        WS_POPUP,
        0, 0, 0, 0,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (g_hwnd) {
        InitNotifyIcon(g_hwnd);
    }
}

void InitNotifyIcon(HWND hwnd) {
    // Create custom icon using GDI+
    const int iconSize = 16;
    Gdiplus::Bitmap bitmap(iconSize, iconSize);
    Gdiplus::Graphics graphics(&bitmap);
    
    // Draw a hexagon with gradient
    Gdiplus::GraphicsPath path;
    const float radius = iconSize / 2.0f - 1;
    const float centerX = iconSize / 2.0f;
    const float centerY = iconSize / 2.0f;
    
    for (int i = 0; i < 6; i++) {
        float angle = i * 60.0f * 3.14159f / 180.0f;
        float x = centerX + radius * cos(angle);
        float y = centerY + radius * sin(angle);
        if (i == 0)
            path.StartFigure();
        else
            path.AddLine(x, y, x, y);
    }
    path.CloseFigure();

    // Create gradient brush
    Gdiplus::LinearGradientBrush gradientBrush(
        Gdiplus::Point(0, 0),
        Gdiplus::Point(iconSize, iconSize),
        Gdiplus::Color(255, 41, 128, 185),  // Nice blue
        Gdiplus::Color(255, 52, 152, 219)   // Lighter blue
    );

    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.FillPath(&gradientBrush, &path);

    // Convert to icon
    HICON hIcon = NULL;
    bitmap.GetHICON(&hIcon);
    g_hIcon = hIcon;

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_hIcon;
    lstrcpy(nid.szTip, _T("Hexagon AI Assistant"));
    Shell_NotifyIcon(NIM_ADD, &nid);

    // Set version for balloon notifications
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
}

void ShowContextMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    HMENU hModelMenu = CreatePopupMenu();

    // Add models submenu with icons and modern styling
    for (size_t i = 0; i < g_models.size(); i++) {
        MENUITEMINFO mii = {sizeof(MENUITEMINFO)};
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
        mii.wID = IDM_MODEL_START + i;
        mii.fState = MFS_ENABLED;
        mii.dwTypeData = (LPSTR)g_models[i].c_str();
        InsertMenuItemA(hModelMenu, i, TRUE, &mii);
    }
    
    // Modern menu styling
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
    // Placeholder for HTTP request implementation
    return "Text generation not implemented yet";
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
                // Show balloon notification for model selection
                lstrcpy(nid.szInfoTitle, _T("Model Selected"));
                std::wstring infoText = std::wstring(_T("Now using: ")) + std::wstring(g_models[modelIndex].begin(), g_models[modelIndex].end());
                lstrcpy(nid.szInfo, infoText.c_str());
                nid.uFlags |= NIF_INFO;
                nid.dwInfoFlags = NIIF_INFO;
                Shell_NotifyIcon(NIM_MODIFY, &nid);
            }
            else if (LOWORD(wParam) == IDM_GENERATE) {
                // Show message box for text generation
                std::string result = GenerateText(g_models[0], "Hello, how are you?");
                MessageBox(hwnd, std::wstring(result.begin(), result.end()).c_str(), 
                          _T("Generated Text"), MB_OK | MB_ICONINFORMATION);
            }
            else if (LOWORD(wParam) == IDM_GENERATE + 1) {
                DestroyWindow(hwnd);
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
