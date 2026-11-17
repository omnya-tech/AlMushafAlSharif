#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "utf8_util.hpp"   // أولاً: يعرّف NOMINMAX قبل windows.h
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <set>

#include "database.hpp"
#include "sajdah_and_formatter.hpp"
#include "a11y_and_config.hpp"
#include "shortcuts.hpp"
#include "dpi.hpp"

namespace mushaf {

// إغلاق نافذة فرعية: نعيد تفعيل النافذة الرئيسية قبل الإغلاق، وإلا أعطى ويندوز التركيز
// لبرنامج آخر لحظة اختفاء النافذة (لأن الرئيسية تكون معطلة)، فيضيع موضع قارئ الشاشة.
inline void close_modal(HWND dlg) {
    if (!dlg || !IsWindow(dlg)) return;
    if (HWND owner = GetWindow(dlg, GW_OWNER)) {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    DestroyWindow(dlg);
}

// ----------------------------------------------------------------------------
// صفحات التبويب مع مظهر ويندوز الحديث (Common Controls 6)
// ----------------------------------------------------------------------------
// عناصر الصفحة إخوة لعنصر التبويب لا أبناء له، فيجب أن يكون التبويب في أسفل الترتيب
// حتى لا يرسم جسمه فوقها فتختفي.
inline void send_tab_to_back(HWND tab) {
    if (tab) SetWindowPos(tab, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// هل يعمل البرنامج بمظهر ويندوز الحديث؟ (يُحمَّل ديناميكياً ليبقى متوافقاً مع كل الأنظمة)
inline bool app_is_themed() {
    using fn_t = BOOL(WINAPI*)();
    static int cached = -1;
    if (cached < 0) {
        cached = 0;
        if (HMODULE ux = LoadLibraryW(L"uxtheme.dll")) {
            auto fn = reinterpret_cast<fn_t>(GetProcAddress(ux, "IsAppThemed"));
            if (fn && fn()) cached = 1;
        }
    }
    return cached == 1;
}

// Ctrl+Tab و Ctrl+Shift+Tab (و Ctrl+PageDown / Ctrl+PageUp) للتنقل بين صفحات التبويب كما في نوافذ ويندوز.
// ينقل التركيز إلى شريط التبويب نفسه فيعلن قارئ الشاشة اسم الصفحة، ثم Tab يدخل إلى عناصرها.
inline bool handle_tab_switch_keys(const MSG& msg, HWND dlg, HWND tab, const std::function<void(int)>& on_switch) {
    if (msg.message != WM_KEYDOWN || !tab) return false;
    if (msg.hwnd != dlg && !IsChild(dlg, msg.hwnd)) return false;
    if (!(GetKeyState(VK_CONTROL) & 0x8000)) return false;
    int dir = 0;
    if (msg.wParam == VK_TAB) dir = (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1;
    else if (msg.wParam == VK_NEXT) dir = 1;
    else if (msg.wParam == VK_PRIOR) dir = -1;
    if (dir == 0) return false;
    int n = (int)SendMessageW(tab, TCM_GETITEMCOUNT, 0, 0);
    if (n <= 0) return false;
    int cur = (int)SendMessageW(tab, TCM_GETCURSEL, 0, 0);
    int next = ((cur < 0 ? 0 : cur) + dir + n) % n;
    SendMessageW(tab, TCM_SETCURSEL, next, 0);
    on_switch(next);
    SetFocus(tab);
    return true;
}

// ترتيب التنقل بمفتاح Tab في نوافذ التبويب: عناصر الصفحة الظاهرة، ثم قائمة التبويبات،
// ثم أزرار الحفظ والإلغاء بعدها. (التبويب في أسفل ترتيب الرسم حتى لا يغطي العناصر،
// فلا يصلح الاعتماد على ترتيب ويندوز التلقائي.)
inline bool handle_custom_tab_order(const MSG& msg, HWND dlg, const std::vector<HWND>& order) {
    if (msg.message != WM_KEYDOWN || msg.wParam != VK_TAB) return false;
    if (GetKeyState(VK_CONTROL) & 0x8000) return false;
    if (msg.hwnd != dlg && !IsChild(dlg, msg.hwnd)) return false;
    std::vector<HWND> live;
    for (HWND h : order) {
        if (h && IsWindowVisible(h) && IsWindowEnabled(h) && (GetWindowLongW(h, GWL_STYLE) & WS_TABSTOP)) live.push_back(h);
    }
    if (live.empty()) return false;
    HWND foc = GetFocus();
    int idx = -1;
    for (size_t i = 0; i < live.size(); ++i) {
        if (live[i] == foc || IsChild(live[i], foc)) { idx = (int)i; break; }
    }
    const bool back = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const int n = (int)live.size();
    int next = (idx < 0) ? (back ? n - 1 : 0) : (idx + (back ? n - 1 : 1)) % n;
    HWND target = live[next];
    SetFocus(target);
    wchar_t cls[16] = {0};
    GetClassNameW(target, cls, 15);
    if (_wcsicmp(cls, L"Edit") == 0) SendMessageW(target, EM_SETSEL, 0, -1);
    return true;
}

// لون خلفية النصوص ومربعات الاختيار فوق جسم التبويب: أبيض مع المظهر الحديث، ورمادي النظام بدونه
inline LRESULT tab_page_ctlcolor(HDC hdc) {
    int idx = app_is_themed() ? COLOR_WINDOW : COLOR_BTNFACE;
    SetBkColor(hdc, GetSysColor(idx));
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    return (LRESULT)GetSysColorBrush(idx);
}

// ============================================================================
// نافذة قراءة سطرية متوافقة مع قارئات الشاشة (معلومات الآية / السورة / الاختصارات / حول)
// ============================================================================
struct LineReaderState {
    std::wstring title;
    std::vector<std::wstring> lines;
    bool is_en = false;
    HFONT font = nullptr;
    HWND list_hwnd = nullptr;
    ScreenReaderEngine* sr = nullptr;
};

static WNDPROC g_orig_line_list_proc = nullptr;

static LRESULT CALLBACK LineListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<LineReaderState*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
    if (msg == WM_KEYDOWN && st) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (wParam == VK_ESCAPE) {
            SendMessageW(GetParent(hwnd), WM_CLOSE, 0, 0);
            return 0;
        }
        if (wParam == VK_RETURN || (wParam == 'C' && ctrl)) {
            int sel = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->lines.size()) {
                set_clipboard_text(GetParent(hwnd), st->lines[sel]);
                if (st->sr) {
                    st->sr->speak(st->is_en ? L"Copied current line" : L"تم نسخ السطر الحالي", true);
                }
            }
            return 0;
        }
    }
    return CallWindowProcW(g_orig_line_list_proc, hwnd, msg, wParam, lParam);
}

// اتجاه السطر من أول حرف قوي فيه: عربي ← من اليمين، لاتيني ← من اليسار، وإلا فاتجاه الواجهة
inline bool line_reads_rtl(const std::wstring& s, bool ui_rtl) {
    for (wchar_t ch : s) {
        if ((ch >= 0x0621 && ch <= 0x064A) || (ch >= 0x0671 && ch <= 0x06D3) ||
            (ch >= 0xFB50 && ch <= 0xFDFF) || (ch >= 0xFE70 && ch <= 0xFEFC)) return true;
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z')) return false;
    }
    return ui_rtl;
}

static const int kLineReaderPad = 10;

// ----------------------------------------------------------------------------
// قوائم تلتفّ سطورها (owner-draw): تُرسم بالتفاف كامل للمبصر، ويقرأ قارئ الشاشة نص العنصر كما هو.
// تُستخدم في نوافذ المعلومات والبحث والمفضلة والسجدات حتى لا ينقطع أي سطر طويل.
// ----------------------------------------------------------------------------
inline bool is_bracket_char(wchar_t c) {
    return c == 0xFD3E || c == 0xFD3F || c == L'(' || c == L')' || c == L'[' || c == L']' ||
           c == 0x00AB || c == 0x00BB;
}

inline std::wstring wrapped_display_text(const std::wstring& t, bool ui_rtl, bool& rtl_line) {
    rtl_line = line_reads_rtl(t, ui_rtl);
    if (!rtl_line || t.empty()) return t;
    // علامة «من اليمين لليسار» الخفية تثبّت موضع القوس (ومنه المزخرف ﴿ ﴾) إذا بدأ به السطر أو انتهى.
    // لا نضيفها لغير ذلك، لأن ويندوز قد يرسمها نقطة ظاهرة في السطر الملتفّ.
    std::wstring out;
    if (is_bracket_char(t.front())) out.push_back((wchar_t)0x200F);
    out += t;
    if (is_bracket_char(t.back())) out.push_back((wchar_t)0x200F);
    return out;
}

inline UINT wrapped_list_measure(HWND list, HFONT font, const std::wstring& text, bool ui_rtl) {
    bool rtl_line = false;
    std::wstring t = wrapped_display_text(text, ui_rtl, rtl_line);
    RECT rc = {0, 0, 700, 0};
    if (list) GetClientRect(list, &rc);
    HDC hdc = GetDC(list);
    HGDIOBJ old = SelectObject(hdc, font);
    RECT calc = {0, 0, std::max(100, (int)(rc.right - rc.left) - 2 * kLineReaderPad), 0};
    DrawTextW(hdc, t.c_str(), (int)t.size(), &calc,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | dt_reading_flag(hdc, rtl_line));
    SelectObject(hdc, old);
    ReleaseDC(list, hdc);
    return (UINT)std::min(250, std::max(26, (int)(calc.bottom - calc.top) + 10));
}

inline void wrapped_list_draw(const DRAWITEMSTRUCT* dis, HFONT font, bool ui_rtl) {
    if (!dis || dis->itemID == (UINT)-1) return;
    int len = (int)SendMessageW(dis->hwndItem, LB_GETTEXTLEN, dis->itemID, 0);
    std::wstring raw(len > 0 ? len : 0, L'\0');
    if (len > 0) SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)&raw[0]);
    bool rtl_line = false;
    std::wstring t = wrapped_display_text(raw, ui_rtl, rtl_line);

    bool sel = (dis->itemState & ODS_SELECTED) != 0;
    FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    HGDIOBJ old = SelectObject(dis->hDC, font);
    bool mirrored = (GetWindowLongW(dis->hwndItem, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    // السطر العربي يبدأ من اليمين المرئي والإنجليزي من اليسار؛ وفي النافذة المعكوسة DT_LEFT هو اليمين
    UINT align = (rtl_line == mirrored) ? DT_LEFT : DT_RIGHT;
    RECT tr = dis->rcItem;
    InflateRect(&tr, -kLineReaderPad, -5);
    DrawTextW(dis->hDC, t.c_str(), (int)t.size(), &tr,
              align | DT_WORDBREAK | DT_NOPREFIX | dt_reading_flag(dis->hDC, rtl_line));
    SelectObject(dis->hDC, old);
    if (dis->itemState & ODS_FOCUS) DrawFocusRect(dis->hDC, &dis->rcItem);
}

// معالجة WM_MEASUREITEM / WM_DRAWITEM لقائمة ملتفّة رقمها list_id داخل نافذة؛ تعيد true إن عالجتها
inline bool handle_wrapped_list_msg(HWND dlg, UINT msg, LPARAM lParam, UINT list_id, HFONT font, bool ui_rtl) {
    if (msg == WM_MEASUREITEM) {
        auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (!mis || mis->CtlID != list_id) return false;
        // أثناء LB_ADDSTRING يكون itemData هو مؤشر النص نفسه
        const wchar_t* txt = reinterpret_cast<const wchar_t*>(mis->itemData);
        std::wstring text;
        if (txt) text = txt;
        else {
            HWND list = GetDlgItem(dlg, (int)list_id);
            int len = (int)SendMessageW(list, LB_GETTEXTLEN, mis->itemID, 0);
            if (len > 0) { text.assign(len, L'\0'); SendMessageW(list, LB_GETTEXT, mis->itemID, (LPARAM)&text[0]); }
        }
        mis->itemHeight = wrapped_list_measure(GetDlgItem(dlg, (int)list_id), font, text, ui_rtl);
        return true;
    }
    if (msg == WM_DRAWITEM) {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!dis || dis->CtlID != list_id) return false;
        wrapped_list_draw(dis, font, ui_rtl);
        return true;
    }
    return false;
}

// أنماط القائمة الملتفّة
static const DWORD kWrappedListStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY |
                                       LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS;

static LRESULT CALLBACK LineReaderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<LineReaderState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_MEASUREITEM:
    case WM_DRAWITEM:
        if (st && handle_wrapped_list_msg(hwnd, msg, lParam, 200, st->font, !st->is_en)) return TRUE;
        break;
    case WM_COMMAND: {
        UINT id = LOWORD(wParam);
        if (id == IDOK || id == IDCANCEL) {
            close_modal(hwnd);
            return 0;
        }
        if (id == 201 && st) { // نسخ السطر الحالي
            int sel = (int)SendMessageW(st->list_hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->lines.size()) {
                set_clipboard_text(hwnd, st->lines[sel]);
                if (st->sr) st->sr->speak(st->is_en ? L"Copied current line" : L"تم نسخ السطر الحالي", true);
            }
            return 0;
        }
        if (id == 202 && st) { // نسخ الكل
            std::wstring all;
            for (size_t i = 0; i < st->lines.size(); ++i) {
                if (i > 0) all += L"\r\n";
                all += st->lines[i];
            }
            set_clipboard_text(hwnd, all);
            if (st->sr) st->sr->speak(st->is_en ? L"Copied all lines" : L"تم نسخ المعلومات", true);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        close_modal(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline void show_line_reader_dialog(
    HWND parent,
    const std::wstring& title,
    const std::vector<std::wstring>& lines,
    bool is_en,
    HFONT ui_font,
    ScreenReaderEngine* sr
) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = LineReaderWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"MushafLineReaderDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    LineReaderState st;
    st.title = title;
    st.lines = lines;
    st.is_en = is_en;
    st.font = ui_font;
    st.sr = sr;

    DWORD ex_style = WS_EX_DLGMODALFRAME | (is_en ? 0 : (WS_EX_LAYOUTRTL | WS_EX_RTLREADING));
    HWND dlg = CreateWindowExW(
        ex_style, L"MushafLineReaderDlg", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        160, 110, 760, 540, parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    st.list_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE /* النافذة المعكوسة تقرأ من اليمين أصلاً */,
        L"LISTBOX", L"",
        kWrappedListStyle,   // يرسمها البرنامج بالتفاف، ويقرؤها قارئ الشاشة نصاً
        16, 16, 718, 424, dlg, (HMENU)200, GetModuleHandleW(nullptr), nullptr
    );
    SendMessageW(st.list_hwnd, WM_SETFONT, (WPARAM)ui_font, TRUE);
    for (const auto& ln : lines) {
        SendMessageW(st.list_hwnd, LB_ADDSTRING, 0, (LPARAM)ln.c_str());
    }
    if (!lines.empty()) SendMessageW(st.list_hwnd, LB_SETCURSEL, 0, 0);
    g_orig_line_list_proc = (WNDPROC)SetWindowLongPtrW(st.list_hwnd, GWLP_WNDPROC, (LONG_PTR)LineListSubclassProc);

    HWND btn_copy_line = CreateWindowExW(
        0, L"BUTTON", is_en ? L"&Copy Line (Ctrl+C)" : L"نسخ ال&سطر الحالي (Ctrl+C)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        16, 452, 220, 34, dlg, (HMENU)201, GetModuleHandleW(nullptr), nullptr
    );
    HWND btn_copy_all = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Copy &Information" : L"نسخ الم&علومات",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        248, 452, 180, 34, dlg, (HMENU)202, GetModuleHandleW(nullptr), nullptr
    );
    HWND btn_close = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Close (Esc)" : L"إغلاق (Esc)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        584, 452, 150, 34, dlg, (HMENU)IDCANCEL, GetModuleHandleW(nullptr), nullptr
    );
    SendMessageW(btn_copy_line, WM_SETFONT, (WPARAM)ui_font, TRUE);
    SendMessageW(btn_copy_all, WM_SETFONT, (WPARAM)ui_font, TRUE);
    SendMessageW(btn_close, WM_SETFONT, (WPARAM)ui_font, TRUE);

    fit_dialog_to_screen(dlg, parent);   // مقاسات الشاشة الحالية، وداخل حدودها
    EnableWindow(parent, FALSE);
    SetFocus(st.list_hwnd);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (handle_enter_on_focused_button(dlg, msg)) continue;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
}

// ============================================================================
// نافذة الخيارات الشاملة (OptionsDialog — Ctrl+O) المطابقة لنسخة بايثون (٣ تبويبات)
// ============================================================================
struct OptionsDlgState {
    AppSettings* settings = nullptr;
    const AyahInfo* sample_ayah = nullptr;
    bool is_en = false;
    bool accepted = false;
    std::wstring initial_default_browse;

    HWND tab_ctrl = nullptr;
    std::vector<HWND> tab0_ctrls;
    std::vector<HWND> tab1_ctrls;
    std::vector<HWND> tab2_ctrls;
    std::vector<HWND> tab3_ctrls;

    // Tab 0: النسخ والتصفح
    HWND combo_browse = nullptr;
    HWND combo_lang = nullptr;
    HWND chk_page_sound = nullptr;
    HWND chk_show_sidebar = nullptr;
    HWND combo_startup_focus = nullptr;
    HWND combo_copy_script = nullptr;
    HWND chk_brackets = nullptr;
    HWND chk_reference = nullptr;
    HWND chk_arabic_digits = nullptr;
    HWND lbl_preview = nullptr;

    // Tab 1: الخط والمظهر
    HWND combo_font = nullptr;
    HWND edit_font_size = nullptr;
    HWND combo_display = nullptr;
    HWND combo_align = nullptr;
    HWND combo_theme = nullptr;
    HWND chk_letter_count = nullptr;

    // Tab 2: قارئات الشاشة
    HWND chk_sr_auto_speak = nullptr;
    HWND chk_sr_phonetic = nullptr;
    HWND chk_sr_muqattaat = nullptr;
    HWND chk_sapi = nullptr;
    HWND chk_sr_waqf = nullptr;

    // Tab 3: تخصيص الاختصارات
    std::map<std::wstring, std::wstring> pending_shortcuts;
    HWND list_shortcuts = nullptr;
    HWND edit_shortcut_capture = nullptr;
    HWND lbl_shortcut_status = nullptr;
    HWND btn_sc_reset_one = nullptr;
    HWND btn_sc_reset_all = nullptr;
    WNDPROC orig_capture_proc = nullptr;
};

static void refresh_options_shortcuts_list(OptionsDlgState* st, int keep_sel = -1) {
    if (!st || !st->list_shortcuts) return;
    int sel = (keep_sel >= 0) ? keep_sel : (int)SendMessageW(st->list_shortcuts, LB_GETCURSEL, 0, 0);
    if (sel < 0) sel = 0;
    SendMessageW(st->list_shortcuts, LB_RESETCONTENT, 0, 0);
    const auto& defs = shortcut_defs();
    for (const auto& d : defs) {
        std::wstring spec = effective_shortcut_spec(st->pending_shortcuts, d);
        std::wstring row = (st->is_en ? d.label_en : d.label_ar) + L"  (" + spec + L")";
        SendMessageW(st->list_shortcuts, LB_ADDSTRING, 0, (LPARAM)row.c_str());
    }
    if (sel >= (int)defs.size()) sel = (int)defs.size() - 1;
    if (sel >= 0) {
        SendMessageW(st->list_shortcuts, LB_SETCURSEL, sel, 0);
        if (st->edit_shortcut_capture) {
            std::wstring cur_spec = effective_shortcut_spec(st->pending_shortcuts, defs[sel]);
            SetWindowTextW(st->edit_shortcut_capture, cur_spec.c_str());
        }
    }
}

static LRESULT CALLBACK ShortcutCaptureProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<OptionsDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!st) return DefWindowProcW(hwnd, msg, wParam, lParam);

    if (msg == WM_GETDLGCODE) {
        LRESULT code = CallWindowProcW(st->orig_capture_proc, hwnd, msg, wParam, lParam);
        // Allow Tab and Escape to navigate dialog normally; capture other keys
        if (lParam) {
            auto* m = reinterpret_cast<MSG*>(lParam);
            if (m->message == WM_KEYDOWN && (m->wParam == VK_TAB || m->wParam == VK_ESCAPE)) {
                return code & ~DLGC_WANTALLKEYS;
            }
        }
        return code | DLGC_WANTALLKEYS;
    }

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        WORD vk = (WORD)wParam;
        if (vk == VK_TAB || vk == VK_ESCAPE) {
            return CallWindowProcW(st->orig_capture_proc, hwnd, msg, wParam, lParam);
        }
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) {
            return 0;
        }
        BYTE fVirt = FVIRTKEY;
        if (GetKeyState(VK_CONTROL) & 0x8000) fVirt |= FCONTROL;
        if (GetKeyState(VK_MENU) & 0x8000)    fVirt |= FALT;
        if (GetKeyState(VK_SHIFT) & 0x8000)   fVirt |= FSHIFT;

        std::wstring rar, ren;
        if (!is_screen_reader_safe_shortcut(fVirt, vk, rar, ren)) {
            std::wstring err = st->is_en ? ren : rar;
            if (st->lbl_shortcut_status) SetWindowTextW(st->lbl_shortcut_status, err.c_str());
            ScreenReaderEngine::instance().speak(err, true, st->settings->sapi_fallback);
            return 0;
        }

        std::wstring new_spec = format_shortcut_spec(fVirt, vk);
        int sel = (int)SendMessageW(st->list_shortcuts, LB_GETCURSEL, 0, 0);
        const auto& defs = shortcut_defs();
        if (sel < 0 || sel >= (int)defs.size()) return 0;

        for (size_t i = 0; i < defs.size(); ++i) {
            if ((int)i == sel) continue;
            if (effective_shortcut_spec(st->pending_shortcuts, defs[i]) == new_spec) {
                std::wstring other = st->is_en ? defs[i].label_en : defs[i].label_ar;
                std::wstring err = st->is_en
                    ? (L"Shortcut " + new_spec + L" is already assigned to: " + other)
                    : (L"الاختصار " + new_spec + L" مستخدم بالفعل للأمر: " + other);
                if (st->lbl_shortcut_status) SetWindowTextW(st->lbl_shortcut_status, err.c_str());
                ScreenReaderEngine::instance().speak(err, true, st->settings->sapi_fallback);
                return 0;
            }
        }

        if (new_spec == defs[sel].default_spec) st->pending_shortcuts.erase(defs[sel].key);
        else st->pending_shortcuts[defs[sel].key] = new_spec;

        refresh_options_shortcuts_list(st, sel);
        std::wstring cmd_lbl = st->is_en ? defs[sel].label_en : defs[sel].label_ar;
        std::wstring ok_msg = st->is_en
            ? (L"Assigned " + new_spec + L" to " + cmd_lbl)
            : (L"تم تعيين " + new_spec + L" لأمر: " + cmd_lbl);
        if (st->lbl_shortcut_status) SetWindowTextW(st->lbl_shortcut_status, ok_msg.c_str());
        ScreenReaderEngine::instance().speak(ok_msg, true, st->settings->sapi_fallback);
        return 0;
    }

    if (msg == WM_CHAR || msg == WM_SYSCHAR) {
        return 0;
    }

    return CallWindowProcW(st->orig_capture_proc, hwnd, msg, wParam, lParam);
}

static void update_options_copy_preview(OptionsDlgState* st) {
    if (!st || !st->lbl_preview) return;
    static const wchar_t* scripts[3] = {L"uthmani", L"qpc", L"imlaei"};
    int s_idx = (int)SendMessageW(st->combo_copy_script, CB_GETCURSEL, 0, 0);
    std::wstring script = (s_idx >= 0 && s_idx < 3) ? scripts[s_idx] : L"uthmani";
    bool brackets = (SendMessageW(st->chk_brackets, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool reference = (SendMessageW(st->chk_reference, BM_GETCHECK, 0, 0) == BST_CHECKED);
    bool ar_digits = (SendMessageW(st->chk_arabic_digits, BM_GETCHECK, 0, 0) == BST_CHECKED);

    std::wstring txt;
    if (st->sample_ayah) {
        txt = format_ayah_custom(*st->sample_ayah, script, brackets, reference, ar_digits);
    } else {
        std::wstring core = (script == L"uthmani" || script == L"qpc")
            ? L"ٱلْحَمْدُ لِلَّهِ رَبِّ ٱلْعَـٰلَمِينَ"
            : L"الْحَمْدُ لِلَّهِ رَبِّ الْعَالَمِينَ";
        std::wstring body = brackets ? (L"﴿ " + core + L" ﴾") : core;
        std::wstring d = ar_digits ? L"٢" : L"2";
        txt = reference ? (body + L" [الفاتحة: " + d + L"]") : body;
    }
    SetWindowTextW(st->lbl_preview, txt.c_str());
}

static void switch_options_tab(OptionsDlgState* st, int active_tab) {
    for (HWND h : st->tab0_ctrls) ShowWindow(h, active_tab == 0 ? SW_SHOW : SW_HIDE);
    for (HWND h : st->tab1_ctrls) ShowWindow(h, active_tab == 1 ? SW_SHOW : SW_HIDE);
    for (HWND h : st->tab2_ctrls) ShowWindow(h, active_tab == 2 ? SW_SHOW : SW_HIDE);
    for (HWND h : st->tab3_ctrls) ShowWindow(h, active_tab == 3 ? SW_SHOW : SW_HIDE);
}

static LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<OptionsDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CTLCOLORSTATIC:
        return tab_page_ctlcolor((HDC)wParam);
    case WM_NOTIFY: {
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm && nm->idFrom == 600 && nm->code == TCN_SELCHANGE && st) {
            int sel = (int)SendMessageW(st->tab_ctrl, TCM_GETCURSEL, 0, 0);
            switch_options_tab(st, sel);
            return 0;
        }
        break;
    }
    case WM_COMMAND: {
        if (!st) break;
        UINT id = LOWORD(wParam);
        UINT code = HIWORD(wParam);
        if (id >= 610 && id <= 619) {
            update_options_copy_preview(st);
            return 0;
        }
        if (id == 640 && code == LBN_SELCHANGE) {
            int sel = (int)SendMessageW(st->list_shortcuts, LB_GETCURSEL, 0, 0);
            const auto& defs = shortcut_defs();
            if (sel >= 0 && sel < (int)defs.size() && st->edit_shortcut_capture) {
                std::wstring spec = effective_shortcut_spec(st->pending_shortcuts, defs[sel]);
                SetWindowTextW(st->edit_shortcut_capture, spec.c_str());
            }
            return 0;
        }
        if (id == 642) {
            int sel = (int)SendMessageW(st->list_shortcuts, LB_GETCURSEL, 0, 0);
            const auto& defs = shortcut_defs();
            if (sel >= 0 && sel < (int)defs.size()) {
                st->pending_shortcuts.erase(defs[sel].key);
                refresh_options_shortcuts_list(st, sel);
                std::wstring msg_txt = st->is_en
                    ? (L"Reset to default: " + defs[sel].default_spec)
                    : (L"تمت استعادة الافتراضي: " + defs[sel].default_spec);
                if (st->lbl_shortcut_status) SetWindowTextW(st->lbl_shortcut_status, msg_txt.c_str());
                ScreenReaderEngine::instance().speak(msg_txt, true, st->settings->sapi_fallback);
            }
            return 0;
        }
        if (id == 643) {
            st->pending_shortcuts.clear();
            refresh_options_shortcuts_list(st);
            std::wstring msg_txt = st->is_en
                ? L"All shortcuts reset to defaults."
                : L"تمت استعادة جميع الاختصارات الافتراضية.";
            if (st->lbl_shortcut_status) SetWindowTextW(st->lbl_shortcut_status, msg_txt.c_str());
            ScreenReaderEngine::instance().speak(msg_txt, true, st->settings->sapi_fallback);
            return 0;
        }
        if (id == IDOK) {
            static const wchar_t* scopes[7] = {L"surah", L"page", L"juz", L"hizb", L"rub", L"manzil", L"ruku"};
            int bm = (int)SendMessageW(st->combo_browse, CB_GETCURSEL, 0, 0);
            std::wstring chosen_bm = (bm >= 0 && bm < 7) ? scopes[bm] : L"surah";
            st->settings->default_browse_mode = chosen_bm;
            if (chosen_bm != st->initial_default_browse) {
                st->settings->browse_scope = chosen_bm;
            }

            int lg = (int)SendMessageW(st->combo_lang, CB_GETCURSEL, 0, 0);
            st->settings->ui_language = (lg == 1) ? L"en" : L"ar";

            st->settings->page_turn_sound = (SendMessageW(st->chk_page_sound, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->show_sidebar = (SendMessageW(st->chk_show_sidebar, BM_GETCHECK, 0, 0) == BST_CHECKED);

            static const wchar_t* start_keys[3] = {L"surah_index", L"ayahs", L"units"};
            int sf = (int)SendMessageW(st->combo_startup_focus, CB_GETCURSEL, 0, 0);
            st->settings->startup_focus = (sf >= 0 && sf < 3) ? start_keys[sf] : L"surah_index";

            static const wchar_t* scripts[3] = {L"uthmani", L"qpc", L"imlaei"};
            int cs = (int)SendMessageW(st->combo_copy_script, CB_GETCURSEL, 0, 0);
            st->settings->copy_script = (cs >= 0 && cs < 3) ? scripts[cs] : L"uthmani";
            st->settings->copy_brackets = (SendMessageW(st->chk_brackets, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->copy_reference = (SendMessageW(st->chk_reference, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->copy_arabic_digits = (SendMessageW(st->chk_arabic_digits, BM_GETCHECK, 0, 0) == BST_CHECKED);

            static const wchar_t* fonts[5] = {
                L"KFGQPC HAFS Uthmanic Script", L"Amiri Quran", L"Amiri", L"Scheherazade New", L"Kitab"
            };
            int fn = (int)SendMessageW(st->combo_font, CB_GETCURSEL, 0, 0);
            st->settings->quran_font_family = (fn >= 0 && fn < 5) ? fonts[fn] : fonts[0];

            wchar_t sz_buf[32] = {0};
            GetWindowTextW(st->edit_font_size, sz_buf, 31);
            int fsz = _wtoi(sz_buf);
            if (fsz >= 16 && fsz <= 52) st->settings->quran_font_size = fsz;

            static const wchar_t* disps[3] = {L"text", L"text_number", L"text_number_surah"};
            int dp = (int)SendMessageW(st->combo_display, CB_GETCURSEL, 0, 0);
            st->settings->ayah_display = (dp >= 0 && dp < 3) ? disps[dp] : L"text_number";

            int al = (int)SendMessageW(st->combo_align, CB_GETCURSEL, 0, 0);
            st->settings->doc_alignment = (al == 1) ? L"right" : L"center";

            static const wchar_t* themes[4] = {L"madinah_cream", L"royal_emerald", L"night_gold", L"high_contrast"};
            int th = (int)SendMessageW(st->combo_theme, CB_GETCURSEL, 0, 0);
            st->settings->theme = (th >= 0 && th < 4) ? themes[th] : L"madinah_cream";

            st->settings->sr_auto_speak_ayah = (SendMessageW(st->chk_sr_auto_speak, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->sr_phonetic_norm = (SendMessageW(st->chk_sr_phonetic, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->sr_spell_muqattaat = (SendMessageW(st->chk_sr_muqattaat, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->sapi_fallback = (SendMessageW(st->chk_sapi, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->sr_speak_waqf = (SendMessageW(st->chk_sr_waqf, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->show_ayah_letter_count = (SendMessageW(st->chk_letter_count, BM_GETCHECK, 0, 0) == BST_CHECKED);
            st->settings->custom_shortcuts = st->pending_shortcuts;

            st->settings->save();
            st->accepted = true;
            close_modal(hwnd);
            return 0;
        }
        if (id == IDCANCEL) {
            close_modal(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        close_modal(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline bool show_options_dialog(
    HWND parent,
    AppSettings& settings,
    const AyahInfo* sample_ayah,
    bool is_en,
    HFONT ui_font,
    int page_count = 604
) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = OptionsWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"MushafOptionsDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    OptionsDlgState st;
    st.settings = &settings;
    st.sample_ayah = sample_ayah;
    st.is_en = is_en;
    st.initial_default_browse = settings.default_browse_mode;
    st.pending_shortcuts = settings.custom_shortcuts;

    DWORD ex_style = WS_EX_DLGMODALFRAME | (is_en ? 0 : (WS_EX_LAYOUTRTL | WS_EX_RTLREADING));
    HWND dlg = CreateWindowExW(
        ex_style, L"MushafOptionsDlg",
        is_en ? L"Program Options (Ctrl+O)" : L"خيارات برنامج المصحف الشريف (Ctrl+O)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        150, 70, 740, 596, parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    st.tab_ctrl = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        14, 14, 698, 486, dlg, (HMENU)600, GetModuleHandleW(nullptr), nullptr
    );
    SendMessageW(st.tab_ctrl, WM_SETFONT, (WPARAM)ui_font, TRUE);

    TCITEMW tie{};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<wchar_t*>(is_en ? L"Copy and browsing" : L"النسخ والتصفح");
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 0, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(is_en ? L"Font and look" : L"الخط والمظهر");
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 1, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(is_en ? L"Screen readers" : L"قارئات الشاشة");
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 2, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(is_en ? L"Shortcuts" : L"تخصيص الاختصارات");
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 3, (LPARAM)&tie);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    auto add_ctrl = [&](std::vector<HWND>& vec, HWND h) {
        SendMessageW(h, WM_SETFONT, (WPARAM)ui_font, TRUE);
        vec.push_back(h);
        return h;
    };

    // ---------------- Tab 0: النسخ والتصفح ----------------

    add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Default browse by:" : L"طريقة التصفح:", WS_CHILD | WS_VISIBLE, 32, 60, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_browse = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 56, 430, 240, dlg, (HMENU)602, hInst, nullptr));
    // عدد الصفحات بحسب الطبعة المختارة (المدينة ٦٠٤، ودار المعارف ٤٨٥)
    int pr = page_count % 100;
    std::wstring pages_ar_lbl = L"الصفحات (" + to_arabic_digits(page_count) + ((pr >= 3 && pr <= 10) ? L" صفحات)" : L" صفحة)");
    std::wstring pages_en_lbl = L"Pages (" + std::to_wstring(page_count) + L")";
    const wchar_t* bm_ar[7] = {L"السور (١١٤ سورة)", pages_ar_lbl.c_str(), L"الأجزاء (٣٠ جزءاً)", L"الأحزاب (٦٠ حزباً)", L"الأرباع (٢٤٠ ربعاً)", L"المنازل السبعة (ختمة الأسبوع)", L"الركوعات (٥٥٨ ركوعاً)"};
    const wchar_t* bm_en[7] = {L"Surahs (114)", pages_en_lbl.c_str(), L"Juz (30)", L"Hizb (60)", L"Quarters (240)", L"Seven Manzils (weekly reading)", L"Rukus (558)"};
    const wchar_t* bm_keys[7] = {L"surah", L"page", L"juz", L"hizb", L"rub", L"manzil", L"ruku"};
    int bm_sel = 0;
    for (int i = 0; i < 7; ++i) {
        SendMessageW(st.combo_browse, CB_ADDSTRING, 0, (LPARAM)(is_en ? bm_en[i] : bm_ar[i]));
        if (settings.default_browse_mode == bm_keys[i]) bm_sel = i;
    }
    SendMessageW(st.combo_browse, CB_SETCURSEL, bm_sel, 0);

    add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Language:" : L"لغة الواجهة:", WS_CHILD | WS_VISIBLE, 32, 98, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_lang = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 94, 430, 200, dlg, (HMENU)603, hInst, nullptr));
    SendMessageW(st.combo_lang, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"Arabic" : L"العربية"));
    SendMessageW(st.combo_lang, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"English" : L"الإنجليزية"));
    SendMessageW(st.combo_lang, CB_SETCURSEL, (settings.ui_language == L"en") ? 1 : 0, 0);

    st.chk_page_sound = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Play a page-turn sound when moving with PageUp and PageDown" : L"تشغيل صوت تقليب الصفحة عند الانتقال بـ PageUp و PageDown", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 134, 640, 26, dlg, (HMENU)604, hInst, nullptr));
    SendMessageW(st.chk_page_sound, BM_SETCHECK, settings.page_turn_sound ? BST_CHECKED : BST_UNCHECKED, 0);

    st.chk_show_sidebar = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Show the side index in the main window (F9)" : L"إظهار قائمة الفهرس الجانبي في الواجهة الرئيسية (F9)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 164, 640, 26, dlg, (HMENU)605, hInst, nullptr));
    SendMessageW(st.chk_show_sidebar, BM_SETCHECK, settings.show_sidebar ? BST_CHECKED : BST_UNCHECKED, 0);

    add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Open program on:" : L"يفتح البرنامج على:", WS_CHILD | WS_VISIBLE, 32, 200, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_startup_focus = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 196, 430, 200, dlg, (HMENU)606, hInst, nullptr));
    const wchar_t* sf_ar[3] = {L"فهرس السور (القائمة الجانبية)", L"قائمة الآيات (موضع القراءة)", L"قائمة الفهرس الكامل (الوحدات)"};
    const wchar_t* sf_en[3] = {L"Surah index (side panel)", L"Ayahs list (reading view)", L"Full units index view"};
    const wchar_t* sf_keys[3] = {L"surah_index", L"ayahs", L"units"};
    int sf_sel = 0;
    for (int i = 0; i < 3; ++i) {
        SendMessageW(st.combo_startup_focus, CB_ADDSTRING, 0, (LPARAM)(is_en ? sf_en[i] : sf_ar[i]));
        if (settings.startup_focus == sf_keys[i]) sf_sel = i;
    }
    SendMessageW(st.combo_startup_focus, CB_SETCURSEL, sf_sel, 0);

    add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Copied script:" : L"نوع رسم الآية عند النسخ:", WS_CHILD | WS_VISIBLE, 32, 242, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_copy_script = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 238, 430, 200, dlg, (HMENU)610, hInst, nullptr));
    // النص القرآني يُنسخ مشكولاً دائماً
    const wchar_t* cs_ar[3] = {L"الرسم العثماني المشكول (مصحف المدينة)", L"رسم حفص (مجمع الملك فهد)", L"الرسم الإملائي المشكول"};
    const wchar_t* cs_en[3] = {L"Uthmani script with diacritics", L"King Fahd Complex Hafs script", L"Standard Arabic with diacritics"};
    const wchar_t* cs_keys[3] = {L"uthmani", L"qpc", L"imlaei"};
    int cs_sel = 0;
    for (int i = 0; i < 3; ++i) {
        SendMessageW(st.combo_copy_script, CB_ADDSTRING, 0, (LPARAM)(is_en ? cs_en[i] : cs_ar[i]));
        if (settings.copy_script == cs_keys[i]) cs_sel = i;
    }
    SendMessageW(st.combo_copy_script, CB_SETCURSEL, cs_sel, 0);

    st.chk_brackets = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Put the ayah between ornate brackets ﴾ ... ﴿" : L"إحاطة نص الآية بقوسين قرآنيين مزخرفين ﴿ ... ﴾", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 278, 640, 26, dlg, (HMENU)611, hInst, nullptr));
    SendMessageW(st.chk_brackets, BM_SETCHECK, settings.copy_brackets ? BST_CHECKED : BST_UNCHECKED, 0);

    st.chk_reference = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Add the surah name and ayah number after the text [surah: ayah]" : L"إضافة اسم السورة ورقم الآية بعد النص [السورة: الآية]", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 308, 640, 26, dlg, (HMENU)612, hInst, nullptr));
    SendMessageW(st.chk_reference, BM_SETCHECK, settings.copy_reference ? BST_CHECKED : BST_UNCHECKED, 0);

    st.chk_arabic_digits = add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Use Arabic-Indic digits (١٢٣) instead of (123)" : L"استخدام الأرقام العربية المشرقية (١٢٣) بدلاً من (123)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 338, 640, 26, dlg, (HMENU)613, hInst, nullptr));
    SendMessageW(st.chk_arabic_digits, BM_SETCHECK, settings.copy_arabic_digits ? BST_CHECKED : BST_UNCHECKED, 0);

    add_ctrl(st.tab0_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Copy preview:" : L"معاينة النسخ:", WS_CHILD | WS_VISIBLE, 32, 378, 120, 26, dlg, nullptr, hInst, nullptr));
    st.lbl_preview = add_ctrl(st.tab0_ctrls, CreateWindowExW(WS_EX_CLIENTEDGE | (is_en ? WS_EX_RTLREADING : 0), L"STATIC", L"", WS_CHILD | WS_VISIBLE | (is_en ? SS_RIGHT : SS_LEFT), 160, 374, 520, 64, dlg, nullptr, hInst, nullptr));

    // ---------------- Tab 1: الخط والمظهر ----------------
    // محاذاة السطور أول خيار في الصفحة، والتوسيط كالمصحف هو الافتراضي
    add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Line alignment:" : L"محاذاة السطور:", WS_CHILD, 32, 70, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_align = add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 66, 430, 200, dlg, (HMENU)623, hInst, nullptr));
    SendMessageW(st.combo_align, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"Centered like a Mushaf (default)" : L"توسيط السطور كالمصحف (الافتراضي)"));
    SendMessageW(st.combo_align, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"Right-aligned" : L"محاذاة لليمين"));
    SendMessageW(st.combo_align, CB_SETCURSEL, (settings.doc_alignment == L"right") ? 1 : 0, 0);

    add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Quran font:" : L"الخط القرآني:", WS_CHILD, 32, 118, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_font = add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 114, 430, 200, dlg, (HMENU)620, hInst, nullptr));
    const wchar_t* fn_ar[5] = {L"خط حفص العثماني — مجمع الملك فهد (طبعة المدينة)", L"الخط الأميري القرآني", L"خط أميري الكلاسيكي", L"خط شهرزاد القرآني الجديد", L"خط كتاب (نسخ المطابع)"};
    const wchar_t* fn_en[5] = {L"KFGQPC HAFS Uthmanic Script (Madinah)", L"Amiri Quran", L"Amiri Classic", L"Scheherazade New", L"Kitab (classic print Naskh)"};
    const wchar_t* fn_keys[5] = {L"KFGQPC HAFS Uthmanic Script", L"Amiri Quran", L"Amiri", L"Scheherazade New", L"Kitab"};
    int fn_sel = 0;
    for (int i = 0; i < 5; ++i) {
        SendMessageW(st.combo_font, CB_ADDSTRING, 0, (LPARAM)(is_en ? fn_en[i] : fn_ar[i]));
        if (settings.quran_font_family == fn_keys[i]) fn_sel = i;
    }
    SendMessageW(st.combo_font, CB_SETCURSEL, fn_sel, 0);

    add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Font size (16 - 52):" : L"حجم خط الآيات (16 - 52):", WS_CHILD, 32, 166, 210, 26, dlg, nullptr, hInst, nullptr));
    st.edit_font_size = add_ctrl(st.tab1_ctrls, CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(settings.quran_font_size).c_str(), WS_CHILD | WS_TABSTOP | ES_NUMBER, 250, 162, 120, 28, dlg, (HMENU)621, hInst, nullptr));

    add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Ayah display:" : L"طريقة عرض الآية:", WS_CHILD, 32, 214, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_display = add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 210, 430, 200, dlg, (HMENU)622, hInst, nullptr));
    SendMessageW(st.combo_display, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"Ayah text only" : L"نص الآية فقط"));
    SendMessageW(st.combo_display, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"Ayah text with its number" : L"نص الآية مع رقمها"));
    SendMessageW(st.combo_display, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"Ayah text with its number and surah name" : L"نص الآية مع رقمها واسم السورة"));
    int dp_sel = (settings.ayah_display == L"text") ? 0 : (settings.ayah_display == L"text_number_surah" ? 2 : 1);
    SendMessageW(st.combo_display, CB_SETCURSEL, dp_sel, 0);

    add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Colour theme:" : L"المظهر اللوني:", WS_CHILD, 32, 262, 210, 26, dlg, nullptr, hInst, nullptr));
    st.combo_theme = add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 250, 258, 430, 200, dlg, (HMENU)624, hInst, nullptr));
    const wchar_t* th_ar[4] = {L"كريمي المدينة (الافتراضي المريح للعين)", L"أخضر زمردي ملكي", L"وضع ليلي ذهبي هادئ", L"تباين عالٍ (أسود وأصفر لضعاف البصر)"};
    const wchar_t* th_en[4] = {L"Madinah Cream (default)", L"Royal Emerald", L"Night Gold", L"High Contrast (yellow on black)"};
    const wchar_t* th_keys[4] = {L"madinah_cream", L"royal_emerald", L"night_gold", L"high_contrast"};
    int th_sel = 0;
    for (int i = 0; i < 4; ++i) {
        SendMessageW(st.combo_theme, CB_ADDSTRING, 0, (LPARAM)(is_en ? th_en[i] : th_ar[i]));
        if (settings.theme == th_keys[i]) th_sel = i;
    }
    SendMessageW(st.combo_theme, CB_SETCURSEL, th_sel, 0);

    st.chk_letter_count = add_ctrl(st.tab1_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Show Arabic letter counter for each ayah (visual and screen reader)" : L"إظهار عداد الحروف لكل آية (في العرض وقارئ الشاشة)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 306, 640, 28, dlg, (HMENU)625, hInst, nullptr));
    SendMessageW(st.chk_letter_count, BM_SETCHECK, settings.show_ayah_letter_count ? BST_CHECKED : BST_UNCHECKED, 0);

    // ---------------- Tab 2: قارئات الشاشة ----------------
    st.chk_sr_auto_speak = add_ctrl(st.tab2_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Read the ayah automatically when you move to it with the arrows" : L"نطق الآية تلقائياً عبر قارئ الشاشة فور الوقوف عليها بالأسهم", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 70, 640, 28, dlg, (HMENU)630, hInst, nullptr));
    SendMessageW(st.chk_sr_auto_speak, BM_SETCHECK, settings.sr_auto_speak_ayah ? BST_CHECKED : BST_UNCHECKED, 0);

    st.chk_sr_phonetic = add_ctrl(st.tab2_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Improve the screen reader's pronunciation of Uthmani script words" : L"تحسين النطق الصوتي لكلمات الرسم العثماني (مثل: الرحمٰن، العٰلمين، ذٰلك) لقارئ الشاشة", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 110, 640, 28, dlg, (HMENU)631, hInst, nullptr));
    SendMessageW(st.chk_sr_phonetic, BM_SETCHECK, settings.sr_phonetic_norm ? BST_CHECKED : BST_UNCHECKED, 0);

    st.chk_sr_muqattaat = add_ctrl(st.tab2_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Spell out the disjointed letters at the start of surahs as letter names (such as الم and كهيعص)" : L"تفكيك الحروف المقطعة في فواتح السور إلى أسمائها ككلمات (مثل: الم ← ألف لام ميم، كهيعص)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 150, 640, 28, dlg, (HMENU)632, hInst, nullptr));
    SendMessageW(st.chk_sr_muqattaat, BM_SETCHECK, settings.sr_spell_muqattaat ? BST_CHECKED : BST_UNCHECKED, 0);

    st.chk_sr_waqf = add_ctrl(st.tab2_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Read the pause marks by their names as written in the Mushaf (such as صلى, قلى, ج)" : L"نطق علامات الوقف بأسمائها كما في المصحف (مثل: صلى، قلى، ج، لا)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 190, 640, 28, dlg, (HMENU)634, hInst, nullptr));
    SendMessageW(st.chk_sr_waqf, BM_SETCHECK, settings.sr_speak_waqf ? BST_CHECKED : BST_UNCHECKED, 0);

    st.chk_sapi = add_ctrl(st.tab2_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Use the built-in Windows voice when no screen reader is running" : L"تشغيل صوت ويندوز المدمج في حال عدم تشغيل قارئ شاشة خارجي", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 32, 230, 640, 28, dlg, (HMENU)633, hInst, nullptr));
    SendMessageW(st.chk_sapi, BM_SETCHECK, settings.sapi_fallback ? BST_CHECKED : BST_UNCHECKED, 0);

    // ---------------- Tab 3: تخصيص الاختصارات ----------------
    add_ctrl(st.tab3_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Select a command, then move to the shortcut box and press the new keys (Ctrl/Alt/F1-F12):" : L"اختر الأمر من القائمة، ثم انتقل لمربع الاختصار واضغط المفاتيح الجديدة (Ctrl / Alt / F1-F12):", WS_CHILD, 32, 56, 650, 24, dlg, nullptr, hInst, nullptr));
    st.list_shortcuts = add_ctrl(st.tab3_ctrls, CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, 32, 86, 650, 220, dlg, (HMENU)640, hInst, nullptr));

    add_ctrl(st.tab3_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"New shortcut (press keys):" : L"الاختصار الجديد (اضغط المفاتيح):", WS_CHILD, 32, 320, 210, 26, dlg, nullptr, hInst, nullptr));
    st.edit_shortcut_capture = add_ctrl(st.tab3_ctrls, CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_READONLY, 250, 316, 200, 28, dlg, (HMENU)641, hInst, nullptr));
    SetWindowLongPtrW(st.edit_shortcut_capture, GWLP_USERDATA, (LONG_PTR)&st);
    st.orig_capture_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(st.edit_shortcut_capture, GWLP_WNDPROC, (LONG_PTR)ShortcutCaptureProc));

    st.btn_sc_reset_one = add_ctrl(st.tab3_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Reset Selected" : L"استعادة الافتراضي للأمر", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 462, 315, 220, 30, dlg, (HMENU)642, hInst, nullptr));
    st.btn_sc_reset_all = add_ctrl(st.tab3_ctrls, CreateWindowExW(0, L"BUTTON", is_en ? L"Reset All Shortcuts" : L"استعادة جميع الاختصارات", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 462, 354, 220, 30, dlg, (HMENU)643, hInst, nullptr));
    st.lbl_shortcut_status = add_ctrl(st.tab3_ctrls, CreateWindowExW(0, L"STATIC", is_en ? L"Screen-reader safe validation is active." : L"التحقق التلقائي من توافق الاختصار مع قارئات الشاشة مفعّل.", WS_CHILD, 32, 358, 420, 48, dlg, nullptr, hInst, nullptr));
    refresh_options_shortcuts_list(&st, 0);

    update_options_copy_preview(&st);
    switch_options_tab(&st, 0);
    send_tab_to_back(st.tab_ctrl);

    HWND btn_save = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Save and apply" : L"حفظ الخيارات وتطبيقها",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        360, 510, 190, 34, dlg, (HMENU)IDOK, hInst, nullptr
    );
    HWND btn_cancel = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Cancel (Esc)" : L"إلغاء (Esc)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        562, 510, 150, 34, dlg, (HMENU)IDCANCEL, hInst, nullptr
    );
    SendMessageW(btn_save, WM_SETFONT, (WPARAM)ui_font, TRUE);
    SendMessageW(btn_cancel, WM_SETFONT, (WPARAM)ui_font, TRUE);

    // التنقل بـ Tab: عناصر الصفحة، ثم «حفظ» و«إلغاء»، ثم قائمة التبويبات بعدهما
    std::vector<HWND> tab_order;
    for (const auto* vec : {&st.tab0_ctrls, &st.tab1_ctrls, &st.tab2_ctrls, &st.tab3_ctrls}) {
        tab_order.insert(tab_order.end(), vec->begin(), vec->end());
    }
    tab_order.push_back(btn_save);
    tab_order.push_back(btn_cancel);
    tab_order.push_back(st.tab_ctrl);

    fit_dialog_to_screen(dlg, parent);   // مقاسات الشاشة الحالية، وداخل حدودها
    EnableWindow(parent, FALSE);
    // النافذة تفتح دائماً على التبويب الأول والتركيز عليه، فيعرف القارئ أقسامها قبل أن يدخل في أي منها
    SendMessageW(st.tab_ctrl, TCM_SETCURSEL, 0, 0);
    SetFocus(st.tab_ctrl);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (handle_tab_switch_keys(msg, dlg, st.tab_ctrl, [&](int t) { switch_options_tab(&st, t); })) continue;
        if (handle_custom_tab_order(msg, dlg, tab_order)) continue;
        if (handle_enter_on_focused_button(dlg, msg)) continue;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return st.accepted;
}

// ============================================================================
// نافذة الانتقال السريع (Ctrl+G)
// ============================================================================
struct GoToResult {
    bool accepted = false;
    std::wstring mode; // "ayah", "page", "juz", "hizb", "rub", "manzil"
    int surah = 1;
    int value = 1;
};

struct GoToDlgState {
    QuranDatabase* db = nullptr;
    bool is_en = false;
    int page_count = 604;
    GoToResult res;
    HWND tab_ctrl = nullptr;
    HWND combo_surah = nullptr;
    HWND edit_ayah = nullptr;
    HWND spin_ayah = nullptr;
    HWND edit_page = nullptr;
    HWND spin_page = nullptr;
    HWND edit_juz = nullptr;
    HWND spin_juz = nullptr;
    HWND edit_rub = nullptr;
    HWND spin_rub = nullptr;
    HWND edit_hizb = nullptr;
    HWND spin_hizb = nullptr;
    HWND edit_manzil = nullptr;
    HWND spin_manzil = nullptr;
    HWND edit_ruku = nullptr;
    HWND spin_ruku = nullptr;
    std::vector<HWND> tab_ctrls[4];
};

static void switch_goto_tab(GoToDlgState* st, int active_tab) {
    for (int t = 0; t < 4; ++t) {
        for (HWND h : st->tab_ctrls[t]) {
            ShowWindow(h, t == active_tab ? SW_SHOW : SW_HIDE);
        }
    }
}

static void accept_goto_action(HWND dlg, GoToDlgState* st, UINT cmd_id) {
    auto read_clamped = [](HWND edit, int min_v, int max_v) {
        wchar_t buf[64] = {0};
        GetWindowTextW(edit, buf, 63);
        int v = _wtoi(buf);
        if (v < min_v) v = min_v;
        if (v > max_v) v = max_v;
        return v;
    };

    if (cmd_id == 321) { // سورة وآية
        int s = std::max(1, (int)SendMessageW(st->combo_surah, CB_GETCURSEL, 0, 0) + 1);
        int max_a = st->db ? std::max(1, st->db->get_surah(s).verses_count) : 286;
        st->res.mode = L"ayah";
        st->res.surah = s;
        st->res.value = read_clamped(st->edit_ayah, 1, max_a);
        st->res.accepted = true;
        close_modal(dlg);
    } else if (cmd_id == 322) { // صفحة
        st->res.mode = L"page";
        st->res.value = read_clamped(st->edit_page, 1, st->page_count);
        st->res.accepted = true;
        close_modal(dlg);
    } else if (cmd_id == 323) { // جزء
        st->res.mode = L"juz";
        st->res.value = read_clamped(st->edit_juz, 1, 30);
        st->res.accepted = true;
        close_modal(dlg);
    } else if (cmd_id == 324) { // ربع
        st->res.mode = L"rub";
        st->res.value = read_clamped(st->edit_rub, 1, 240);
        st->res.accepted = true;
        close_modal(dlg);
    } else if (cmd_id == 325) { // حزب
        st->res.mode = L"hizb";
        st->res.value = read_clamped(st->edit_hizb, 1, 60);
        st->res.accepted = true;
        close_modal(dlg);
    } else if (cmd_id == 326) { // منزلة
        st->res.mode = L"manzil";
        st->res.value = read_clamped(st->edit_manzil, 1, 7);
        st->res.accepted = true;
        close_modal(dlg);
    } else if (cmd_id == 327) { // ركوع
        st->res.mode = L"ruku";
        st->res.value = read_clamped(st->edit_ruku, 1, 558);
        st->res.accepted = true;
        close_modal(dlg);
    }
}

static LRESULT CALLBACK GoToWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<GoToDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CTLCOLORSTATIC:
        return tab_page_ctlcolor((HDC)wParam);
    case WM_NOTIFY: {
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm && nm->idFrom == 300 && nm->code == TCN_SELCHANGE && st) {
            int sel = (int)SendMessageW(st->tab_ctrl, TCM_GETCURSEL, 0, 0);
            switch_goto_tab(st, sel);
            return 0;
        }
        break;
    }
    case WM_COMMAND: {
        UINT id = LOWORD(wParam);
        UINT code = HIWORD(wParam);
        if (id == 302 && code == CBN_SELCHANGE && st && st->db) {
            int s = std::max(1, (int)SendMessageW(st->combo_surah, CB_GETCURSEL, 0, 0) + 1);
            int max_a = std::max(1, st->db->get_surah(s).verses_count);
            SendMessageW(st->spin_ayah, UDM_SETRANGE32, 1, max_a);
            SendMessageW(st->spin_ayah, UDM_SETPOS32, 0, 1);
            SetWindowTextW(st->edit_ayah, L"1");
            return 0;
        }
        if (id >= 321 && id <= 327 && st) {
            accept_goto_action(hwnd, st, id);
            return 0;
        }
        if (id == IDOK && st) {
            HWND foc = GetFocus();
            if (foc == st->edit_page) accept_goto_action(hwnd, st, 322);
            else if (foc == st->edit_juz) accept_goto_action(hwnd, st, 323);
            else if (foc == st->edit_rub) accept_goto_action(hwnd, st, 324);
            else if (foc == st->edit_hizb) accept_goto_action(hwnd, st, 325);
            else if (foc == st->edit_manzil) accept_goto_action(hwnd, st, 326);
            else if (foc == st->edit_ruku) accept_goto_action(hwnd, st, 327);
            else {
                int t = (int)SendMessageW(st->tab_ctrl, TCM_GETCURSEL, 0, 0);
                if (t == 1) accept_goto_action(hwnd, st, 322);
                else if (t == 2) accept_goto_action(hwnd, st, 323);
                else if (t == 3) accept_goto_action(hwnd, st, 325);
                else accept_goto_action(hwnd, st, 321);
            }
            return 0;
        }
        if (id == IDCANCEL) {
            close_modal(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        close_modal(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline GoToResult show_goto_dialog(
    HWND parent,
    QuranDatabase& db,
    int cur_surah,
    int cur_ayah,
    int cur_page,
    int cur_juz,
    int cur_hizb,
    int cur_rub,
    int cur_manzil,
    int cur_ruku,
    int page_count,
    bool is_en,
    HFONT ui_font
) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = GoToWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"MushafGoToDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    GoToDlgState st;
    st.db = &db;
    st.is_en = is_en;
    st.page_count = std::max(1, page_count);
    st.res.surah = cur_surah;
    st.res.value = cur_ayah;

    DWORD ex_style = WS_EX_DLGMODALFRAME | (is_en ? 0 : (WS_EX_LAYOUTRTL | WS_EX_RTLREADING));
    HWND dlg = CreateWindowExW(
        ex_style, L"MushafGoToDlg",
        is_en ? L"Go to (Ctrl+G)" : L"انتقال سريع في المصحف الشريف (Ctrl+G)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        220, 140, 560, 355, parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    st.tab_ctrl = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        14, 14, 518, 248, dlg, (HMENU)300, hInst, nullptr
    );
    SendMessageW(st.tab_ctrl, WM_SETFONT, (WPARAM)ui_font, TRUE);

    std::wstring tab1_lbl = is_en
        ? (L"Page (1-" + std::to_wstring(st.page_count) + L")")
        : (L"بالصفحة (١-" + to_arabic_digits(st.page_count) + L")");

    TCITEMW tie{};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<wchar_t*>(is_en ? L"Surah and ayah" : L"بالسورة والآية");
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 0, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(tab1_lbl.c_str());
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 1, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(is_en ? L"Juz or quarter" : L"بالجزء أو الربع");
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 2, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(is_en ? L"Hizb, manzil or ruku" : L"بالحزب أو المنزلة أو الركوع");
    SendMessageW(st.tab_ctrl, TCM_INSERTITEMW, 3, (LPARAM)&tie);

    auto add_c = [&](int tab, HWND h) {
        SendMessageW(h, WM_SETFONT, (WPARAM)ui_font, TRUE);
        st.tab_ctrls[tab].push_back(h);
        return h;
    };

    auto make_spin = [&](int tab, int x, int y, int w, int min_v, int max_v, int init_v, int edit_id, HWND& out_edit, HWND& out_spin) {
        int clamped = std::max(min_v, std::min(max_v, init_v));
        out_edit = add_c(tab, CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(clamped).c_str(),
            WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
            x, y, w, 28, dlg, (HMENU)(INT_PTR)edit_id, hInst, nullptr
        ));
        out_spin = add_c(tab, CreateWindowExW(
            0, UPDOWN_CLASSW, L"",
            WS_CHILD | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
            0, 0, 0, 0, dlg, nullptr, hInst, nullptr
        ));
        SendMessageW(out_spin, UDM_SETBUDDY, (WPARAM)out_edit, 0);
        SendMessageW(out_spin, UDM_SETRANGE32, min_v, max_v);
        SendMessageW(out_spin, UDM_SETPOS32, 0, clamped);
    };

    // --- Tab 0: بالسورة والآية ---
    add_c(0, CreateWindowExW(0, L"STATIC", is_en ? L"Surah:" : L"السورة:", WS_CHILD, 32, 62, 140, 26, dlg, nullptr, hInst, nullptr));
    st.combo_surah = add_c(0, CreateWindowExW(
        0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        180, 58, 325, 300, dlg, (HMENU)302, hInst, nullptr
    ));
    for (const auto& s : db.get_all_surahs()) {
        std::wstring lbl = std::to_wstring(s.number) + L". " + (is_en ? s.name_arabic : (L"سورة " + s.name_arabic));
        SendMessageW(st.combo_surah, CB_ADDSTRING, 0, (LPARAM)lbl.c_str());
    }
    SendMessageW(st.combo_surah, CB_SETCURSEL, std::max(0, cur_surah - 1), 0);

    int max_ayah = std::max(1, db.get_surah(cur_surah).verses_count);
    add_c(0, CreateWindowExW(0, L"STATIC", is_en ? L"Ayah number:" : L"رقم الآية:", WS_CHILD, 32, 104, 140, 26, dlg, nullptr, hInst, nullptr));
    make_spin(0, 180, 100, 140, 1, max_ayah, cur_ayah, 311, st.edit_ayah, st.spin_ayah);
    add_c(0, CreateWindowExW(0, L"BUTTON", is_en ? L"Go to surah and ayah" : L"انتقال إلى السورة والآية", WS_CHILD | WS_TABSTOP | BS_DEFPUSHBUTTON, 180, 148, 220, 34, dlg, (HMENU)321, hInst, nullptr));

    // --- Tab 1: بالصفحة ---
    std::wstring pg_lbl = is_en
        ? (L"Page number (1 - " + std::to_wstring(st.page_count) + L"):")
        : (L"رقم الصفحة (١ - " + to_arabic_digits(st.page_count) + L"):");
    add_c(1, CreateWindowExW(0, L"STATIC", pg_lbl.c_str(), WS_CHILD, 32, 74, 190, 26, dlg, nullptr, hInst, nullptr));
    make_spin(1, 230, 70, 140, 1, st.page_count, cur_page, 312, st.edit_page, st.spin_page);
    add_c(1, CreateWindowExW(0, L"BUTTON", is_en ? L"Go to page" : L"انتقال إلى الصفحة", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 230, 120, 200, 34, dlg, (HMENU)322, hInst, nullptr));

    // --- Tab 2: بالجزء أو الربع ---
    add_c(2, CreateWindowExW(0, L"STATIC", is_en ? L"Juz (1 - 30):" : L"رقم الجزء (١ - ٣٠):", WS_CHILD, 32, 66, 175, 26, dlg, nullptr, hInst, nullptr));
    make_spin(2, 215, 62, 110, 1, 30, cur_juz, 313, st.edit_juz, st.spin_juz);
    add_c(2, CreateWindowExW(0, L"BUTTON", is_en ? L"Go to juz" : L"انتقال إلى الجزء", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 340, 60, 165, 32, dlg, (HMENU)323, hInst, nullptr));

    add_c(2, CreateWindowExW(0, L"STATIC", is_en ? L"Quarter (1 - 240):" : L"رقم الربع (١ - ٢٤٠):", WS_CHILD, 32, 122, 175, 26, dlg, nullptr, hInst, nullptr));
    make_spin(2, 215, 118, 110, 1, 240, cur_rub, 314, st.edit_rub, st.spin_rub);
    add_c(2, CreateWindowExW(0, L"BUTTON", is_en ? L"Go to quarter" : L"انتقال إلى الربع", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 340, 116, 165, 32, dlg, (HMENU)324, hInst, nullptr));

    // --- Tab 3: بالحزب أو المنزلة ---
    add_c(3, CreateWindowExW(0, L"STATIC", is_en ? L"Hizb (1 - 60):" : L"رقم الحزب (١ - ٦٠):", WS_CHILD, 32, 66, 175, 26, dlg, nullptr, hInst, nullptr));
    make_spin(3, 215, 62, 110, 1, 60, cur_hizb, 315, st.edit_hizb, st.spin_hizb);
    add_c(3, CreateWindowExW(0, L"BUTTON", is_en ? L"Go to hizb" : L"انتقال إلى الحزب", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 340, 60, 165, 32, dlg, (HMENU)325, hInst, nullptr));

    add_c(3, CreateWindowExW(0, L"STATIC", is_en ? L"Manzil (1 - 7):" : L"رقم المنزلة (١ - ٧):", WS_CHILD, 32, 122, 175, 26, dlg, nullptr, hInst, nullptr));
    make_spin(3, 215, 118, 110, 1, 7, cur_manzil, 316, st.edit_manzil, st.spin_manzil);
    add_c(3, CreateWindowExW(0, L"BUTTON", is_en ? L"Go to manzil" : L"انتقال إلى المنزلة", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 340, 116, 165, 32, dlg, (HMENU)326, hInst, nullptr));

    add_c(3, CreateWindowExW(0, L"STATIC", is_en ? L"Ruku (1 - 558):" : L"رقم الركوع (١ - ٥٥٨):", WS_CHILD, 32, 178, 175, 26, dlg, nullptr, hInst, nullptr));
    make_spin(3, 215, 174, 110, 1, 558, cur_ruku, 317, st.edit_ruku, st.spin_ruku);
    add_c(3, CreateWindowExW(0, L"BUTTON", is_en ? L"Go to ruku" : L"انتقال إلى الركوع", WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, 340, 172, 165, 32, dlg, (HMENU)327, hInst, nullptr));

    switch_goto_tab(&st, 0);
    send_tab_to_back(st.tab_ctrl);

    HWND btn_cancel = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Close (Esc)" : L"إغلاق (Esc)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        402, 272, 130, 34, dlg, (HMENU)IDCANCEL, hInst, nullptr
    );
    SendMessageW(btn_cancel, WM_SETFONT, (WPARAM)ui_font, TRUE);

    std::vector<HWND> tab_order;
    // كنافذة الخيارات: عناصر الصفحة، ثم «إغلاق»، ثم التبويبات؛ والنافذة تفتح على التبويب الأول
    for (int t = 0; t < 4; ++t) tab_order.insert(tab_order.end(), st.tab_ctrls[t].begin(), st.tab_ctrls[t].end());
    tab_order.push_back(btn_cancel);
    tab_order.push_back(st.tab_ctrl);

    fit_dialog_to_screen(dlg, parent);   // مقاسات الشاشة الحالية، وداخل حدودها
    EnableWindow(parent, FALSE);
    SetFocus(st.tab_ctrl);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (handle_tab_switch_keys(msg, dlg, st.tab_ctrl, [&](int t) { switch_goto_tab(&st, t); })) continue;
        if (handle_custom_tab_order(msg, dlg, tab_order)) continue;
        if (handle_enter_on_focused_button(dlg, msg)) continue;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            HWND foc = GetFocus();
            if (foc == st.edit_ayah || foc == st.edit_page || foc == st.edit_juz ||
                foc == st.edit_rub || foc == st.edit_hizb || foc == st.edit_manzil || foc == st.edit_ruku) {
                SendMessageW(dlg, WM_COMMAND, IDOK, 0);
                continue;
            }
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return st.res;
}

// ============================================================================
// نافذة البحث الشامل في آيات المصحف الشريف (SearchDialog — Ctrl+F)
// ============================================================================
struct SearchDlgResult {
    bool jump = false;
    AyahInfo target;
};

struct SearchDlgState {
    QuranDatabase* db = nullptr;
    AppSettings* settings = nullptr;
    ScreenReaderEngine* sr = nullptr;
    bool is_en = false;
    int cur_surah = 1;
    HFONT font = nullptr;
    HWND edit_query = nullptr;
    HWND combo_surah_filter = nullptr;
    HWND lbl_status = nullptr;
    HWND list_results = nullptr;
    std::vector<AyahInfo> results;
    SearchDlgResult out;
};

static WNDPROC g_orig_search_list_proc = nullptr;

static LRESULT CALLBACK SearchListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<SearchDlgState*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));
    if (msg == WM_KEYDOWN && st) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if ((wParam == VK_RETURN && !ctrl) || (wParam == 'C' && !ctrl)) {
            SendMessageW(GetParent(hwnd), WM_COMMAND, 403, 0); // نسخ النتيجة
            return 0;
        }
        if ((wParam == 'G' && ctrl) || (wParam == VK_RETURN && ctrl)) {
            SendMessageW(GetParent(hwnd), WM_COMMAND, 404, 0); // الانتقال للآية
            return 0;
        }
    }
    return CallWindowProcW(g_orig_search_list_proc, hwnd, msg, wParam, lParam);
}

static void run_search_now(HWND /*hwnd*/, SearchDlgState* st, bool keep_focus_and_silent = false) {
    wchar_t qbuf[256] = {0};
    GetWindowTextW(st->edit_query, qbuf, 255);
    std::wstring query(qbuf);
    if (keep_focus_and_silent && query.empty()) return;
    if (query.find_first_not_of(L" \t") == std::wstring::npos) {
        std::wstring hint = st->is_en ? L"Type a word or a reference (such as 2:255) first, then press Enter."
                                      : L"اكتب كلمة البحث أو المرجع أولاً (مثل ٢:٢٥٥) ثم اضغط Enter.";
        SetWindowTextW(st->lbl_status, hint.c_str());
        if (st->sr) st->sr->speak(hint, true);
        SetFocus(st->edit_query);
        return;
    }
    int filter_sel = (int)SendMessageW(st->combo_surah_filter, CB_GETCURSEL, 0, 0);
    int surah_filter = (filter_sel > 0) ? filter_sel : 0;

    st->results = st->db->search_ayahs(query, surah_filter, 250);
    SendMessageW(st->list_results, WM_SETREDRAW, FALSE, 0);
    SendMessageW(st->list_results, LB_RESETCONTENT, 0, 0);

    for (const auto& a : st->results) {
        std::wstring s_name = a.surah_name;
        std::wstring item = st->is_en
            ? (L"﴿ " + a.text_uthmani + L" ﴾ — [Surah " + s_name + L": Ayah " + std::to_wstring(a.ayah_number) +
               L" • Page " + std::to_wstring(a.page_number) + L" • Juz " + std::to_wstring(a.juz_number) +
               L" • Quarter " + std::to_wstring(a.rub_el_hizb_number) + L"]")
            : (L"﴿ " + a.text_uthmani + L" ﴾ — [سورة " + s_name + L": الآية " + to_arabic_digits(a.ayah_number) +
               L" • الصفحة " + to_arabic_digits(a.page_number) + L" • الجزء " + to_arabic_digits(a.juz_number) +
               L" • الربع " + to_arabic_digits(a.rub_el_hizb_number) + L"]");
        SendMessageW(st->list_results, LB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
    SendMessageW(st->list_results, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(st->list_results, nullptr, TRUE);

    const size_t n = st->results.size();
    std::wstring status_txt;
    if (n == 0) {
        status_txt = st->is_en ? L"No matching ayahs. Try fewer words or another spelling."
                               : L"لم يُعثر على آيات مطابقة. جرّب كلمات أقل أو كتابة أخرى للكلمة.";
    } else if (n >= 250) {
        status_txt = st->is_en ? L"Showing the first 250 matching ayahs — choose a surah under Within to see the rest."
                               : L"عُرضت أول ٢٥٠ آية مطابقة — حدّد سورة من «نطاق البحث» لترى البقية.";
    } else {
        status_txt = st->is_en
            ? (L"Found " + std::to_wstring(n) + L" matching ayahs — press the down arrow to browse them.")
            : (L"تم العثور على " + to_arabic_digits((int)n) + L" آية مطابقة — اضغط السهم للأسفل لتصفح النتائج.");
    }
    SetWindowTextW(st->lbl_status, status_txt.c_str());
    if (st->sr && !keep_focus_and_silent) st->sr->speak(status_txt, true);

    if (!st->results.empty()) {
        SendMessageW(st->list_results, LB_SETCURSEL, 0, 0);
        if (!keep_focus_and_silent) SetFocus(st->list_results);
    }
}

static LRESULT CALLBACK SearchWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<SearchDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_MEASUREITEM:
    case WM_DRAWITEM:
        if (st && handle_wrapped_list_msg(hwnd, msg, lParam, 405, st->font, !st->is_en)) return TRUE;
        break;
    case WM_COMMAND: {
        UINT id = LOWORD(wParam);
        UINT code = HIWORD(wParam);
        if ((id == IDOK || id == 402 || (id == 406 && code == CBN_SELCHANGE)) && st) {
            bool from_combo = (id == 406 && code == CBN_SELCHANGE);
            run_search_now(hwnd, st, from_combo);
            return 0;
        }
        if (id == 403 && st) { // نسخ الآية المحددة
            int sel = (int)SendMessageW(st->list_results, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->results.size()) {
                const auto& a = st->results[sel];
                std::wstring formatted = format_ayah_custom(
                    a, st->settings->copy_script, st->settings->copy_brackets,
                    st->settings->copy_reference, st->settings->copy_arabic_digits
                );
                set_clipboard_text(hwnd, formatted);
                if (st->sr) {
                    st->sr->speak(st->is_en ? L"Copied selected ayah" : L"تم نسخ الآية المحددة إلى الحافظة", true);
                }
            }
            return 0;
        }
        if ((id == 404 || (id == 405 && code == LBN_DBLCLK)) && st) { // الانتقال للآية
            int sel = (int)SendMessageW(st->list_results, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->results.size()) {
                st->out.jump = true;
                st->out.target = st->results[sel];
                close_modal(hwnd);
            }
            return 0;
        }
        if (id == IDCANCEL) {
            close_modal(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        close_modal(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline SearchDlgResult show_search_dialog(
    HWND parent,
    QuranDatabase& db,
    AppSettings& settings,
    int cur_surah,
    bool is_en,
    HFONT ui_font,
    ScreenReaderEngine* sr
) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SearchWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"MushafSearchDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    SearchDlgState st;
    st.db = &db;
    st.settings = &settings;
    st.sr = sr;
    st.is_en = is_en;
    st.cur_surah = cur_surah;
    st.font = ui_font;

    DWORD ex_style = WS_EX_DLGMODALFRAME | (is_en ? 0 : (WS_EX_LAYOUTRTL | WS_EX_RTLREADING));
    HWND dlg = CreateWindowExW(
        ex_style, L"MushafSearchDlg",
        is_en ? L"Search the Quran (Ctrl+F)" : L"البحث الشامل في آيات المصحف الشريف (Ctrl+F)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        140, 90, 860, 580, parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    HWND lbl_q = CreateWindowExW(
        0, L"STATIC", is_en ? L"&Search text or reference:" : L"&كلمة البحث أو المرجع:",
        WS_CHILD | WS_VISIBLE, 16, 18, 150, 26, dlg, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    st.edit_query = CreateWindowExW(
        WS_EX_CLIENTEDGE /* النافذة المعكوسة تقرأ من اليمين أصلاً */, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        170, 14, 250, 30, dlg, (HMENU)401, GetModuleHandleW(nullptr), nullptr
    );
    HWND lbl_within = CreateWindowExW(
        0, L"STATIC", is_en ? L"&Within:" : L"&نطاق البحث:",
        WS_CHILD | WS_VISIBLE, 428, 18, 84, 26, dlg, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    st.combo_surah_filter = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
        516, 14, 216, 320, dlg, (HMENU)406, GetModuleHandleW(nullptr), nullptr
    );
    SendMessageW(st.combo_surah_filter, CB_ADDSTRING, 0, (LPARAM)(is_en ? L"The whole Quran (114 surahs)" : L"القرآن كله (١١٤ سورة)"));
    for (const auto& s : db.get_all_surahs()) {
        std::wstring lbl = std::to_wstring(s.number) + L". " + (is_en ? s.name_arabic : (L"سورة " + s.name_arabic));
        SendMessageW(st.combo_surah_filter, CB_ADDSTRING, 0, (LPARAM)lbl.c_str());
    }
    SendMessageW(st.combo_surah_filter, CB_SETCURSEL, 0, 0);

    HWND btn_search = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Search" : L"بحث",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        740, 14, 90, 30, dlg, (HMENU)402, GetModuleHandleW(nullptr), nullptr
    );
    st.lbl_status = CreateWindowExW(
        0, L"STATIC", is_en ? L"Type your search and press Enter to list matching ayahs." : L"اكتب عبارة البحث ثم اضغط Enter لعرض الآيات المطابقة.",
        WS_CHILD | WS_VISIBLE, 16, 54, 814, 26, dlg, nullptr, GetModuleHandleW(nullptr), nullptr
    );

    st.list_results = CreateWindowExW(
        WS_EX_CLIENTEDGE /* النافذة المعكوسة تقرأ من اليمين أصلاً */, L"LISTBOX", L"",
        kWrappedListStyle,   // النتيجة الطويلة تلتفّ بدل أن تنقطع
        16, 86, 814, 396, dlg, (HMENU)405, GetModuleHandleW(nullptr), nullptr
    );
    g_orig_search_list_proc = (WNDPROC)SetWindowLongPtrW(st.list_results, GWLP_WNDPROC, (LONG_PTR)SearchListSubclassProc);

    HWND btn_copy = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Copy selected ayah (Enter / C)" : L"نسخ الآية المحددة (Enter / C)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        16, 494, 240, 34, dlg, (HMENU)403, GetModuleHandleW(nullptr), nullptr
    );
    HWND btn_goto = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Open the ayah in the Mushaf" : L"فتح الآية والوقوف عليها في المصحف",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        268, 494, 270, 34, dlg, (HMENU)404, GetModuleHandleW(nullptr), nullptr
    );
    HWND btn_close = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Close (Esc)" : L"إغلاق (Esc)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        680, 494, 150, 34, dlg, (HMENU)IDCANCEL, GetModuleHandleW(nullptr), nullptr
    );

    for (HWND h : {lbl_q, st.edit_query, lbl_within, st.combo_surah_filter, btn_search, st.lbl_status, st.list_results, btn_copy, btn_goto, btn_close}) {
        SendMessageW(h, WM_SETFONT, (WPARAM)ui_font, TRUE);
    }

    fit_dialog_to_screen(dlg, parent);   // مقاسات الشاشة الحالية، وداخل حدودها
    EnableWindow(parent, FALSE);
    SetFocus(st.edit_query);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (handle_enter_on_focused_button(dlg, msg)) continue;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return st.out;
}

// ============================================================================
// نافذة العلامات المرجعية وفاصل الختمة (BookmarksDialog — Ctrl+B) المطابقة لنسخة بايثون
// ============================================================================
struct BookmarksDlgState {
    QuranDatabase* db = nullptr;
    AppSettings* settings = nullptr;
    const AyahInfo* current_ayah = nullptr;
    ScreenReaderEngine* sr = nullptr;
    bool is_en = false;
    HFONT font = nullptr;
    HWND lbl_khatmah_info = nullptr;
    HWND btn_resume_khatmah = nullptr;
    HWND list_hwnd = nullptr;
    std::vector<AyahInfo> bm_ayahs;
    SearchDlgResult out;
};

static void refresh_bookmarks_dialog_ui(BookmarksDlgState* st) {
    // 1. تحديث قسم فاصل الختمة
    AyahInfo ka;
    bool has_khatmah = (!st->settings->khatmah_verse_key.empty() && st->db->get_ayah_by_key(st->settings->khatmah_verse_key, ka));
    if (!has_khatmah) {
        SetWindowTextW(st->lbl_khatmah_info, st->is_en
            ? L"No reading mark set yet. Press M or Ctrl+M while reading to mark the ayah where you stopped."
            : L"لم يتم تحديد فاصل الختمة بعد. اضغط M أو Ctrl+M أثناء القراءة لوضع علامة التوقف عند الآية التي وصلت إليها.");
        EnableWindow(st->btn_resume_khatmah, FALSE);
    } else {
        int pct = (int)((ka.id * 100.0) / std::max(1, st->db->total_ayahs()) + 0.5);
        std::wstring s_name = ka.surah_name;
        std::wstring ktxt = st->is_en
            ? (L"Current stop position: Surah " + s_name + L", ayah " + std::to_wstring(ka.ayah_number) +
               L" — page " + std::to_wstring(ka.page_number) + L", juz " + std::to_wstring(ka.juz_number) +
               L", hizb " + std::to_wstring(ka.hizb_number) + L", quarter " + std::to_wstring(ka.rub_el_hizb_number) +
               L", manzil " + std::to_wstring(ka.manzil_number) + L" (" + std::to_wstring(pct) + L"% of the Mushaf)")
            : (L"موضع التوقف الحالي: سورة " + s_name + L"، الآية " + std::to_wstring(ka.ayah_number) +
               L" — الصفحة " + std::to_wstring(ka.page_number) + L"، الجزء " + std::to_wstring(ka.juz_number) +
               L"، الحزب " + std::to_wstring(ka.hizb_number) + L"، الربع " + std::to_wstring(ka.rub_el_hizb_number) +
               L"، المنزلة " + std::to_wstring(ka.manzil_number) + L" (أتممت " + std::to_wstring(pct) + L"٪ من المصحف)");
        SetWindowTextW(st->lbl_khatmah_info, ktxt.c_str());
        EnableWindow(st->btn_resume_khatmah, TRUE);
    }

    // 2. تحديث قائمة المفضلة (مع الإبقاء على موضع التحديد قدر الإمكان)
    int keep_sel = (int)SendMessageW(st->list_hwnd, LB_GETCURSEL, 0, 0);
    st->bm_ayahs.clear();
    SendMessageW(st->list_hwnd, LB_RESETCONTENT, 0, 0);
    for (const auto& vk : st->settings->bookmarks) {
        AyahInfo ba;
        if (st->db->get_ayah_by_key(vk, ba)) {
            std::wstring s_name = ba.surah_name;
            std::wstring lbl = st->is_en
                ? (L"Surah " + s_name + L", ayah " + std::to_wstring(ba.ayah_number) +
                   L" — Page " + std::to_wstring(ba.page_number) + L", Juz " + std::to_wstring(ba.juz_number) +
                   L", Hizb " + std::to_wstring(ba.hizb_number) + L"  |  ﴿ " + ba.text_uthmani + L" ﴾")
                : (L"سورة " + s_name + L"، الآية " + std::to_wstring(ba.ayah_number) +
                   L" — الصفحة " + std::to_wstring(ba.page_number) + L"، الجزء " + std::to_wstring(ba.juz_number) +
                   L"، الحزب " + std::to_wstring(ba.hizb_number) + L"  |  ﴿ " + ba.text_uthmani + L" ﴾");
            st->bm_ayahs.push_back(ba);
            SendMessageW(st->list_hwnd, LB_ADDSTRING, 0, (LPARAM)lbl.c_str());
        }
    }
    if (st->bm_ayahs.empty()) {
        // سطر توضيحي حتى لا يجد مستخدم قارئ الشاشة قائمة صامتة
        SendMessageW(st->list_hwnd, LB_ADDSTRING, 0, (LPARAM)(st->is_en
            ? L"No bookmarks yet. While reading, press B on any ayah to save it here."
            : L"لا توجد علامات محفوظة بعد. أثناء القراءة اضغط حرف B على أي آية لحفظها هنا."));
        SendMessageW(st->list_hwnd, LB_SETCURSEL, 0, 0);
    } else {
        int sel = std::max(0, std::min(keep_sel, (int)st->bm_ayahs.size() - 1));
        SendMessageW(st->list_hwnd, LB_SETCURSEL, sel, 0);
    }
}

static WNDPROC g_orig_bm_list_proc = nullptr;

// مفاتيح قائمة المفضلة كما تعد أزرارها: Delete للحذف، و Ctrl+C للنسخ
static LRESULT CALLBACK BookmarksListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (wParam == VK_DELETE) {
            SendMessageW(GetParent(hwnd), WM_COMMAND, 502, 0);
            return 0;
        }
        if (wParam == 'C' && ctrl) {
            SendMessageW(GetParent(hwnd), WM_COMMAND, 503, 0);
            return 0;
        }
    }
    return CallWindowProcW(g_orig_bm_list_proc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK BookmarksWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<BookmarksDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_MEASUREITEM:
    case WM_DRAWITEM:
        if (st && handle_wrapped_list_msg(hwnd, msg, lParam, 500, st->font, !st->is_en)) return TRUE;
        break;
    case WM_COMMAND: {
        UINT id = LOWORD(wParam);
        UINT code = HIWORD(wParam);
        if (id == 510 && st) { // متابعة القراءة من موضع التوقف
            AyahInfo ka;
            if (!st->settings->khatmah_verse_key.empty() && st->db->get_ayah_by_key(st->settings->khatmah_verse_key, ka)) {
                st->out.jump = true;
                st->out.target = ka;
                close_modal(hwnd);
            }
            return 0;
        }
        if (id == 511 && st && st->current_ayah) { // وضع فاصل الختمة عند الآية الحالية
            st->settings->khatmah_verse_key = st->current_ayah->verse_key;
            st->settings->save();
            refresh_bookmarks_dialog_ui(st);
            return 0;
        }
        if ((id == IDOK || id == 501 || (id == 500 && code == LBN_DBLCLK)) && st) { // الانتقال إلى العلامة
            int sel = (int)SendMessageW(st->list_hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->bm_ayahs.size()) {
                st->out.jump = true;
                st->out.target = st->bm_ayahs[sel];
                close_modal(hwnd);
            }
            return 0;
        }
        if (id == 503 && st) { // نسخ الآية
            int sel = (int)SendMessageW(st->list_hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->bm_ayahs.size()) {
                std::wstring formatted = format_ayah_custom(
                    st->bm_ayahs[sel], st->settings->copy_script, st->settings->copy_brackets,
                    st->settings->copy_reference, st->settings->copy_arabic_digits
                );
                set_clipboard_text(hwnd, formatted);
                if (st->sr) st->sr->speak(st->is_en ? L"Copied bookmarked ayah" : L"تم نسخ الآية إلى الحافظة", true);
            }
            return 0;
        }
        if (id == 502 && st) { // حذف العلامة المحددة
            int sel = (int)SendMessageW(st->list_hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->bm_ayahs.size()) {
                auto it = std::find(st->settings->bookmarks.begin(), st->settings->bookmarks.end(), st->bm_ayahs[sel].verse_key);
                if (it != st->settings->bookmarks.end()) st->settings->bookmarks.erase(it);
                st->settings->save();
                refresh_bookmarks_dialog_ui(st);
                if (st->sr) st->sr->speak(st->is_en ? L"Bookmark removed" : L"تم حذف العلامة", true);
            }
            return 0;
        }
        if (id == IDCANCEL) {
            close_modal(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        close_modal(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline SearchDlgResult show_bookmarks_dialog(
    HWND parent,
    QuranDatabase& db,
    AppSettings& settings,
    const AyahInfo* current_ayah,
    bool is_en,
    HFONT ui_font,
    ScreenReaderEngine* sr
) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = BookmarksWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"MushafBookmarksDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    BookmarksDlgState st;
    st.db = &db;
    st.settings = &settings;
    st.current_ayah = current_ayah;
    st.sr = sr;
    st.is_en = is_en;
    st.font = ui_font;

    DWORD ex_style = WS_EX_DLGMODALFRAME | (is_en ? 0 : (WS_EX_LAYOUTRTL | WS_EX_RTLREADING));
    HWND dlg = CreateWindowExW(
        ex_style, L"MushafBookmarksDlg",
        is_en ? L"Bookmarks and reading mark (Ctrl+B)" : L"العلامات المرجعية وفاصل الختمة (Ctrl+B)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        150, 95, 780, 560, parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    HINSTANCE hInst = GetModuleHandleW(nullptr);

    // 1. قسم فاصل الختمة
    HWND grp_k = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Reading mark (Khatmah stop position)" : L"فاصل الختمة (موضع التوقف في القراءة)",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 16, 12, 738, 134, dlg, nullptr, hInst, nullptr
    );
    st.lbl_khatmah_info = CreateWindowExW(
        0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 30, 38, 708, 48, dlg, nullptr, hInst, nullptr
    );
    st.btn_resume_khatmah = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Resume reading from stop mark (Alt+M)" : L"متابعة القراءة من موضع التوقف (Alt+M)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 30, 92, 320, 34, dlg, (HMENU)510, hInst, nullptr
    );
    HWND btn_set_khatmah = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Set reading mark at current ayah (M)" : L"وضع فاصل الختمة عند الآية الحالية (M)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 364, 92, 320, 34, dlg, (HMENU)511, hInst, nullptr
    );
    EnableWindow(btn_set_khatmah, current_ayah != nullptr);

    // 2. قسم العلامات المرجعية المحفوظة
    HWND grp_bm = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Saved bookmarks (press B on an ayah while reading)" : L"العلامات المرجعية المحفوظة (احفظ الآية بحرف B أثناء القراءة)",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 16, 158, 738, 354, dlg, nullptr, hInst, nullptr
    );
    st.list_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE /* النافذة المعكوسة تقرأ من اليمين أصلاً */, L"LISTBOX", L"",
        kWrappedListStyle,
        30, 186, 708, 266, dlg, (HMENU)500, hInst, nullptr
    );
    g_orig_bm_list_proc = (WNDPROC)SetWindowLongPtrW(st.list_hwnd, GWLP_WNDPROC, (LONG_PTR)BookmarksListSubclassProc);

    HWND btn_go = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Go to bookmark (Enter)" : L"الانتقال إلى العلامة (Enter)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        30, 462, 200, 34, dlg, (HMENU)501, hInst, nullptr
    );
    HWND btn_copy = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Copy ayah (Ctrl+C)" : L"نسخ الآية (Ctrl+C)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        240, 462, 165, 34, dlg, (HMENU)503, hInst, nullptr
    );
    HWND btn_del = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Remove bookmark (Delete)" : L"حذف العلامة (Delete)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        415, 462, 180, 34, dlg, (HMENU)502, hInst, nullptr
    );
    HWND btn_close = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Close (Esc)" : L"إغلاق (Esc)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        608, 462, 130, 34, dlg, (HMENU)IDCANCEL, hInst, nullptr
    );

    for (HWND h : {grp_k, st.lbl_khatmah_info, st.btn_resume_khatmah, btn_set_khatmah, grp_bm, st.list_hwnd, btn_go, btn_copy, btn_del, btn_close}) {
        SendMessageW(h, WM_SETFONT, (WPARAM)ui_font, TRUE);
    }

    refresh_bookmarks_dialog_ui(&st);

    fit_dialog_to_screen(dlg, parent);   // مقاسات الشاشة الحالية، وداخل حدودها
    EnableWindow(parent, FALSE);
    SetFocus(st.list_hwnd);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (handle_enter_on_focused_button(dlg, msg)) continue;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return st.out;
}

// ============================================================================
// نافذة التنقل بين مواضع سجدات التلاوة الـ 15
// ============================================================================
struct SajdahNavState {
    bool is_en = false;
    HFONT font = nullptr;
    bool accepted = false;
    std::wstring selected_verse_key;
    HWND list_hwnd = nullptr;
    std::vector<std::wstring> verse_keys;
};

static LRESULT CALLBACK SajdahNavWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<SajdahNavState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_MEASUREITEM:
    case WM_DRAWITEM:
        if (st && handle_wrapped_list_msg(hwnd, msg, lParam, 750, st->font, !st->is_en)) return TRUE;
        break;
    case WM_COMMAND: {
        if (!st) break;
        UINT id = LOWORD(wParam);
        UINT code = HIWORD(wParam);
        if (id == IDOK || (id == 750 && code == LBN_DBLCLK)) {
            int sel = (int)SendMessageW(st->list_hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)st->verse_keys.size()) {
                st->selected_verse_key = st->verse_keys[sel];
                st->accepted = true;
            }
            close_modal(hwnd);
            return 0;
        }
        if (id == IDCANCEL) {
            close_modal(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        close_modal(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline bool show_sajdah_navigator_dialog(
    HWND parent,
    QuranDatabase& db,
    const AppSettings& settings,
    std::wstring& out_verse_key,
    HFONT ui_font
) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SajdahNavWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);   // مثل بقية النوافذ
        wc.lpszClassName = L"MushafSajdahNavDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    bool is_en = (settings.ui_language == L"en");
    SajdahNavState st;
    st.is_en = is_en;
    st.font = ui_font;

    DWORD ex_style = WS_EX_DLGMODALFRAME | (is_en ? 0 : (WS_EX_LAYOUTRTL | WS_EX_RTLREADING));
    HWND dlg = CreateWindowExW(
        ex_style, L"MushafSajdahNavDlg",
        is_en ? L"Sajdah Places in the Quran (15)" : L"مواضع سجدات التلاوة في القرآن (١٥ موضعاً)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        180, 110, 740, 480, parent, nullptr, GetModuleHandleW(nullptr), nullptr
    );
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    HWND lbl = CreateWindowExW(
        0, L"STATIC",
        is_en ? L"Choose a Sajdah place and press Enter to jump to the ayah:"
              : L"اختر موضع السجدة واضغط Enter للانتقال مباشرة إلى الآية:",
        WS_CHILD | WS_VISIBLE, 18, 16, 690, 24, dlg, nullptr, hInst, nullptr
    );
    st.list_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        kWrappedListStyle,
        18, 46, 690, 336, dlg, (HMENU)750, hInst, nullptr
    );

    const auto& places = all_sajdah_places();
    int idx = 1;
    for (const auto& sp : places) {
        st.verse_keys.push_back(sp.verse_key);
        AyahInfo a;
        bool ok = db.get_ayah_by_key(sp.verse_key, a);
        std::wstring s_name = ok ? (a.surah_name) : std::to_wstring(sp.surah);
        int page = ok ? a.page_number : 0;
        std::wstring note = is_en ? sp.note_en : sp.note_ar;
        std::wstring line = is_en
            ? (std::to_wstring(idx) + L". Surah " + s_name + L" (" + sp.verse_key + L") — Page " + std::to_wstring(page) + L" — " + note)
            : (to_arabic_digits(idx) + L" - سورة " + s_name + L" (" + to_arabic_digits(sp.surah) + L":" + to_arabic_digits(sp.ayah) + L") — صفحة " + to_arabic_digits(page) + L" — " + note);
        SendMessageW(st.list_hwnd, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        ++idx;
    }
    if (!places.empty()) SendMessageW(st.list_hwnd, LB_SETCURSEL, 0, 0);

    HWND btn_go = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Go to Ayah (Enter)" : L"الانتقال للآية (Enter)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        360, 396, 190, 34, dlg, (HMENU)IDOK, hInst, nullptr
    );
    HWND btn_close = CreateWindowExW(
        0, L"BUTTON", is_en ? L"Close (Esc)" : L"إغلاق (Esc)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        564, 396, 144, 34, dlg, (HMENU)IDCANCEL, hInst, nullptr
    );

    for (HWND h : {lbl, st.list_hwnd, btn_go, btn_close}) {
        SendMessageW(h, WM_SETFONT, (WPARAM)ui_font, TRUE);
    }

    fit_dialog_to_screen(dlg, parent);   // مقاسات الشاشة الحالية، وداخل حدودها
    EnableWindow(parent, FALSE);
    SetFocus(st.list_hwnd);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (handle_enter_on_focused_button(dlg, msg)) continue;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    if (st.accepted) out_verse_key = st.selected_verse_key;
    return st.accepted;
}


// ============================================================================
// نافذة «النسخ المتعدد» (Ctrl+Alt+C): آيات الوحدة الحالية في قائمة بمربعات اختيار.
// المسافة تحدد الآية أو تلغيها، و Enter ينسخ المحدد بترتيب المصحف.
// التحديد هنا هو نفسه تظليل الآيات في المصحف، فما ظلّله المبصر بالفأرة يظهر محدداً.
// ============================================================================
struct MultiCopyState {
    QuranDatabase* db = nullptr;
    AppSettings* settings = nullptr;
    ScreenReaderEngine* sr = nullptr;
    bool is_en = false;
    HWND list = nullptr;
    HWND lbl_count = nullptr;
    std::vector<AyahInfo> ayahs;
    std::vector<char> final_checked;   // حالة المربعات لحظة إغلاق النافذة
    bool copied = false;
};

inline std::wstring multi_copy_count_text(int n, bool is_en) {
    if (is_en) return std::to_wstring(n) + (n == 1 ? L" ayah selected" : L" ayahs selected");
    if (n == 0) return L"لم تُحدَّد أي آية";
    if (n == 1) return L"آية واحدة محددة";
    if (n == 2) return L"آيتان محددتان";
    if (n <= 10) return to_arabic_digits(n) + L" آيات محددة";
    return to_arabic_digits(n) + L" آية محددة";
}

inline int multi_copy_checked_count(MultiCopyState* st) {
    int n = 0;
    for (int i = 0; i < (int)st->ayahs.size(); ++i) {
        if (ListView_GetCheckState(st->list, i)) ++n;
    }
    return n;
}

inline void multi_copy_update_count(MultiCopyState* st) {
    if (st->lbl_count) SetWindowTextW(st->lbl_count, multi_copy_count_text(multi_copy_checked_count(st), st->is_en).c_str());
}

inline void multi_copy_check_all(MultiCopyState* st, bool on) {
    for (int i = 0; i < (int)st->ayahs.size(); ++i) ListView_SetCheckState(st->list, i, on ? TRUE : FALSE);
    multi_copy_update_count(st);
    if (st->sr) {
        st->sr->speak(on ? (st->is_en ? L"All ayahs selected" : L"تم تحديد كل الآيات")
                         : (st->is_en ? L"Selection cleared" : L"تم إلغاء تحديد كل الآيات"), true, st->settings->sapi_fallback);
    }
}

inline void multi_copy_do_copy(HWND dlg, MultiCopyState* st) {
    std::wstring all;
    int n = 0;
    for (int i = 0; i < (int)st->ayahs.size(); ++i) {   // القائمة مرتبة أصلاً بترتيب المصحف
        if (!ListView_GetCheckState(st->list, i)) continue;
        if (n++ > 0) all += L"\r\n";
        all += format_ayah_custom(st->ayahs[i], st->settings->copy_script, st->settings->copy_brackets,
                                  st->settings->copy_reference, st->settings->copy_arabic_digits);
    }
    if (n == 0) {
        if (st->sr) st->sr->speak(st->is_en ? L"No ayah is selected. Press Space on an ayah to select it."
                                            : L"لم تحدد أي آية. اضغط المسافة على الآية لتحديدها.", true, st->settings->sapi_fallback);
        return;
    }
    set_clipboard_text(dlg, all);
    st->copied = true;
    if (st->sr) {
        std::wstring msg = st->is_en ? (L"Copied " + std::to_wstring(n) + (n == 1 ? L" ayah" : L" ayahs"))
                       : (n == 1 ? L"تم نسخ آية واحدة" : n == 2 ? L"تم نسخ آيتين"
                          : n <= 10 ? (L"تم نسخ " + std::to_wstring(n) + L" آيات") : (L"تم نسخ " + std::to_wstring(n) + L" آية"));
        st->sr->speak(msg, true, st->settings->sapi_fallback);
    }
    close_modal(dlg);
}

static LRESULT CALLBACK MultiCopyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<MultiCopyState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_NOTIFY: {
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (st && nm && nm->idFrom == 800 && nm->code == LVN_ITEMCHANGED) {
            auto* lv = reinterpret_cast<NMLISTVIEW*>(lParam);
            if (lv->uChanged & LVIF_STATE) multi_copy_update_count(st);
        }
        break;
    }
    case WM_COMMAND: {
        if (!st) break;
        UINT id = LOWORD(wParam);
        if (id == IDOK || id == 801) { multi_copy_do_copy(hwnd, st); return 0; }
        if (id == 802) { multi_copy_check_all(st, true); return 0; }
        if (id == 803) { multi_copy_check_all(st, false); return 0; }
        if (id == IDCANCEL) { close_modal(hwnd); return 0; }
        break;
    }
    case WM_DESTROY:
        // تصل هذه الرسالة إلى النافذة قبل أبنائها، فالقائمة ما زالت موجودة
        if (st && st->list) {
            st->final_checked.assign(st->ayahs.size(), 0);
            for (int i = 0; i < (int)st->ayahs.size(); ++i) {
                st->final_checked[i] = ListView_GetCheckState(st->list, i) ? 1 : 0;
            }
        }
        return 0;
    case WM_CLOSE:
        close_modal(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// يعرض النافذة ويحدّث marked_ids بما حُدد فيها؛ يعيد true إن تم النسخ
inline bool show_multi_copy_dialog(
    HWND parent,
    QuranDatabase& db,
    AppSettings& settings,
    const std::vector<AyahInfo>& ayahs,
    std::set<int>& marked_ids,
    int focus_ayah_id,
    bool is_en,
    HFONT ui_font,
    HFONT list_font,
    ScreenReaderEngine* sr
) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = MultiCopyWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"MushafMultiCopyDlg";
        RegisterClassExW(&wc);
        reg = true;
    }
    if (ayahs.empty()) return false;

    MultiCopyState st;
    st.db = &db;
    st.settings = &settings;
    st.sr = sr;
    st.is_en = is_en;
    st.ayahs = ayahs;

    bool multi_surah = ayahs.front().surah_number != ayahs.back().surah_number;
    std::wstring title = is_en ? (L"Multiple Copy (Ctrl+Alt+C)")
                               : (L"النسخ المتعدد (Ctrl+Alt+C)");

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    DWORD ex_style = WS_EX_DLGMODALFRAME | (is_en ? 0 : (WS_EX_LAYOUTRTL | WS_EX_RTLREADING));
    HWND dlg = CreateWindowExW(
        ex_style, L"MushafMultiCopyDlg", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        150, 80, 820, 600, parent, nullptr, hInst, nullptr
    );
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    HWND lbl = CreateWindowExW(0, L"STATIC",
        is_en ? L"Press Space to select or unselect an ayah, then Enter to copy the selected ayahs:"
              : L"اضغط المسافة لتحديد الآية أو إلغاء تحديدها، ثم Enter لنسخ الآيات المحددة:",
        WS_CHILD | WS_VISIBLE, 16, 14, 772, 24, dlg, nullptr, hInst, nullptr);

    st.list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        16, 44, 772, 410, dlg, (HMENU)800, hInst, nullptr);
    ListView_SetExtendedListViewStyle(st.list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    SendMessageW(st.list, WM_SETFONT, (WPARAM)(list_font ? list_font : ui_font), TRUE);

    RECT lrc;
    GetClientRect(st.list, &lrc);
    LVCOLUMNW col{};
    col.mask = LVCF_WIDTH;
    col.cx = (lrc.right - lrc.left) - GetSystemMetrics(SM_CXVSCROLL) - 4;
    ListView_InsertColumn(st.list, 0, &col);

    int focus_idx = 0;
    for (int i = 0; i < (int)ayahs.size(); ++i) {
        const AyahInfo& a = ayahs[i];
        // نص الآية أولاً ثم رقمها (واسم السورة إن تعددت السور في الوحدة)
        std::wstring where = multi_surah
            ? (is_en ? (a.surah_name + L" " + std::to_wstring(a.ayah_number)) : (L"سورة " + a.surah_name + L"، الآية " + to_arabic_digits(a.ayah_number)))
            : (is_en ? (L"ayah " + std::to_wstring(a.ayah_number)) : (L"الآية " + to_arabic_digits(a.ayah_number)));
        std::wstring text = a.text_uthmani + L" — " + where;
        LVITEMW it{};
        it.mask = LVIF_TEXT;
        it.iItem = i;
        it.pszText = const_cast<wchar_t*>(text.c_str());
        ListView_InsertItem(st.list, &it);
        if (marked_ids.count(a.id)) ListView_SetCheckState(st.list, i, TRUE);
        if (a.id == focus_ayah_id) focus_idx = i;
    }
    ListView_SetItemState(st.list, focus_idx, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    ListView_EnsureVisible(st.list, focus_idx, FALSE);

    st.lbl_count = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 16, 464, 772, 24, dlg, nullptr, hInst, nullptr);
    multi_copy_update_count(&st);

    HWND btn_copy = CreateWindowExW(0, L"BUTTON", is_en ? L"Copy (Enter)" : L"نسخ (Enter)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 16, 500, 170, 34, dlg, (HMENU)801, hInst, nullptr);
    HWND btn_all = CreateWindowExW(0, L"BUTTON", is_en ? L"Select all (Ctrl+A)" : L"تحديد الكل (Ctrl+A)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 196, 500, 190, 34, dlg, (HMENU)802, hInst, nullptr);
    HWND btn_none = CreateWindowExW(0, L"BUTTON", is_en ? L"Clear selection" : L"إلغاء التحديد",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 396, 500, 170, 34, dlg, (HMENU)803, hInst, nullptr);
    HWND btn_close = CreateWindowExW(0, L"BUTTON", is_en ? L"Close (Esc)" : L"إغلاق (Esc)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 618, 500, 170, 34, dlg, (HMENU)IDCANCEL, hInst, nullptr);
    for (HWND h : {lbl, st.lbl_count, btn_copy, btn_all, btn_none, btn_close}) {
        SendMessageW(h, WM_SETFONT, (WPARAM)ui_font, TRUE);
    }

    fit_dialog_to_screen(dlg, parent);   // مقاسات الشاشة الحالية، وداخل حدودها
    EnableWindow(parent, FALSE);
    SetFocus(st.list);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        // Ctrl+A داخل القائمة يحدد كل الآيات
        if (msg.message == WM_KEYDOWN && msg.wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) &&
            (msg.hwnd == st.list || IsChild(dlg, msg.hwnd))) {
            multi_copy_check_all(&st, true);
            continue;
        }
        if (handle_enter_on_focused_button(dlg, msg)) continue;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);

    // ما حُدد في النافذة يصبح هو التظليل في المصحف لهذه الآيات
    for (size_t i = 0; i < st.final_checked.size() && i < st.ayahs.size(); ++i) {
        if (st.final_checked[i]) marked_ids.insert(st.ayahs[i].id);
        else marked_ids.erase(st.ayahs[i].id);
    }
    return st.copied;
}

} // namespace mushaf
