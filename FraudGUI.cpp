#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <vector>
#include <string>
#include <ctime>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_TREE_CHECK_CHANGED (WM_USER + 1)

// 색상 정의
const COLORREF COLOR_BG = RGB(45, 45, 48);
const COLORREF COLOR_TEXT = RGB(255, 255, 255);
const COLORREF COLOR_ACCENT = RGB(0, 150, 255);
const COLORREF COLOR_CONTROL_BG = RGB(60, 60, 65);

// 전역 변수
HINSTANCE hInst;
HWND hWndTree, hWndLog, hWndBtnRun, hWndBtnCustom, hWndProgress;
HIMAGELIST hImageList;
HFONT hFontMain, hFontTitle;
HBRUSH hBrushBG, hBrushControl;

struct TreeItemData {
    std::wstring fullPath;
    bool isDirectory;
    bool isLoaded;
};

struct CheckedItem {
    HTREEITEM hItem;
    std::wstring fullPath;
};

// 함수 선언
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitTreeView(HWND hWnd);
void AddDriveNodes();
void ExpandNode(HWND hWnd, HTREEITEM hItem);
void ProcessCheckedItems(HWND hWnd, time_t fixedTime = 0);
void GetCheckedFiles(HTREEITEM hItem, std::vector<CheckedItem>& items);
void SetCheckStateRecursive(HWND hWnd, HTREEITEM hItem, BOOL checked);
void WriteLog(const std::wstring& message);
time_t getFileTime(const std::wstring& filePath);
bool modifyFileTime(const std::wstring& filePath, time_t newTime);
time_t subtractOneYear(time_t originalTime);
std::wstring timeToWString(time_t timeValue);
std::wstring timeToWString(time_t timeValue);
LRESULT CALLBACK DatePickerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void ShowDatePickerAndRun(HWND hWndParent);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    
    // Common Controls 초기화 (확장 버전)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_DATE_CLASSES;
    InitCommonControlsEx(&icex);

    hBrushBG = CreateSolidBrush(COLOR_BG);
    hBrushControl = CreateSolidBrush(COLOR_CONTROL_BG);
    hFontMain = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    hFontTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    const wchar_t CLASS_NAME[] = L"FraudGUIPremium";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = hBrushBG;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // 날짜 선택기 클래스 등록
    wc.lpszClassName = L"DatePickerClass";
    wc.hbrBackground = hBrushBG;
    wc.lpfnWndProc = DatePickerProc;
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(0, CLASS_NAME, L"Fraud - File Date Modifier",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        NULL, NULL, hInstance, NULL);

    if (hWnd == NULL) return 0;
    ShowWindow(hWnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            HWND hTitle = CreateWindowW(L"STATIC", L"File Date Modifier", WS_VISIBLE | WS_CHILD,
                20, 15, 300, 35, hWnd, NULL, hInst, NULL);
            SendMessageW(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            hWndTree = CreateWindowExW(0, WC_TREEVIEWW, NULL,
                WS_VISIBLE | WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_CHECKBOXES,
                20, 60, 420, 500, hWnd, (HMENU)100, hInst, NULL);
            SendMessageW(hWndTree, WM_SETFONT, (WPARAM)hFontMain, TRUE);
            
            // 트리뷰 다크 모드 색상 설정
            TreeView_SetBkColor(hWndTree, COLOR_CONTROL_BG);
            TreeView_SetTextColor(hWndTree, COLOR_TEXT);
            TreeView_SetLineColor(hWndTree, COLOR_ACCENT);

            hWndProgress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
                20, 570, 420, 25, hWnd, NULL, hInst, NULL);
            SendMessageW(hWndProgress, PBM_SETBKCOLOR, 0, (LPARAM)COLOR_CONTROL_BG);
            SendMessageW(hWndProgress, PBM_SETBARCOLOR, 0, (LPARAM)COLOR_ACCENT);

            hWndBtnRun = CreateWindowW(L"BUTTON", L"Run (1Y Ago)", 
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                20, 605, 205, 45, hWnd, (HMENU)1, hInst, NULL);
            SendMessageW(hWndBtnRun, WM_SETFONT, (WPARAM)hFontMain, TRUE);

            hWndBtnCustom = CreateWindowW(L"BUTTON", L"Run (Pick Date)", 
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                235, 605, 205, 45, hWnd, (HMENU)2, hInst, NULL);
            SendMessageW(hWndBtnCustom, WM_SETFONT, (WPARAM)hFontMain, TRUE);

            hWndLog = CreateWindowExW(0, L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                460, 60, 400, 590, hWnd, (HMENU)101, hInst, NULL);
            SendMessageW(hWndLog, WM_SETFONT, (WPARAM)hFontMain, TRUE);

            InitTreeView(hWndTree);
            AddDriveNodes();
            WriteLog(L"System ready. Select files to modify.");
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_BG);
            return (LRESULT)hBrushBG;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_CONTROL_BG);
            return (LRESULT)hBrushControl;
        }
        case WM_NOTIFY: {
            LPNMHDR lpnmhdr = (LPNMHDR)lParam;
            if (lpnmhdr->code == TVN_ITEMEXPANDINGW) {
                LPNMTREEVIEWW lpnmtv = (LPNMTREEVIEWW)lParam;
                if (lpnmtv->action == TVE_EXPAND) {
                    ExpandNode(hWndTree, lpnmtv->itemNew.hItem);
                }
            } else if (lpnmhdr->code == TVN_DELETEITEMW) {
                LPNMTREEVIEWW lpnmtv = (LPNMTREEVIEWW)lParam;
                if (lpnmtv->itemOld.lParam) {
                    delete (TreeItemData*)lpnmtv->itemOld.lParam;
                }
            } else if (lpnmhdr->code == NM_CLICK && lpnmhdr->hwndFrom == hWndTree) {
                TVHITTESTINFO ht = {0};
                GetCursorPos(&ht.pt);
                ScreenToClient(hWndTree, &ht.pt);
                TreeView_HitTest(hWndTree, &ht);
                if (ht.flags & TVHT_ONITEMSTATEICON) {
                    PostMessage(hWnd, WM_TREE_CHECK_CHANGED, 0, (LPARAM)ht.hItem);
                }
            }
            break;
        }
        case WM_TREE_CHECK_CHANGED: {
            HTREEITEM hItem = (HTREEITEM)lParam;
            BOOL checked = TreeView_GetCheckState(hWndTree, hItem);
            SetCheckStateRecursive(hWndTree, hItem, checked);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1) ProcessCheckedItems(hWnd, 0); // 0 means 1 year ago
            else if (LOWORD(wParam) == 2) ShowDatePickerAndRun(hWnd);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

void InitTreeView(HWND hWnd) {
    hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
    SHFILEINFOW sfi;
    SHGetFileInfoW(L"C:\\", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    SHGetFileInfoW(L"test.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    TreeView_SetImageList(hWnd, hImageList, TVSIL_NORMAL);
}

void AddDriveNodes() {
    wchar_t drives[256];
    GetLogicalDriveStringsW(256, drives);
    wchar_t* pDrive = drives;
    while (*pDrive) {
        TVINSERTSTRUCTW tvis = {0};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
        tvis.item.pszText = pDrive;
        tvis.item.iImage = 0;
        tvis.item.iSelectedImage = 0;
        tvis.item.lParam = (LPARAM)new TreeItemData{pDrive, true, false};
        
        HTREEITEM hParent = TreeView_InsertItem(hWndTree, &tvis);
        
        // 자식이 있음을 명시적으로 설정
        TVITEMW item = {0};
        item.mask = TVIF_CHILDREN | TVIF_HANDLE;
        item.hItem = hParent;
        item.cChildren = 1;
        TreeView_SetItem(hWndTree, &item);
        
        pDrive += wcslen(pDrive) + 1;
    }
}

struct FileInfo {
    std::wstring name;
    bool isDir;
    std::wstring fullPath;
    std::wstring dateStr;
};

bool CompareFileInfo(const FileInfo& a, const FileInfo& b) {
    if (a.isDir != b.isDir) return a.isDir;
    return a.name < b.name;
}

void ExpandNode(HWND hWnd, HTREEITEM hItem) {
    TVITEMW item = {0};
    item.mask = TVIF_PARAM | TVIF_HANDLE;
    item.hItem = hItem;
    TreeView_GetItem(hWnd, &item);
    TreeItemData* pData = (TreeItemData*)item.lParam;
    if (!pData || pData->isLoaded) return;

    // 더미 자식 제거
    HTREEITEM hChild = TreeView_GetChild(hWnd, hItem);
    while (hChild) {
        HTREEITEM hNext = TreeView_GetNextSibling(hWnd, hChild);
        TreeView_DeleteItem(hWnd, hChild);
        hChild = hNext;
    }

    std::wstring searchPath = pData->fullPath;
    if (searchPath.back() != L'\\') searchPath += L"\\";
    
    std::vector<FileInfo> fileList;
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW((searchPath + L"*.*").c_str(), &ffd);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
            FileInfo info;
            info.name = ffd.cFileName;
            info.isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            info.fullPath = searchPath + ffd.cFileName;
            if (!info.isDir) {
                ULARGE_INTEGER ull;
                ull.LowPart = ffd.ftLastWriteTime.dwLowDateTime;
                ull.HighPart = ffd.ftLastWriteTime.dwHighDateTime;
                time_t t = (time_t)((ull.QuadPart - 116444736000000000LL) / 10000000);
                info.dateStr = L" [" + timeToWString(t) + L"]";
            }
            fileList.push_back(info);
        } while (FindNextFileW(hFind, &ffd) != 0);
        FindClose(hFind);
    }

    std::sort(fileList.begin(), fileList.end(), CompareFileInfo);
    for (const auto& info : fileList) {
        TVINSERTSTRUCTW tvis = {0};
        tvis.hParent = hItem;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
        std::wstring displayText = info.name + info.dateStr;
        tvis.item.pszText = (LPWSTR)displayText.c_str();
        tvis.item.iImage = info.isDir ? 0 : 1;
        tvis.item.iSelectedImage = info.isDir ? 0 : 1;
        tvis.item.lParam = (LPARAM)new TreeItemData{info.fullPath, info.isDir, false};
        
        HTREEITEM hNewItem = TreeView_InsertItem(hWnd, &tvis);
        if (TreeView_GetCheckState(hWnd, hItem)) {
            TreeView_SetCheckState(hWnd, hNewItem, TRUE);
        }
        if (info.isDir) {
            TVITEMW childItem = {0};
            childItem.mask = TVIF_CHILDREN | TVIF_HANDLE;
            childItem.hItem = hNewItem;
            childItem.cChildren = 1;
            TreeView_SetItem(hWnd, &childItem);
        }
    }
    pData->isLoaded = true;
}

void GetCheckedFiles(HTREEITEM hItem, std::vector<CheckedItem>& items) {
    while (hItem) {
        if (TreeView_GetCheckState(hWndTree, hItem)) {
            TVITEMW item = {0};
            item.mask = TVIF_PARAM | TVIF_HANDLE;
            item.hItem = hItem;
            TreeView_GetItem(hWndTree, &item);
            TreeItemData* pData = (TreeItemData*)item.lParam;
            if (pData && !pData->isDirectory) items.push_back({hItem, pData->fullPath});
        }
        HTREEITEM hChild = TreeView_GetChild(hWndTree, hItem);
        if (hChild) GetCheckedFiles(hChild, items);
        hItem = TreeView_GetNextSibling(hWndTree, hItem);
    }
}

void ProcessCheckedItems(HWND hWnd, time_t fixedTime) {
    std::vector<CheckedItem> items;
    GetCheckedFiles(TreeView_GetRoot(hWndTree), items);
    if (items.empty()) {
        MessageBoxW(hWnd, L"Please check files first.", L"Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring confirmMsg = (fixedTime == 0) ? L"Modify selected files to 1 year ago?" : L"Modify selected files to the chosen date?";
    if (MessageBoxW(hWnd, confirmMsg.c_str(), L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    int successCount = 0;
    int total = (int)items.size();
    SendMessage(hWndProgress, PBM_SETRANGE32, 0, total);
    SendMessage(hWndProgress, PBM_SETPOS, 0, 0);

    for (int i = 0; i < total; ++i) {
        const auto& item = items[i];
        time_t currentTime = getFileTime(item.fullPath);
        if (currentTime != 0) {
            time_t newTime = (fixedTime == 0) ? subtractOneYear(currentTime) : fixedTime;
            if (modifyFileTime(item.fullPath, newTime)) {
                WriteLog(L"[Success] " + item.fullPath);
                size_t lastSlash = item.fullPath.find_last_of(L"\\");
                std::wstring fileName = (lastSlash == std::wstring::npos) ? item.fullPath : item.fullPath.substr(lastSlash + 1);
                std::wstring newText = fileName + L" [" + timeToWString(newTime) + L"]";
                TVITEMW tvItem = {0};
                tvItem.mask = TVIF_TEXT | TVIF_HANDLE;
                tvItem.hItem = item.hItem;
                tvItem.pszText = (LPWSTR)newText.c_str();
                TreeView_SetItem(hWndTree, &tvItem);
                TreeView_SetCheckState(hWndTree, item.hItem, FALSE);
                successCount++;
            }
        }
        SendMessage(hWndProgress, PBM_SETPOS, i + 1, 0);
    }
    UpdateWindow(hWndTree);
    SetFocus(hWndTree);

    std::wstringstream res;
    res << L"Complete. Success: " << successCount << L"/" << items.size();
    MessageBoxW(hWnd, res.str().c_str(), L"Done", MB_OK | MB_ICONINFORMATION);
}

void WriteLog(const std::wstring& message) {
    int len = GetWindowTextLengthW(hWndLog);
    SendMessageW(hWndLog, EM_SETSEL, len, len);
    SendMessageW(hWndLog, EM_REPLACESEL, 0, (LPARAM)(message + L"\r\n").c_str());
}

time_t getFileTime(const std::wstring& filePath) {
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fileData)) {
        ULARGE_INTEGER ull;
        ull.LowPart = fileData.ftLastWriteTime.dwLowDateTime;
        ull.HighPart = fileData.ftLastWriteTime.dwHighDateTime;
        return (time_t)((ull.QuadPart - 116444736000000000LL) / 10000000);
    }
    return 0;
}

bool modifyFileTime(const std::wstring& filePath, time_t newTime) {
    HANDLE hFile = CreateFileW(filePath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    LONGLONG ll = Int32x32To64(newTime, 10000000) + 116444736000000000LL;
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)ll; ft.dwHighDateTime = (DWORD)(ll >> 32);
    BOOL result = SetFileTime(hFile, &ft, &ft, &ft);
    CloseHandle(hFile);
    return result != 0;
}

time_t subtractOneYear(time_t originalTime) {
    struct tm* tm = localtime(&originalTime);
    tm->tm_year -= 1;
    if (tm->tm_mon == 1 && tm->tm_mday == 29) {
        int targetYear = tm->tm_year + 1900;
        bool isLeapYear = (targetYear % 4 == 0 && targetYear % 100 != 0) || (targetYear % 400 == 0);
        if (!isLeapYear) tm->tm_mday = 28;
    }
    return mktime(tm);
}

std::wstring timeToWString(time_t timeValue) {
    wchar_t buffer[100];
    struct tm* timeinfo = localtime(&timeValue);
    wcsftime(buffer, 100, L"%Y-%m-%d", timeinfo);
    return std::wstring(buffer);
}

void SetCheckStateRecursive(HWND hWnd, HTREEITEM hItem, BOOL checked) {
    HTREEITEM hChild = TreeView_GetChild(hWnd, hItem);
    while (hChild) {
        TreeView_SetCheckState(hWnd, hChild, checked);
        SetCheckStateRecursive(hWnd, hChild, checked);
        hChild = TreeView_GetNextSibling(hWnd, hChild);
    }
}

LRESULT CALLBACK DatePickerProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static time_t* pSelectedTime = NULL;

    switch (message) {
        case WM_CREATE: {
            CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
            pSelectedTime = (time_t*)pcs->lpCreateParams;

            HWND hDP = CreateWindowExW(0, DATETIMEPICK_CLASSW, NULL,
                WS_BORDER | WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
                20, 20, 200, 30, hDlg, (HMENU)1000, hInst, NULL);
            SendMessageW(hDP, WM_SETFONT, (WPARAM)hFontMain, TRUE);
            
            // 날짜 선택기 색상 설정 (일부 시스템에서 지원)
            SendMessageW(hDP, DTM_SETMCCOLOR, MCSC_BACKGROUND, (LPARAM)COLOR_CONTROL_BG);
            SendMessageW(hDP, DTM_SETMCCOLOR, MCSC_TEXT, (LPARAM)COLOR_TEXT);
            SendMessageW(hDP, DTM_SETMCCOLOR, MCSC_TITLEBK, (LPARAM)COLOR_ACCENT);
            SendMessageW(hDP, DTM_SETMCCOLOR, MCSC_TITLETEXT, (LPARAM)COLOR_TEXT);

            HWND hOk = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 70, 90, 35, hDlg, (HMENU)IDOK, hInst, NULL);
            SendMessageW(hOk, WM_SETFONT, (WPARAM)hFontMain, TRUE);

            HWND hCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                130, 70, 90, 35, hDlg, (HMENU)IDCANCEL, hInst, NULL);
            SendMessageW(hCancel, WM_SETFONT, (WPARAM)hFontMain, TRUE);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_BG);
            return (LRESULT)hBrushBG;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                HWND hDP = GetDlgItem(hDlg, 1000);
                SYSTEMTIME st;
                SendMessageW(hDP, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);

                struct tm t = { 0 };
                t.tm_year = st.wYear - 1900;
                t.tm_mon = st.wMonth - 1;
                t.tm_mday = st.wDay;
                t.tm_hour = 12;
                t.tm_isdst = -1;

                if (pSelectedTime) *pSelectedTime = mktime(&t);
                SendMessage(hDlg, WM_CLOSE, 0, 0);
            }
            else if (LOWORD(wParam) == IDCANCEL) {
                if (pSelectedTime) *pSelectedTime = 0;
                SendMessage(hDlg, WM_CLOSE, 0, 0);
            }
            break;
        case WM_CLOSE:
            EnableWindow(GetWindow(hDlg, GW_OWNER), TRUE);
            SetFocus(GetWindow(hDlg, GW_OWNER));
            DestroyWindow(hDlg);
            break;
    }
    return DefWindowProcW(hDlg, message, wParam, lParam);
}

void ShowDatePickerAndRun(HWND hWndParent) {
    static time_t selectedTime = 0;
    selectedTime = 0;

    int w = 260, h = 160;
    RECT rcParent;
    GetWindowRect(hWndParent, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, L"DatePickerClass", L"Select Date",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h, hWndParent, NULL, hInst, &selectedTime);

    if (hPopup) {
        EnableWindow(hWndParent, FALSE);
        
        MSG msg;
        while (IsWindow(hPopup) && GetMessage(&msg, NULL, 0, 0)) {
            if (!IsDialogMessage(hPopup, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (selectedTime != 0) {
            ProcessCheckedItems(hWndParent, selectedTime);
        }
    }
}
