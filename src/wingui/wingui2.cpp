/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ListBoxCtrl.h"
#include "wingui/wingui2.h"

#include "utils/Log.h"

Kind kindWnd = "wnd";

// this is experimantal win32 gui wrappers based on
// https://github.com/erengy/windows

namespace wg {

// window_map.h / window_map.cpp
struct WindowToHwnd {
    Wnd* window = nullptr;
    HWND hwnd = nullptr;
};

Vec<WindowToHwnd> gWindowToHwndMap;

static Wnd* WindowMapGetWindow(HWND hwnd) {
    for (auto& el : gWindowToHwndMap) {
        if (el.hwnd == hwnd) {
            return el.window;
        }
    }
    return nullptr;
}

static void WindowMapAdd(HWND hwnd, Wnd* w) {
    if (!hwnd || (WindowMapGetWindow(hwnd) != nullptr)) {
        return;
    }
    WindowToHwnd el = {w, hwnd};
    gWindowToHwndMap.Append(el);
}

/*
static bool WindowMapRemove(HWND hwnd) {
    int n = gWindowToHwndMap.isize();
    for (int i = 0; i < n; i++) {
        auto&& el = gWindowToHwndMap[i];
        if (el.hwnd == hwnd) {
            gWindowToHwndMap.RemoveAtFast(i);
            return true;
        }
    }
    return false;
}
*/

static bool WindowMapRemove(Wnd* w) {
    int n = gWindowToHwndMap.isize();
    for (int i = 0; i < n; i++) {
        auto&& el = gWindowToHwndMap[i];
        if (el.window == w) {
            gWindowToHwndMap.RemoveAtFast(i);
            return true;
        }
    }
    return false;
}

//- Taskbar.cpp

const DWORD WM_TASKBARCALLBACK = WM_APP + 0x15;
const DWORD WM_TASKBARCREATED = ::RegisterWindowMessage(L"TaskbarCreated");
const DWORD WM_TASKBARBUTTONCREATED = ::RegisterWindowMessage(L"TaskbarButtonCreated");

//- Window.h / Window.cpp

Wnd* gWindowBeingCreated = nullptr;
const WCHAR* kDefaultClassName = L"SumatraWgDefaultWinClass";

LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Wnd* window = WindowMapGetWindow(hwnd);

    if (!window) {
        // I think it's meant to ensure we associate Window with HWND
        // as early as possible given than CreateWindow
        window = gWindowBeingCreated;
        if (window) {
            window->hwnd = hwnd;
            WindowMapAdd(hwnd, window);
        }
    }

    if (window) {
        return window->WndProc(hwnd, msg, wparam, lparam);
    } else {
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

Wnd::Wnd() {
    instance = GetModuleHandleW(nullptr);
    gWindowBeingCreated = nullptr;
    kind = kindWnd;

    // Create default window class
    WNDCLASSEX wc = {};
    if (!::GetClassInfoExW(instance, kDefaultClassName, &wc)) {
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = StaticWindowProc;
        wc.hInstance = instance;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
        wc.lpszClassName = kDefaultClassName;
        ::RegisterClassEx(&wc);
    }
}

Wnd::Wnd(HWND hwnd) {
    instance = GetModuleHandleW(nullptr);
    gWindowBeingCreated = nullptr;
    kind = kindWnd;

    this->hwnd = hwnd;
}

Wnd::~Wnd() {
    Destroy();
}

Kind Wnd::GetKind() {
    return kind;
}

void Wnd::SetText(const WCHAR* s) {
    // can be set before we create the window
    if (hwnd) {
        HwndSetText(hwnd, s);
        HwndInvalidate(hwnd);
    }
}

void Wnd::SetVisibility(Visibility newVisibility) {
    // TODO: make it work before Create()?
    CrashIf(!hwnd);
    visibility = newVisibility;
    bool isVisible = IsVisible();
    // TODO: a different way to determine if is top level vs. child window?
    if (GetParent(hwnd) == nullptr) {
        ::ShowWindow(hwnd, isVisible ? SW_SHOW : SW_HIDE);
    } else {
        BOOL bIsVisible = toBOOL(isVisible);
        SetWindowStyle(hwnd, WS_VISIBLE, bIsVisible);
    }
}

Visibility Wnd::GetVisibility() {
    return visibility;
#if 0
    if (GetParent(hwnd) == nullptr) {
        // TODO: what to do for top-level window?
        CrashMe();
        return true;
    }
    bool isVisible = IsWindowStyleSet(hwnd, WS_VISIBLE);
    return isVisible;
#endif
}

void Wnd::SetIsVisible(bool isVisible) {
    SetVisibility(isVisible ? Visibility::Visible : Visibility::Collapse);
}

bool Wnd::IsVisible() const {
    return visibility == Visibility::Visible;
}

void Wnd::Destroy() {
    HwndDestroyWindowSafe(&hwnd);
    /*if (font && parent) {
        DeleteFontSafe(&font);
    }*/
    // TODO: move to Frame subclass
    // DestroyIconSafe(&icon_large);
    // DestroyIconSafe(&icon_small);

    /*
    if (prevWindowProc) {
        UnSubclass();
    }
    */

    Cleanup();
}

LRESULT Wnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

// This function is called when a window is attached to Wnd.
// Override it to automatically perform tasks when the window is attached.
// Note:  Window controls are attached.
void Wnd::OnAttach() {
}

// Override this to handle WM_COMMAND messages
bool Wnd::OnCommand(WPARAM wparam, LPARAM lparam) {
    //  UINT id = LOWORD(wparam);
    //  switch (id)
    //  {
    //  case IDM_FILE_NEW:
    //      OnFileNew();
    //      return true;   // return TRUE for handled commands
    //  }

    // return false for unhandled commands

    return false;
}

// Called during window creation. Override this functions to perform tasks
// such as creating child windows.
int Wnd::OnCreate(CREATESTRUCT*) {
    // This function is called when a WM_CREATE message is received
    // Override it to automatically perform tasks during window creation.
    // Return 0 to continue creating the window.

    // Note: Window controls don't call OnCreate. They are sublcassed (attached)
    //  after their window is created.

    /*
    LOGFONT logfont;
    ::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(logfont), &logfont);
    font = ::CreateFontIndirect(&logfont);
    ::SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    */

    return 0;
}

// This function is called when a window is destroyed.
// Override it to do additional tasks, such as ending the application
//  with PostQuitMessage.
void Wnd::OnDestroy() {
}

// Called when the background of the window's client area needs to be erased.
// Override this function in your derived class to perform drawing tasks.
// Return Value: Return FALSE to also permit default erasure of the background
//               Return TRUE to prevent default erasure of the background
bool Wnd::OnEraseBkgnd(HDC) {
    return false;
}

// Called in response to WM_CLOSE, before the window is destroyed.
// Override this function to suppress destroying the window.
// WM_CLOSE is sent by SendMessage(WM_CLOSE, 0, 0) or by clicking X
//  in the top right corner.
// Child windows don't receive WM_CLOSE unless they are closed using
//  the Close function.
void Wnd::OnClose() {
    Destroy();
}

void Wnd::OnContextMenu(HWND hwnd, Point pt) {
}

void Wnd::OnDropFiles(HDROP drop_info) {
}

void Wnd::OnGetMinMaxInfo(MINMAXINFO* mmi) {
}

LRESULT Wnd::OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam) {
    return -1;
}

void Wnd::OnMove(LPPOINTS pts) {
}

// Processes notification (WM_NOTIFY) messages from a child window.
LRESULT Wnd::OnNotify(int controlId, NMHDR* nmh) {
    // You can use either OnNotifyReflect or OnNotify to handle notifications
    // Override OnNotifyReflect to handle notifications in the CWnd class that
    //   generated the notification.   OR
    // Override OnNotify to handle notifications in the PARENT of the CWnd class
    //   that generated the notification.

    // Your overriding function should look like this ...

    // LPNMHDR pHeader = reinterpret_cast<LPNMHDR>(lparam);
    // switch (pHeader->code)
    // {
    //      Handle your notifications from the CHILD window here
    //      Return the value recommended by the Windows API documentation.
    //      For many notifications, the return value doesn't matter, but for some it does.
    // }

    // return 0 for unhandled notifications
    // The framework will call SetWindowLongPtr(DWLP_MSGRESULT, result) for dialogs.
    return 0;
}

// Processes the notification (WM_NOTIFY) messages in the child window that originated them.
LRESULT Wnd::OnNotifyReflect(WPARAM, LPARAM) {
    // Override OnNotifyReflect to handle notifications in the CWnd class that
    //   generated the notification.

    // Your overriding function should look like this ...

    // LPNMHDR pHeader = reinterpret_cast<LPNMHDR>(lparam);
    // switch (pHeader->code)
    // {
    //      Handle your notifications from this window here
    //      Return the value recommended by the Windows API documentation.
    // }

    // Return 0 for unhandled notifications.
    // The framework will call SetWindowLongPtr(DWLP_MSGRESULT, result) for dialogs.
    return 0;
}

void Wnd::OnPaint(HDC hdc, PAINTSTRUCT* ps) {
}

void Wnd::OnSize(UINT msg, UINT type, SIZE size) {
}

void Wnd::OnTaskbarCallback(UINT msg, LPARAM lparam) {
}

void Wnd::OnTimer(UINT_PTR event_id) {
}

void Wnd::OnWindowPosChanging(WINDOWPOS* window_pos) {
}

// This function processes those special messages sent by some older controls,
// and reflects them back to the originating CWnd object.
// Override this function in your derived class to handle these special messages:
// WM_COMMAND, WM_CTLCOLORBTN, WM_CTLCOLOREDIT, WM_CTLCOLORDLG, WM_CTLCOLORLISTBOX,
// WM_CTLCOLORSCROLLBAR, WM_CTLCOLORSTATIC, WM_CHARTOITEM,  WM_VKEYTOITEM,
// WM_HSCROLL, WM_VSCROLL, WM_DRAWITEM, WM_MEASUREITEM, WM_DELETEITEM,
// WM_COMPAREITEM, WM_PARENTNOTIFY.
LRESULT Wnd::OnMessageReflect(UINT, WPARAM, LPARAM) {
    // This function processes those special messages (see above) sent
    // by some older controls, and reflects them back to the originating CWnd object.
    // Override this function in your derived class to handle these special messages.

    // Your overriding function should look like this ...

    // switch (msg)
    // {
    //      Handle your reflected messages here
    // }

    // return 0 for unhandled messages
    return 0;
}

Size Wnd::GetIdealSize() {
    return {};
}

Size Wnd::Layout(const Constraints bc) {
    dbglayoutf("WindowBase::Layout() %s ", GetKind());
    LogConstraints(bc, "\n");

    auto hinset = insets.left + insets.right;
    auto vinset = insets.top + insets.bottom;
    auto innerConstraints = bc.Inset(hinset, vinset);

    int dx = MinIntrinsicWidth(0);
    int dy = MinIntrinsicHeight(0);
    childSize = innerConstraints.Constrain(Size{dx, dy});
    auto res = Size{
        childSize.dx + hinset,
        childSize.dy + vinset,
    };
    return res;
}

int Wnd::MinIntrinsicHeight(int) {
#if 0
    auto vinset = insets.top + insets.bottom;
    Size s = GetIdealSize();
    return s.dy + vinset;
#else
    Size s = GetIdealSize();
    return s.dy;
#endif
}

int Wnd::MinIntrinsicWidth(int) {
#if 0
    auto hinset = insets.left + insets.right;
    Size s = GetIdealSize();
    return s.dx + hinset;
#else
    Size s = GetIdealSize();
    return s.dx;
#endif
}

void Wnd::SetPos(RECT* r) {
    ::MoveWindow(hwnd, r);
}

void Wnd::SetBounds(Rect bounds) {
    dbglayoutf("WindowBaseLayout:SetBounds() %s %d,%d - %d, %d\n", GetKind(), bounds.x, bounds.y, bounds.dx, bounds.dy);

    lastBounds = bounds;

    bounds.x += insets.left;
    bounds.y += insets.top;
    bounds.dx -= (insets.right + insets.left);
    bounds.dy -= (insets.bottom + insets.top);

    auto r = RectToRECT(bounds);
    ::MoveWindow(hwnd, &r);
    // TODO: optimize if doesn't change position
    ::InvalidateRect(hwnd, nullptr, TRUE);
}

// A function used internally to call OnMessageReflect. Don't call or override this function.
LRESULT Wnd::MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    HWND wnd = 0;
    switch (msg) {
        case WM_COMMAND:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
            wnd = reinterpret_cast<HWND>(lparam);
            break;

        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_DELETEITEM:
        case WM_COMPAREITEM:
            wnd = GetDlgItem(hwnd, static_cast<int>(wparam));
            break;

        case WM_PARENTNOTIFY:
            switch (LOWORD(wparam)) {
                case WM_CREATE:
                case WM_DESTROY:
                    wnd = reinterpret_cast<HWND>(lparam);
                    break;
            }
    }

    Wnd* pWnd = WindowMapGetWindow(wnd);

    if (pWnd != NULL)
        return pWnd->OnMessageReflect(msg, wparam, lparam);

    return 0;
}

// for interop with windows not wrapped in Wnd, run this at the beginning of message loop
LRESULT TryReflectNotify(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // hwnd is a parent of control sending WM_NOTIFY message
    LRESULT result = 0;
    if (msg != WM_NOTIFY) {
        return result;
    }
    // Do notification reflection if message came from a child window.
    // Restricting OnNotifyReflect to child windows avoids double handling.
    NMHDR* hdr = reinterpret_cast<LPNMHDR>(lparam);
    HWND from = hdr->hwndFrom;
    Wnd* wndFrom = WindowMapGetWindow(from);
    if (wndFrom) {
        if (hwnd == GetParent(wndFrom->hwnd)) {
            result = wndFrom->OnNotifyReflect(wparam, lparam);
        }
    }
    return result;
}

LRESULT Wnd::WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    switch (msg) {
        case WM_CLOSE: {
            OnClose();
            return 0;
        }

        case WM_COMMAND: {
            // Reflect this message if it's from a control.
            Wnd* pWnd = WindowMapGetWindow(reinterpret_cast<HWND>(lparam));
            if (pWnd != NULL)
                result = pWnd->OnCommand(wparam, lparam);

            // Handle user commands.
            if (0 == result)
                result = OnCommand(wparam, lparam);

            if (0 != result)
                return 0;
        } break; // Note: Some MDI commands require default processing.

        case WM_CREATE: {
            OnCreate((CREATESTRUCT*)lparam);
            break;
        }

        case WM_DESTROY: {
            OnDestroy();
            break; // Note: Some controls require default processing.
        }

        case WM_NOTIFY: {
            // Do notification reflection if message came from a child window.
            // Restricting OnNotifyReflect to child windows avoids double handling.
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lparam);
            HWND from = hdr->hwndFrom;
            Wnd* wndFrom = WindowMapGetWindow(from);

            if (wndFrom != nullptr) {
                if (::GetParent(from) == this->hwnd) {
                    result = wndFrom->OnNotifyReflect(wparam, lparam);
                }
            }

            // Handle user notifications
            if (result == 0)
                result = OnNotify((int)wparam, (NMHDR*)lparam);
            if (result != 0)
                return result;
            break;
        }

        case WM_PAINT: {
            if (!prevWindowProc) {
                if (::GetUpdateRect(hwnd, nullptr, FALSE)) {
                    PAINTSTRUCT ps;
                    HDC hdc = ::BeginPaint(hwnd, &ps);
                    OnPaint(hdc, &ps);
                    ::EndPaint(hwnd, &ps);
                } else {
                    HDC hdc = ::GetDC(hwnd);
                    OnPaint(hdc, nullptr);
                    ::ReleaseDC(hwnd, hdc);
                }
            }
            return 0;
        }

        case WM_ERASEBKGND: {
            HDC dc = (HDC)(wparam);
            BOOL preventErasure;

            preventErasure = OnEraseBkgnd(dc);
            if (preventErasure)
                return TRUE;
        } break;

        // A set of messages to be reflected back to the control that generated them.
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
        case WM_DRAWITEM:
        case WM_MEASUREITEM:
        case WM_DELETEITEM:
        case WM_COMPAREITEM:
        case WM_CHARTOITEM:
        case WM_VKEYTOITEM:
        case WM_HSCROLL:
        case WM_VSCROLL:
        case WM_PARENTNOTIFY: {
            result = MessageReflect(msg, wparam, lparam);
            if (result != 0)
                return result; // Message processed so return.
        } break;               // Do default processing when message not already processed.

        case WM_DROPFILES: {
            OnDropFiles(reinterpret_cast<HDROP>(wparam));
            break;
        }

        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE: {
            SIZE size = {0};
            OnSize(msg, 0, size);
            break;
        }
        case WM_GETMINMAXINFO: {
            OnGetMinMaxInfo(reinterpret_cast<MINMAXINFO*>(lparam));
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MOUSEACTIVATE:
        case WM_MOUSEHOVER:
        case WM_MOUSEHWHEEL:
        case WM_MOUSELEAVE:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL: {
            LRESULT lResult = OnMouseEvent(msg, wparam, lparam);
            if (lResult != -1)
                return lResult;
            break;
        }
        case WM_MOVE: {
            POINTS pts = MAKEPOINTS(lparam);
            OnMove(&pts);
            break;
        }

        case WM_CONTEXTMENU: {
            Point pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            OnContextMenu(reinterpret_cast<HWND>(wparam), pt);
            break;
        }

        case WM_SIZE: {
            SIZE size = {LOWORD(lparam), HIWORD(lparam)};
            OnSize(msg, static_cast<UINT>(wparam), size);
            break;
        }
        case WM_TIMER: {
            OnTimer(static_cast<UINT>(wparam));
            break;
        }
        case WM_WINDOWPOSCHANGING: {
            OnWindowPosChanging(reinterpret_cast<LPWINDOWPOS>(lparam));
            break;
        }

        default: {
            if (msg == WM_TASKBARCREATED || msg == WM_TASKBARBUTTONCREATED || msg == WM_TASKBARCALLBACK) {
                OnTaskbarCallback(msg, lparam);
                return 0;
            }
            break;
        }
    }

    // Now hand all messages to the default procedure.
    return FinalWindowProc(msg, wparam, lparam);
}

LRESULT Wnd::FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (prevWindowProc) {
        return ::CallWindowProc(prevWindowProc, hwnd, msg, wparam, lparam);
    } else {
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

// Called by CWnd::Create to set some window creation parameters.
void Wnd::PreCreate(CREATESTRUCT& cs) {
    // Override this function to set the CREATESTRUCT values prior to window creation.
    // Here we set the initial values for the following:
    //  window styles (WS_VISABLE, WS_CHILD, WS_WS_MAXIMIZEBOX etc.)
    //  window extended styles
    //  window position
    //  window menu
    //  window class name
    //  window name (caption)
}

static void WndRegisterClass(WNDCLASS& wc) {
    WNDCLASS wcTest = {};
    if (::GetClassInfo(GetInstance(), wc.lpszClassName, &wcTest)) {
        wc = wcTest;
        return;
    }
    wc.hInstance = GetInstance();
    wc.lpfnWndProc = StaticWindowProc;
    bool ok = ::RegisterClass(&wc);
    CrashIf(!ok);
}

/*
void Wnd::PreRegisterClass(WNDCLASS& wc) {
    window_class.style = wc.style;
    window_class.lpfnWndProc = StaticWindowProc;
    window_class.cbClsExtra = wc.cbClsExtra;
    window_class.cbWndExtra = wc.cbWndExtra;
    window_class.hInstance = instance;
    window_class.hIcon = wc.hIcon;
    window_class.hCursor = wc.hCursor;
    window_class.hbrBackground = wc.hbrBackground;
    window_class.lpszMenuName = wc.lpszMenuName;
    window_class.lpszClassName = wc.lpszClassName;
}
*/

bool Wnd::PreTranslateMessage(MSG& msg) {
    return false;
}

bool Wnd::RegisterClass(WNDCLASSEX& wc) const {
    WNDCLASSEX wc_existing = {0};
    if (::GetClassInfoEx(instance, wc.lpszClassName, &wc_existing)) {
        wc = wc_existing;
        return TRUE;
    }

    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = StaticWindowProc;

    auto res = ::RegisterClassEx(&wc);
    return ToBool(res);
}

void Wnd::Attach(HWND hwnd) {
    CrashIf(!IsWindow(hwnd));
    CrashIf(WindowMapGetWindow(hwnd));

    Subclass(hwnd);
    OnAttach();
}

// Attaches a CWnd object to a dialog item.
void Wnd::AttachDlgItem(UINT id, HWND parent) {
    CrashIf(!::IsWindow(parent));
    HWND wnd = ::GetDlgItem(parent, id);
    Attach(wnd);
}

HWND Wnd::Detach() {
    CrashIf(!prevWindowProc);
    if (IsWindow(hwnd)) {
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)(prevWindowProc));
    }

    HWND wnd = this->hwnd;
    WindowMapRemove(this);
    this->hwnd = nullptr;
    prevWindowProc = nullptr;
    return wnd;
}

void Wnd::Cleanup() {
    WindowMapRemove(this);
    hwnd = nullptr;
    prevWindowProc = nullptr;
}

HWND Wnd::Create(HWND parent) {
    WNDCLASS wc = {};
    CREATESTRUCT cs = {};

    // PreRegisterClass(wc);
    wc.lpszClassName = wndClassName;
    if (wc.lpszClassName) {
        WndRegisterClass(wc);
        cs.lpszClass = wc.lpszClassName;
    } else {
        cs.lpszClass = kDefaultClassName;
    }

    // Set a reasonable default window style.
    DWORD dwOverlappedStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    cs.style = WS_VISIBLE;
    if (parent) {
        cs.style |= WS_CHILD;
    } else {
        cs.style |= dwOverlappedStyle;
    }

    // Set a reasonable default window position
    if (0 == parent) {
        cs.x = CW_USEDEFAULT;
        cs.cx = CW_USEDEFAULT;
        cs.y = CW_USEDEFAULT;
        cs.cy = CW_USEDEFAULT;
    }

    // Allow the CREATESTRUCT parameters to be modified.
    PreCreate(cs);

    DWORD style = cs.style & ~WS_VISIBLE;

    CreateEx(cs.dwExStyle, cs.lpszClass, cs.lpszName, style, cs.x, cs.y, cs.cx, cs.cy, parent, cs.hMenu,
             cs.lpCreateParams);
    CrashIf(!::IsWindow(hwnd));

    if (cs.style & WS_VISIBLE) {
        if (cs.style & WS_MAXIMIZE)
            ::ShowWindow(hwnd, SW_MAXIMIZE);
        else if (cs.style & WS_MINIMIZE)
            ::ShowWindow(hwnd, SW_MINIMIZE);
        else
            ::ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    return hwnd;
}

// Creates the window by specifying each parameter. The lpszClassName must
//  be a predefined class name or registered with RegisterClass. A failure
//  to create a window throws an exception.
HWND Wnd::CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, int x, int y, int width,
                   int height, HWND parent, HMENU idOrMenu, LPVOID lparam /*= NULL*/) {
    CrashIf(hwnd);
    CrashIf(IsWindow(hwnd)); // Only one window per CWnd instance allowed.

    // Ensure a window class is registered.
    if (!className || !*className) {
        className = kDefaultClassName;
    }

    WNDCLASS wc = {};
    wc.lpszClassName = (WCHAR*)className;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    wc.hCursor = ::LoadCursor(0, IDC_ARROW);

    // Register the window class (if not already registered).
    WndRegisterClass(wc);

    gWindowBeingCreated = this;
    // Create window
    hwnd = ::CreateWindowEx(exStyle, className, windowName, style, x, y, width, height, parent, idOrMenu, GetInstance(),
                            lparam);

    gWindowBeingCreated = nullptr;
    CrashIf(!hwnd);

    if (!str::Eq(className, kDefaultClassName)) {
        // Automatically subclass predefined window class types.
        Subclass(hwnd);
        // Override this to perform tasks after the window is attached.
        OnAttach();
    }
    return hwnd;
}

void Wnd::Subclass(HWND hwnd) {
    CrashIf(!IsWindow(hwnd));
    CrashIf(prevWindowProc); // don't subclass multiple times

    this->hwnd = hwnd;
    WindowMapAdd(hwnd, this);
    LONG_PTR pWndProc = reinterpret_cast<LONG_PTR>(StaticWindowProc);
    LONG_PTR pRes = ::SetWindowLongPtr(hwnd, GWLP_WNDPROC, pWndProc);
    prevWindowProc = reinterpret_cast<WNDPROC>(pRes);
}

// application.cpp
bool PreTranslateMessage(MSG& msg) {
    bool shouldProcess = (WM_KEYFIRST <= msg.message && msg.message <= WM_KEYLAST) ||
                         (WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST);
    if (!shouldProcess) {
        return false;
    }
    for (HWND hwnd = msg.hwnd; hwnd != nullptr; hwnd = ::GetParent(hwnd)) {
        if (auto window = WindowMapGetWindow(hwnd)) {
            if (window->PreTranslateMessage(msg)) {
                return true;
            }
        }
    }
    return false;
}

#if 0
int MessageLoop() {
    MSG msg;

    // TODO: add handling of accelerators
    while (::GetMessage(&msg, nullptr, 0, 0)) {
        if (!PreTranslateMessage(msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

    return static_cast<int>(LOWORD(msg.wParam));
}
#endif

} // namespace wg

//- Button

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons
namespace wg {

Kind kindButton = "button";

Button::Button() {
    kind = kindButton;
    winClass = WC_BUTTONW;
}

Button::~Button() = default;

LRESULT Button::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg != WM_COMMAND) {
        return 0;
    }
    auto code = HIWORD(wparam);
    if (code != BN_CLICKED) {
        return 0;
    }
    if (!onClicked) {
        return 0;
    }
    onClicked();
    return 1;
}

void Button::PreCreate(CREATESTRUCT& cs) {
    LONG style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (isDefault) {
        style |= BS_DEFPUSHBUTTON;
    } else {
        style |= BS_PUSHBUTTON;
    }
    cs.style = style;
}

HWND Button::Create(HWND parent) {
    HWND ret = Wnd::Create(parent);
    CrashIf(!ret);

    auto size = GetIdealSize();
    RECT r{0, 0, size.dx, size.dy};
    SetBounds(r);
    return ret;
}

Size Button::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

#if 0
Size ButtonCtrl::SetTextAndResize(const WCHAR* s) {
    win::SetText(this->hwnd, s);
    Size size = this->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}
#endif

Button* CreateButton(HWND parent, const WCHAR* s, const ClickedHandler& onClicked) {
    auto b = new Button();
    b->onClicked = onClicked;
    b->SetText(s);
    b->Create(parent);
    return b;
}

} // namespace wg

namespace wg {

Kind kindEdit = "edit";

static bool EditSetCueText(HWND hwnd, std::string_view s) {
    if (!hwnd) {
        return false;
    }
    auto ws = ToWstrTemp(s);
    bool ok = Edit_SetCueBannerText(hwnd, ws.Get()) == TRUE;
    return ok;
}

Edit::Edit() {
    winClass = WC_EDIT;
    kind = kindEdit;
}

Edit::~Edit() {
    // DeleteObject(bgBrush);
}

bool Edit::SetCueText(std::string_view s) {
    cueText.Set(s);
    return EditSetCueText(hwnd, cueText.AsView());
}

void Edit::SetSelection(int start, int end) {
    Edit_SetSel(hwnd, start, end);
}

void Edit::PreCreate(CREATESTRUCT& cs) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/edit-control-styles
    LONG style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT;
    style |= WS_BORDER;
    hasBorder = bit::IsMaskSet<LONG>(style, WS_BORDER);
    if (isMultiLine) {
        style |= ES_MULTILINE | WS_VSCROLL | ES_WANTRETURN;
    } else {
        // ES_AUTOHSCROLL disable wrapping in multi-line setup
        style |= ES_AUTOHSCROLL;
    }
    cs.style = style;
}

HWND Edit::Create(HWND parent) {
    // Note: has to remember this here because when I GetWindowStyle() later on,
    // WS_BORDER is not set, which is a mystery, because it is being drawn.
    // also, WS_BORDER seems to be painted in client area

    auto hwnd = Wnd::Create(parent);
    CrashIf(!hwnd);
    if (!hwnd) {
        return nullptr;
    }

    EditSetCueText(hwnd, cueText.AsView());
    return hwnd;
}

Size Edit::GetIdealSize() {
    // TODO: use font on the control
    HFONT hfont = GetDefaultGuiFont();
    Size s1 = HwndMeasureText(hwnd, L"Minimal", hfont);
    // logf("EditCtrl::GetIdealSize: s1.dx=%d, s2.dy=%d\n", (int)s1.cx, (int)s1.cy);
    auto txt = win::GetTextTemp(hwnd);
    Size s2 = HwndMeasureText(hwnd, txt, hfont);
    // logf("EditCtrl::GetIdealSize: s2.dx=%d, s2.dy=%d\n", (int)s2.cx, (int)s2.cy);

    int dx = std::max(s1.dx, s2.dx);
    if (maxDx > 0 && dx > maxDx) {
        dx = maxDx;
    }
    // for multi-line text, this measures multiple line.
    // TODO: maybe figure out better protocol
    int dy = std::min(s1.dy, s2.dy);
    if (dy == 0) {
        dy = std::max(s1.dy, s2.dy);
    }
    dy = dy * idealSizeLines;
    // logf("EditCtrl::GetIdealSize: dx=%d, dy=%d\n", (int)dx, (int)dy);

    LRESULT margins = SendMessageW(hwnd, EM_GETMARGINS, 0, 0);
    int lm = (int)LOWORD(margins);
    int rm = (int)HIWORD(margins);
    dx += lm + rm;

    if (this->hasBorder) {
        dx += DpiScale(hwnd, 4);
        dy += DpiScale(hwnd, 4);
    }
    // logf("EditCtrl::GetIdealSize(): dx=%d, dy=%d\n", int(res.cx), int(res.cy));
    return {dx, dy};
}

LRESULT Edit::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_COMMAND) {
        auto code = HIWORD(wparam);
        if (code == EN_CHANGE && onTextChanged) {
            onTextChanged();
            return 1;
        }
        return 0;
    }

    if (msg == WM_CTLCOLOREDIT) {
        // TOOD: return brush
        return 0;
    }
    return 0;
}

} // namespace wg

namespace wg {

Kind kindListBox = "listbox";

ListBox::ListBox() {
    kind = kindListBox;
    winClass = L"LISTBOX";
#if 0
    dwExStyle = 0;
    dwStyle = WS_CHILD | WS_BORDER | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL;
    dwStyle |= LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
    ctrlID = 0;
#endif
}

ListBox::~ListBox() {
    delete this->model;
}

HWND ListBox::Create(HWND parent) {
    idealSize = {DpiScale(parent, 120), DpiScale(parent, 32)};
    HWND ret = Wnd::Create(parent);
    CrashIf(!ret);

    // TODO: update ideal size based on the size of the model?
    if (!ret) {
        return nullptr;
    }
    if (model != nullptr) {
        FillWithItems(this->hwnd, model);
    }
    return ret;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/lb-getitemheight
int ListBox::GetItemHeight(int idx) {
    // idx only valid for LBS_OWNERDRAWVARIABLE, otherwise should be 0
    int res = (int)SendMessageW(hwnd, LB_GETITEMHEIGHT, idx, 0);
    if (res == LB_ERR) {
        // if failed for some reason, fallback to measuring text in default font
        // HFONT f = GetFont();
        HFONT f = GetDefaultGuiFont();
        Size sz = HwndMeasureText(hwnd, L"A", f);
        res = sz.dy;
    }
    return res;
}

Size ListBox::GetIdealSize() {
    Size res = idealSize;
    if (idealSizeLines > 0) {
        int dy = GetItemHeight(0) * idealSizeLines + DpiScale(hwnd, 2 + 2); // padding of 2 at top and bottom
        res.dy = dy;
    }
    return res;
}

int ListBox::GetCurrentSelection() {
    LRESULT res = ListBox_GetCurSel(hwnd);
    return (int)res;
}

// -1 to clear selection
// returns false on error
bool ListBox::SetCurrentSelection(int n) {
    if (n < 0) {
        ListBox_SetCurSel(hwnd, -1);
        return true;
    }
    int nItems = model->ItemsCount();
    if (n >= nItems) {
        return false;
    }
    LRESULT res = ListBox_SetCurSel(hwnd, n);
    return res != LB_ERR;
}

// for efficiency you can re-use model:
// get the model, change data, call SetModel() again
void ListBox::SetModel(ListBoxModel* model) {
    if (this->model && (this->model != model)) {
        delete this->model;
    }
    this->model = model;
    if (model != nullptr) {
        FillWithItems(this->hwnd, model);
    }
    SetCurrentSelection(-1);
    // TODO: update ideal size based on the size of the model
}

} // namespace wg