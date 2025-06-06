#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <intrin.h>
#include <winternl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ntdll.lib")

using namespace std;

#define ID_TABCTRL 1001
#define ID_BENCHMARK_BUTTON 1002
#define TIMER_ID 2001

HWND hTabCtrl, hBenchmarkButton;
HWND hLabels[10];
HWND hProgressBar;

MEMORYSTATUSEX memStatus;

typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

bool GetRealOSVersion(OSVERSIONINFOEXW& osvi) {
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr) {
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            return fxPtr((PRTL_OSVERSIONINFOW)&osvi) == 0;
        }
    }
    return false;
}

wstring GetOSInfo() {
    wstringstream ss;

    OSVERSIONINFOEXW osvi = {};
    if (GetRealOSVersion(osvi)) {
        ss << L"Nome: Windows\n";
        ss << L"Versão: " << osvi.dwMajorVersion << L"." << osvi.dwMinorVersion << L"\n";
        ss << L"Build: " << osvi.dwBuildNumber << L"\n";
    }

    // Data de instalação
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD installDate = 0;
        DWORD size = sizeof(installDate);
        if (RegQueryValueEx(hKey, L"InstallDate", NULL, NULL, (LPBYTE)&installDate, &size) == ERROR_SUCCESS) {
            time_t rawTime = installDate;
            tm timeInfo;
            localtime_s(&timeInfo, &rawTime);
            wchar_t buffer[100];
            wcsftime(buffer, 100, L"%d/%m/%Y %H:%M:%S", &timeInfo);
            ss << L"Instalado em: " << buffer << L"\n";
        }

        wchar_t arch[32];
        DWORD archSize = sizeof(arch);
        if (RegQueryValueEx(hKey, L"BuildLabEx", NULL, NULL, (LPBYTE)arch, &archSize) == ERROR_SUCCESS) {
            ss << L"Arquitetura: " << (sizeof(void*) == 8 ? L"x64" : L"x86") << L"\n";
        }

        wchar_t prodId[128];
        DWORD prodSize = sizeof(prodId);
        if (RegQueryValueEx(hKey, L"ProductId", NULL, NULL, (LPBYTE)prodId, &prodSize) == ERROR_SUCCESS) {
            ss << L"ID do Produto: " << prodId;
        }

        RegCloseKey(hKey);
    }

    return ss.str();
}

wstring GetCPUInfo() {
    wstringstream ss;

    HKEY hKey;
    wchar_t cpuName[256] = L"Desconhecido";
    DWORD size = sizeof(cpuName);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueEx(hKey, L"ProcessorNameString", nullptr, nullptr, (LPBYTE)cpuName, &size);
        RegCloseKey(hKey);
    }

    ss << L"Nome: " << cpuName << L"\n";

    // Clock atual
    int speedMHz = 0;
    HKEY hSpeedKey;
    DWORD speedSize = sizeof(speedMHz);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hSpeedKey) == ERROR_SUCCESS) {
        RegQueryValueEx(hSpeedKey, L"~MHz", nullptr, nullptr, (LPBYTE)&speedMHz, &speedSize);
        RegCloseKey(hSpeedKey);
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    ss << L"Núcleos: " << sysInfo.dwNumberOfProcessors << L"\n";
    ss << L"Clock Atual: " << speedMHz << L" MHz\n";
    ss << L"Clock Máximo: (estimado) " << speedMHz << L" MHz\n";
    ss << L"Voltagem: (não disponível via WinAPI padrão)";

    return ss.str();
}

wstring GetDiskInfo() {
    wstringstream ss;

    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFree;
    if (GetDiskFreeSpaceEx(L"C:\\", &freeBytesAvailable, &totalBytes, &totalFree)) {
        ss << L"Tamanho total: " << totalBytes.QuadPart / (1024 * 1024 * 1024) << L" GB\n";
    }

    DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
    if (GetDiskFreeSpace(L"C:\\", &sectorsPerCluster, &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters)) {
        ss << L"Bytes por Setor: " << bytesPerSector << L"\n";
        ss << L"Número de Setores: " << (DWORD64)totalNumberOfClusters * sectorsPerCluster;
    }

    return ss.str();
}

wstring GetMemoryType() {
    // Informação real do tipo de memória requer WMI ou driver específico, aqui é simulado
    return L"Tipo: DDR4 (estimado)";
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
    for (int i = 0; i < 10; ++i) ShowWindow(hLabels[i], SW_HIDE);
    ShowWindow(hProgressBar, SW_HIDE);
    ShowWindow(hBenchmarkButton, SW_HIDE);

    switch (tabIndex) {
    case 0: // Sistema
        ShowWindow(hLabels[0], SW_SHOW);
        break;
    case 1: // CPU
        ShowWindow(hLabels[1], SW_SHOW);
        break;
    case 2: // Disco
        ShowWindow(hLabels[2], SW_SHOW);
        break;
    case 3: // Memória
        for (int i = 3; i <= 6; ++i) ShowWindow(hLabels[i], SW_SHOW);
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
        tie.pszText = const_cast<LPWSTR>(L"Disco");   TabCtrl_InsertItem(hTabCtrl, 2, &tie);
        tie.pszText = const_cast<LPWSTR>(L"Memória"); TabCtrl_InsertItem(hTabCtrl, 3, &tie);

        hLabels[0] = CreateWindow(L"STATIC", GetOSInfo().c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, 30, 40, 480, 120, hwnd, NULL, NULL, NULL);
        hLabels[1] = CreateWindow(L"STATIC", GetCPUInfo().c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, 30, 40, 480, 120, hwnd, NULL, NULL, NULL);
        hLabels[2] = CreateWindow(L"STATIC", GetDiskInfo().c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, 30, 40, 480, 120, hwnd, NULL, NULL, NULL);

        hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 30, 90, 400, 30, hwnd, NULL, NULL, NULL);
        hLabels[3] = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 30, 130, 200, 20, hwnd, NULL, NULL, NULL);
        hLabels[4] = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 240, 130, 200, 20, hwnd, NULL, NULL, NULL);
        hLabels[5] = CreateWindow(L"STATIC", GetMemoryType().c_str(), WS_CHILD | WS_VISIBLE, 30, 160, 200, 20, hwnd, NULL, NULL, NULL);
        hLabels[6] = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 30, 190, 200, 20, hwnd, NULL, NULL, NULL);

        hBenchmarkButton = CreateWindow(L"BUTTON", L"Benchmark da RAM", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 30, 230, 200, 30, hwnd, (HMENU)ID_BENCHMARK_BUTTON, NULL, NULL);

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
        WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME),
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
