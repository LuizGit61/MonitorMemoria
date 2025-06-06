#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <Wbemidl.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "wbemuuid.lib")

using namespace std;

#define ID_TABCTRL 1001
#define ID_RAM_PROGRESS 1002
#define ID_BENCHMARK_BTN 1003

HWND hTab, hRamProgress;
HWND hSystemTab, hCpuTab, hDiskTab, hMemoryTab;
HWND hBenchmarkBtn;
RECT clientRect;

vector<pair<string, string>> GetWMIInfo(const wstring& wmiClass, const vector<wstring>& properties) {
    vector<pair<string, string>> results;

    CoInitializeEx(0, COINIT_MULTITHREADED);
    CoInitializeSecurity(NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);

    IWbemLocator* pLocator = NULL;
    IWbemServices* pServices = NULL;

    if (CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLocator) == S_OK) {
        if (pLocator->ConnectServer(
            BSTR(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pServices) == S_OK) {
            CoSetProxyBlanket(pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                NULL, EOAC_NONE);

            IEnumWbemClassObject* pEnumerator = NULL;
            if (pServices->ExecQuery(
                BSTR(L"WQL"),
                BSTR((L"SELECT * FROM " + wmiClass).c_str()),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                NULL, &pEnumerator) == S_OK) {

                IWbemClassObject* pClassObject = NULL;
                ULONG uReturn = 0;
                if (pEnumerator->Next(WBEM_INFINITE, 1, &pClassObject, &uReturn) == S_OK) {
                    for (auto& prop : properties) {
                        VARIANT vtProp;
                        VariantInit(&vtProp);
                        if (pClassObject->Get(prop.c_str(), 0, &vtProp, 0, 0) == S_OK) {
                            wstring val = vtProp.bstrVal ? vtProp.bstrVal : L"";
                            results.emplace_back(string(prop.begin(), prop.end()), string(val.begin(), val.end()));
                        }
                        VariantClear(&vtProp);
                    }
                    pClassObject->Release();
                }
                pEnumerator->Release();
            }
            pServices->Release();
        }
        pLocator->Release();
    }
    CoUninitialize();
    return results;
}

void AddLabel(HWND hWnd, int x, int y, const wstring& text) {
    CreateWindowW(L"STATIC", text.c_str(), WS_VISIBLE | WS_CHILD,
        x, y, 230, 20, hWnd, NULL, NULL, NULL);
}

void CreateSystemTab(HWND hWndParent) {
    setlocale(LC_ALL, "Portuguese");
    hSystemTab = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE,
        10, 40, 460, 260, hWndParent, NULL, NULL, NULL);

    auto info = GetWMIInfo(L"Win32_OperatingSystem", {
        L"Caption", L"WindowsDirectory", L"SystemDirectory", L"SerialNumber" });
    auto mobo = GetWMIInfo(L"Win32_BaseBoard", { L"Product" });

    int y = 10;
    for (auto& pair : info) AddLabel(hSystemTab, 10, y += 25, wstring(pair.first.begin(), pair.first.end()) + L": " + wstring(pair.second.begin(), pair.second.end()));
    AddLabel(hSystemTab, 10, y += 25, L"Placa-mãe: " + wstring(mobo[0].second.begin(), mobo[0].second.end()));
}

void CreateCpuTab(HWND hWndParent) {
    setlocale(LC_ALL, "Portuguese");
    hCpuTab = CreateWindowW(L"STATIC", NULL, WS_CHILD,
        10, 40, 460, 260, hWndParent, NULL, NULL, NULL);

    auto info = GetWMIInfo(L"Win32_Processor", {
        L"Name", L"NumberOfCores", L"Manufacturer", L"CurrentClockSpeed", L"CurrentVoltage" });

    int y = 10;
    for (auto& pair : info) AddLabel(hCpuTab, 10, y += 25, wstring(pair.first.begin(), pair.first.end()) + L": " + wstring(pair.second.begin(), pair.second.end()));
}

void CreateDiskTab(HWND hWndParent) {
    setlocale(LC_ALL, "Portuguese");
    hDiskTab = CreateWindowW(L"STATIC", NULL, WS_CHILD,
        10, 40, 460, 260, hWndParent, NULL, NULL, NULL);

    auto info = GetWMIInfo(L"Win32_DiskDrive", {
        L"Model", L"Size", L"TotalCylinders", L"BytesPerSector" });

    int y = 10;
    for (auto& pair : info) AddLabel(hDiskTab, 10, y += 25, wstring(pair.first.begin(), pair.first.end()) + L": " + wstring(pair.second.begin(), pair.second.end()));
}

void CreateMemoryTab(HWND hWndParent) {
    setlocale(LC_ALL, "Portuguese");
    hMemoryTab = CreateWindowW(L"STATIC", NULL, WS_CHILD,
        10, 40, 460, 260, hWndParent, NULL, NULL, NULL);

    MEMORYSTATUSEX memStatus = { sizeof(memStatus) };
    GlobalMemoryStatusEx(&memStatus);
    DWORD usage = (DWORD)((memStatus.ullTotalPhys - memStatus.ullAvailPhys) * 100 / memStatus.ullTotalPhys);

    AddLabel(hMemoryTab, 10, 20, L"Uso de RAM:");
    hRamProgress = CreateWindowExW(0, PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE,
        110, 20, 250, 20,
        hMemoryTab, (HMENU)ID_RAM_PROGRESS, NULL, NULL);
    SendMessage(hRamProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(hRamProgress, PBM_SETPOS, usage, 0);

    AddLabel(hMemoryTab, 10, 60, L"Total RAM: " + to_wstring(memStatus.ullTotalPhys / (1024 * 1024)) + L" MB");
    AddLabel(hMemoryTab, 10, 85, L"RAM em uso: " + to_wstring((memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024)) + L" MB");

    auto memInfo = GetWMIInfo(L"Win32_PhysicalMemory", {
        L"Manufacturer", L"DeviceLocator", L"Speed", L"MemoryType", L"Capacity" });

    int y = 120;
    for (auto& pair : memInfo)
        AddLabel(hMemoryTab, 10, y += 25, wstring(pair.first.begin(), pair.first.end()) + L": " + wstring(pair.second.begin(), pair.second.end()));

    hBenchmarkBtn = CreateWindowW(L"BUTTON", L"Benchmark",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        350, 20, 90, 25,
        hMemoryTab, (HMENU)ID_BENCHMARK_BTN, NULL, NULL);
}

void ShowTab(int index) {
    ShowWindow(hSystemTab, index == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(hCpuTab, index == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(hDiskTab, index == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hMemoryTab, index == 3 ? SW_SHOW : SW_HIDE);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    setlocale(LC_ALL, "Portuguese");
    switch (uMsg) {
    case WM_CREATE: {
        GetClientRect(hWnd, &clientRect);
        hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            10, 10, clientRect.right - 20, clientRect.bottom - 20,
            hWnd, (HMENU)ID_TABCTRL, NULL, NULL);

        TCITEM tie = { TCIF_TEXT };
        tie.pszText = (LPWSTR)L"Sistema"; TabCtrl_InsertItem(hTab, 0, &tie);
        tie.pszText = (LPWSTR)L"CPU";     TabCtrl_InsertItem(hTab, 1, &tie);
        tie.pszText = (LPWSTR)L"Disco";   TabCtrl_InsertItem(hTab, 2, &tie);
        tie.pszText = (LPWSTR)L"Memória"; TabCtrl_InsertItem(hTab, 3, &tie);

        CreateSystemTab(hWnd);
        CreateCpuTab(hWnd);
        CreateDiskTab(hWnd);
        CreateMemoryTab(hWnd);

        ShowTab(0);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BENCHMARK_BTN) {
            MessageBoxW(hWnd, L"Benchmark iniciado (simulado)...", L"Benchmark", MB_OK);
        }
        break;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->idFrom == ID_TABCTRL && ((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(hTab);
            ShowTab(sel);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    InitCommonControls();

    const wchar_t CLASS_NAME[] = L"MonitorMemoriaWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hWnd = CreateWindowExW(0, CLASS_NAME, L"Monitor de Hardware", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 380,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
