#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <string>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

using namespace std;

#define ID_TABCTRL 1001
#define ID_BENCHMARK_BUTTON 1002
#define TIMER_ID 2001

HWND hTabCtrl, hBenchmarkButton;
HWND hLabels[5];
HWND hProgressBar;
MEMORYSTATUSEX memStatus;

// Funções para obter informações

std::wstring GetCPUName() {
    HKEY hKey;
    wchar_t cpuName[256] = L"Desconhecido";
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

void UpdateMemoryProgress() {
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);

    int usage = static_cast<int>(memStatus.dwMemoryLoad);
    SendMessage(hProgressBar, PBM_SETPOS, usage, 0);

    wstringstream ssUsed, ssTotal;
    ssUsed << L"Usada: " << (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024) << L" MB";
    ssTotal << L"Total: " << memStatus.ullTotalPhys / (1024 * 1024) << L" MB";

    SetWindowText(hLabels[3], ssUsed.str().c_str());
    SetWindowText(hLabels[4], ssTotal.str().c_str());
}

void ShowBenchmarkResult(HWND hwnd) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    const int size = 100000000;
    int* arr = new int[size];
    for (int i = 0; i < size; ++i) arr[i] = i;

    long long sum = 0;
    for (int i = 0; i < size; ++i) sum += arr[i];

    QueryPerformanceCounter(&end);
    delete[] arr;

    double elapsed = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
    wstringstream ss;
    ss << L"Tempo de acesso a memória: " << elapsed << L" segundos.";

    MessageBox(hwnd, ss.str().c_str(), L"Benchmark da Memória RAM", MB_OK | MB_ICONINFORMATION);
}

void ShowOnlyTabContents(int tabIndex) {
    // Esconder tudo primeiro
    for (int i = 0; i < 5; ++i) {
        ShowWindow(hLabels[i], SW_HIDE);
    }
    ShowWindow(hProgressBar, SW_HIDE);
    ShowWindow(hBenchmarkButton, SW_HIDE);

    switch (tabIndex) {
    case 0: // Sistema
        ShowWindow(hLabels[0], SW_SHOW);
        break;
    case 1: // CPU
        ShowWindow(hLabels[1], SW_SHOW);
        break;
    case 2: // GPU
        ShowWindow(hLabels[2], SW_SHOW);
        break;
    case 3: // Memória
        ShowWindow(hLabels[3], SW_SHOW);
        ShowWindow(hLabels[4], SW_SHOW);
        ShowWindow(hProgressBar, SW_SHOW);
        ShowWindow(hBenchmarkButton, SW_SHOW);
        break;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES | ICC_PROGRESS_CLASS };
        InitCommonControlsEx(&icex);

        hTabCtrl = CreateWindowEx(0, WC_TABCONTROL, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            10, 10, 520, 320,
            hwnd, (HMENU)ID_TABCTRL, GetModuleHandle(NULL), NULL);

        TCITEM tie = { TCIF_TEXT };
        tie.pszText = const_cast<LPWSTR>(L"Sistema"); TabCtrl_InsertItem(hTabCtrl, 0, &tie);
        tie.pszText = const_cast<LPWSTR>(L"CPU");     TabCtrl_InsertItem(hTabCtrl, 1, &tie);
        tie.pszText = const_cast<LPWSTR>(L"GPU");     TabCtrl_InsertItem(hTabCtrl, 2, &tie);
        tie.pszText = const_cast<LPWSTR>(L"Memória"); TabCtrl_InsertItem(hTabCtrl, 3, &tie);
        // Aba Disco removida

        // Criar controles, mas ocultos inicialmente
        wchar_t* envValue = nullptr;
        size_t len = 0;
        _wdupenv_s(&envValue, &len, L"OS");
        wstring sistemaTexto = envValue ? (L"Windows: " + wstring(envValue)) : L"Windows: ";
        free(envValue);

        hLabels[0] = CreateWindow(L"STATIC", sistemaTexto.c_str(), WS_CHILD | WS_VISIBLE, 30, 50, 480, 30, hwnd, NULL, NULL, NULL);
        hLabels[1] = CreateWindow(L"STATIC", GetCPUName().c_str(), WS_CHILD | WS_VISIBLE, 30, 50, 480, 30, hwnd, NULL, NULL, NULL);
        hLabels[2] = CreateWindow(L"STATIC", GetGPUName().c_str(), WS_CHILD | WS_VISIBLE, 30, 50, 480, 30, hwnd, NULL, NULL, NULL);

        hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 30, 90, 400, 30, hwnd, NULL, NULL, NULL);
        hLabels[3] = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 30, 130, 200, 20, hwnd, NULL, NULL, NULL);
        hLabels[4] = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 240, 130, 200, 20, hwnd, NULL, NULL, NULL);

        hBenchmarkButton = CreateWindow(L"BUTTON", L"Benchmark da RAM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 30, 180, 200, 30, hwnd, (HMENU)ID_BENCHMARK_BUTTON, NULL, NULL);

        // Inicialmente mostrar só a aba Sistema
        ShowOnlyTabContents(0);

        SetTimer(hwnd, TIMER_ID, 1000, NULL);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BENCHMARK_BUTTON) {
            ShowBenchmarkResult(hwnd);
        }
        break;
    case WM_TIMER:
        if (wParam == TIMER_ID) {
            UpdateMemoryProgress();
        }
        break;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == hTabCtrl && ((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
            int selectedTab = TabCtrl_GetCurSel(hTabCtrl);
            ShowOnlyTabContents(selectedTab);
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"HardwareMonitorClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Monitor de Hardware",
        WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME),  // Janela não redimensionável
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 400,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
