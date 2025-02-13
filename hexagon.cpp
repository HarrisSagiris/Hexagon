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
LRESULT CALLBACK ChatWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
LRESULT CALLBACK ImageWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
LRESULT CALLBACK TextWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
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
