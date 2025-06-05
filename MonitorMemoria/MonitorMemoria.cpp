#include <windows.h>
#include <psapi.h>
#include <shlobj.h>
#include <fstream>
#include <string>
#include <sstream>
#include <commctrl.h>
#include <tchar.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <winioctl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "pdh.lib")

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void ExportHardwareInfo();
void UpdateMemoryUsage(HWND hwnd);
std::wstring GetCPUName();
std::wstring GetGPUName();
std::wstring GetDiskInfo();
std::wstring GetCPUSpeedMHz();

MEMORYSTATUSEX memStatus;
HWND hProgressBar;
HWND hMemInfoLabel;  // Novo: label de uso de memória
UINT_PTR timerId = 1;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"HardwareInfoWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Monitoramento de Hardware",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 400,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetTimer(hwnd, timerId, 1000, NULL);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        CreateWindow(L"STATIC", L"Uso de Memória (em tempo real):",
            WS_VISIBLE | WS_CHILD,
            20, 20, 250, 20,
            hwnd, nullptr, nullptr, nullptr);

        hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, nullptr,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 50, 500, 30,
            hwnd, nullptr, nullptr, nullptr);

        hMemInfoLabel = CreateWindow(L"STATIC", L"",
            WS_VISIBLE | WS_CHILD,
            20, 85, 500, 20,
            hwnd, nullptr, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Exportar Informações para .txt",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            20, 120, 250, 30,
            hwnd, (HMENU)1, nullptr, nullptr);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            ExportHardwareInfo();
        }
        break;
    case WM_TIMER:
        if (wParam == timerId) {
            UpdateMemoryUsage(hwnd);
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, timerId);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void UpdateMemoryUsage(HWND hwnd) {
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);

    int usage = static_cast<int>(memStatus.dwMemoryLoad);
    SendMessage(hProgressBar, PBM_SETPOS, usage, 0);

    DWORDLONG totalMB = memStatus.ullTotalPhys / (1024 * 1024);
    DWORDLONG usedMB = (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024);

    std::wstringstream ss;
    ss << L"Memória Total: " << totalMB << L" MB - Utilizada: " << usedMB << L" MB";
    SetWindowText(hMemInfoLabel, ss.str().c_str());
}

void ExportHardwareInfo() {
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);

    std::wofstream file("hardware_info.txt", std::ios::out | std::ios::binary);

    // Escreve BOM UTF-8 para suportar acentos corretamente
    file << L"\xFEFF";

    file << L"=== INFORMAÇÕES DE HARDWARE ===\n";
    file << L"Processador: " << GetCPUName() << L"\n";
    file << L"Frequência do CPU: " << GetCPUSpeedMHz() << L" MHz\n";
    file << L"GPU: " << GetGPUName() << L"\n";
    file << GetDiskInfo();
    file << L"\n=== MEMÓRIA ===\n";
    file << L"Memória Total: " << memStatus.ullTotalPhys / (1024 * 1024) << L" MB\n";
    file << L"Memória Disponível: " << memStatus.ullAvailPhys / (1024 * 1024) << L" MB\n";
    file << L"Uso de Memória: " << memStatus.dwMemoryLoad << L"%\n";

    file.close();
}

std::wstring GetCPUName() {
    HKEY hKey;
    wchar_t cpuName[256];
    DWORD size = sizeof(cpuName);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueEx(hKey, L"ProcessorNameString", nullptr, nullptr, (LPBYTE)cpuName, &size);
        RegCloseKey(hKey);
        return cpuName;
    }
    return L"Desconhecido";
}

std::wstring GetGPUName() {
    DISPLAY_DEVICE dd;
    dd.cb = sizeof(dd);
    if (EnumDisplayDevices(NULL, 0, &dd, 0)) {
        return dd.DeviceString;
    }
    return L"Desconhecido";
}

std::wstring GetDiskInfo() {
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceEx(L"C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        std::wstringstream ss;
        ss << L"\n=== DISCO C: ===\n";
        ss << L"Espaço Total: " << totalBytes.QuadPart / (1024 * 1024 * 1024) << L" GB\n";
        ss << L"Espaço Livre: " << totalFreeBytes.QuadPart / (1024 * 1024 * 1024) << L" GB\n";
        return ss.str();
    }
    return L"Informações de disco indisponíveis\n";
}

std::wstring GetCPUSpeedMHz() {
    HKEY hKey;
    DWORD speed = 0;
    DWORD size = sizeof(speed);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueEx(hKey, L"~MHz", nullptr, nullptr, (LPBYTE)&speed, &size);
        RegCloseKey(hKey);
    }
    std::wstringstream ss;
    ss << speed;
    return ss.str();
}
