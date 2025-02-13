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
#include <winhttp.h>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

// Link required libraries
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib") 
#pragma comment(lib, "winhttp.lib")

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ChatWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ImageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TextWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowContextMenu(HWND hwnd, POINT pt);
void CreateMainWindow();
void InitNotifyIcon(HWND hwnd);
void CleanupNotifyIcon(HWND hwnd);
void LoadModels();
std::string GenerateText(const std::string& model, const std::string& prompt);
void ShowError(const std::string& message);
void LoadConfig();
void SaveConfig();
void ShowChatWindow();
void ShowImageWindow();
void ShowTextWindow();
std::string MakeHttpRequest(const std::string& url, const std::string& data, const std::string& authToken);
void ProcessMessageQueue();

// Global variables
NOTIFYICONDATA nid = {};
HWND g_hwnd = NULL;
HWND g_chatWnd = NULL;
HWND g_imageWnd = NULL;
HWND g_textWnd = NULL;
std::vector<std::string> g_models;
const UINT WM_TRAYICON = WM_APP + 1;
const int IDM_MODEL_START = 1000;
const int IDM_CHAT = 2000;
const int IDM_IMAGE = 2001;
const int IDM_TEXT = 2002;
const int IDM_EXIT = 2003;
const int IDC_EDIT = 3000;
const int IDC_SEND = 3001;
const int IDC_OUTPUT = 3002;
HICON g_hIcon = NULL;
HFONT g_hFont = NULL;

// Message queue for async processing
std::queue<std::pair<HWND, std::string>> g_messageQueue;
std::mutex g_queueMutex;
std::condition_variable g_queueCV;
bool g_running = true;

// Config struct
struct Config {
    std::string apiToken = "hf_your_token_here";
    std::string selectedModel;
    bool darkMode;
} g_config;

// Main window class names
const TCHAR* CLASS_NAME = _T("HexagonTrayApp");
const TCHAR* CHAT_CLASS = _T("HexagonChatWindow");
const TCHAR* IMAGE_CLASS = _T("HexagonImageWindow");
const TCHAR* TEXT_CLASS = _T("HexagonTextWindow");

// Hugging Face API endpoint
const std::string API_ENDPOINT = "https://api-inference.huggingface.co/models/";

// Helper function to make HTTP requests
std::string MakeHttpRequest(const std::string& url, const std::string& data, const std::string& authToken) {
    HINTERNET hSession = WinHttpOpen(L"Hexagon AI/1.0", 
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, 
                                   WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (!hSession) return "Failed to initialize HTTP session";

    URL_COMPONENTS urlComp = {sizeof(URL_COMPONENTS)};
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;

    std::wstring wideUrl(url.begin(), url.end());
    WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &urlComp);

    HINTERNET hConnect = WinHttpConnect(hSession, 
                                      std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength).c_str(),
                                      urlComp.nPort, 0);
    
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "Failed to connect to server";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                          std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength).c_str(),
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "Failed to create request";
    }

    // Add authorization header
    std::string authHeader = "Authorization: Bearer " + authToken;
    std::wstring wideAuthHeader(authHeader.begin(), authHeader.end());
    WinHttpAddRequestHeaders(hRequest, wideAuthHeader.c_str(), -1L,
                           WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           (LPVOID)data.c_str(), data.length(), data.length(), 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "Failed to send/receive request";
    }

    // Read response
    std::string response;
    DWORD bytesAvailable;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            buffer[bytesRead] = 0;
            response += buffer.data();
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

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
            token.erase(0, token.find_first_not_of(" \t\n\r\""));
            token.erase(token.find_last_not_of(" \t\n\r\"") + 1);
            g_config.apiToken = token;
        }

        size_t modelPos = content.find("\"selectedModel\"");
        if (modelPos != std::string::npos) {
            size_t valueStart = content.find(":", modelPos) + 1;
            size_t valueEnd = content.find(",", valueStart);
            std::string model = content.substr(valueStart, valueEnd - valueStart);
            model.erase(0, model.find_first_not_of(" \t\n\r\""));
            model.erase(model.find_last_not_of(" \t\n\r\"") + 1);
            g_config.selectedModel = model;
        }

        size_t darkModePos = content.find("\"darkMode\"");
        if (darkModePos != std::string::npos) {
            size_t valueStart = content.find(":", darkModePos) + 1;
            size_t valueEnd = content.find("}", valueStart);
            std::string darkMode = content.substr(valueStart, valueEnd - valueStart);
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

void ProcessMessageQueue() {
    while (g_running) {
        std::unique_lock<std::mutex> lock(g_queueMutex);
        g_queueCV.wait(lock, [] { return !g_messageQueue.empty() || !g_running; });
        
        if (!g_running) break;
        
        auto [hwnd, prompt] = g_messageQueue.front();
        g_messageQueue.pop();
        lock.unlock();

        std::string response = GenerateText(g_config.selectedModel, prompt);
        
        // Update UI on main thread
        PostMessage(hwnd, WM_APP, 0, (LPARAM)_strdup(response.c_str()));
    }
}

void ShowChatWindow() {
    if (g_chatWnd) {
        SetForegroundWindow(g_chatWnd);
        return;
    }

    WNDCLASSEX wc = {sizeof(WNDCLASSEX)};
    wc.lpfnWndProc = ChatWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CHAT_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassEx(&wc);

    g_chatWnd = CreateWindowEx(
        0, CHAT_CLASS, _T("Chat with AI"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    ShowWindow(g_chatWnd, SW_SHOW);
    UpdateWindow(g_chatWnd);
}

void ShowImageWindow() {
    if (g_imageWnd) {
        SetForegroundWindow(g_imageWnd);
        return;
    }

    WNDCLASSEX wc = {sizeof(WNDCLASSEX)};
    wc.lpfnWndProc = ImageWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = IMAGE_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassEx(&wc);

    g_imageWnd = CreateWindowEx(
        0, IMAGE_CLASS, _T("Generate Image"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    ShowWindow(g_imageWnd, SW_SHOW);
    UpdateWindow(g_imageWnd);
}

void ShowTextWindow() {
    if (g_textWnd) {
        SetForegroundWindow(g_textWnd);
        return;
    }

    WNDCLASSEX wc = {sizeof(WNDCLASSEX)};
    wc.lpfnWndProc = TextWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassEx(&wc);

    g_textWnd = CreateWindowEx(
        0, TEXT_CLASS, _T("Generate Text"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    ShowWindow(g_textWnd, SW_SHOW);
    UpdateWindow(g_textWnd);
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

    // Create default font
    g_hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("Segoe UI"));

    // Start message processing thread
    std::thread messageThread(ProcessMessageQueue);

    // Register window classes
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
    g_running = false;
    g_queueCV.notify_one();
    messageThread.join();
    
    Gdiplus::GdiplusShutdown(gdiplusToken);
    if (g_hIcon) DestroyIcon(g_hIcon);
    if (g_hFont) DeleteObject(g_hFont);
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
        "facebook/bart-large-cnn",
        "stabilityai/stable-diffusion-2",
        "openai/clip-vit-base-patch32"
    };
    
    if (g_config.selectedModel.empty()) {
        g_config.selectedModel = g_models[0];
    }
}

void ShowContextMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    HMENU hModelMenu = CreatePopupMenu();

    // Add AI interaction options
    AppendMenu(hMenu, MF_STRING, IDM_CHAT, _T("Chat with AI"));
    AppendMenu(hMenu, MF_STRING, IDM_IMAGE, _T("Generate Image"));
    AppendMenu(hMenu, MF_STRING, IDM_TEXT, _T("Generate Text"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Add model selection submenu
    for (size_t i = 0; i < g_models.size(); i++) {
        MENUITEMINFO mii = {sizeof(MENUITEMINFO)};
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE;
        mii.wID = IDM_MODEL_START + i;
        mii.fState = (g_models[i] == g_config.selectedModel) ? MFS_CHECKED : MFS_ENABLED;
        mii.dwTypeData = (LPSTR)g_models[i].c_str();
        InsertMenuItemA(hModelMenu, i, TRUE, &mii);
    }
    
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hModelMenu, _T("Select Model"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, _T("Exit"));

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_VERNEGANIMATION,
                   pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

std::string GenerateText(const std::string& model, const std::string& prompt) {
    std::string url = API_ENDPOINT + model;
    std::string data = "{\"inputs\":\"" + prompt + "\"}";
    return MakeHttpRequest(url, data, g_config.apiToken);
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
                std::string infoText = "Now using: " + g_config.selectedModel;
                lstrcpyA(nid.szInfo, infoText.c_str());
                nid.uFlags |= NIF_INFO;
                nid.dwInfoFlags = NIIF_INFO;
                Shell_NotifyIcon(NIM_MODIFY, &nid);
                SaveConfig();
            }
            else {
                switch (LOWORD(wParam)) {
                    case IDM_CHAT:
                        ShowChatWindow();
                        break;
                    case IDM_IMAGE:
                        ShowImageWindow();
                        break;
                    case IDM_TEXT:
                        ShowTextWindow();
                        break;
                    case IDM_EXIT:
                        DestroyWindow(hwnd);
                        break;
                }
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
