// -*- coding: utf-8 -*-
// main.cpp — النسخة الأصلية بلغة C++ (Win32 API) من برنامج «المصحف الشريف»
// مطابقة تمامًا في الواجهة والتجربة لنسخة بايثون، ومتوافقة من ويندوز 7 (32-bit / 64-bit) حتى ويندوز 11.

#include "a11y_and_config.hpp"
#include "database.hpp"
#include "sajdah_and_formatter.hpp"
#include <cmath>
#include <map>
#include "shortcuts.hpp"
#include "dialogs.hpp"
#include "update_check.hpp"
#include <commctrl.h>
#include <shellapi.h>
#include <unordered_map>
#include <windowsx.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

namespace mushaf {

// أبعاد صفوف قائمة الآيات. صناديق القوائم في ويندوز لا تقبل صفًا أطول من ٢٥٥ بكسل،
// فالآية الطويلة تُقسَّم على أكثر من صف متصل حتى لا ينقطع شيء من نصها أبدًا.
constexpr int kRowMaxHeight   = 250;
constexpr int kRowPadX        = 22;
constexpr int kRowPadY        = 10;
constexpr int kAccentWidth    = 5;
constexpr int kInlineBtnWidth = 72;
// هامشا الصفحة يمينًا ويسارًا، كحواشي المصحف الورقي: فيهما علامات الأرباع والأحزاب والأجزاء والسجدات،
// في الهامش الخارجي للصفحة (الأيمن للصفحة الفردية، والأيسر للزوجية). يُلغيان إن ضاقت القائمة عنهما
constexpr int kPageMarginWidth = 76;
constexpr int kMarkFrameGap = 20;      // المسافة بين زخرفة الهامش وإطار الصفحة
constexpr int kPageMinWidth = 380;      // أقل عرض لصفحة المصحف بين عمودي العلامات
constexpr int kMinWindowWidth = 768;    // أصغر عرض للنافذة: يتسع للصفحة وإطارها وعلامات الهامش والسهمين
constexpr int kHeaderInsetX   = 28;
constexpr int kHeaderInsetY   = 8;
constexpr int kToolbarHeight  = 52;
constexpr UINT_PTR kRelayoutTimer = 1;
constexpr UINT_PTR kControlsTimer = 2;     // إخفاء العلامات المرسومة بعد سكون الفأرة
constexpr UINT_PTR kUpdateTimer = 3;       // البحث التلقائي عن التحديثات بعد ثوانٍ من فتح البرنامج
constexpr UINT WM_UPDATE_RESULT = WM_APP + 40;   // نتيجة البحث عن التحديثات من خيطه
constexpr UINT kControlsHideMs = 3000;
// سهما التقليب للمبصر على جانبي صفحة المصحف: يُرسمان على خلفية النافذة دون أي عنصر تحكم،
// فلا يراهما قارئ الشاشة ولا يدخلان في التنقل بلوحة المفاتيح.
constexpr int kArrowGutter = 78;       // هامش كل سهم: السهم عند حافة النافذة والنص بعيد عنه
constexpr int kArrowDiameter = 52;
constexpr int kArrowEdge = 10;         // بُعد السهم عن حافة النافذة
constexpr int kArrowMinWindow = 640;
constexpr int kPageMaxWidth = 980;     // أقصى عرض لصفحة المصحف بمقاس ٩٦ نقطة (الشاشات الكبيرة)
constexpr int kPageMaxEm = 28;         // أو ٢٨ ضعف حجم خط القرآن إن كبّره القارئ   // أضيق من هذا يُخفى السهمان ويأخذ النص العرض كله

enum MenuIds : UINT {
    ID_COMBO_BROWSE           = 102,
    ID_COMBO_EDITION          = 111,
    IDM_EDITION_FIRST         = 1050,   // 1050.. لكل طبعة متاحة
    ID_LIST_UNITS             = 103,
    ID_LIST_AYAHS             = 104,
    ID_EDIT_SIDEBAR_FILTER    = 105,
    ID_TAB_SIDEBAR            = 106,
    ID_LIST_SIDEBAR_INDEX     = 107,
    ID_LIST_SIDEBAR_BOOKMARKS = 108,
    ID_BTN_SB_COPY_BM         = 109,
    ID_BTN_SB_DEL_BM          = 110,

    // 1) قائمة التصفح والتنقل
    IDM_BROWSE_SURAH  = 1001,
    IDM_BROWSE_PAGE   = 1002,
    IDM_BROWSE_JUZ    = 1003,
    IDM_BROWSE_RUB    = 1004,
    IDM_BROWSE_HIZB   = 1005,
    IDM_BROWSE_MANZIL = 1006,
    IDM_BROWSE_RUKU   = 1007,
    IDM_GOTO             = 1020,
    IDM_SEARCH           = 1021,
    IDM_FOCUS_AYAHS      = 1022,
    IDM_SHOW_UNITS       = 1023,
    IDM_FOCUS_INDEX      = 1024,
    IDM_CYCLE_LANDMARKS  = 1025,
    IDM_NEXT_UNIT        = 1026,
    IDM_PREV_UNIT        = 1027,
    IDM_EXIT             = 1029,
    IDM_SAJDAH_LIST      = 1030,
    IDM_HISTORY_BACK     = 1031,
    IDM_HISTORY_FORWARD  = 1032,

    // 2) قائمة النسخ والمفضلة وفاصل الختمة
    IDM_COPY_AYAH        = 1101,
    IDM_COPY_PLAIN       = 1102,
    IDM_COPY_ALL         = 1103,
    IDM_AYAH_INFO        = 1104,
    IDM_SURAH_INFO       = 1105,
    IDM_QUICK_INFO       = 1106,
    IDM_COPY_SURAH_HEAD  = 1107,
    IDM_COPY_SURAH_FULL  = 1108,
    IDM_SURAH_START      = 1109,
    IDM_TOGGLE_BOOKMARK  = 1110,
    IDM_SET_KHATMAH      = 1111,
    IDM_JUMP_KHATMAH     = 1112,
    IDM_BOOKMARKS_DLG    = 1113,
    IDM_CLEAR_MARKS      = 1114,
    IDM_MULTI_COPY       = 1115,

    // 3) قائمة الخيارات والعرض
    IDM_OPTIONS          = 1200,
    IDM_LANG_AR          = 1201,
    IDM_LANG_EN          = 1202,
    IDM_FONT_INC         = 1210,
    IDM_FONT_DEC         = 1211,
    IDM_FONT_RESET       = 1212,
    IDM_TOGGLE_SIDEBAR   = 1213,
    IDM_FOCUS_MODE       = 1214,
    IDM_DISPLAY_TEXT     = 1220,
    IDM_DISPLAY_NUM      = 1221,
    IDM_DISPLAY_SURAH    = 1222,
    IDM_ALIGN_CENTER     = 1223,
    IDM_ALIGN_RIGHT      = 1224,
    IDM_MUSHAF_LINES     = 1225,
    IDM_MUSHAF_FRAME     = 1226,
    IDM_CYCLE_THEME      = 1234,
    IDM_TOGGLE_MUQATTAAT = 1240,
    IDM_TOGGLE_SOUND     = 1241,
    IDM_CYCLE_EDITION    = 1242,
    IDM_CYCLE_DISPLAY    = 1243,
    IDM_TOGGLE_ALIGN     = 1244,
    IDM_TOGGLE_SR_WAQF   = 1245,

    // 4) قائمة المساعدة
    IDM_SHORTCUTS_HELP   = 1301,
    IDM_ABOUT            = 1302,
    IDM_USER_GUIDE       = 1303,
    IDM_CHECK_UPDATES    = 1304,
};

enum class ListItemKind {
    SurahHeader,
    SurahDetails,
    Basmala,
    Ayah
};

struct AyahListItemData {
    ListItemKind kind = ListItemKind::Ayah;
    std::wstring visual_text;
    std::wstring spoken_text;
    AyahInfo ayah;
    int seg_index = 0;   // ترتيب الصف داخل الآية المقسّمة (0 = الصف الأول)
    int seg_count = 1;   // عدد صفوف الآية
    int first_row = 0;   // رقم الصف الأول لهذه الآية في القائمة
    int height = 48;     // ارتفاع الصف بالبكسل (لا يتجاوز kRowMaxHeight)
    std::vector<std::wstring> lines;  // سطور الآية بعد اللفّ، تُرسم كما قيست تمامًا
    int line_h = 0;
    // بيان السورة بجوار اسمها في سطر الترويسة نفسه (فارغ = الاسم وحده)، وحجم خطه بعد التصغير (٠ = الأصلي)
    std::wstring sub_text;
    int sub_px = 0;

    // سطر من سطور الطبعة (عرض «سطور الطبعة»): كلماته تُوزَّع على عرض السطر كالمصحف الورقي
    bool mushaf_line = false;
    bool justify = false;              // سطر كامل يُمدّ على العرض، وإلا يُوسَّط (كآخر سطر في السورة)
    std::vector<std::wstring> words;   // الكلمات وعلامات نهاية الآيات بترتيب القراءة
    std::vector<int> word_w;           // عرض كل كلمة بخط السطور
};

// إجراء قائمة الآيات الأصلي (قبل الاستبدال) — يُستخدم لإكمال حركة الأسهم فوق الصفوف التكميلية
static WNDPROC g_orig_ayah_list_proc = nullptr;

// أشكال زخارف الحواشي: قبة بخيط من أعلاها، وشمسة بقبتين وخيطين، وشمسة صغيرة بلا خيط
enum class MarginShape { Arch, Medallion, Small };
static int margin_mark_height(HDC hdc, int width, const std::vector<std::wstring>& lines, HFONT font, HFONT big,
                              MarginShape shape, std::vector<int>* heights = nullptr);
static void draw_margin_mark(HDC hdc, const RECT& box, const std::vector<std::wstring>& lines,
                             const ThemePalette& pal, HFONT font, HFONT big, MarginShape shape);

// ------------------------------------------------------------------
// أسماء اختصارات البرنامج تتبع لغته. الاختصارات في مجلدات كل المستخدمين، فتغييرها يحتاج إذن المسؤول:
// يشغّل البرنامج نسخة من نفسه بهذا الإذن لتغيير الأسماء فقط، ثم تنتهي.
// ------------------------------------------------------------------
static std::wstring special_folder(int csidl) {
    wchar_t p[MAX_PATH] = {0};
    return SUCCEEDED(SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, p)) ? std::wstring(p) : std::wstring();
}

static bool file_exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

// يعيد true إن وُجد اختصار باسم اللغة الأخرى؛ ومع apply يغيّر اسمه
static bool rename_app_shortcuts(bool to_english, bool apply) {
    const std::wstring from = to_english ? L"المصحف الشريف" : L"Al-Mushaf Al-Sharif";
    const std::wstring to = to_english ? L"Al-Mushaf Al-Sharif" : L"المصحف الشريف";
    bool found = false;
    for (int csidl : {CSIDL_COMMON_PROGRAMS, CSIDL_COMMON_DESKTOPDIRECTORY}) {
        const std::wstring dir = special_folder(csidl);
        if (dir.empty()) continue;
        const std::wstring a = dir + L"\\" + from + L".lnk", z = dir + L"\\" + to + L".lnk";
        if (!file_exists(a)) continue;
        found = true;
        if (apply) MoveFileExW(a.c_str(), z.c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    return found;
}

// تُنفَّذ في النسخة المرفوعة الصلاحية: الاختصارات واسم البرنامج في «إزالة البرامج»
static int run_sync_shortcuts(bool to_english) {
    rename_app_shortcuts(to_english, true);
    HKEY hk = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{AF7629F3-F29A-4FAD-ABF9-2A506A5375E6}_is1",
                      0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
        const std::wstring name = std::wstring(to_english ? L"Al-Mushaf Al-Sharif" : L"المصحف الشريف") + L" " + kAppVersion;
        RegSetValueExW(hk, L"DisplayName", 0, REG_SZ, (const BYTE*)name.c_str(), (DWORD)((name.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hk);
    }
    return 0;
}

static bool running_from_program_files() {
    wchar_t exe[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    for (int csidl : {CSIDL_PROGRAM_FILES, CSIDL_PROGRAM_FILESX86}) {
        const std::wstring dir = special_folder(csidl);
        if (!dir.empty() && _wcsnicmp(exe, dir.c_str(), dir.size()) == 0) return true;
    }
    return false;
}

static void sync_shortcuts_with_language(bool to_english) {
    if (!running_from_program_files() || !rename_app_shortcuts(to_english, false)) return;
    wchar_t exe[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.lpParameters = to_english ? L"--sync-shortcuts en" : L"--sync-shortcuts ar";
    sei.nShow = SW_HIDE;
    if (ShellExecuteExW(&sei) && sei.hProcess) {   // إن رفض المستخدم الإذن تبقى الأسماء وتتغير لغة البرنامج
        WaitForSingleObject(sei.hProcess, 20000);
        CloseHandle(sei.hProcess);
    }
}

class MushafAppWindow {
public:
    HWND hwnd = nullptr;
    HWND lbl_edition = nullptr;    // «الطبعة» — تظهر فقط إذا توفرت أكثر من طبعة
    HWND combo_edition = nullptr;
    HWND lbl_browse = nullptr;
    HWND combo_browse = nullptr;
    HWND btn_search = nullptr;     // زر «بحث في القرآن» بجوار «تصفح حسب»
    HWND lbl_position = nullptr;   // سطر الموضع الحالي في الشريط العلوي (للمبصرين)

    // عناصر الفهرس الجانبي (المطابقة لـ sidebar_panel في نسخة بايثون)
    HWND edit_sidebar_filter = nullptr;
    HWND tab_sidebar = nullptr;
    HWND list_sidebar_index = nullptr;
    HWND list_sidebar_bookmarks = nullptr;
    HWND btn_sb_copy_bm = nullptr;
    HWND btn_sb_del_bm = nullptr;

    // منطقة المصحف (مكدس الفهرس وقائمة الآيات)
    HWND list_units = nullptr;
    HWND list_ayahs = nullptr;

    std::wstring app_dir;
    std::wstring assets_dir;
    AppSettings settings;
    std::unique_ptr<QuranDatabase> db;

    int current_surah = 1;
    int current_page = 1;
    int current_juz = 1;
    int current_hizb = 1;
    int current_rub = 1;
    int current_manzil = 1;
    int current_ruku = 1;
    int current_ayah_num = 1;
    int pending_surah_info_num = 0;
    bool showing_units = false;
    bool focus_mode = false;
    WINDOWPLACEMENT saved_placement{};   // موضع النافذة قبل ملء الشاشة، يُستعاد عند الخروج
    LONG saved_style = 0;

    // وضع التركيز يملأ الشاشة فعلًا: بلا شريط عنوان ولا حواف، على الشاشة التي فيها النافذة
    void apply_fullscreen(bool on) {
        if (on) {
            saved_placement = {};
            saved_placement.length = sizeof(saved_placement);
            GetWindowPlacement(hwnd, &saved_placement);
            saved_style = GetWindowLongW(hwnd, GWL_STYLE);
            MONITORINFO mi{sizeof(mi)};
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
            SetWindowLongW(hwnd, GWL_STYLE, saved_style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        } else if (saved_style) {
            SetWindowLongW(hwnd, GWL_STYLE, saved_style);
            SetWindowPlacement(hwnd, &saved_placement);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            saved_style = 0;
        }
    }
    bool updating_combos = false;

    std::vector<AyahInfo> current_ayahs;
    std::vector<AyahListItemData> ayah_items;
    std::vector<int> unit_numbers;
    std::vector<int> sidebar_unit_numbers;
    std::vector<AyahInfo> sidebar_bm_ayahs;

    HFONT font_ui = nullptr;
    HFONT font_units = nullptr;
    HFONT font_quran = nullptr;
    HFONT font_header = nullptr;
    HFONT font_details = nullptr;
    HFONT font_line = nullptr;     // خط سطور الطبعة (أميري قرآن)، يُصغَّر حتى يتسع أطول سطر في الصفحة
    HFONT font_margin = nullptr;   // خط علامات الهامش
    HFONT font_margin_big = nullptr;   // الكلمة الأساسية في العلامة: الحزب والجزء والسجدة ورقم الحزب
    mutable std::map<int, HFONT> details_fonts;   // خط بيان السورة مصغّرًا ليتسع له سطر واحد
    bool pending_unit_skip = false;   // تقدّم العدّ بضغطات متتابعة دون بناء الوحدات التي بينها
    // علامات الهامش خارج الإطار المذهّب (كحواشي المصحف الورقي) حين يتسع لها ما بين الإطار والسهم
    bool marks_outside = false;
    RECT mark_col_left{};          // عمودا العلامات بإحداثيات النافذة المنطقية، يسار الإطار المنطقي ويمينه
    RECT mark_col_right{};
    int font_line_px = 0;
    HBRUSH brush_paper = nullptr;
    HBRUSH brush_toolbar = nullptr;
    HBRUSH brush_window = nullptr;
    int ayah_rows_width = 0;       // عرض القائمة الذي قُسّمت عليه صفوف الآيات آخر مرة
    RECT arrow_rect_next{};        // سهم «التالي» (على اليسار المرئي كتقليب المصحف)
    RECT arrow_rect_prev{};        // سهم «السابق» (على اليمين المرئي)
    RECT exit_full_rect{};         // زر ملء الشاشة / الخروج منه (للفأرة فقط)
    RECT close_app_rect{};         // زر إغلاق البرنامج في ملء الشاشة (لا شريط عنوان فيه)
    RECT frame_rect{};             // الإطار المذهّب حول صفحة المصحف (فارغ إذا أُخفي)
    // العلامات المرسومة لا تظهر إلا حين تتحرك الفأرة، فلا يشغل القارئَ عن القرآن شيء
    bool controls_visible = false;
    POINT last_mouse{-1, -1};

    void poke_controls() {
        POINT pt;
        GetCursorPos(&pt);
        if (pt.x == last_mouse.x && pt.y == last_mouse.y) return;   // رسائل حركة بلا حركة فعلية
        last_mouse = pt;
        if (!controls_visible) {
            controls_visible = true;
            invalidate_nav_arrows();
        }
        SetTimer(hwnd, kControlsTimer, kControlsHideMs, nullptr);
    }

    void hide_controls_if_idle() {
        if (arrow_hover != 0) {   // الفأرة فوق علامة: تبقى ظاهرة
            SetTimer(hwnd, kControlsTimer, kControlsHideMs, nullptr);
            return;
        }
        KillTimer(hwnd, kControlsTimer);
        controls_visible = false;
        invalidate_nav_arrows();
    }
    int arrow_hover = 0;           // +1 فوق التالي، -1 فوق السابق، 0 لا شيء
    bool tracking_mouse = false;

    // تظليل الآيات بالفأرة للمبصر: يتراكم مع كل سحب جديد، ويُنسخ كله بأمر «نسخ»
    std::set<int> marked_ids;      // أرقام الآيات المظللة (ترتيبها العام في المصحف)
    bool mark_dragging = false;
    int mark_drag_start = -1;      // صف بداية السحب
    int mark_drag_end = -1;        // الصف الذي وصل إليه السحب
    POINT mark_drag_origin{};
    HICON app_icon = nullptr;
    HACCEL haccel = nullptr;

    std::vector<std::pair<int, int>> nav_history;
    int nav_history_pos = -1;
    bool navigating_history = false;

    std::wstring sc(UINT cmd_id) const {
        return shortcut_for_cmd(settings.custom_shortcuts, cmd_id);
    }

    void rebuild_accelerators() {
        if (haccel) {
            DestroyAcceleratorTable(haccel);
            haccel = nullptr;
        }
        std::vector<ACCEL> list;
        for (const auto& def : shortcut_defs()) {
            std::wstring spec = effective_shortcut_spec(settings.custom_shortcuts, def);
            ACCEL ac{};
            if (parse_shortcut_spec(spec, ac, def.cmd_id)) {
                list.push_back(ac);
            }
        }
        list.push_back({FVIRTKEY, VK_F3, IDM_SEARCH});
        if (!list.empty()) {
            haccel = CreateAcceleratorTableW(list.data(), (int)list.size());
        }
    }

    void push_history(int surah, int ayah) {
        if (navigating_history || surah <= 0 || ayah <= 0) return;
        std::pair<int, int> pt(surah, ayah);
        if (nav_history_pos >= 0 && nav_history_pos < (int)nav_history.size() && nav_history[nav_history_pos] == pt) {
            return;
        }
        if (nav_history_pos + 1 < (int)nav_history.size()) {
            nav_history.resize(nav_history_pos + 1);
        }
        nav_history.push_back(pt);
        if (nav_history.size() > 60) {
            nav_history.erase(nav_history.begin());
        }
        nav_history_pos = (int)nav_history.size() - 1;
    }

    bool is_en() const { return settings.ui_language == L"en"; }
    std::wstring tr(const std::wstring& ar, const std::wstring& en_txt) const {
        return is_en() ? en_txt : ar;
    }
    std::wstring num_str(int v) const {
        return is_en() ? std::to_wstring(v) : to_arabic_digits(v);
    }

    void resolve_paths() {
        wchar_t buf[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring exe_path(buf);
        size_t slash = exe_path.find_last_of(L"\\/");
        app_dir = (slash != std::wstring::npos) ? exe_path.substr(0, slash) : L".";

        for (const std::wstring& cand : {
                 app_dir + L"\\assets",
                 app_dir + L"\\..\\assets",
                 app_dir + L"\\..\\..\\assets"
             }) {
            DWORD attr = GetFileAttributesW((cand + L"\\data\\quran.sqlite3").c_str());
            if (attr != INVALID_FILE_ATTRIBUTES) {
                assets_dir = cand;
                break;
            }
        }
        if (assets_dir.empty()) assets_dir = app_dir + L"\\assets";
    }

    void load_private_fonts() {
        std::vector<std::wstring> fonts = {
            L"UthmanicHafs1Ver18.ttf",
            L"AmiriQuran-Regular.ttf",
            L"AmiriQuran.ttf",
            L"Amiri-Regular.ttf",
            L"Amiri-Bold.ttf",
            L"ScheherazadeNew-Regular.ttf",
            L"ScheherazadeNew-Bold.ttf",
            L"Kitab-Regular.ttf",
            L"Kitab-Bold.ttf",
        };
        for (const auto& f : fonts) {
            std::wstring p = assets_dir + L"\\fonts\\" + f;
            AddFontResourceExW(p.c_str(), FR_PRIVATE, nullptr);
        }
        std::wstring ico_path = assets_dir + L"\\icon.ico";
        app_icon = (HICON)LoadImageW(nullptr, ico_path.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    }

    void recreate_fonts_and_brushes() {
        if (font_ui) DeleteObject(font_ui);
        if (font_units) DeleteObject(font_units);
        if (font_quran) DeleteObject(font_quran);
        if (font_header) DeleteObject(font_header);
        if (font_details) DeleteObject(font_details);
        if (font_margin) DeleteObject(font_margin);
        if (font_margin_big) DeleteObject(font_margin_big);
        for (auto& kv : details_fonts) DeleteObject(kv.second);
        details_fonts.clear();
        if (brush_paper) DeleteObject(brush_paper);
        if (brush_toolbar) DeleteObject(brush_toolbar);
        if (brush_window) DeleteObject(brush_window);

        const int dpi_y = ui_dpi();

        auto pt_to_px = [&](int pt) { return -MulDiv(pt, dpi_y, 72); };

        font_ui = CreateFontW(
            pt_to_px(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
        );
        font_units = CreateFontW(
            pt_to_px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Amiri"
        );
        font_quran = create_quran_font(pt_to_px(settings.quran_font_size), settings.quran_font_family.c_str());
        // اسم السورة سطر واحد داخل إطاره، وسطر بياناتها قد يلتف إلى سطرين: كلاهما يتسع لحد ارتفاع الصف
        font_header = create_quran_font(pt_to_px(std::max(16, (int)(settings.quran_font_size * 0.8))), L"Amiri", FW_BOLD,
                                        kRowMaxHeight - 2 * DS(kHeaderInsetY) - DS(30));
        // بيان السورة سطر واحد بخط النسخ كما في المصاحف، ويصغر إن لم يتسع له السطر
        font_details = create_quran_font(pt_to_px(std::max(17, (int)(settings.quran_font_size * 0.74))), L"Amiri", FW_NORMAL,
                                         (kRowMaxHeight - 2 * DS(kRowPadY)) / 2);

        font_margin = CreateFontW(pt_to_px(11), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Amiri");
        font_margin_big = CreateFontW(pt_to_px(15), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Amiri");

        ThemePalette pal = get_theme_palette(effective_theme());
        brush_paper = CreateSolidBrush(pal.paper_bg);
        brush_toolbar = CreateSolidBrush(pal.toolbar_bg);
        brush_window = CreateSolidBrush(pal.window_bg);

        // الخطوط القديمة حُذفت للتو، فلا بد من ربط الجديدة بكل عنصر وإلا رجع لخط النظام
        for (HWND w : {lbl_edition, combo_edition, lbl_browse, combo_browse, btn_search, lbl_position, edit_sidebar_filter, tab_sidebar,
                       list_sidebar_index, list_sidebar_bookmarks, btn_sb_copy_bm, btn_sb_del_bm}) {
            if (w) SendMessageW(w, WM_SETFONT, (WPARAM)font_ui, TRUE);
        }
        if (list_ayahs) SendMessageW(list_ayahs, WM_SETFONT, (WPARAM)font_quran, FALSE);
        if (list_units) SendMessageW(list_units, WM_SETFONT, (WPARAM)font_units, TRUE);
    }

    std::wstring edition_short_label() const {
        if (db) {
            if (const EditionInfo* e = db->edition_info(db->edition())) return is_en() ? e->name_en : e->name_ar;
        }
        return tr(L"طبعة المدينة", L"Madinah print");
    }

    bool has_multiple_editions() const { return db && db->editions().size() > 1; }

    // «٦٠٤ صفحات» و«٣٠٢ صفحة»: الجمع إذا انتهى العدد بـ ٣ إلى ١٠، والمفرد فيما سواه
    std::wstring pages_ar(int n) const {
        int r = n % 100;
        return num_str(n) + ((r >= 3 && r <= 10) ? L" صفحات" : L" صفحة");
    }

    bool restarting = false;
    HWND last_focus = nullptr;  // آخر عنصر كان عليه التركيز قبل مغادرة النافذة

    // العنصر الذي يعود إليه التركيز: آخر عنصر كان عليه إن ما زال ظاهرًا، وإلا القائمة المعروضة
    HWND focus_target() const {
        if (last_focus && IsWindow(last_focus) && IsWindowVisible(last_focus) && IsWindowEnabled(last_focus)) return last_focus;
        return showing_units ? list_units : list_ayahs;
    }   // يُعاد فتح البرنامج (لتغيير اللغة): لا نحفظ مرة ثانية عند الإغلاق

    // حفظ موضع القراءة وحجم النافذة في الإعدادات
    void save_state() {
        flush_word_widths();
        settings.last_surah = current_surah;
        settings.last_page = current_page;
        settings.last_juz = current_juz;
        settings.last_hizb = current_hizb;
        settings.last_rub = current_rub;
        settings.last_manzil = current_manzil;
        settings.last_ruku = current_ruku;
        settings.last_ayah = current_ayah_num;
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        bool have = false;
        if (focus_mode && saved_placement.length) { wp = saved_placement; have = true; }   // لا نحفظ حجم ملء الشاشة
        else if (hwnd) have = GetWindowPlacement(hwnd, &wp) != FALSE;
        if (have) {
            const RECT& r = wp.rcNormalPosition;
            settings.window_x = r.left;
            settings.window_y = r.top;
            settings.window_w = r.right - r.left;
            settings.window_h = r.bottom - r.top;
            settings.window_maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        }
        settings.save();
    }

    // اتجاه النافذة (من اليمين أو من اليسار) يُحدَّد عند إنشائها، فتغيير اللغة يعيد فتح البرنامج
    // على الآية نفسها وبالحجم نفسه
    void relaunch_for_language() {
        say(tr(L"يُعاد فتح البرنامج باللغة العربية", L"Reopening the program in English"));
        save_state();
        sync_shortcuts_with_language(is_en());
        wchar_t exe[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(exe, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            restarting = true;
            DestroyWindow(hwnd);
        } else {
            // إن تعذّر إعادة الفتح نطبّق اللغة على النصوص على الأقل
            build_menus();
            populate_toolbar_combos();
            load_current_browse_unit(current_ayah_num, current_surah);
        }
    }

    // وضع التباين العالي في ويندوز يفرض مظهر التباين العالي في البرنامج دون تغيير اختيار المستخدم
    static bool system_high_contrast() {
        HIGHCONTRASTW hc{};
        hc.cbSize = sizeof(hc);
        return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON);
    }
    std::wstring effective_theme() const {
        return system_high_contrast() ? std::wstring(L"high_contrast") : settings.theme;
    }

    // التحويل إلى طبعة أخرى مع البقاء على الآية نفسها
    void switch_edition(const std::wstring& key) {
        AyahInfo keep;
        bool have = db->get_ayah(current_surah, current_ayah_num, keep);
        settings.mushaf_edition = db->set_edition(key);
        settings.save();
        units_cache_key.clear();
        build_menus();
        populate_toolbar_combos();
        if (have) {
            AyahInfo fresh;
            if (db->get_ayah_by_id(keep.id, fresh)) jump_to_ayah(fresh);
        }
        say(tr(L"صارت الطبعة: " + edition_short_label() + L"، " + pages_ar(db->page_count()),
               L"Switched to " + edition_short_label() + L", " + std::to_wstring(db->page_count()) + L" pages"));
    }

    bool list_is_rtl(HWND w) const {
        return w && (GetWindowLongW(w, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    }

    std::wstring page_range_text(int start_p, int end_p, bool spoken = false) const {
        if (start_p == end_p) {
            if (spoken) return tr(L"الصفحة " + std::to_wstring(start_p), L"page " + std::to_wstring(start_p));
            return tr(L"ص " + num_str(start_p), L"page " + std::to_wstring(start_p));
        }
        if (spoken) {
            return tr(
                L"من الصفحة " + std::to_wstring(start_p) + L" إلى " + std::to_wstring(end_p),
                L"pages " + std::to_wstring(start_p) + L" - " + std::to_wstring(end_p)
            );
        }
        return tr(
            L"ص " + num_str(start_p) + L" - " + num_str(end_p),
            L"pages " + std::to_wstring(start_p) + L" - " + std::to_wstring(end_p)
        );
    }

    std::wstring division_ayah_range_text(const DivisionRangeInfo& d, bool spoken = false) const {
        // أسماء السور بالعربية في الواجهتين
        std::wstring s1 = d.start_surah_name;
        std::wstring s2 = d.end_surah_name;
        std::wstring a1 = spoken ? std::to_wstring(d.start_ayah_number) : num_str(d.start_ayah_number);
        std::wstring a2 = spoken ? std::to_wstring(d.end_ayah_number) : num_str(d.end_ayah_number);
        if (d.start_surah_number == d.end_surah_number) {
            if (d.start_ayah_number == d.end_ayah_number) {
                return tr(L"الآية " + a1 + L" في سورة " + s1, L"ayah " + a1 + L" in Surah " + s1);
            }
            return tr(
                L"من الآية " + a1 + L" إلى الآية " + a2 + L" في سورة " + s1,
                L"from ayah " + a1 + L" to ayah " + a2 + L" in Surah " + s1
            );
        }
        return tr(
            L"من الآية " + a1 + L" في سورة " + s1 + L" إلى الآية " + a2 + L" في سورة " + s2,
            L"from ayah " + a1 + L" in Surah " + s1 + L" to ayah " + a2 + L" in Surah " + s2
        );
    }

    void say(const std::wstring& msg, bool interrupt = true) {
        ScreenReaderEngine::instance().speak(msg, interrupt, settings.sapi_fallback);
    }

    // ------------------------------------------------------------------
    // القوائم العلوية المطابقة لـ _build_menus في main_window.py
    // ------------------------------------------------------------------
    void build_menus() {
        if (focus_mode) {
            HMENU old_mb = GetMenu(hwnd);
            SetMenu(hwnd, nullptr);
            if (old_mb) DestroyMenu(old_mb);
            return;
        }

        HMENU mb = CreateMenu();

        auto lbl = [&](const std::wstring& ar, const std::wstring& en_txt, UINT cid) -> std::wstring {
            std::wstring s = sc(cid);
            std::wstring base = tr(ar, en_txt);
            return s.empty() ? base : (base + L"\t" + s);
        };

        // 1) التصفح والتنقل
        HMENU m_nav = CreatePopupMenu();
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_SURAH,  lbl(L"التصفح بالسور (١١٤ سورة)", L"Browse by surah (114)", IDM_BROWSE_SURAH).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_PAGE,   lbl(L"التصفح بالصفحات (" + pages_ar(db->page_count()) + L")",
                                                                 L"Browse by page (" + std::to_wstring(db->page_count()) + L")", IDM_BROWSE_PAGE).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_JUZ,    lbl(L"التصفح بالأجزاء (٣٠ جزءًا)", L"Browse by juz (30)", IDM_BROWSE_JUZ).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_RUB,    lbl(L"التصفح بالأرباع (٢٤٠ ربعًا)", L"Browse by quarter (240)", IDM_BROWSE_RUB).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_HIZB,   lbl(L"التصفح بالأحزاب (٦٠ حزبًا)", L"Browse by hizb (60)", IDM_BROWSE_HIZB).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_MANZIL, lbl(L"التصفح بالمنازل السبعة (ختمة الأسبوع — فمي بشوق)", L"Browse by the Seven Manzils (weekly reading)", IDM_BROWSE_MANZIL).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_RUKU,   lbl(L"التصفح بالركوعات (٥٥٨ ركوعًا)", L"Browse by ruku (558)", IDM_BROWSE_RUKU).c_str());
        if (has_multiple_editions()) {
            // اختيار الطبعة يظهر فقط حين تتوفر أكثر من طبعة
            HMENU m_ed = CreatePopupMenu();
            const auto& eds = db->editions();
            for (size_t i = 0; i < eds.size(); ++i) {
                std::wstring nm = (is_en() ? eds[i].name_en : eds[i].name_ar) +
                    tr(L" (" + pages_ar(eds[i].page_count) + L")", L" (" + std::to_wstring(eds[i].page_count) + L" pages)");
                AppendMenuW(m_ed, MF_STRING | (eds[i].key == db->edition() ? MF_CHECKED : 0), IDM_EDITION_FIRST + (UINT)i, nm.c_str());
            }
            AppendMenuW(m_ed, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(m_ed, MF_STRING, IDM_CYCLE_EDITION, lbl(L"التبديل إلى الطبعة التالية", L"Switch to the next print", IDM_CYCLE_EDITION).c_str());
            AppendMenuW(m_nav, MF_POPUP, (UINT_PTR)m_ed, tr(L"طبعة المصحف", L"Mushaf print").c_str());
        }
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_GOTO,            lbl(L"انتقال سريع إلى سورة / آية / صفحة / جزء / ربع...", L"Go to surah / ayah / page / juz / quarter...", IDM_GOTO).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_SEARCH,          lbl(L"البحث الشامل في آيات القرآن الكريم...", L"Search the Quran...", IDM_SEARCH).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_SAJDAH_LIST,     lbl(L"مواضع سجدات التلاوة (١٥ موضعًا)...", L"Sajdah places in the Quran (15)...", IDM_SAJDAH_LIST).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_HISTORY_BACK,    lbl(L"الرجوع إلى الموضع السابق في سجل التنقل", L"Back to previous reading position", IDM_HISTORY_BACK).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_HISTORY_FORWARD, lbl(L"التقدم إلى الموضع التالي في سجل التنقل", L"Forward to next reading position", IDM_HISTORY_FORWARD).c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_JUMP_KHATMAH,    lbl(L"الانتقال إلى فاصل الختمة (متابعة القراءة)", L"Go to reading mark (resume reading)", IDM_JUMP_KHATMAH).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BOOKMARKS_DLG,   lbl(L"العلامات المرجعية وفاصل الختمة...", L"Bookmarks and reading mark...", IDM_BOOKMARKS_DLG).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_COPY_ALL,        lbl(L"نسخ كل الآيات المعروضة (الوحدة الحالية)", L"Copy all displayed ayahs (current unit)", IDM_COPY_ALL).c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_FOCUS_AYAHS,     lbl(L"التركيز على قائمة الآيات", L"Focus the ayah list", IDM_FOCUS_AYAHS).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_SHOW_UNITS,      lbl(L"فهرس السور / الصفحات / الأجزاء / الأرباع", L"Index list", IDM_SHOW_UNITS).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_FOCUS_INDEX,     lbl(L"التركيز على قائمة الفهرس الجانبي", L"Focus the side index", IDM_FOCUS_INDEX).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_CYCLE_LANDMARKS, lbl(L"التنقل الدائري بين مناطق النافذة", L"Move between window areas", IDM_CYCLE_LANDMARKS).c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_NEXT_UNIT, tr(L"الوحدة التالية (سورة / صفحة / جزء / ربع)\tPageDown", L"Next (surah / page / juz / quarter)\tPageDown").c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_PREV_UNIT, tr(L"الوحدة السابقة (سورة / صفحة / جزء / ربع)\tPageUp", L"Previous (surah / page / juz / quarter)\tPageUp").c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_EXIT, tr(L"الخروج من البرنامج\tAlt+F4", L"Exit\tAlt+F4").c_str());
        AppendMenuW(mb, MF_POPUP, (UINT_PTR)m_nav, tr(L"&التصفح والتنقل", L"&Browse").c_str());

        // أوامر الآية والسورة لا تظهر في شريط القوائم؛ مكانها زر التطبيقات (أو الزر الأيمن)
        // على الآية أو على عنوان السورة، واختصاراتها تعمل من أي مكان.

        // 2) الخيارات والعرض
        HMENU m_opts = CreatePopupMenu();
        AppendMenuW(m_opts, MF_STRING, IDM_OPTIONS, lbl(L"خيارات البرنامج (النسخ والتصفح والخط والمظهر والاختصارات)...", L"Program options (copy, browsing, font, look, shortcuts)...", IDM_OPTIONS).c_str());

        // محاذاة السطور في أعلى القائمة: التوسيط كالمصحف هو الافتراضي
        HMENU m_align = CreatePopupMenu();
        AppendMenuW(m_align, MF_STRING | (settings.doc_alignment != L"right" ? MF_CHECKED : 0), IDM_ALIGN_CENTER,
                    tr(L"توسيط السطور كالمصحف", L"Centered like a Mushaf").c_str());
        AppendMenuW(m_align, MF_STRING | (settings.doc_alignment == L"right" ? MF_CHECKED : 0), IDM_ALIGN_RIGHT,
                    tr(L"محاذاة لليمين", L"Right-aligned").c_str());
        AppendMenuW(m_align, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_align, MF_STRING, IDM_TOGGLE_ALIGN, lbl(L"التبديل بين المحاذاتين", L"Switch alignment", IDM_TOGGLE_ALIGN).c_str());
        AppendMenuW(m_opts, MF_POPUP, (UINT_PTR)m_align, tr(L"محاذاة السطور", L"Line alignment").c_str());
        AppendMenuW(m_opts, MF_STRING | (settings.show_mushaf_frame ? MF_CHECKED : 0), IDM_MUSHAF_FRAME,
                    lbl(L"إطار المصحف المذهّب", L"Gilded Mushaf frame", IDM_MUSHAF_FRAME).c_str());
        // يظهر مفعّلًا فقط للطبعة التي لها ملف سطور (كالشمرلي)، ويعمل عند التصفح بالصفحة
        AppendMenuW(m_opts, MF_STRING | (settings.mushaf_lines ? MF_CHECKED : 0) |
                    ((db && db->has_edition_lines()) ? 0 : MF_GRAYED), IDM_MUSHAF_LINES,
                    tr(L"سطور الطبعة كما في المصحف (عند التصفح بالصفحة)",
                       L"Page lines as in the printed Mushaf (when browsing by page)").c_str());

        HMENU m_lang = CreatePopupMenu();
        AppendMenuW(m_lang, MF_STRING | (!is_en() ? MF_CHECKED : 0), IDM_LANG_AR, tr(L"العربية", L"Arabic").c_str());
        AppendMenuW(m_lang, MF_STRING | (is_en() ? MF_CHECKED : 0),  IDM_LANG_EN, tr(L"الإنجليزية", L"English").c_str());
        AppendMenuW(m_opts, MF_POPUP, (UINT_PTR)m_lang, tr(L"لغة الواجهة", L"Interface language").c_str());
        AppendMenuW(m_opts, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(m_opts, MF_STRING, IDM_FONT_INC,       lbl(L"تكبير خط الآيات", L"Larger Quran font", IDM_FONT_INC).c_str());
        AppendMenuW(m_opts, MF_STRING, IDM_FONT_DEC,       lbl(L"تصغير خط الآيات", L"Smaller Quran font", IDM_FONT_DEC).c_str());
        AppendMenuW(m_opts, MF_STRING, IDM_FONT_RESET,     lbl(L"الحجم الافتراضي لخط الآيات", L"Default Quran font size", IDM_FONT_RESET).c_str());
        AppendMenuW(m_opts, MF_STRING | (settings.show_sidebar ? MF_CHECKED : 0),
                    IDM_TOGGLE_SIDEBAR, lbl(L"إظهار / إخفاء الفهرس الجانبي", L"Show or hide the side index", IDM_TOGGLE_SIDEBAR).c_str());
        AppendMenuW(m_opts, MF_STRING | (focus_mode ? MF_CHECKED : 0),
                    IDM_FOCUS_MODE, lbl(L"وضع التركيز: المصحف بملء الشاشة", L"Full screen", IDM_FOCUS_MODE).c_str());

        HMENU m_disp = CreatePopupMenu();
        AppendMenuW(m_disp, MF_STRING | (settings.ayah_display == L"text" ? MF_CHECKED : 0),             IDM_DISPLAY_TEXT,  tr(L"نص الآية فقط", L"Ayah text only").c_str());
        AppendMenuW(m_disp, MF_STRING | (settings.ayah_display == L"text_number" ? MF_CHECKED : 0),      IDM_DISPLAY_NUM,   tr(L"نص الآية مع رقمها", L"Ayah text with its number").c_str());
        AppendMenuW(m_disp, MF_STRING | (settings.ayah_display == L"text_number_surah" ? MF_CHECKED : 0),IDM_DISPLAY_SURAH, tr(L"نص الآية مع رقمها واسم السورة", L"Ayah text with its number and surah name").c_str());
        AppendMenuW(m_disp, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_disp, MF_STRING, IDM_CYCLE_DISPLAY, lbl(L"التبديل إلى الطريقة التالية", L"Switch to the next display", IDM_CYCLE_DISPLAY).c_str());
        AppendMenuW(m_opts, MF_POPUP, (UINT_PTR)m_disp, tr(L"طريقة عرض الآيات", L"Ayah display").c_str());
        AppendMenuW(m_opts, MF_STRING | (settings.page_turn_sound ? MF_CHECKED : 0), IDM_TOGGLE_SOUND,
                    lbl(L"صوت تقليب الصفحة", L"Page-turn sound", IDM_TOGGLE_SOUND).c_str());
        AppendMenuW(m_opts, MF_STRING | (settings.sr_speak_waqf ? MF_CHECKED : 0), IDM_TOGGLE_SR_WAQF,
                    lbl(L"نطق علامات الوقف لقارئ الشاشة", L"Speak pause marks", IDM_TOGGLE_SR_WAQF).c_str());
        AppendMenuW(m_opts, MF_STRING | (settings.sr_spell_muqattaat ? MF_CHECKED : 0), IDM_TOGGLE_MUQATTAAT,
                    lbl(L"تفكيك الحروف المقطعة لقارئ الشاشة", L"Spell disjointed letters", IDM_TOGGLE_MUQATTAAT).c_str());

        AppendMenuW(m_opts, MF_STRING, IDM_CYCLE_THEME, lbl(L"تبديل المظهر اللوني", L"Switch color theme", IDM_CYCLE_THEME).c_str());
        AppendMenuW(mb, MF_POPUP, (UINT_PTR)m_opts, tr(L"ال&خيارات والعرض", L"&Options").c_str());

        // 3) المساعدة
        HMENU m_help = CreatePopupMenu();
        AppendMenuW(m_help, MF_STRING, IDM_USER_GUIDE,     lbl(L"دليل استخدام البرنامج...", L"User guide (in Arabic)...", IDM_USER_GUIDE).c_str());
        AppendMenuW(m_help, MF_STRING, IDM_SHORTCUTS_HELP, lbl(L"دليل اختصارات لوحة المفاتيح وقارئات الشاشة...", L"Keyboard and screen reader guide...", IDM_SHORTCUTS_HELP).c_str());
        AppendMenuW(m_help, MF_STRING, IDM_CHECK_UPDATES,  tr(L"البحث عن تحديثات البرنامج...", L"Check for updates...").c_str());
        AppendMenuW(m_help, MF_STRING, IDM_ABOUT,          tr(L"حول برنامج المصحف الشريف...", L"About Al-Mushaf Al-Sharif...").c_str());
        AppendMenuW(mb, MF_POPUP, (UINT_PTR)m_help,        tr(L"الم&ساعدة", L"&Help").c_str());

        HMENU old_mb = GetMenu(hwnd);
        SetMenu(hwnd, mb);
        if (old_mb) DestroyMenu(old_mb);
    }

    void populate_toolbar_combos() {
        updating_combos = true;
        if (combo_edition) {
            SetWindowTextW(lbl_edition, tr(L"ال&طبعة:", L"&Print:").c_str());
            SendMessageW(combo_edition, CB_RESETCONTENT, 0, 0);
            const auto& eds = db->editions();
            for (size_t i = 0; i < eds.size(); ++i) {
                SendMessageW(combo_edition, CB_ADDSTRING, 0, (LPARAM)(is_en() ? eds[i].name_en : eds[i].name_ar).c_str());
                if (eds[i].key == db->edition()) SendMessageW(combo_edition, CB_SETCURSEL, i, 0);
            }
        }
        SetWindowTextW(lbl_browse, tr(L"&تصفح حسب:", L"&Browse by:").c_str());
        if (btn_search) {
            std::wstring s_sc = sc(IDM_SEARCH);
            std::wstring s_lbl = tr(L"بحث في القرآن...", L"Search the Quran...");
            if (!s_sc.empty()) s_lbl += L" (" + s_sc + L")";
            SetWindowTextW(btn_search, s_lbl.c_str());
        }
        SetWindowTextW(btn_sb_copy_bm, tr(L"نسخ الآية", L"Copy ayah").c_str());
        SetWindowTextW(btn_sb_del_bm, tr(L"حذف", L"Remove").c_str());
        SendMessageW(
            edit_sidebar_filter, EM_SETCUEBANNER, TRUE,
            (LPARAM)tr(L"تصفية القائمة بالاسم أو الرقم...", L"Filter by name or number...").c_str()
        );

        SendMessageW(combo_browse, CB_RESETCONTENT, 0, 0);
        std::wstring pages_lbl = tr(
            L"الصفحات (" + pages_ar(db->page_count()) + L")",
            L"Pages (" + std::to_wstring(db->page_count()) + L")"
        );
        const std::pair<std::wstring, std::wstring> modes[7] = {
            {L"surah",  tr(L"السور (١١٤ سورة)", L"Surahs (114)")},
            {L"page",   pages_lbl},
            {L"juz",    tr(L"الأجزاء (٣٠ جزءًا)", L"Juz (30)")},
            {L"hizb",   tr(L"الأحزاب (٦٠ حزبًا)", L"Hizb (60)")},
            {L"rub",    tr(L"الأرباع (٢٤٠ ربعًا / ٦٠ حزبًا)", L"Quarters (240 / 60 hizb)")},
            {L"manzil", tr(L"المنازل السبعة (ختمة الأسبوع — فمي بشوق)", L"Seven Manzils (weekly reading)")},
            {L"ruku",   tr(L"الركوعات (٥٥٨ ركوعًا)", L"Rukus (558)")},
        };
        int sel_idx = 0;
        for (int i = 0; i < 7; ++i) {
            SendMessageW(combo_browse, CB_ADDSTRING, 0, (LPARAM)modes[i].second.c_str());
            if (modes[i].first == settings.browse_scope) sel_idx = i;
        }
        SendMessageW(combo_browse, CB_SETCURSEL, sel_idx, 0);
        updating_combos = false;

        update_sidebar_tabs_text();
        populate_sidebar_index_list();
        populate_sidebar_bookmarks_list();
    }

    void update_sidebar_tabs_text() {
        if (!tab_sidebar) return;
        std::wstring tab0_txt;
        if (settings.browse_scope == L"surah") tab0_txt = tr(L"السور (١١٤)", L"Surahs (114)");
        else if (settings.browse_scope == L"page") tab0_txt = tr(L"الصفحات (" + num_str(db->page_count()) + L")", L"Pages (" + std::to_wstring(db->page_count()) + L")");
        else if (settings.browse_scope == L"juz") tab0_txt = tr(L"الأجزاء (٣٠)", L"Juz (30)");
        else if (settings.browse_scope == L"hizb") tab0_txt = tr(L"الأحزاب (٦٠)", L"Hizb (60)");
        else if (settings.browse_scope == L"rub") tab0_txt = tr(L"الأرباع (٢٤٠)", L"Quarters (240)");
        else if (settings.browse_scope == L"manzil") tab0_txt = tr(L"المنازل (٧)", L"Manzils (7)");
        else if (settings.browse_scope == L"ruku") tab0_txt = tr(L"الركوعات (٥٥٨)", L"Rukus (558)");

        TCITEMW tie{};
        tie.mask = TCIF_TEXT;
        tie.pszText = const_cast<wchar_t*>(tab0_txt.c_str());
        SendMessageW(tab_sidebar, TCM_SETITEMW, 0, (LPARAM)&tie);

        std::wstring tab1_txt = tr(L"المفضلة", L"Bookmarks");
        tie.pszText = const_cast<wchar_t*>(tab1_txt.c_str());
        SendMessageW(tab_sidebar, TCM_SETITEMW, 1, (LPARAM)&tie);
    }

    void populate_units_and_index_items(std::vector<std::pair<int, std::wstring>>& out_items, int& out_target_unit) const {
        out_items.clear();
        out_target_unit = 1;
        if (settings.browse_scope == L"surah") {
            out_target_unit = current_surah;
            for (const auto& s : db->get_all_surahs()) {
                std::wstring s_name = s.name_arabic;
                std::wstring place_txt = is_en() ? ((s.revelation_place == L"مكية") ? L"Meccan" : L"Medinan") : s.revelation_place;
                std::wstring pages = page_range_text(s.start_page, s.end_page);
                std::wstring label = is_en()
                    ? (std::to_wstring(s.number) + L". Surah " + s_name + L" — " + std::to_wstring(s.verses_count) + L" ayahs (" + place_txt + L" • " + pages + L")")
                    : (num_str(s.number) + L" - سورة " + s_name + L" — " + ayah_count_ar(s.verses_count, false) + L" (" + place_txt + L" • " + pages + L")");
                out_items.push_back({s.number, label});
            }
        } else if (settings.browse_scope == L"page") {
            out_target_unit = current_page;
            for (const auto& d : db->get_page_info_list()) {
                std::wstring rng = division_ayah_range_text(d);
                std::wstring label = is_en()
                    ? (L"Page " + std::to_wstring(d.unit_number) + L" — " + rng + L" (juz " + std::to_wstring(d.juz_number) + L")")
                    : (L"الصفحة " + num_str(d.unit_number) + L" — " + rng + L" (الجزء " + num_str(d.juz_number) + L")");
                out_items.push_back({d.unit_number, label});
            }
        } else if (settings.browse_scope == L"juz") {
            out_target_unit = current_juz;
            for (const auto& d : db->get_juz_info_list()) {
                std::wstring rng = division_ayah_range_text(d);
                std::wstring pages = page_range_text(d.start_page, d.end_page);
                std::wstring label = is_en()
                    ? (L"Juz " + std::to_wstring(d.unit_number) + L" — " + rng + L" (" + pages + L")")
                    : (L"الجزء " + num_str(d.unit_number) + L" — " + rng + L" (" + pages + L")");
                out_items.push_back({d.unit_number, label});
            }
        } else if (settings.browse_scope == L"hizb") {
            out_target_unit = current_hizb;
            for (const auto& d : db->get_hizb_info_list()) {
                std::wstring rng = division_ayah_range_text(d);
                std::wstring pages = page_range_text(d.start_page, d.end_page);
                std::wstring label = is_en()
                    ? (L"Hizb " + std::to_wstring(d.unit_number) + L" (Juz " + std::to_wstring(d.juz_number) + L") — " + rng + L" (" + pages + L")")
                    : (L"الحزب " + num_str(d.unit_number) + L" (الجزء " + num_str(d.juz_number) + L") — " + rng + L" (" + pages + L")");
                out_items.push_back({d.unit_number, label});
            }
        } else if (settings.browse_scope == L"rub") {
            out_target_unit = current_rub;
            for (const auto& d : db->get_rub_info_list()) {
                std::wstring rng = division_ayah_range_text(d);
                std::wstring pages = page_range_text(d.start_page, d.end_page);
                std::wstring q_lbl = quarter_label(d.unit_number, d.hizb_number, is_en());
                std::wstring label = is_en()
                    ? (L"Quarter " + std::to_wstring(d.unit_number) + L" (" + q_lbl + L") — " + rng + L" (" + pages + L")")
                    : (L"الربع " + num_str(d.unit_number) + L" (" + q_lbl + L") — " + rng + L" (" + pages + L")");
                out_items.push_back({d.unit_number, label});
            }
        } else if (settings.browse_scope == L"manzil") {
            out_target_unit = current_manzil;
            for (const auto& d : db->get_manzil_info_list()) {
                std::wstring rng = division_ayah_range_text(d);
                std::wstring pages = page_range_text(d.start_page, d.end_page);
                std::wstring day_lbl = manzil_day_label(d.unit_number, is_en());
                std::wstring label = is_en()
                    ? (L"Manzil " + std::to_wstring(d.unit_number) + L" — " + day_lbl + L": " + rng + L" (" + pages + L")")
                    : (L"المنزلة " + num_str(d.unit_number) + L" — " + day_lbl + L": " + rng + L" (" + pages + L")");
                out_items.push_back({d.unit_number, label});
            }
        } else if (settings.browse_scope == L"ruku") {
            out_target_unit = current_ruku;
            for (const auto& d : db->get_ruku_info_list()) {
                std::wstring rng = division_ayah_range_text(d);
                std::wstring pages = page_range_text(d.start_page, d.end_page);
                std::wstring label = is_en()
                    ? (L"Ruku " + std::to_wstring(d.unit_number) + L" — " + rng + L" (" + pages + L")")
                    : (L"الركوع " + num_str(d.unit_number) + L" — " + rng + L" (" + pages + L")");
                out_items.push_back({d.unit_number, label});
            }
        }
    }

    std::wstring units_cache_key;

    void populate_units_list() {
        std::wstring key = settings.browse_scope + L"|" + settings.ui_language + L"|" + db->edition();
        if (key == units_cache_key && !unit_numbers.empty()) {
            int target_unit = (settings.browse_scope == L"surah") ? current_surah
                            : (settings.browse_scope == L"page") ? current_page
                            : (settings.browse_scope == L"juz") ? current_juz
                            : (settings.browse_scope == L"hizb") ? current_hizb
                            : (settings.browse_scope == L"rub") ? current_rub
                            : (settings.browse_scope == L"manzil") ? current_manzil
                            : current_ruku;
            for (size_t i = 0; i < unit_numbers.size(); ++i) {
                if (unit_numbers[i] == target_unit) {
                    SendMessageW(list_units, LB_SETCURSEL, (WPARAM)i, 0);
                    break;
                }
            }
            return;
        }
        units_cache_key = key;

        std::vector<std::pair<int, std::wstring>> items;
        int target_unit = 1;
        populate_units_and_index_items(items, target_unit);

        SendMessageW(list_units, WM_SETREDRAW, FALSE, 0);
        SendMessageW(list_units, LB_RESETCONTENT, 0, 0);
        unit_numbers.clear();

        int target_sel = 0;
        for (const auto& kv : items) {
            SendMessageW(list_units, LB_ADDSTRING, 0, (LPARAM)kv.second.c_str());
            unit_numbers.push_back(kv.first);
            if (kv.first == target_unit) target_sel = (int)unit_numbers.size() - 1;
        }
        SendMessageW(list_units, LB_SETCURSEL, target_sel, 0);
        SendMessageW(list_units, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list_units, nullptr, TRUE);
    }

    void populate_sidebar_index_list() {
        if (!list_sidebar_index || !settings.show_sidebar) return;
        wchar_t qbuf[128] = {0};
        if (edit_sidebar_filter) GetWindowTextW(edit_sidebar_filter, qbuf, 127);
        std::wstring norm_q = normalize_arabic_search(qbuf);
        for (wchar_t& c : norm_q) if (c >= L'A' && c <= L'Z') c += 32;

        std::vector<std::pair<int, std::wstring>> items;
        int target_unit = 1;
        populate_units_and_index_items(items, target_unit);

        SendMessageW(list_sidebar_index, WM_SETREDRAW, FALSE, 0);
        SendMessageW(list_sidebar_index, LB_RESETCONTENT, 0, 0);
        sidebar_unit_numbers.clear();

        int target_sel = 0;
        for (const auto& kv : items) {
            if (!norm_q.empty()) {
                std::wstring norm_lbl = normalize_arabic_search(kv.second);
                for (wchar_t& c : norm_lbl) if (c >= L'A' && c <= L'Z') c += 32;
                std::wstring num_s = std::to_wstring(kv.first);
                if (norm_lbl.find(norm_q) == std::wstring::npos && num_s.find(norm_q) == std::wstring::npos) {
                    continue;
                }
            }
            SendMessageW(list_sidebar_index, LB_ADDSTRING, 0, (LPARAM)kv.second.c_str());
            sidebar_unit_numbers.push_back(kv.first);
            if (kv.first == target_unit) target_sel = (int)sidebar_unit_numbers.size() - 1;
        }
        if (!sidebar_unit_numbers.empty()) {
            SendMessageW(list_sidebar_index, LB_SETCURSEL, target_sel, 0);
        }
        SendMessageW(list_sidebar_index, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list_sidebar_index, nullptr, TRUE);
    }

    void populate_sidebar_bookmarks_list() {
        if (!list_sidebar_bookmarks) return;
        sidebar_bm_ayahs.clear();
        SendMessageW(list_sidebar_bookmarks, WM_SETREDRAW, FALSE, 0);
        SendMessageW(list_sidebar_bookmarks, LB_RESETCONTENT, 0, 0);

        for (const auto& vk : settings.bookmarks) {
            AyahInfo ba;
            if (db->get_ayah_by_key(vk, ba)) {
                std::wstring s_name = ba.surah_name;
                std::wstring lbl = is_en()
                    ? (L"Surah " + s_name + L": " + std::to_wstring(ba.ayah_number) + L" (p. " + std::to_wstring(ba.page_number) + L") — " + ba.text_uthmani)
                    : (L"سورة " + s_name + L": " + num_str(ba.ayah_number) + L" (ص " + num_str(ba.page_number) + L") — " + ba.text_uthmani);
                sidebar_bm_ayahs.push_back(ba);
                SendMessageW(list_sidebar_bookmarks, LB_ADDSTRING, 0, (LPARAM)lbl.c_str());
            }
        }
        if (!sidebar_bm_ayahs.empty()) {
            SendMessageW(list_sidebar_bookmarks, LB_SETCURSEL, 0, 0);
        }
        SendMessageW(list_sidebar_bookmarks, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list_sidebar_bookmarks, nullptr, TRUE);
    }

    void sync_sidebar_selection() {
        if (!list_sidebar_index || sidebar_unit_numbers.empty()) return;
        int target_unit = current_surah;
        if (settings.browse_scope == L"page") target_unit = current_page;
        else if (settings.browse_scope == L"juz") target_unit = current_juz;
        else if (settings.browse_scope == L"hizb") target_unit = current_hizb;
        else if (settings.browse_scope == L"rub") target_unit = current_rub;
        else if (settings.browse_scope == L"manzil") target_unit = current_manzil;
        else if (settings.browse_scope == L"ruku") target_unit = current_ruku;

        for (size_t i = 0; i < sidebar_unit_numbers.size(); ++i) {
            if (sidebar_unit_numbers[i] == target_unit) {
                SendMessageW(list_sidebar_index, LB_SETCURSEL, (WPARAM)i, 0);
                break;
            }
        }
    }

    // العدد الترتيبي بالعربية (مذكر): الأول، الثاني... الحادي والعشرون... الستون
    static std::wstring ordinal_ar(int n) {
        static const wchar_t* u[] = {L"", L"الأول", L"الثاني", L"الثالث", L"الرابع", L"الخامس",
                                     L"السادس", L"السابع", L"الثامن", L"التاسع", L"العاشر"};
        static const wchar_t* uc[] = {L"", L"الحادي", L"الثاني", L"الثالث", L"الرابع", L"الخامس",
                                      L"السادس", L"السابع", L"الثامن", L"التاسع"};
        static const wchar_t* t[] = {L"", L"", L"العشرون", L"الثلاثون", L"الأربعون", L"الخمسون",
                                     L"الستون", L"السبعون", L"الثمانون", L"التسعون"};
        if (n >= 1 && n <= 10) return u[n];
        if (n >= 11 && n <= 19) return std::wstring(uc[n - 10]) + L" عشر";
        if (n >= 20 && n <= 99) {
            if (n % 10 == 0) return t[n / 10];
            return std::wstring(uc[n % 10]) + L" و" + t[n / 10];   // «الحادي والعشرون»
        }
        return to_arabic_digits(n);
    }

    // خط مجمع الملك فهد يرسم الأرقام العربية (١٢٣) زخارفَ لنهاية الآية، فالأرقام في البيانات
    // الملحقة بالآية تُكتب بالكلمات (الجزء الثالث) أو بالأرقام الغربية حتى لا تُشبه أرقام الآيات
    static std::wstring ordinals_for_quran_font(const std::wstring& s) {
        std::wstring out;
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] >= 0x0660 && s[i] <= 0x0669) {
                int v = 0;
                while (i < s.size() && s[i] >= 0x0660 && s[i] <= 0x0669) { v = v * 10 + (s[i] - 0x0660); ++i; }
                out += ordinal_ar(v);
            } else {
                out.push_back(s[i++]);
            }
        }
        return out;
    }

    std::wstring format_visual_ayah(const AyahInfo& a) const {
        bool is_kfgqpc = settings.quran_font_family.find(L"KFGQPC") != std::wstring::npos;
        bool with_num = (settings.ayah_display == L"text_number" || settings.ayah_display == L"text_number_surah");
        std::wstring text;
        if (is_kfgqpc) {
            text = with_num ? a.text_qpc_hafs : a.text_qpc_clean;
        } else {
            text = a.text_uthmani;
            if (with_num) {
                text += L" ۝" + to_arabic_digits(a.ayah_number);
            }
        }

        bool is_rub = (a.rub_el_hizb_number >= 2 && db->rub_start_ids().count(a.id) > 0);
        const bool edition_details = db->edition_has_details();
        // في طبعتي المدينة لا يوضع ۞ قبل أول آية في السورة، وتكفي علامة الهامش
        if (is_rub && (edition_details || a.ayah_number != 1) && text.find(L"۞") != 0) {
            text = L"۞ " + text;
        }

        const SajdahPlace* sp = print_sajdah_place(a.verse_key, edition_details);
        if (sp) {
            if (text.find(L"۩") == std::wstring::npos) text += L" ۩";
            if (marks_inline() && edition_details && (!sp->is_agreed() || a.verse_key == L"41:38")) {
                std::wstring lbl = is_en() ? sajdah_short_label(a.verse_key, true) : (L"سَجْدَةٌ " + sajdah_imams_note(a.verse_key, true));
                text += L"  [" + (is_kfgqpc ? to_western_digits(lbl) : lbl) + L"]";
            }
        }
        if (marks_inline()) {
            if (const SaktaPlace* sk = find_sakta_place(a.verse_key)) {
                text += L"  [" + sakta_spoken_label(*sk, is_en()) + L"]";
            }
        }
        if (is_rub && marks_inline()) {
            std::wstring badge = division_badge_text(a, is_en());
            if (is_kfgqpc && !is_en()) badge = ordinals_for_quran_font(badge);
            if (!badge.empty()) text += L"  " + badge;
        }
        if (settings.ayah_display == L"text_number_surah") {
            text += is_en() ? (L"   [Surah " + a.surah_name + L"]")
                            : (L"   [سورة " + a.surah_name + L"]");
        }
        if (marks_inline()) {
            if (!settings.khatmah_verse_key.empty() && a.verse_key == settings.khatmah_verse_key) {
                text += is_en() ? L"  [Reading mark]" : L"  [فاصل الختمة]";
            } else if (std::find(settings.bookmarks.begin(), settings.bookmarks.end(), a.verse_key) != settings.bookmarks.end()) {
                text += is_en() ? L"  [Bookmarked]" : L"  [في المفضلة]";
            }
        }
        if (settings.show_ayah_letter_count) {
            int lcnt = count_arabic_letters(a.text_simple);
            text += is_en() ? (L"  [" + std::to_wstring(lcnt) + L" letters]")
                            : (L"  〔" + (is_kfgqpc ? to_western_digits(letters_ar(lcnt)) : letters_ar(lcnt)) + L"〕");
        }
        return text;
    }

    std::wstring format_spoken_ayah(const AyahInfo& a) const {
        bool is_rub = (a.rub_el_hizb_number >= 2 && db->rub_start_ids().count(a.id) > 0);
        std::wstring bm;
        if (!settings.khatmah_verse_key.empty() && a.verse_key == settings.khatmah_verse_key) {
            bm = tr(L"فاصل الختمة (موضع التوقف)", L"reading mark");
        } else if (std::find(settings.bookmarks.begin(), settings.bookmarks.end(), a.verse_key) != settings.bookmarks.end()) {
            bm = tr(L"محفوظة في المفضلة", L"bookmarked");
        }
        int lcnt = settings.show_ayah_letter_count ? count_arabic_letters(a.text_simple) : 0;
        return build_ayah_accessible_name(
            a,
            settings.sr_phonetic_norm,
            settings.sr_spell_muqattaat,
            settings.ayah_display,
            is_rub,
            bm,
            is_en(),
            lcnt,
            settings.sr_speak_waqf,
            settings.sr_speak_marks,
            db->edition_has_details(),
            db->edition() == L"madinah_1405" ? L"سَجْدَةُ تِلَاوَةٍ" : L"سَجْدَة"
        );
    }

    // ------------------------------------------------------------------
    // بناء صفوف قائمة الآيات وقياسها
    // ------------------------------------------------------------------
    int ayah_list_client_width() const {
        RECT rc = {0, 0, 820, 100};
        if (list_ayahs) GetClientRect(list_ayahs, &rc);
        return std::max(260, (int)(rc.right - rc.left));
    }

    // العلامة تُكتب بعد الآية في سطرها فقط إن لم يكن لها هامش داخل الإطار ولا خارجه
    bool marks_inline() const { return !marks_outside; }

    void invalidate_outside_marks() {
        if (!marks_outside) return;
        InvalidateRect(hwnd, &mark_col_left, TRUE);
        InvalidateRect(hwnd, &mark_col_right, TRUE);
    }

    // علامات الآيات الظاهرة في عمودي الهامش خارج الإطار، كل نوع في موضعه من حواشي مصحف المدينة:
    // الحزب والجزء والسكتة وسطها على سطرها، والربع وأخواه والسجدة أسفلها عند سطرها،
    // والسجدة التي يليها ربع أو حزب زخرفة واحدة وسطها بين الآيتين
    void paint_outside_marks(HDC hdc) const {
        if (!marks_outside || !list_ayahs || showing_units || !IsWindowVisible(list_ayahs)) return;
        DWORD old_layout = SetLayout(hdc, 0);
        RECT client;
        GetClientRect(hwnd, &client);
        auto to_visual = [&](RECT r) {
            if (old_layout & LAYOUT_RTL) {
                LONG w = client.right - client.left;
                RECT v = {w - r.right, r.top, w - r.left, r.bottom};
                return v;
            }
            return r;
        };
        const RECT a = to_visual(mark_col_left), b = to_visual(mark_col_right);
        const RECT vright = (a.left > b.left) ? a : b;
        const RECT vleft = (a.left > b.left) ? b : a;
        RECT lr;
        GetClientRect(list_ayahs, &lr);
        const int list_top = mark_col_left.top;
        const int view_bottom = list_top + lr.bottom;
        const int saved = SaveDC(hdc);
        IntersectClipRect(hdc, 0, list_top, client.right, view_bottom);

        struct Card {
            bool right = false;
            int anchor = 0;
            bool anchor_is_bottom = false;
            MarginShape shape = MarginShape::Small;
            std::vector<std::wstring> lines;
            int top = 0, height = 0;
        };
        std::vector<Card> cards;
        auto add_card = [&](const AyahInfo& ayah, int anchor, bool anchor_is_bottom, MarginShape shape, std::vector<std::wstring> lines) {
            Card c;
            // الصفحة الفردية على اليمين المرئي، والزوجية على اليسار، كالمصحف المفتوح
            c.right = (ayah.page_number % 2) == 1;
            c.anchor = anchor;
            c.anchor_is_bottom = anchor_is_bottom;
            c.shape = shape;
            c.lines = std::move(lines);
            cards.push_back(std::move(c));
        };

        const int count = (int)ayah_items.size();
        int merged_id = 0;   // آية بدأ ربعها في زخرفة السجدة التي قبلها
        for (int i = (int)SendMessageW(list_ayahs, LB_GETTOPINDEX, 0, 0); i >= 0 && i < count; ++i) {
            RECT ir{};
            if (SendMessageW(list_ayahs, LB_GETITEMRECT, i, (LPARAM)&ir) == LB_ERR || ir.top >= lr.bottom) break;
            const auto& it = ayah_items[i];
            if (it.kind != ListItemKind::Ayah || it.mushaf_line || it.lines.empty() || it.line_h <= 0) continue;
            const AyahInfo& ayah = it.ayah;
            const int lh = it.line_h;
            const int n = (int)it.lines.size();
            const int y0 = list_top + ir.top + (it.seg_index == 0 ? DS(kRowPadY) : 0);
            // العلامة تظهر ما دام سطرها ظاهرًا
            auto line_visible = [&](int k) {
                const int mid = y0 + k * lh + lh / 2;
                return mid >= list_top && mid < view_bottom;
            };

            if (it.seg_index == 0 && line_visible(0)) {
                auto division = margin_division_lines(ayah);
                auto personal = margin_personal_lines(ayah);
                if (ayah.id == merged_id) division.clear();
                if (!division.empty()) {
                    const bool quarter = (ayah.rub_el_hizb_number - 1) % 4 != 0;
                    division.insert(division.end(), personal.begin(), personal.end());
                    add_card(ayah, quarter ? y0 + lh : y0 + lh / 2, quarter,
                             quarter ? MarginShape::Arch : MarginShape::Medallion, std::move(division));
                } else if (!personal.empty()) {
                    add_card(ayah, y0 + lh / 2, false, MarginShape::Small, std::move(personal));
                }
            }

            if (const SaktaPlace* sk = find_sakta_place(ayah.verse_key)) {
                for (int k = 0; k < n; ++k) {
                    if (line_visible(k) && line_has_arabic_word(it.lines[k], sk->word)) {
                        add_card(ayah, y0 + k * lh + lh / 2, false, MarginShape::Small,
                                 {tr(L"سكتة لطيفة", L"Slight pause"), is_en() ? sk->where_en : sk->where_ar});
                        break;
                    }
                }
            }

            if (it.seg_index + 1 == it.seg_count && line_visible(n - 1)) {
                auto sajdah = margin_sajdah_lines(ayah);
                if (!sajdah.empty()) {
                    std::vector<std::wstring> next_division;
                    if (i + 1 < count && ayah_items[i + 1].kind == ListItemKind::Ayah && !ayah_items[i + 1].mushaf_line &&
                        ayah_items[i + 1].ayah.id == ayah.id + 1 && ayah_items[i + 1].ayah.page_number == ayah.page_number) {
                        next_division = margin_division_lines(ayah_items[i + 1].ayah);
                    }
                    if (!next_division.empty()) {
                        merged_id = ayah.id + 1;
                        sajdah.insert(sajdah.end(), next_division.begin(), next_division.end());
                        add_card(ayah, list_top + ir.bottom, false, MarginShape::Medallion, std::move(sajdah));
                    } else {
                        add_card(ayah, y0 + n * lh, true, MarginShape::Arch, std::move(sajdah));
                    }
                }
            }
        }

        ThemePalette pal = get_theme_palette(effective_theme());
        const int inset = DS(4), gap = DS(6);
        for (int side = 0; side < 2; ++side) {
            const RECT& col = side ? vright : vleft;
            const int width = col.right - col.left;
            if (width < DS(30)) continue;
            std::vector<Card*> column;
            for (auto& c : cards) {
                if (c.right != (side == 1)) continue;
                c.height = margin_mark_height(hdc, width, c.lines, font_margin, font_margin_big, c.shape);
                c.top = c.anchor_is_bottom ? c.anchor - c.height : c.anchor - c.height / 2;
                column.push_back(&c);
            }
            std::stable_sort(column.begin(), column.end(), [](const Card* x, const Card* y) { return x->top < y->top; });
            // لا تغطي زخرفة زخرفة، وتبقى كل واحدة كاملة داخل حدود الصفحة
            int floor_y = list_top + inset;
            for (Card* c : column) {
                c->top = std::max(c->top, floor_y);
                floor_y = c->top + c->height + gap;
            }
            int ceil_y = view_bottom - inset;
            for (auto p = column.rbegin(); p != column.rend(); ++p) {
                (*p)->top = std::min((*p)->top, ceil_y - (*p)->height);
                ceil_y = (*p)->top - gap;
            }
            for (Card* c : column) {
                const RECT box = {col.left, std::max(c->top, list_top + inset), col.right,
                                  std::max(c->top, list_top + inset) + c->height};
                draw_margin_mark(hdc, box, c->lines, pal, font_margin, font_margin_big, c->shape);
            }
        }
        RestoreDC(hdc, saved);
        SetLayout(hdc, old_layout);
    }

    // علامة التقسيم في الهامش: الجزء والحزب وأرباعه، سطرًا سطرًا
    std::vector<std::wstring> margin_division_lines(const AyahInfo& a) const {
        std::vector<std::wstring> out;
        const bool en = is_en();
        auto num = [&](int v) { return en ? std::to_wstring(v) : to_arabic_digits(v); };
        const int rub = a.rub_el_hizb_number;
        if (rub >= 2 && db->rub_start_ids().count(a.id) > 0) {
            const int pos = (rub - 1) % 4;
            if ((rub - 1) % 8 == 0) {
                out.push_back(en ? (L"Juz " + num(a.juz_number)) : (L"الجُزْءُ " + num(a.juz_number)));
                out.push_back(en ? (L"Hizb " + num(a.hizb_number)) : (L"الحِزْبُ " + num(a.hizb_number)));
            } else {
                // كما في الحاشية المطبوعة: «ربع» ثم «الحزب» ثم رقمه في سطر وحده
                if (pos != 0) out.push_back(pos == 1 ? tr(L"رُبْعُ", L"Quarter") : pos == 2 ? tr(L"نِصْفُ", L"Half") : tr(L"ثَلَاثَةُ أَرْبَاعِ", L"3 quarters"));
                out.push_back(tr(pos != 0 ? L"الحِزْبِ" : L"الحِزْبُ", L"Hizb"));
                out.push_back(num(a.hizb_number));
            }
        }
        return out;
    }

    // علامة السجدة في الهامش
    std::vector<std::wstring> margin_sajdah_lines(const AyahInfo& a) const {
        std::vector<std::wstring> out;
        const bool en = is_en();
        const bool details = db->edition_has_details();
        const SajdahPlace* sp = print_sajdah_place(a.verse_key, details);
        if (!sp) return out;
        // «سَجْدَة» في المدينة الحديثة كمصحفها، و«سَجْدَةُ تِلَاوَةٍ» في المدينة القديمة،
        // وفي دار المعارف «سَجْدَةٌ» ومعها المذاهب أو أنها متفق عليها
        const bool agreed = sp->is_agreed() && a.verse_key != L"41:38";
        out.push_back(details ? tr(L"سَجْدَةٌ", L"Sajdah")
                      : db->edition() == L"madinah_1405" ? tr(L"سَجْدَةُ تِلَاوَةٍ", L"Sajdah") : tr(L"سَجْدَة", L"Sajdah"));
        if (details) {
            std::wstring note;
            if (en) {
                if (!agreed) {
                    note = sajdah_short_label(a.verse_key, true);
                    const std::wstring pre = L"prostration (";
                    if (note.compare(0, pre.size(), pre) == 0) note = note.substr(pre.size());
                    if (!note.empty() && note.back() == L')') note.pop_back();
                }
                if (!note.empty()) out.push_back(note);
            } else if (agreed) {
                out.push_back(sajdah_agreed_note_vocalized());
            } else {
                // المذاهب سطر، وموضع المالكية في فصلت سطر بعده
                note = sajdah_imams_note(a.verse_key, true);
                const size_t paren = note.find(L" (");
                out.push_back(note.substr(0, paren));
                if (paren != std::wstring::npos) out.push_back(note.substr(paren + 1));
            }
        }
        return out;
    }

    // ما يخص القارئ: فاصل الختمة والمفضلة
    std::vector<std::wstring> margin_personal_lines(const AyahInfo& a) const {
        std::vector<std::wstring> out;
        if (!settings.khatmah_verse_key.empty() && a.verse_key == settings.khatmah_verse_key) {
            out.push_back(tr(L"فاصل الختمة", L"Reading mark"));
        } else if (std::find(settings.bookmarks.begin(), settings.bookmarks.end(), a.verse_key) != settings.bookmarks.end()) {
            out.push_back(tr(L"في المفضلة", L"Bookmarked"));
        }
        return out;
    }

    // العرض المتاح لنص الآية بعد الهوامش وشريط التحديد (وزر النسخ المضمّن إن كان مفعّلًا)
    int ayah_text_width(ListItemKind kind) const {
        int w = ayah_list_client_width() - 2 * DS(kRowPadX) - DS(kAccentWidth);
        if (kind == ListItemKind::SurahHeader) w -= 2 * DS(kHeaderInsetX) + 28;
        if (kind == ListItemKind::Ayah && settings.show_inline_copy_btn) w -= DS(kInlineBtnWidth);
        return std::max(120, w);
    }

    // خط بيان السورة بحجم بكسل محدد (٠ = الحجم الأصلي)، محفوظ حتى لا يُنشأ مع كل رسم
    HFONT details_font(int px) const {
        if (px <= 0 || !font_details) return font_details;
        auto f = details_fonts.find(px);
        if (f != details_fonts.end()) return f->second;
        LOGFONTW lf{};
        GetObjectW(font_details, sizeof(lf), &lf);
        lf.lfHeight = px;
        HFONT small = CreateFontIndirectW(&lf);
        details_fonts[px] = small;
        return small;
    }

    static int single_line_width(HDC hdc, HFONT font, const std::wstring& text) {
        HGDIOBJ prev = SelectObject(hdc, font);
        RECT cr = {0, 0, 0, 0};
        DrawTextW(hdc, text.c_str(), (int)text.size(), &cr, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX | dt_reading_flag(hdc, true));
        SelectObject(hdc, prev);
        return (int)(cr.right - cr.left);
    }

    // بيان السورة الذي يُكتب بجوار اسمها في الترويسة، مشكولًا كما في المصحف
    std::wstring surah_header_sub(const SurahInfo& s) const {
        if (!db) return L"";
        if (db->edition_has_details()) return build_surah_heading(*db, s.number, is_en(), !is_en());
        if (db->edition() == L"madinah_1405") {
            const std::wstring b = build_surah_brief(s, is_en());
            return is_en() ? b : vocalize_marks_text(b);
        }
        return L"";   // المدينة الحديثة: الاسم وحده كمصحفها
    }

    // المسافة بين اسم السورة وبيانها في سطر الترويسة
    int header_gap() const { return DS(34); }

    // نص الترويسة المرئي والمنطوق: قارئ الشاشة ينطق البيان مشكولًا كما يراه المبصر
    // البيان في أول السورة وحده؛ الصفحة التي تبدأ من وسط السورة يعلوها اسمها فقط
    void fill_surah_header(AyahListItemData& hdr, const SurahInfo& s, bool surah_start = true) const {
        hdr.visual_text = s.name_uthmani;   // «سُورَةُ ...» داخل إطار مزخرف
        hdr.sub_text = surah_start ? surah_header_sub(s) : L"";
        hdr.spoken_text = is_en() ? (L"Surah " + s.name_arabic) : (L"سورة " + s.name_arabic);
        if (!hdr.sub_text.empty()) {
            const std::wstring spoken_sub = (is_en() && !db->edition_has_details()) ? build_surah_brief(s, true, true) : hdr.sub_text;
            hdr.spoken_text += (is_en() ? L", " : L"، ") + spoken_sub;
        }
    }

    // حجم خط بيان السورة الذي يتسع به في سطر واحد (٠ = الحجم الأصلي)
    int details_font_px(HDC hdc, const std::wstring& text, int width) const {
        HGDIOBJ prev = SelectObject(hdc, font_details);
        RECT cr = {0, 0, 0, 0};
        DrawTextW(hdc, text.c_str(), (int)text.size(), &cr, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX | dt_reading_flag(hdc, true));
        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);
        SelectObject(hdc, prev);
        const int avail = width - DS(12), w = (int)(cr.right - cr.left);
        if (w <= 0 || w <= avail) return 0;
        return std::max(8, (int)((long long)tm.tmHeight * avail / w));
    }

    HFONT font_for_item(const AyahListItemData& it) const {
        if (it.kind == ListItemKind::SurahDetails && it.line_h > 0 && font_details) return details_font(it.line_h);
        return font_for_kind(it.kind);
    }

    HFONT font_for_kind(ListItemKind kind) const {
        return (kind == ListItemKind::SurahHeader) ? font_header
             : (kind == ListItemKind::SurahDetails) ? font_details
             : font_quran;
    }

    static int measure_text_height(HDC hdc, const std::wstring& text, int width) {
        RECT calc = {0, 0, width, 0};
        DrawTextW(hdc, text.c_str(), (int)text.size(), &calc,
                  DT_CALCRECT | DT_WORDBREAK | dt_reading_flag(hdc, true) | DT_NOPREFIX);
        return (int)(calc.bottom - calc.top);
    }

    // لفّ النص على سطور بعرض محدد، كلمةً كلمة. قياس الكلمات منفردة أسرع بعشر مرات تقريبًا
    // من DrawText مع DT_CALCRECT للنص العربي، والسطور الناتجة تُرسم كما هي فلا يختلف العرض عن القياس.
    // عرض الكلمة بالخط المحدد في السياق، محفوظًا بعد أول قياس. قياس الكلمة بالخط العثماني هو أبطأ ما في
    // بناء الصفحة، وكلمات القرآن تتكرر كثيرًا، فالحفظ يجعل الانتقال بين السور والصفحات شبه فوري.
    // المفتاح وصف الخط نفسه (اسمه وحجمه ووزنه) لا مقبضه، لأن المقبض قد يُعاد استعماله لخط آخر
    using WordWidths = std::unordered_map<std::wstring, int>;

    // تُحفظ قياسات خط القرآن في ملف صغير لتُستعمل في الفتحة التالية، فلا تُقاس كلمات السورة من جديد.
    // الملف مساعد فقط: إن فُقد أو تلف أو اختلف الخط أو الحجم تجاهله البرنامج وقاس كالمعتاد.
    struct WordWidthStore {
        std::wstring path;   // فارغ في الاختبار الذاتي، فلا يُقرأ ملف المستخدم ولا يُكتب
        std::wstring key;    // الخط الذي تُحفظ قياساته
        bool dirty = false;
    };
    static WordWidthStore& width_store() {
        static WordWidthStore st;
        return st;
    }
    static std::unordered_map<std::wstring, WordWidths>& width_caches() {
        static std::unordered_map<std::wstring, WordWidths> caches;
        return caches;
    }

    static uint32_t width_file_hash(const unsigned char* p, size_t n) {
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
        return h;
    }

    static bool save_word_widths(const std::wstring& path, const std::wstring& key, const WordWidths& words) {
        std::vector<unsigned char> b;
        auto put = [&](const void* d, size_t n) { const auto* c = (const unsigned char*)d; b.insert(b.end(), c, c + n); };
        auto put32 = [&](uint32_t v) { put(&v, 4); };
        put("MWW1", 4);
        put32((uint32_t)key.size());
        put(key.data(), key.size() * sizeof(wchar_t));
        uint32_t n = 0;
        for (const auto& kv : words) if (!kv.first.empty() && kv.first.size() <= 200) ++n;
        put32(n);
        for (const auto& kv : words) {
            if (kv.first.empty() || kv.first.size() > 200) continue;
            uint16_t len = (uint16_t)kv.first.size();
            int32_t w = kv.second;
            put(&len, 2);
            put(kv.first.data(), kv.first.size() * sizeof(wchar_t));
            put(&w, 4);
        }
        put32(width_file_hash(b.data(), b.size()));
        const std::wstring tmp = path + L".tmp";
        FILE* f = _wfopen(tmp.c_str(), L"wb");
        if (!f) return false;
        const bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size();
        std::fclose(f);
        if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileW(tmp.c_str());
            return false;
        }
        return true;
    }

    static bool load_word_widths(const std::wstring& path, const std::wstring& key, WordWidths& out) {
        FILE* f = _wfopen(path.c_str(), L"rb");
        if (!f) return false;
        std::vector<unsigned char> b;
        unsigned char buf[65536];
        size_t got;
        while ((got = std::fread(buf, 1, sizeof(buf), f)) > 0 && b.size() < 16u * 1024 * 1024) b.insert(b.end(), buf, buf + got);
        std::fclose(f);
        if (b.size() < 16 || std::memcmp(b.data(), "MWW1", 4) != 0) return false;
        uint32_t stored = 0;
        std::memcpy(&stored, b.data() + b.size() - 4, 4);
        const size_t end = b.size() - 4;
        if (width_file_hash(b.data(), end) != stored) return false;
        size_t pos = 4;
        auto read32 = [&](uint32_t& v) { if (pos + 4 > end) return false; std::memcpy(&v, b.data() + pos, 4); pos += 4; return true; };
        uint32_t key_len = 0, count = 0;
        if (!read32(key_len) || key_len != key.size() || pos + key_len * sizeof(wchar_t) > end) return false;
        if (key_len && std::memcmp(b.data() + pos, key.data(), key_len * sizeof(wchar_t)) != 0) return false;
        pos += key_len * sizeof(wchar_t);
        if (!read32(count) || count > 200000) return false;
        WordWidths words;
        words.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            uint16_t len = 0;
            if (pos + 2 > end) return false;
            std::memcpy(&len, b.data() + pos, 2);
            pos += 2;
            if (len == 0 || len > 200 || pos + len * sizeof(wchar_t) + 4 > end) return false;
            std::wstring w(len, L' ');
            std::memcpy(&w[0], b.data() + pos, len * sizeof(wchar_t));
            pos += len * sizeof(wchar_t);
            int32_t width = 0;
            std::memcpy(&width, b.data() + pos, 4);
            pos += 4;
            if (width < 0 || width > 100000) return false;
            words.emplace(std::move(w), (int)width);
        }
        if (pos != end) return false;
        out.swap(words);
        return true;
    }

    static WordWidths& word_width_cache(HDC hdc) {
        LOGFONTW lf{};
        GetObjectW(GetCurrentObject(hdc, OBJ_FONT), sizeof(lf), &lf);
        std::wstring key = std::wstring(lf.lfFaceName) + L"|" + std::to_wstring(lf.lfHeight) + L"|" + std::to_wstring(lf.lfWeight);
        auto& caches = width_caches();
        auto it = caches.find(key);
        WordWidthStore& st = width_store();
        if (it == caches.end()) {
            it = caches.emplace(key, WordWidths()).first;
            if (!st.path.empty() && load_word_widths(st.path, key, it->second)) {
                // عيّنة تتأكد أن القياسات المحفوظة ما زالت تطابق الخط على هذا الجهاز
                int checked = 0;
                for (const auto& kv : it->second) {
                    SIZE sz{};
                    GetTextExtentPoint32W(hdc, kv.first.c_str(), (int)kv.first.size(), &sz);
                    if (sz.cx != kv.second) { it->second.clear(); break; }
                    if (++checked == 8) break;
                }
            }
        }
        st.key = key;
        return it->second;
    }

    static int word_width(HDC hdc, WordWidths& cache, const wchar_t* w, int len) {
        std::wstring k(w, (size_t)len);
        auto it = cache.find(k);
        if (it != cache.end()) return it->second;
        SIZE sz{};
        GetTextExtentPoint32W(hdc, w, len, &sz);
        cache.emplace(std::move(k), (int)sz.cx);
        width_store().dirty = true;
        return sz.cx;
    }

    static void flush_word_widths() {
        WordWidthStore& st = width_store();
        if (st.path.empty() || st.key.empty() || !st.dirty) return;
        auto it = width_caches().find(st.key);
        if (it != width_caches().end() && save_word_widths(st.path, st.key, it->second)) st.dirty = false;
    }

    static std::vector<std::wstring> wrap_text_lines(HDC hdc, const std::wstring& text, int width) {
        auto& cache = word_width_cache(hdc);
        SIZE space{};
        space.cx = word_width(hdc, cache, L" ", 1);
        std::vector<std::wstring> lines;
        std::wstring cur;
        int cur_w = 0;
        const size_t n = text.size();
        size_t i = 0;
        while (i < n) {
            while (i < n && text[i] == L' ') ++i;
            if (i >= n) break;
            size_t j = text.find(L' ', i);
            if (j == std::wstring::npos) j = n;
            SIZE sz{};
            sz.cx = word_width(hdc, cache, text.c_str() + i, (int)(j - i));
            if (cur.empty()) {
                cur.assign(text, i, j - i);
                cur_w = sz.cx;
            } else if (cur_w + space.cx + sz.cx <= width) {
                cur.push_back(L' ');
                cur.append(text, i, j - i);
                cur_w += space.cx + sz.cx;
            } else {
                lines.push_back(cur);
                cur.assign(text, i, j - i);
                cur_w = sz.cx;
            }
            i = j;
        }
        if (!cur.empty()) lines.push_back(cur);
        if (lines.empty()) lines.push_back(std::wstring());
        return lines;
    }

    // ارتفاع سطر الآية مع مسافة بسيطة بين السطور تريح العين
    // ويندوز لا يقبل صفًا أعلى من ٢٥٥ نقطة في القائمة: في الشاشة عالية الدقة مع أكبر خط قد لا يتسع سطر
    // واحد لصفه فيُقصّ، فنصغّر الخط إلى أكبر حجم يتسع سطره (ما دون ذلك يبقى بالحجم المختار تمامًا)
    static HFONT create_quran_font(int height, const wchar_t* family, int weight = FW_NORMAL, int max_lh = 0) {
        auto make = [&](int h) {
            return CreateFontW(h, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family);
        };
        HFONT f = make(height);
        if (max_lh <= 0) max_lh = kRowMaxHeight - 2 * DS(kRowPadY);
        HDC dc = CreateCompatibleDC(nullptr);
        for (int pass = 0; dc && f && pass < 4; ++pass) {
            HGDIOBJ old = SelectObject(dc, f);
            const int lh = quran_line_height(dc);
            SelectObject(dc, old);
            if (lh <= max_lh) break;
            height = (int)(height * (double)max_lh / lh);
            if (height > -8) height = -8;
            DeleteObject(f);
            f = make(height);
        }
        if (dc) DeleteDC(dc);
        return f;
    }

    static int quran_line_height(HDC hdc) {
        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);
        return tm.tmHeight + tm.tmExternalLeading + tm.tmHeight / 12;
    }

    // يضيف عنصرًا إلى القائمة، ويقسّم الآية الطويلة على صفوف متصلة؛ يعيد رقم صفها الأول
    int add_ayah_list_row(HDC hdc, AyahListItemData item) {
        const int first = (int)ayah_items.size();
        HGDIOBJ old_f = SelectObject(hdc, font_for_kind(item.kind));
        const int width = ayah_text_width(item.kind);

        if (item.kind == ListItemKind::Ayah) {
            // هامش أمان صغير حتى لا يلامس آخر حرف حافة السطر
            auto lines = wrap_text_lines(hdc, item.visual_text, width - 6);
            const int lh = quran_line_height(hdc);
            const int per_row = std::max(1, (kRowMaxHeight - 2 * DS(kRowPadY)) / std::max(1, lh));
            const int n = ((int)lines.size() + per_row - 1) / per_row;
            for (int i = 0; i < n; ++i) {
                AyahListItemData row = item;
                auto b = lines.begin() + i * per_row;
                auto e = lines.begin() + std::min((int)lines.size(), (i + 1) * per_row);
                row.lines.assign(b, e);
                row.visual_text.clear();
                for (const auto& ln : row.lines) {
                    if (!row.visual_text.empty()) row.visual_text += L" ";
                    row.visual_text += ln;
                }
                row.line_h = lh;
                row.seg_index = i;
                row.seg_count = n;
                row.first_row = first;
                int pad_top = (i == 0) ? DS(kRowPadY) : 0;
                int pad_bottom = (i == n - 1) ? DS(kRowPadY) : 0;
                int h = (int)row.lines.size() * lh + pad_top + pad_bottom;
                row.height = std::min(kRowMaxHeight, std::max(n == 1 ? DS(48) : DS(8), h));
                ayah_items.push_back(row);
                // كل صفوف الآية تحمل نصها المنطوق كاملًا، فأي صف يقف عليه قارئ الشاشة يقرأ الآية كلها
                SendMessageW(list_ayahs, LB_ADDSTRING, 0, (LPARAM)row.spoken_text.c_str());
            }
        } else {
            // الترويسة: هامش الإطار + حشوة النص داخله + مسافة تنفّس
            int pad = (item.kind == ListItemKind::SurahHeader) ? (2 * DS(kHeaderInsetY) + DS(30)) : DS(kRowPadY) * 2;
            int h = 0;
            if (item.kind == ListItemKind::SurahHeader && !item.sub_text.empty() && font_details) {
                // الاسم وبيانه في سطر واحد: يصغر خط البيان وحده حتى يتسع له ما بقي من السطر
                const int name_w = single_line_width(hdc, font_header, item.visual_text);
                const int avail = std::max(DS(60), width - name_w - header_gap());
                item.sub_px = details_font_px(hdc, item.sub_text, avail);
                TEXTMETRICW tn{}, ts{};
                HGDIOBJ prev = SelectObject(hdc, font_header);
                GetTextMetricsW(hdc, &tn);
                SelectObject(hdc, details_font(item.sub_px));
                GetTextMetricsW(hdc, &ts);
                SelectObject(hdc, prev);
                h = std::max(tn.tmAscent, ts.tmAscent) + std::max(tn.tmDescent, ts.tmDescent) + pad;
            } else if (item.kind == ListItemKind::SurahDetails) {
                item.line_h = details_font_px(hdc, item.visual_text, width);
                HGDIOBJ prev = SelectObject(hdc, font_for_item(item));
                TEXTMETRICW tm{};
                GetTextMetricsW(hdc, &tm);
                SelectObject(hdc, prev);
                h = tm.tmHeight + tm.tmExternalLeading + pad;
            } else {
                h = measure_text_height(hdc, item.visual_text, width) + pad;
            }
            item.first_row = first;
            item.height = std::min(kRowMaxHeight, std::max(DS(40), h));
            ayah_items.push_back(item);
            SendMessageW(list_ayahs, LB_ADDSTRING, 0, (LPARAM)item.spoken_text.c_str());
        }
        SelectObject(hdc, old_f);
        return first;
    }

    // عرض «سطور الطبعة»: تصفح بالصفحة في طبعة لها ملف سطور
    bool mushaf_lines_active() const {
        return settings.mushaf_lines && settings.browse_scope == L"page" && db && db->has_edition_lines();
    }

    // خط السطور بحجم بكسل محدد (يُعاد إنشاؤه فقط إذا تغيّر الحجم)
    void ensure_line_font(int px) {
        if (font_line && font_line_px == px) return;
        if (font_line) DeleteObject(font_line);
        font_line = create_quran_font(-px, L"Amiri Quran");
        font_line_px = px;
    }

    // يبني صفوف الصفحة سطرًا سطرًا كما في الطبعة: ترويسة السورة، والبسملة، وسطور الآيات
    void build_mushaf_line_rows(int page, int target_ayah, int target_surah) {
        std::vector<EditionLine> lines = db->get_page_lines(page);
        std::map<int, AyahInfo> by_id;
        for (const auto& a : current_ayahs) by_id[a.id] = a;
        std::map<int, std::vector<std::wstring>> words_of;
        auto ayah_of = [&](int id) -> const AyahInfo& {
            auto it = by_id.find(id);
            if (it == by_id.end()) {
                AyahInfo a;
                db->get_ayah_by_id(id, a);
                it = by_id.emplace(id, a).first;
            }
            return it->second;
        };
        auto words = [&](int id) -> const std::vector<std::wstring>& {
            auto it = words_of.find(id);
            if (it == words_of.end()) it = words_of.emplace(id, uthmani_words(ayah_of(id).text_uthmani)).first;
            return it->second;
        };

        // كلمات كل سطر، والنص المنطوق له
        struct Built { const EditionLine* src; std::vector<std::wstring> w; std::wstring spoken; int ayah_id; };
        std::vector<Built> built;
        for (const auto& ln : lines) {
            Built b{&ln, {}, L"", 0};
            for (const auto& sg : ln.segs) {
                const AyahInfo& a = ayah_of(sg.ayah_id);
                if (sg.first_word > 0) {
                    if (!b.ayah_id) b.ayah_id = sg.ayah_id;
                    const auto& ws = words(sg.ayah_id);
                    for (int k = sg.first_word; k <= sg.last_word && k <= (int)ws.size(); ++k) {
                        b.w.push_back(ws[k - 1]);
                        if (!b.spoken.empty()) b.spoken += L" ";
                        b.spoken += ws[k - 1];
                    }
                }
                if (sg.ends) {
                    b.w.push_back(L"۝" + to_arabic_digits(a.ayah_number));
                    b.spoken += L" (" + num_str(a.ayah_number) + L")";
                }
            }
            if (!b.ayah_id && !ln.segs.empty()) b.ayah_id = ln.segs.front().ayah_id;
            built.push_back(std::move(b));
        }

        // حجم الخط: حجم القراءة المختار، مصغَّرًا عند الحاجة حتى يتسع أطول سطر في الصفحة
        HDC hdc = GetDC(list_ayahs);
        const int avail = ayah_list_client_width() - 2 * DS(kRowPadX) - DS(kAccentWidth) - 8;
        int px = MulDiv(settings.quran_font_size, ui_dpi(), 72);
        auto natural = [&](const Built& b, std::vector<int>* ww) {
            SIZE sp{};
            GetTextExtentPoint32W(hdc, L" ", 1, &sp);
            int total = 0;
            for (const auto& w : b.w) {
                SIZE sz{};
                GetTextExtentPoint32W(hdc, w.c_str(), (int)w.size(), &sz);
                if (ww) ww->push_back(sz.cx);
                total += sz.cx + sp.cx;
            }
            return std::max(0, total - (int)sp.cx);
        };
        for (int pass = 0; pass < 3; ++pass) {
            ensure_line_font(px);
            HGDIOBJ old = SelectObject(hdc, font_line);
            int widest = 0;
            for (const auto& b : built) widest = std::max(widest, natural(b, nullptr));
            SelectObject(hdc, old);
            if (widest <= avail || px <= 12) break;
            px = std::max(12, (int)(px * (double)avail / widest) - 1);
        }
        HGDIOBJ old_f = SelectObject(hdc, font_line);
        int widest = 0;
        for (const auto& b : built) widest = std::max(widest, natural(b, nullptr));
        const int lh = quran_line_height(hdc);

        int target_row = -1, first_text_row = -1;
        for (size_t i = 0; i < built.size(); ++i) {
            const auto& b = built[i];
            const EditionLine& ln = *b.src;
            if (ln.kind != EditionLineKind::Text) {
                // الترويسة والبسملة بالشكل المعتاد في البرنامج
                SurahInfo s = db->get_surah(ln.surah);
                AyahInfo first;
                db->get_ayah(ln.surah, 1, first);
                AyahListItemData it;
                it.ayah = first;
                it.ayah.page_number = page;
                if (ln.kind == EditionLineKind::SurahBanner) {
                    it.kind = ListItemKind::SurahHeader;
                    fill_surah_header(it, s);
                } else {
                    it.kind = ListItemKind::Basmala;
                    it.visual_text = L"بِسْمِ ٱللَّهِ ٱلرَّحْمَـٰنِ ٱلرَّحِيمِ";
                    it.spoken_text = L"بِسْمِ اللَّهِ الرَّحْمَنِ الرَّحِيمِ";
                }
                SelectObject(hdc, old_f);
                add_ayah_list_row(hdc, it);
                old_f = SelectObject(hdc, font_line);
                continue;
            }
            AyahListItemData it;
            it.kind = ListItemKind::Ayah;
            it.mushaf_line = true;
            it.ayah = ayah_of(b.ayah_id);
            it.ayah.page_number = page;   // السطر الذي يكمل آية من الصفحة السابقة يُعدّ من صفحته
            it.words = b.w;
            int nat = natural(b, &it.word_w);
            // السطر الذي يملأ معظم العرض يُمدّ ككل سطور المصحف، والقصير (آخر السورة) يُوسَّط
            it.justify = nat >= widest * 82 / 100;
            it.spoken_text = b.spoken;
            it.visual_text = b.spoken;
            it.line_h = lh;
            it.first_row = (int)ayah_items.size();
            it.height = std::min(kRowMaxHeight, lh + DS(8));
            const int row = (int)ayah_items.size();
            ayah_items.push_back(it);
            SendMessageW(list_ayahs, LB_ADDSTRING, 0, (LPARAM)it.spoken_text.c_str());
            if (first_text_row < 0) first_text_row = row;
            if (target_row < 0 && target_ayah > 0) {
                for (const auto& sg : ln.segs) {
                    const AyahInfo& a = ayah_of(sg.ayah_id);
                    if (sg.first_word == 1 && a.ayah_number == target_ayah &&
                        (target_surah <= 0 || a.surah_number == target_surah)) {
                        target_row = row;
                        break;
                    }
                }
            }
        }
        SelectObject(hdc, old_f);
        ReleaseDC(list_ayahs, hdc);
        if (target_row < 0) target_row = (first_text_row >= 0) ? first_text_row : 0;
        if (!ayah_items.empty()) {
            SendMessageW(list_ayahs, LB_SETCURSEL, target_row, 0);
            update_current_ayah_from_selection(false);
        }
    }

    void set_ayahs_in_list(const std::vector<AyahInfo>& ayahs, int target_ayah, int target_surah) {
        if (mushaf_lines_active() && !ayahs.empty()) {
            current_ayahs = ayahs;
            ayah_items.clear();
            ayah_rows_width = ayah_list_client_width();
            SendMessageW(list_ayahs, WM_SETREDRAW, FALSE, 0);
            SendMessageW(list_ayahs, LB_RESETCONTENT, 0, 0);
            build_mushaf_line_rows(current_page, target_ayah, target_surah);
            SendMessageW(list_ayahs, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(list_ayahs, nullptr, TRUE);
            return;
        }
        current_ayahs = ayahs;
        ayah_items.clear();
        ayah_rows_width = ayah_list_client_width();

        SendMessageW(list_ayahs, WM_SETREDRAW, FALSE, 0);
        SendMessageW(list_ayahs, LB_RESETCONTENT, 0, 0);
        if (!ayahs.empty()) {
            SendMessageW(list_ayahs, LB_INITSTORAGE, (WPARAM)(ayahs.size() + 4), (LPARAM)(ayahs.size() * 256));
        }

        HDC hdc = GetDC(list_ayahs);
        int prev_surah = -1;
        int target_row = -1;
        int first_ayah_row = -1;

        for (const auto& a : current_ayahs) {
            if (a.surah_number != prev_surah) {
                prev_surah = a.surah_number;
                SurahInfo s = db->get_surah(a.surah_number);
                std::wstring s_name = s.name_arabic;
                std::wstring place_txt = is_en() ? ((s.revelation_place == L"مكية") ? L"Meccan" : L"Medinan") : s.revelation_place;

                AyahListItemData hdr;
                hdr.kind = ListItemKind::SurahHeader;
                hdr.ayah = a;
                const bool edition_details = db->edition_has_details();
                // اسم السورة وبيانها في سطر واحد داخل الإطار: المدينة الحديثة الاسم وحده كمصحفها،
                // والقديمة مكان النزول وعدد الآيات، ودار المعارف ترويسة الشمرلي
                (void)edition_details; (void)place_txt; (void)s_name;
                fill_surah_header(hdr, s, a.ayah_number == 1);
                add_ayah_list_row(hdc, hdr);

                if (a.ayah_number == 1 && s.bismillah_pre) {
                    AyahListItemData bsm;
                    bsm.kind = ListItemKind::Basmala;
                    bsm.ayah = a;
                    bsm.visual_text = L"بِسْمِ ٱللَّهِ ٱلرَّحْمَـٰنِ ٱلرَّحِيمِ";
                    bsm.spoken_text = L"بِسْمِ اللَّهِ الرَّحْمَنِ الرَّحِيمِ";   // نص قرآني يبقى بالعربية في الواجهتين
                    add_ayah_list_row(hdc, bsm);
                }
            }

            AyahListItemData item;
            item.kind = ListItemKind::Ayah;
            item.ayah = a;
            item.visual_text = format_visual_ayah(a);
            item.spoken_text = format_spoken_ayah(a);
            int idx = add_ayah_list_row(hdc, item);

            if (first_ayah_row < 0) first_ayah_row = idx;
            if (target_ayah > 0 && a.ayah_number == target_ayah && (target_surah <= 0 || a.surah_number == target_surah)) {
                target_row = idx;
            }
        }
        ReleaseDC(list_ayahs, hdc);

        if (target_row < 0) target_row = (first_ayah_row >= 0) ? first_ayah_row : 0;
        if (!ayah_items.empty()) {
            SendMessageW(list_ayahs, LB_SETCURSEL, target_row, 0);
            update_current_ayah_from_selection(false);
        }
        SendMessageW(list_ayahs, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list_ayahs, nullptr, TRUE);
        invalidate_outside_marks();
    }

    // أول صف لعنصر تالٍ (آية أو ترويسة) بعد الصف sel، أو -1 إن كان sel في آخر عنصر بالقائمة
    static int next_item_start_row(const std::vector<AyahListItemData>& items, int sel) {
        for (int i = std::max(0, sel + 1); i < (int)items.size(); ++i) {
            if (items[i].seg_index == 0) return i;
        }
        return -1;
    }

    // بعد أي حركة بالأسهم أو بالفأرة: إن وقع التحديد على صف تكميلي من آية مقسّمة
    // نكمل الحركة بمفاتيح القائمة نفسها حتى يقف على أول صف لآية، فيبقى نطق قارئ الشاشة طبيعيًا.
    void snap_ayah_selection(bool forward) {
        if (!list_ayahs || !g_orig_ayah_list_proc) return;
        const int count = (int)ayah_items.size();
        int sel = (int)SendMessageW(list_ayahs, LB_GETCURSEL, 0, 0);
        int guard = 0;
        while (sel >= 0 && sel < count && ayah_items[sel].seg_index > 0 && guard++ < 128) {
            int before = sel;
            bool go_down = forward && sel + 1 < count;
            CallWindowProcW(g_orig_ayah_list_proc, list_ayahs, WM_KEYDOWN, go_down ? VK_DOWN : VK_UP, 0);
            sel = (int)SendMessageW(list_ayahs, LB_GETCURSEL, 0, 0);
            if (sel == before) {
                if (!forward) break;
                forward = false;   // وصلنا لنهاية القائمة: نرجع لأول صف في الآية
            }
        }
        InvalidateRect(list_ayahs, nullptr, FALSE);
    }

    // عند تغيير عرض النافذة يُعاد تقسيم الآيات على العرض الجديد مع الإبقاء على الآية الحالية
    void relayout_ayah_rows() {
        if (current_ayahs.empty() || !list_ayahs) return;
        if (ayah_list_client_width() == ayah_rows_width) return;
        int keep_surah = current_surah, keep_ayah = current_ayah_num;
        int top = (int)SendMessageW(list_ayahs, LB_GETTOPINDEX, 0, 0);
        int top_surah = 0, top_ayah = 0;
        if (top >= 0 && top < (int)ayah_items.size()) {
            top_surah = ayah_items[top].ayah.surah_number;
            top_ayah = ayah_items[top].ayah.ayah_number;
        }
        std::vector<AyahInfo> copy = current_ayahs;
        set_ayahs_in_list(copy, keep_ayah, keep_surah);
        for (size_t i = 0; i < ayah_items.size(); ++i) {
            if (ayah_items[i].ayah.surah_number == top_surah && ayah_items[i].ayah.ayah_number == top_ayah) {
                SendMessageW(list_ayahs, LB_SETTOPINDEX, (WPARAM)i, 0);
                break;
            }
        }
    }

    const AyahInfo* focused_ayah() const {
        int sel = (int)SendMessageW(list_ayahs, LB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < (int)ayah_items.size()) {
            return &ayah_items[sel].ayah;
        }
        return current_ayahs.empty() ? nullptr : &current_ayahs[0];
    }

    void update_window_title_for_ayah(const AyahInfo& a) {
        std::wstring s_name = a.surah_name;
        int total_p = db ? db->page_count() : 604;
        int pct = (total_p > 0) ? (a.page_number * 100) / total_p : 0;
        if (pct < 1 && a.page_number >= 1) pct = 1;
        if (pct > 100) pct = 100;
        std::wstring title = is_en()
            ? (L"Al-Mushaf Al-Sharif — " + s_name + L", ayah " + std::to_wstring(a.ayah_number) +
               L" • Page " + std::to_wstring(a.page_number) + L"/" + std::to_wstring(total_p) +
               L" (" + std::to_wstring(pct) + L"%) • Juz " + std::to_wstring(a.juz_number) +
               L" • Hizb " + std::to_wstring(a.hizb_number))
            : (L"المصحف الشريف — سورة " + s_name + L"، الآية " + num_str(a.ayah_number) +
               L" • الصفحة " + num_str(a.page_number) + L" من " + num_str(total_p) +
               L" (" + num_str(pct) + L"٪) • الجزء " + num_str(a.juz_number) +
               L" • الحزب " + num_str(a.hizb_number));
        SetWindowTextW(hwnd, title.c_str());

        if (lbl_position) {
            std::wstring pos = is_en()
                ? (L"Surah " + s_name + L" • Ayah " + std::to_wstring(a.ayah_number) +
                   L" • Page " + std::to_wstring(a.page_number) + L" • Juz " + std::to_wstring(a.juz_number) +
                   L" • Hizb " + std::to_wstring(a.hizb_number))
                : (L"سورة " + s_name + L"  •  الآية " + num_str(a.ayah_number) +
                   L"  •  الصفحة " + num_str(a.page_number) + L"  •  الجزء " + num_str(a.juz_number) +
                   L"  •  الحزب " + num_str(a.hizb_number));
            if (!marked_ids.empty()) {
                pos += is_en() ? (L" • Highlighted: " + std::to_wstring(marked_ids.size()))
                               : (L"  •  المظلَّل: " + ayah_count_ar((int)marked_ids.size(), false));
            }
            SetWindowTextW(lbl_position, pos.c_str());
        }
    }

    void update_current_ayah_from_selection(bool speak_now) {
        int sel = (int)SendMessageW(list_ayahs, LB_GETCURSEL, 0, 0);
        if (sel < 0 || sel >= (int)ayah_items.size()) return;
        const auto& it = ayah_items[sel];
        current_surah = it.ayah.surah_number;
        current_ayah_num = it.ayah.ayah_number;
        current_page = it.ayah.page_number;
        current_juz = it.ayah.juz_number;
        current_hizb = it.ayah.hizb_number;
        current_rub = it.ayah.rub_el_hizb_number;
        current_manzil = it.ayah.manzil_number;
        current_ruku = it.ayah.ruku_number;
        update_window_title_for_ayah(it.ayah);
        sync_sidebar_selection();
        invalidate_nav_arrows();
        if (speak_now && settings.sr_auto_speak_ayah) {
            say(it.spoken_text, true);
        }
    }

    void show_ayahs_view() {
        showing_units = false;
        ShowWindow(list_units, SW_HIDE);
        ShowWindow(list_ayahs, SW_SHOW);
        SetFocus(list_ayahs);
        if (const AyahInfo* a = focused_ayah()) {
            update_window_title_for_ayah(*a);
        }
    }

    void show_units_view(bool focus_units = true) {
        populate_units_list();
        showing_units = true;
        ShowWindow(list_ayahs, SW_HIDE);
        ShowWindow(list_units, SW_SHOW);
        if (focus_units) SetFocus(list_units);
    }

    void open_unit_by_number(int num) {
        if (settings.browse_scope == L"surah") load_surah(num, (num == current_surah) ? current_ayah_num : 1);
        else if (settings.browse_scope == L"page") load_page(num);
        else if (settings.browse_scope == L"juz") load_juz(num);
        else if (settings.browse_scope == L"hizb") load_hizb(num);
        else if (settings.browse_scope == L"rub") load_rub(num);
        else if (settings.browse_scope == L"manzil") load_manzil(num);
        else if (settings.browse_scope == L"ruku") load_ruku(num);
    }

    void load_surah(int s_num, int target_ayah = 1) {
        s_num = std::max(1, std::min(114, s_num));
        auto ayahs = db->get_surah_ayahs(s_num);
        if (ayahs.empty()) return;
        current_surah = s_num;
        set_ayahs_in_list(ayahs, target_ayah, s_num);
        show_ayahs_view();
    }

    void load_page(int p_num, int target_ayah = -1, int target_surah = -1) {
        p_num = std::max(1, std::min(db->page_count(), p_num));
        auto ayahs = db->get_page_ayahs(p_num);
        if (ayahs.empty()) return;
        current_page = p_num;
        set_ayahs_in_list(ayahs, target_ayah, target_surah);
        show_ayahs_view();
    }

    void load_juz(int j_num, int target_ayah = -1, int target_surah = -1) {
        j_num = std::max(1, std::min(30, j_num));
        auto ayahs = db->get_juz_ayahs(j_num);
        if (ayahs.empty()) return;
        current_juz = j_num;
        set_ayahs_in_list(ayahs, target_ayah, target_surah);
        show_ayahs_view();
    }

    void load_hizb(int h_num, int target_ayah = -1, int target_surah = -1) {
        h_num = std::max(1, std::min(60, h_num));
        auto ayahs = db->get_hizb_ayahs(h_num);
        if (ayahs.empty()) return;
        current_hizb = h_num;
        set_ayahs_in_list(ayahs, target_ayah, target_surah);
        show_ayahs_view();
    }

    void load_rub(int r_num, int target_ayah = -1, int target_surah = -1) {
        r_num = std::max(1, std::min(240, r_num));
        auto ayahs = db->get_rub_ayahs(r_num);
        if (ayahs.empty()) return;
        current_rub = r_num;
        set_ayahs_in_list(ayahs, target_ayah, target_surah);
        show_ayahs_view();
    }

    void load_manzil(int m_num, int target_ayah = -1, int target_surah = -1) {
        m_num = std::max(1, std::min(7, m_num));
        auto ayahs = db->get_manzil_ayahs(m_num);
        if (ayahs.empty()) return;
        current_manzil = m_num;
        set_ayahs_in_list(ayahs, target_ayah, target_surah);
        show_ayahs_view();
    }

    void load_ruku(int r_num, int target_ayah = -1, int target_surah = -1) {
        r_num = std::max(1, std::min(558, r_num));
        auto ayahs = db->get_ruku_ayahs(r_num);
        if (ayahs.empty()) return;
        current_ruku = r_num;
        set_ayahs_in_list(ayahs, target_ayah, target_surah);
        show_ayahs_view();
    }

    void load_current_browse_unit(int target_ayah = -1, int target_surah = -1) {
        if (settings.browse_scope == L"surah") load_surah(current_surah, target_ayah > 0 ? target_ayah : 1);
        else if (settings.browse_scope == L"page") load_page(current_page, target_ayah, target_surah);
        else if (settings.browse_scope == L"juz") load_juz(current_juz, target_ayah, target_surah);
        else if (settings.browse_scope == L"hizb") load_hizb(current_hizb, target_ayah, target_surah);
        else if (settings.browse_scope == L"rub") load_rub(current_rub, target_ayah, target_surah);
        else if (settings.browse_scope == L"manzil") load_manzil(current_manzil, target_ayah, target_surah);
        else if (settings.browse_scope == L"ruku") load_ruku(current_ruku, target_ayah, target_surah);
    }

    void jump_to_ayah(const AyahInfo& a) {
        current_surah = a.surah_number;
        current_ayah_num = a.ayah_number;
        current_page = a.page_number;
        current_juz = a.juz_number;
        current_hizb = a.hizb_number;
        current_rub = a.rub_el_hizb_number;
        current_manzil = a.manzil_number;
        current_ruku = a.ruku_number;
        load_current_browse_unit(a.ayah_number, a.surah_number);
    }

    // الوحدة الحالية وعدد الوحدات حسب طريقة التصفح
    void unit_position(int& cur, int& count) const {
        const std::wstring& b = settings.browse_scope;
        if (b == L"page")        { cur = current_page;   count = db ? db->page_count() : 604; }
        else if (b == L"juz")    { cur = current_juz;    count = 30; }
        else if (b == L"hizb")   { cur = current_hizb;   count = 60; }
        else if (b == L"rub")    { cur = current_rub;    count = 240; }
        else if (b == L"manzil") { cur = current_manzil; count = 7; }
        else if (b == L"ruku")   { cur = current_ruku;   count = 558; }
        else                     { cur = current_surah;  count = 114; }
    }

    bool can_navigate(int delta) const {
        int cur = 1, count = 1;
        unit_position(cur, count);
        int nxt = cur + delta;
        return nxt >= 1 && nxt <= count;
    }

    void invalidate_nav_arrows() {
        if (!hwnd) return;
        // الإطار السميك عند مرور الفأرة يتجاوز حدود الدائرة قليلًا، فنمسح حولها بهامش
        for (const RECT* r : {&arrow_rect_next, &arrow_rect_prev, &exit_full_rect, &close_app_rect}) {
            if (IsRectEmpty(r)) continue;
            RECT big = *r;
            InflateRect(&big, 6, 6);
            InvalidateRect(hwnd, &big, TRUE);
        }
    }

    // +1 فوق سهم التالي، -1 فوق السابق، kHitExitFull فوق زر ملء الشاشة، kHitClose فوق زر الإغلاق، 0 خارجها
    static constexpr int kHitExitFull = 2;
    static constexpr int kHitClose = 3;
    bool hit_enabled(int hit) const { return hit == kHitExitFull || hit == kHitClose || (hit != 0 && can_navigate(hit)); }
    int arrow_hit_test(POINT pt) const {
        if (!controls_visible) return 0;
        if (PtInRect(&close_app_rect, pt)) return kHitClose;
        if (PtInRect(&exit_full_rect, pt)) return kHitExitFull;
        if (PtInRect(&arrow_rect_next, pt)) return +1;
        if (PtInRect(&arrow_rect_prev, pt)) return -1;
        return 0;
    }

    // دائرة بإطار ذهبي وفي وسطها شارة اتجاه؛ visual_dir: -1 تشير لليسار المرئي، +1 لليمين المرئي
    void draw_nav_arrow(HDC hdc, const RECT& rc, int visual_dir, bool enabled, bool hover) const {
        ThemePalette pal = get_theme_palette(effective_theme());
        auto blend = [](COLORREF a, COLORREF b) {
            return RGB((GetRValue(a) + GetRValue(b)) / 2, (GetGValue(a) + GetGValue(b)) / 2, (GetBValue(a) + GetBValue(b)) / 2);
        };
        // دائرة ممتلئة بلون الإطار الذهبي وسهم بلون الورق: تباين واضح على أي مظهر.
        // المعطّل (أول المصحف أو آخره) باهت، وعند مرور الفأرة يغمق الإطار
        COLORREF ring = enabled ? (hover ? pal.text_header : pal.selected_border) : blend(pal.selected_border, pal.window_bg);
        COLORREF fill = enabled ? pal.selected_border : blend(pal.toolbar_bg, pal.window_bg);
        COLORREF mark = enabled ? pal.paper_bg : blend(pal.selected_border, pal.window_bg);

        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, hover ? 4 : 2, ring);
        HGDIOBJ ob = SelectObject(hdc, br), op = SelectObject(hdc, pen);
        Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(br); DeleteObject(pen);

        // يُرسم السهم على سطح غير معكوس (انظر paint_nav_arrows)، فالاتجاه المرئي هو المنطقي
        int dir = visual_dir;
        int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
        int w = (rc.right - rc.left) / 7, h = (rc.bottom - rc.top) / 4;
        POINT pts[3] = {{cx - dir * w, cy - h}, {cx + dir * w, cy}, {cx - dir * w, cy + h}};
        LOGBRUSH lb{BS_SOLID, mark, 0};
        HPEN cp = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, 6, &lb, 0, nullptr);
        HGDIOBJ old = SelectObject(hdc, cp);
        Polyline(hdc, pts, 3);
        SelectObject(hdc, old);
        DeleteObject(cp);
    }

    // إطار المصحف المذهّب حول الصفحة: خطان ذهبيان بينهما شريط خفيف، ومعيّن صغير في الأركان وأوساط الأضلاع.
    // ثابت لا يتحرك، ويُرسم في الهامش خارج القائمة فلا يمس النص. يُرسم بالإحداثيات المنطقية لأنه متناظر.
    void paint_mushaf_frame(HDC hdc) const {
        if (IsRectEmpty(&frame_rect)) return;
        const std::wstring th = effective_theme();
        if (th == L"high_contrast") return;   // التباين العالي للوضوح لا للزخرفة
        ThemePalette pal = get_theme_palette(th);
        auto mix = [](COLORREF a, COLORREF b, int pa) {   // pa% من a
            return RGB((GetRValue(a) * pa + GetRValue(b) * (100 - pa)) / 100,
                       (GetGValue(a) * pa + GetGValue(b) * (100 - pa)) / 100,
                       (GetBValue(a) * pa + GetBValue(b) * (100 - pa)) / 100);
        };
        const bool dark = (th == L"night_gold");
        const COLORREF gold = dark ? mix(pal.selected_border, pal.window_bg, 70) : pal.selected_border;
        const COLORREF band = mix(gold, pal.window_bg, dark ? 14 : 18);

        RECT outer = frame_rect;
        RECT inner = frame_rect;
        InflateRect(&inner, -DS(7), -DS(7));
        // الشريط بين الخطين
        HBRUSH bb = CreateSolidBrush(band);
        RECT strips[4] = {{outer.left, outer.top, outer.right, inner.top}, {outer.left, inner.bottom, outer.right, outer.bottom},
                          {outer.left, inner.top, inner.left, inner.bottom}, {inner.right, inner.top, outer.right, inner.bottom}};
        for (const RECT& s : strips) FillRect(hdc, &s, bb);
        DeleteObject(bb);
        // الخط الخارجي أعرض، والداخلي رفيع
        HBRUSH gb = CreateSolidBrush(gold);
        auto frame = [&](RECT r, int t) {
            RECT e[4] = {{r.left, r.top, r.right, r.top + t}, {r.left, r.bottom - t, r.right, r.bottom},
                         {r.left, r.top, r.left + t, r.bottom}, {r.right - t, r.top, r.right, r.bottom}};
            for (const RECT& x : e) FillRect(hdc, &x, gb);
        };
        frame(outer, std::max(2, DS(3)));
        frame(inner, std::max(1, DS(1)));
        // معيّنات صغيرة: كبيرة في الأركان، وأصغر في أوساط الأضلاع
        HPEN np = (HPEN)GetStockObject(NULL_PEN);
        HGDIOBJ op = SelectObject(hdc, np), ob = SelectObject(hdc, gb);
        auto diamond = [&](int cx, int cy, int r) {
            POINT p[4] = {{cx, cy - r}, {cx + r, cy}, {cx, cy + r}, {cx - r, cy}};
            Polygon(hdc, p, 4);
        };
        const int ix = (outer.left + inner.left) / 2, iy = (outer.top + inner.top) / 2;
        const int jx = (outer.right + inner.right) / 2, jy = (outer.bottom + inner.bottom) / 2;
        for (POINT c : {POINT{ix, iy}, POINT{jx, iy}, POINT{ix, jy}, POINT{jx, jy}}) diamond(c.x, c.y, DS(8));
        diamond((ix + jx) / 2, iy, DS(5));
        diamond((ix + jx) / 2, jy, DS(5));
        diamond(ix, (iy + jy) / 2, DS(5));
        diamond(jx, (iy + jy) / 2, DS(5));
        // نقطة بلون الورق في قلب معيّنات الأركان
        HBRUSH pb = CreateSolidBrush(band);
        SelectObject(hdc, pb);
        for (POINT c : {POINT{ix, iy}, POINT{jx, iy}, POINT{ix, jy}, POINT{jx, jy}}) diamond(c.x, c.y, DS(3));
        SelectObject(hdc, op); SelectObject(hdc, ob);
        DeleteObject(pb);
        DeleteObject(gb);
    }

    // زر الخروج من ملء الشاشة: دائرة كالسهمين وفي وسطها علامة ×
    void draw_exit_full(HDC hdc, const RECT& rc, bool hover, bool close_mark = false) const {
        ThemePalette pal = get_theme_palette(effective_theme());
        HBRUSH br = CreateSolidBrush(pal.selected_border);
        HPEN pen = CreatePen(PS_SOLID, hover ? 4 : 2, hover ? pal.text_header : pal.selected_border);
        HGDIOBJ ob = SelectObject(hdc, br), op = SelectObject(hdc, pen);
        Ellipse(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(br); DeleteObject(pen);
        int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2, d = (rc.right - rc.left) / 6;
        LOGBRUSH lb{BS_SOLID, pal.paper_bg, 0};
        HPEN cp = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, close_mark ? 5 : 3, &lb, 0, nullptr);
        HGDIOBJ old = SelectObject(hdc, cp);
        if (close_mark) {   // × لإغلاق البرنامج
            MoveToEx(hdc, cx - d, cy - d, nullptr); LineTo(hdc, cx + d, cy + d);
            MoveToEx(hdc, cx + d, cy - d, nullptr); LineTo(hdc, cx - d, cy + d);
        } else {
            // أربع زوايا: متجهة للخارج للتكبير، وللداخل للخروج من ملء الشاشة
            int e = d + 2, k = d;
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
                if (!focus_mode) {
                    POINT pts[3] = {{cx + sx * e, cy + sy * (e - k)}, {cx + sx * e, cy + sy * e}, {cx + sx * (e - k), cy + sy * e}};
                    Polyline(hdc, pts, 3);
                } else {
                    // زاوية رأسها قريب من المركز وذراعاها نحو حافة الدائرة (علامة التصغير المعروفة)
                    int q = d / 2 + 2, outer = d + 6;
                    POINT pts[3] = {{cx + sx * q, cy + sy * outer}, {cx + sx * q, cy + sy * q}, {cx + sx * outer, cy + sy * q}};
                    Polyline(hdc, pts, 3);
                }
            }
        }
        SelectObject(hdc, old);
        DeleteObject(cp);
    }

    void paint_nav_arrows(HDC hdc) const {
        if (!controls_visible) return;
        if (IsRectEmpty(&arrow_rect_next) && IsRectEmpty(&exit_full_rect)) return;
        // نرسم على سطح غير معكوس حتى لا ينقلب شكل السهم، ونحوّل موضعيهما إلى الإحداثيات المرئية
        DWORD old_layout = SetLayout(hdc, 0);
        RECT client;
        GetClientRect(hwnd, &client);
        auto to_visual = [&](RECT r) {
            if (old_layout & LAYOUT_RTL) {
                LONG w = client.right - client.left;
                RECT v = {w - r.right, r.top, w - r.left, r.bottom};
                return v;
            }
            return r;
        };
        // التالي على اليسار المرئي ويشير إلى اليسار، والسابق على اليمين ويشير إلى اليمين (كتقليب المصحف)
        if (!IsRectEmpty(&arrow_rect_next)) {
            draw_nav_arrow(hdc, to_visual(arrow_rect_next), -1, can_navigate(+1), arrow_hover == +1);
            draw_nav_arrow(hdc, to_visual(arrow_rect_prev), +1, can_navigate(-1), arrow_hover == -1);
        }
        if (!IsRectEmpty(&exit_full_rect)) draw_exit_full(hdc, to_visual(exit_full_rect), arrow_hover == kHitExitFull);
        if (!IsRectEmpty(&close_app_rect)) draw_exit_full(hdc, to_visual(close_app_rect), arrow_hover == kHitClose, true);
        SetLayout(hdc, old_layout);
    }

    // ضغطات PageDown و PageUp المنتظرة في الطابور (عند إبقاء المفتاح مضغوطًا)
    bool more_unit_keys_waiting() const {
        MSG m;
        if (!PeekMessageW(&m, list_ayahs, WM_KEYDOWN, WM_KEYDOWN, PM_NOREMOVE)) return false;
        return m.wParam == VK_NEXT || m.wParam == VK_PRIOR;
    }

    void navigate_unit(int delta) {
        // والمفتاح مضغوط: نتقدم في العدّ فقط دون بناء الصفحة، ويُبنى ما يقف عنده القارئ مرة واحدة
        if (more_unit_keys_waiting()) {
            const std::wstring& b = settings.browse_scope;
            auto step = [&](int& cur, int max_n) {
                int nxt = cur + delta;
                if (nxt >= 1 && nxt <= max_n) cur = nxt;
            };
            if (b == L"surah") step(current_surah, 114);
            else if (b == L"page") step(current_page, db->page_count());
            else if (b == L"juz") step(current_juz, 30);
            else if (b == L"hizb") step(current_hizb, 60);
            else if (b == L"rub") step(current_rub, 240);
            else if (b == L"manzil") step(current_manzil, 7);
            else if (b == L"ruku") step(current_ruku, 558);
            pending_unit_skip = true;
            return;
        }
        // آخر ضغطة في السلسلة: تتقدم خطوتها كالمعتاد أدناه، وإن كان العدّ قد بلغ أول الوحدات أو آخرها
        // تُبنى الوحدة التي وقف عندها، فلا تبقى الصفحة المعروضة غير الموضع المحسوب
        const bool after_skip = pending_unit_skip;
        pending_unit_skip = false;
        bool moved = false;
        if (settings.browse_scope == L"surah") {
            int nxt = current_surah + delta;
            if (nxt >= 1 && nxt <= 114) { load_surah(nxt, 1); moved = true; }
        } else if (settings.browse_scope == L"page") {
            int nxt = current_page + delta;
            if (nxt >= 1 && nxt <= db->page_count()) { load_page(nxt); moved = true; }
        } else if (settings.browse_scope == L"juz") {
            int nxt = current_juz + delta;
            if (nxt >= 1 && nxt <= 30) { load_juz(nxt); moved = true; }
        } else if (settings.browse_scope == L"hizb") {
            int nxt = current_hizb + delta;
            if (nxt >= 1 && nxt <= 60) { load_hizb(nxt); moved = true; }
        } else if (settings.browse_scope == L"rub") {
            int nxt = current_rub + delta;
            if (nxt >= 1 && nxt <= 240) { load_rub(nxt); moved = true; }
        } else if (settings.browse_scope == L"manzil") {
            int nxt = current_manzil + delta;
            if (nxt >= 1 && nxt <= 7) { load_manzil(nxt); moved = true; }
        } else if (settings.browse_scope == L"ruku") {
            int nxt = current_ruku + delta;
            if (nxt >= 1 && nxt <= 558) { load_ruku(nxt); moved = true; }
        }
        if (!moved && after_skip) {
            load_current_browse_unit();
            moved = true;
        }
        if (moved) {
            play_page_turn_sound(assets_dir + L"\\sounds\\page_turn.wav", settings.page_turn_sound);
            update_current_ayah_from_selection(false);
            std::wstring msg = current_unit_spoken();
            int sel = (int)SendMessageW(list_ayahs, LB_GETCURSEL, 0, 0);
            if (settings.sr_auto_speak_ayah && sel >= 0 && sel < (int)ayah_items.size())
                msg += L"، " + ayah_items[sel].spoken_text;
            say(msg);
        } else {
            // السورة والصفحة والمنزلة مؤنثة: «هذه آخر صفحة»، والباقي «هذا آخر جزء»
            const std::wstring& b = settings.browse_scope;
            const std::wstring dem = (b == L"surah" || b == L"page" || b == L"manzil") ? L"هذه " : L"هذا ";
            say(delta > 0 ? tr(dem + L"آخر " + unit_kind_word(), L"This is the last " + unit_kind_word())
                          : tr(dem + L"أول " + unit_kind_word(), L"This is the first " + unit_kind_word()));
        }
        invalidate_nav_arrows();
    }

    // هل الصف داخل نطاق السحب الجاري (معاينة التظليل قبل رفع الزر)؟
    bool row_in_drag(int row) const {
        if (!mark_dragging || mark_drag_start < 0 || mark_drag_end < 0) return false;
        int a = std::min(mark_drag_start, mark_drag_end), b = std::max(mark_drag_start, mark_drag_end);
        return row >= a && row <= b;
    }

    bool row_is_marked(int row) const {
        if (row < 0 || row >= (int)ayah_items.size()) return false;
        const auto& it = ayah_items[row];
        if (it.kind != ListItemKind::Ayah) return false;
        return marked_ids.count(it.ayah.id) > 0 || row_in_drag(row);
    }

    // إضافة الآيات بين صفين إلى التظليل (عناوين السور والبسملة لا تُظلَّل)
    void mark_rows(int r1, int r2) {
        int a = std::max(0, std::min(r1, r2));
        int b = std::min((int)ayah_items.size() - 1, std::max(r1, r2));
        for (int i = a; i <= b; ++i) {
            if (ayah_items[i].kind == ListItemKind::Ayah) marked_ids.insert(ayah_items[i].ayah.id);
        }
        on_marks_changed();
    }

    void toggle_mark_row(int row) {
        if (row < 0 || row >= (int)ayah_items.size() || ayah_items[row].kind != ListItemKind::Ayah) return;
        int id = ayah_items[row].ayah.id;
        if (!marked_ids.erase(id)) marked_ids.insert(id);
        on_marks_changed();
    }

    void clear_marks() {
        if (marked_ids.empty()) return;
        marked_ids.clear();
        on_marks_changed();
        say(tr(L"أُلغي التظليل", L"Highlighting cleared"));
    }

    // تمييز العدد بالعربية: آية واحدة، آيتان، ٣ آيات، ١١ آية
    std::wstring ayah_count_ar(int n, bool spoken) const {
        std::wstring d = spoken ? std::to_wstring(n) : num_str(n);
        if (n == 1) return L"آية واحدة";
        if (n == 2) return L"آيتان";
        if ((n % 100) >= 3 && (n % 100) <= 10) return d + L" آيات";
        return d + L" آية";
    }

    void on_marks_changed() {
        if (list_ayahs) InvalidateRect(list_ayahs, nullptr, FALSE);
        if (const AyahInfo* a = focused_ayah()) update_window_title_for_ayah(*a);
    }

    // الصف المحدد في قائمة الآيات (آية أو عنوان سورة أو بيانها أو البسملة)
    const AyahListItemData* current_row() const {
        int sel = (int)SendMessageW(list_ayahs, LB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < (int)ayah_items.size()) return &ayah_items[sel];
        return nullptr;
    }

    bool current_row_is_ayah() const {
        const AyahListItemData* r = current_row();
        return !r || r->kind == ListItemKind::Ayah;
    }

    // أوامر الآية لا تعمل على صفوف السورة؛ نرشد المستخدم إلى قائمة السورة بدلًا منها
    bool require_ayah_row() {
        if (current_row_is_ayah()) return true;
        const AyahListItemData* r = current_row();
        std::wstring s_name = r->ayah.surah_name;
        say(tr(
            L"أنت على عنوان سورة " + s_name + L". اضغط زر التطبيقات لخيارات السورة، أو انزل بالسهم إلى الآيات",
            L"This is the heading of Surah " + s_name + L". Press the Applications key for surah options, or arrow down to the ayahs"
        ));
        return false;
    }

    // أوامر السورة: تعمل على السورة المحددة في قائمة السياق، وإلا فعلى السورة الحالية
    int take_context_surah() {
        int n = (pending_surah_info_num > 0) ? pending_surah_info_num : current_surah;
        pending_surah_info_num = 0;
        return std::max(1, std::min(114, n));
    }

    // السورة كما تبدأ في المصحف: اسمها، ثم بيانها، ثم البسملة، ثم آياتها بإعدادات النسخ
    void copy_whole_surah(int surah_num) {
        auto ayahs = db->get_surah_ayahs(surah_num);
        SurahInfo s = db->get_surah(surah_num);
        std::wstring all = s.name_uthmani + L"\r\n";
        if (db->edition_has_details()) all += build_surah_heading(*db, surah_num, is_en()) + L"\r\n";
        else if (db->edition() == L"madinah_1405") all += build_surah_brief(s, is_en()) + L"\r\n";
        AyahInfo first;
        if (s.bismillah_pre && db->get_ayah(1, 1, first)) {
            all += format_ayah_custom(first, settings.copy_script, false, false, settings.copy_arabic_digits) + L"\r\n";
        }
        for (size_t i = 0; i < ayahs.size(); ++i) {
            if (i > 0) all += L"\r\n";
            all += format_ayah_in_surah(ayahs[i], settings.copy_script, settings.copy_brackets,
                                        settings.copy_reference, settings.copy_arabic_digits);
        }
        set_clipboard_text(hwnd, all);
        say(tr(L"نُسخت سورة " + s.name_arabic + L" كاملةً ببيانها، " + ayahs_ar((int)ayahs.size()),
               L"Copied Surah " + s.name_arabic + L" with its heading, " + std::to_wstring(ayahs.size()) + L" ayahs"));
    }

    void go_to_surah_start(int surah_num) {
        push_history(current_surah, current_ayah_num);
        AyahInfo first;
        if (settings.browse_scope == L"surah") {
            load_surah(surah_num, 1);
        } else if (db->get_ayah(surah_num, 1, first)) {
            jump_to_ayah(first);
        }
        push_history(current_surah, current_ayah_num);
    }

    void copy_current_ayah(bool force_plain = false) {
        // إن ظلّل المبصر آيات بالفأرة تُنسخ كلها بترتيب المصحف
        if (!marked_ids.empty()) {
            std::wstring all;
            int n = 0;
            for (int id : marked_ids) {
                AyahInfo a;
                if (!db->get_ayah_by_id(id, a)) continue;
                if (n++ > 0) all += L"\r\n";
                all += format_ayah_custom(a, force_plain ? L"plain" : settings.copy_script,
                                          force_plain ? false : settings.copy_brackets,
                                          settings.copy_reference, settings.copy_arabic_digits);
            }
            set_clipboard_text(hwnd, all);
            say(tr(L"نُسخ المظلَّل: " + ayah_count_ar(n, true), L"Copied " + std::to_wstring(n) + L" highlighted ayahs"));
            return;
        }
        // على عنوان السورة أو بيانها يُنسخ بيان السورة، وعلى البسملة تُنسخ البسملة
        if (const AyahListItemData* r = current_row()) {
            if (r->kind == ListItemKind::SurahHeader || r->kind == ListItemKind::SurahDetails) {
                if (db->edition_has_details()) {
                    set_clipboard_text(hwnd, build_surah_heading(*db, r->ayah.surah_number, is_en()));
                    say(tr(L"نُسخ بيان السورة", L"Copied the surah heading"));
                } else if (r->kind == ListItemKind::SurahDetails || !r->sub_text.empty()) {
                    set_clipboard_text(hwnd, build_surah_brief(db->get_surah(r->ayah.surah_number), is_en()));
                    say(tr(L"نُسخ بيان السورة", L"Copied the surah heading"));
                } else {
                    set_clipboard_text(hwnd, db->get_surah(r->ayah.surah_number).name_uthmani);
                    say(tr(L"نُسخ اسم السورة", L"Copied the surah name"));
                }
                return;
            }
            if (r->kind == ListItemKind::Basmala) {
                set_clipboard_text(hwnd, L"بِسْمِ ٱللَّهِ ٱلرَّحْمَـٰنِ ٱلرَّحِيمِ");
                say(tr(L"نُسخت البسملة", L"Copied the Basmala"));
                return;
            }
        }
        const AyahInfo* a = focused_ayah();
        if (!a) return;
        std::wstring formatted = format_ayah_custom(
            *a,
            force_plain ? L"plain" : settings.copy_script,
            force_plain ? false : settings.copy_brackets,
            settings.copy_reference,
            settings.copy_arabic_digits
        );
        set_clipboard_text(hwnd, formatted);
        std::wstring s_name = a->surah_name;   // اسم السورة بالعربية في الواجهتين
        say(tr(
            L"نُسخت الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name,
            L"Copied ayah " + std::to_wstring(a->ayah_number) + L" of Surah " + s_name
        ));
    }

    void set_khatmah_at_current() {
        if (!require_ayah_row()) return;
        const AyahInfo* a = focused_ayah();
        if (!a) return;
        if (settings.khatmah_verse_key == a->verse_key) {
            settings.khatmah_verse_key.clear();
            settings.save();
            load_current_browse_unit(a->ayah_number, a->surah_number);
            say(tr(L"أُزيل فاصل الختمة", L"Reading mark removed"));
            return;
        }
        settings.khatmah_verse_key = a->verse_key;
        settings.save();
        int s = a->surah_number, ay = a->ayah_number;
        load_current_browse_unit(ay, s);
        int pct = (int)((a->id * 100.0) / std::max(1, db->total_ayahs()) + 0.5);
        std::wstring s_name = a->surah_name;   // اسم السورة بالعربية في الواجهتين
        say(tr(
            L"وُضع فاصل الختمة عند الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name +
            L"، الصفحة " + std::to_wstring(a->page_number) + L"، الجزء " + std::to_wstring(a->juz_number) +
            L"، أتممت " + std::to_wstring(pct) + L" بالمئة من المصحف",
            L"Reading mark set at ayah " + std::to_wstring(a->ayah_number) + L" of Surah " + s_name +
            L", page " + std::to_wstring(a->page_number) + L", juz " + std::to_wstring(a->juz_number) +
            L", " + std::to_wstring(pct) + L"% of the Mushaf"
        ));
    }

    void jump_to_khatmah() {
        if (settings.khatmah_verse_key.empty()) {
            say(tr(
                L"لم يُحدَّد فاصل الختمة بعد. اضغط M أو Ctrl+M على أي آية لتضع علامة التوقف عندها",
                L"No reading mark is set yet. Press M or Ctrl+M on any ayah to set it"
            ));
            return;
        }
        AyahInfo target;
        if (db->get_ayah_by_key(settings.khatmah_verse_key, target)) {
            jump_to_ayah(target);
            std::wstring s_name = target.surah_name;
            say(tr(
                L"متابعة القراءة من فاصل الختمة: سورة " + s_name + L"، الآية " + std::to_wstring(target.ayah_number),
                L"Resuming from reading mark: Surah " + s_name + L", ayah " + std::to_wstring(target.ayah_number)
            ));
        }
    }

    void toggle_bookmark_at_current() {
        if (!require_ayah_row()) return;
        const AyahInfo* a = focused_ayah();
        if (!a) return;
        auto it = std::find(settings.bookmarks.begin(), settings.bookmarks.end(), a->verse_key);
        std::wstring s_name = a->surah_name;   // اسم السورة بالعربية في الواجهتين
        if (it != settings.bookmarks.end()) {
            settings.bookmarks.erase(it);
            say(tr(
                L"أُزيلت الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name + L" من المفضلة",
                L"Removed ayah " + std::to_wstring(a->ayah_number) + L" of " + s_name + L" from bookmarks"
            ));
        } else {
            settings.bookmarks.insert(settings.bookmarks.begin(), a->verse_key);
            say(tr(
                L"أُضيفت الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name + L" إلى المفضلة",
                L"Added ayah " + std::to_wstring(a->ayah_number) + L" of " + s_name + L" to bookmarks"
            ));
        }
        settings.save();
        populate_sidebar_bookmarks_list();
        int s = a->surah_number, ay = a->ayah_number;
        load_current_browse_unit(ay, s);
    }

    // دليل الاستخدام: صفحة HTML مع البرنامج تفتح في المتصفح الافتراضي
    std::wstring user_guide_path() const { return assets_dir + L"\\docs\\guide_ar.html"; }

    bool open_user_guide() {
        const std::wstring path = user_guide_path();
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(hwnd, tr(L"تعذّر العثور على ملف دليل الاستخدام. أعد تثبيت البرنامج لاستعادته.",
                                 L"The user guide file could not be found. Reinstall the program to restore it.").c_str(),
                        tr(L"دليل الاستخدام", L"User guide").c_str(),
                        MB_OK | MB_ICONWARNING | (is_en() ? 0 : MB_RTLREADING | MB_RIGHT));
            return false;
        }
        return (INT_PTR)ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
    }

    // بعد التثبيت، مرة واحدة: سؤال بنعم أو لا عن فتح الدليل. العلامة في السجل لا في ملف الإعدادات،
    // فلا يتكرر السؤال إن حُذفت الإعدادات، ويعود بعد إزالة البرنامج وتثبيته من جديد (المثبّت يحذف المفتاح)
    void offer_user_guide_once() {
        if (is_en()) return;   // الدليل بالعربية؛ يُعرض حين تكون الواجهة عربية
        if (GetFileAttributesW(user_guide_path().c_str()) == INVALID_FILE_ATTRIBUTES) return;
        HKEY hk = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\AlMushafAlSharif", 0, nullptr, 0,
                            KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &hk, nullptr) != ERROR_SUCCESS) return;
        DWORD done = 0, cb = sizeof(done);
        bool offered = RegQueryValueExW(hk, L"GuideOffered", nullptr, nullptr, (LPBYTE)&done, &cb) == ERROR_SUCCESS && done;
        if (!offered) {
            done = 1;
            RegSetValueExW(hk, L"GuideOffered", 0, REG_DWORD, (const BYTE*)&done, sizeof(done));
        }
        RegCloseKey(hk);
        if (offered) return;

        HWND prev_focus = GetFocus();
        int r = MessageBoxW(hwnd,
            L"مرحبًا بك في برنامج المصحف الشريف.\n\n"
            L"هل تودّ الاطلاع على دليل الاستخدام؟ يشرح الدليل أقسام البرنامج، وتقسيمات المصحف، "
            L"وطرق التصفح والبحث والنسخ، واختصارات لوحة المفاتيح.\n\n"
            L"ويمكنك الرجوع إليه متى شئت من قائمة «المساعدة».",
            L"المصحف الشريف", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_RTLREADING | MB_RIGHT);
        if (prev_focus && IsWindow(prev_focus)) SetFocus(prev_focus);
        if (r == IDYES) open_user_guide();
    }

    // ------------------------------------------------------------------
    // البحث عن التحديثات: يقرأ آخر إصدار من صفحة الإصدارات في خيط مستقل، فلا تتأخر الواجهة،
    // ولا ينزّل شيئًا؛ إن وُجد إصدار أحدث سأل المستخدم عن فتح صفحة التنزيل في المتصفح
    bool update_check_running = false;

    struct UpdateThreadArg { HWND hwnd; bool manual; };
    static DWORD WINAPI update_thread_proc(LPVOID p) {
        std::unique_ptr<UpdateThreadArg> arg(static_cast<UpdateThreadArg*>(p));
        auto* r = new UpdateCheckResult(fetch_latest_release());
        r->manual = arg->manual;
        if (!PostMessageW(arg->hwnd, WM_UPDATE_RESULT, 0, (LPARAM)r)) delete r;
        return 0;
    }

    void start_update_check(bool manual) {
        if (update_check_running) {
            if (manual) say(tr(L"جارٍ البحث عن تحديثات", L"Already checking for updates"));
            return;
        }
        update_check_running = true;
        if (manual) say(tr(L"جارٍ البحث عن تحديثات البرنامج", L"Checking for updates"));
        auto* arg = new UpdateThreadArg{hwnd, manual};
        HANDLE th = CreateThread(nullptr, 0, update_thread_proc, arg, 0, nullptr);
        if (th) {
            CloseHandle(th);
        } else {
            delete arg;
            update_check_running = false;
        }
    }

    // البحث التلقائي مرة في اليوم على الأكثر، بعد ثوانٍ من فتح البرنامج
    void maybe_auto_check_updates() {
        if (!settings.auto_check_updates) return;
        SYSTEMTIME st{};
        GetLocalTime(&st);
        wchar_t today[16];
        swprintf(today, 16, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
        if (settings.last_update_check == today) return;
        settings.last_update_check = today;
        settings.save();
        SetTimer(hwnd, kUpdateTimer, 5000, nullptr);
    }

    void on_update_result(UpdateCheckResult* raw) {
        std::unique_ptr<UpdateCheckResult> r(raw);
        update_check_running = false;
        const bool en = is_en();
        const UINT dir = en ? 0 : (MB_RTLREADING | MB_RIGHT);
        const std::wstring title = tr(L"البحث عن تحديثات", L"Check for updates");
        auto ver = [&](const std::wstring& v) { return en ? v : to_arabic_digits(v); };
        HWND prev_focus = GetFocus();
        if (r->status == UpdateCheckResult::Status::Newer) {
            const std::wstring msg = en
                ? (L"A new version of Al-Mushaf Al-Sharif is available: " + r->latest_version +
                   L".\nYour installed version: " + kAppVersion + L".\n\nOpen the download page in your browser?")
                : (L"يتوفر إصدار جديد من برنامج المصحف الشريف: الإصدار " + ver(r->latest_version) +
                   L".\nالإصدار المثبّت لديك: " + ver(kAppVersion) + L".\n\nهل تريد فتح صفحة التنزيل في المتصفح؟");
            int ans = MessageBoxW(hwnd, msg.c_str(), title.c_str(), MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON1 | dir);
            if (ans == IDYES) ShellExecuteW(hwnd, L"open", r->page_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (r->manual) {
            std::wstring msg;
            UINT icon = MB_ICONINFORMATION;
            if (r->status == UpdateCheckResult::Status::UpToDate) {
                msg = en ? (std::wstring(L"You are using the latest version (") + kAppVersion + L").")
                         : (L"أنت تستخدم أحدث إصدار من البرنامج (الإصدار " + ver(kAppVersion) + L").");
            } else if (r->status == UpdateCheckResult::Status::NoRelease) {
                msg = tr(L"لم يُنشر إصدار للبرنامج على صفحة الإصدارات بعد.", L"No version has been published on the releases page yet.");
            } else {
                icon = MB_ICONWARNING;
                msg = tr(L"تعذّر الاتصال بصفحة الإصدارات. تأكد من اتصالك بالإنترنت، ثم حاول مرة أخرى.",
                         L"Could not reach the releases page. Check your internet connection, then try again.");
            }
            MessageBoxW(hwnd, msg.c_str(), title.c_str(), MB_OK | icon | dir);
        }
        if (prev_focus && IsWindow(prev_focus)) SetFocus(prev_focus);
    }

    // اسم الوحدة الحالية بحسب طريقة التصفح، للنطق: «سورة البقرة»، «الصفحة ٥»، «الجزء ٣»...
    std::wstring unit_kind_word() const {
        const std::wstring& b = settings.browse_scope;
        if (b == L"page")   return tr(L"صفحة", L"page");
        if (b == L"juz")    return tr(L"جزء", L"juz");
        if (b == L"hizb")   return tr(L"حزب", L"hizb");
        if (b == L"rub")    return tr(L"ربع", L"quarter");
        if (b == L"manzil") return tr(L"منزلة", L"manzil");
        if (b == L"ruku")   return tr(L"ركوع", L"ruku");
        return tr(L"سورة", L"surah");
    }

    std::wstring current_unit_spoken() const {
        const std::wstring& b = settings.browse_scope;
        auto n = [&](const wchar_t* ar, const wchar_t* en, int v) {
            return is_en() ? (std::wstring(en) + L" " + std::to_wstring(v)) : (std::wstring(ar) + L" " + num_str(v));
        };
        if (b == L"page")   return n(L"الصفحة", L"Page", current_page);
        if (b == L"juz")    return n(L"الجزء", L"Juz", current_juz);
        if (b == L"hizb")   return n(L"الحزب", L"Hizb", current_hizb);
        if (b == L"rub")    return n(L"الربع", L"Quarter", current_rub);
        if (b == L"manzil") return n(L"المنزلة", L"Manzil", current_manzil);
        if (b == L"ruku")   return n(L"الركوع", L"Ruku", current_ruku);
        SurahInfo si = db->get_surah(current_surah);
        return tr(L"سورة " + si.name_arabic, L"Surah " + si.name_arabic);
    }

    std::wstring browse_mode_spoken() const {
        const std::wstring& b = settings.browse_scope;
        if (b == L"page")   return tr(L"التصفح بالصفحات", L"Browsing by page");
        if (b == L"juz")    return tr(L"التصفح بالأجزاء", L"Browsing by juz");
        if (b == L"hizb")   return tr(L"التصفح بالأحزاب", L"Browsing by hizb");
        if (b == L"rub")    return tr(L"التصفح بالأرباع", L"Browsing by quarter");
        if (b == L"manzil") return tr(L"التصفح بالمنازل", L"Browsing by manzil");
        if (b == L"ruku")   return tr(L"التصفح بالركوعات", L"Browsing by ruku");
        return tr(L"التصفح بالسور", L"Browsing by surah");
    }

    // بعد تغيير خط الآيات: الحجم الجديد، أو أنه بلغ أكبر حجم أو أصغره
    void announce_font_size(int before) {
        const int sz = settings.quran_font_size;
        std::wstring v = is_en() ? std::to_wstring(sz) : num_str(sz);
        if (sz == before && sz >= 52)      say(tr(L"خط الآيات في أكبر حجم، " + v, L"Quran font is at its largest, " + v));
        else if (sz == before && sz <= 16) say(tr(L"خط الآيات في أصغر حجم، " + v, L"Quran font is at its smallest, " + v));
        else                               say(tr(L"حجم خط الآيات " + v, L"Quran font size " + v));
    }

    void announce_browse_mode() {
        say(browse_mode_spoken() + L"، " + current_unit_spoken());
    }

    void announce_quick_info() {
        const AyahInfo* a = focused_ayah();
        if (!a) return;
        SurahInfo s = db->get_surah(a->surah_number);
        std::wstring s_name = s.name_arabic;
        std::wstring msg = is_en()
            ? (L"Surah " + s_name + L", ayah " + std::to_wstring(a->ayah_number) + L" of " + std::to_wstring(s.verses_count) +
               L", page " + std::to_wstring(a->page_number) + L" of " + std::to_wstring(db->page_count()) +
               L" in the " + edition_short_label() + L", juz " + std::to_wstring(a->juz_number) +
               L", hizb " + std::to_wstring(a->hizb_number) + L", quarter " + std::to_wstring(a->rub_el_hizb_number) +
               L", manzil " + std::to_wstring(a->manzil_number))
            : (L"سورة " + s_name + L" (" + s.revelation_place + L")، الآية " + std::to_wstring(a->ayah_number) +
               L" من " + std::to_wstring(s.verses_count) + L"، الصفحة " + std::to_wstring(a->page_number) +
               L" من " + std::to_wstring(db->page_count()) + L" في " + edition_short_label() +
               L"، الجزء " + std::to_wstring(a->juz_number) + L"، الحزب " + std::to_wstring(a->hizb_number) +
               L"، الربع " + std::to_wstring(a->rub_el_hizb_number) + L"، المنزلة " + std::to_wstring(a->manzil_number));
        say(msg);
    }

    bool sidebar_fits = true;   // تتسع النافذة للفهرس الجانبي مع صفحة مريحة (يُحسب في التخطيط)
    bool sidebar_shown = false;  // آخر حالة طُبّقت على عناصر الفهرس
    bool sidebar_visible() const { return settings.show_sidebar && !focus_mode && sidebar_fits; }

    void apply_sidebar_visibility(bool sb_vis) {
        if (sb_vis && !sidebar_shown) populate_sidebar_index_list();
        // إن اختفى الفهرس والتركيز فيه ننقله إلى المصحف حتى لا يضيع قارئ الشاشة
        if (!sb_vis && sidebar_shown) {
            HWND f = GetFocus();
            if (f && (f == edit_sidebar_filter || f == tab_sidebar || f == list_sidebar_index || f == list_sidebar_bookmarks ||
                      f == btn_sb_copy_bm || f == btn_sb_del_bm))
                SetFocus(showing_units ? list_units : list_ayahs);
        }
        sidebar_shown = sb_vis;
        int active_tab = tab_sidebar ? (int)SendMessageW(tab_sidebar, TCM_GETCURSEL, 0, 0) : 0;
        ShowWindow(edit_sidebar_filter,    sb_vis ? SW_SHOW : SW_HIDE);
        ShowWindow(tab_sidebar,            sb_vis ? SW_SHOW : SW_HIDE);
        ShowWindow(list_sidebar_index,     (sb_vis && active_tab == 0) ? SW_SHOW : SW_HIDE);
        ShowWindow(list_sidebar_bookmarks, (sb_vis && active_tab == 1) ? SW_SHOW : SW_HIDE);
        ShowWindow(btn_sb_copy_bm,         (sb_vis && active_tab == 1) ? SW_SHOW : SW_HIDE);
        ShowWindow(btn_sb_del_bm,          (sb_vis && active_tab == 1) ? SW_SHOW : SW_HIDE);
    }

    void update_sidebar_visibility() {
        bool ed_vis = !focus_mode && has_multiple_editions();
        ShowWindow(lbl_edition,   ed_vis ? SW_SHOW : SW_HIDE);
        ShowWindow(combo_edition, ed_vis ? SW_SHOW : SW_HIDE);
        ShowWindow(lbl_browse,    focus_mode ? SW_HIDE : SW_SHOW);
        ShowWindow(combo_browse,  focus_mode ? SW_HIDE : SW_SHOW);
        ShowWindow(btn_search,    focus_mode ? SW_HIDE : SW_SHOW);
        ShowWindow(lbl_position,  focus_mode ? SW_HIDE : SW_SHOW);
        sidebar_shown = !sidebar_visible();   // نجبر التطبيق التالي على تحديث العناصر

        RECT rc;
        GetClientRect(hwnd, &rc);
        layout_children(rc.right - rc.left, rc.bottom - rc.top);
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    void cycle_landmarks_f6() {
        HWND foc = GetFocus();
        bool sb_vis = sidebar_visible();
        if (foc == list_ayahs || foc == list_units) {
            if (!focus_mode) SetFocus(combo_browse);
        } else if (foc == combo_browse) {
            if (sb_vis) SetFocus(list_sidebar_index);
            else SetFocus(showing_units ? list_units : list_ayahs);
        } else {
            SetFocus(showing_units ? list_units : list_ayahs);
        }
    }

    void layout_children(int width, int height) {
        int tb_h = focus_mode ? 0 : DS(kToolbarHeight);
        if (!focus_mode) {
            // «الطبعة» أولًا إن وُجدت أكثر من طبعة، ثم «تصفح حسب» وزر البحث، وسطر الموضع في الطرف الآخر.
            // في الشاشة الضيقة تنكمش القوائم والزر، ويُخفى سطر الموضع (موضع القراءة في عنوان النافذة أيضًا)
            const bool ed = has_multiple_editions();
            int ed_w = 220, br_w = 340, se_w = 210;
            const int fixed = 16 + (ed ? 66 + 18 : 0) + 104 + 14 + 16;
            const int want = fixed + (ed ? ed_w : 0) + br_w + se_w;
            const int avail = MulDiv(width, 96, ui_dpi());          // العرض المتاح بمقاسات ٩٦ نقطة
            if (avail < want) {
                se_w = 190;
                double f = std::max(0.45, (double)(avail - fixed - se_w) / (ed_w + br_w));
                ed_w = (int)(ed_w * f); br_w = (int)(br_w * f);
            }
            int x = 16;
            if (ed) {
                MoveWindow(lbl_edition, DS(x), DS(13), DS(62), DS(26), TRUE);
                MoveWindow(combo_edition, DS(x + 66), DS(10), DS(ed_w), DS(320), TRUE);
                x += 66 + ed_w + 18;
            }
            MoveWindow(lbl_browse, DS(x), DS(13), DS(100), DS(26), TRUE);
            MoveWindow(combo_browse, DS(x + 104), DS(10), DS(br_w), DS(320), TRUE);
            MoveWindow(btn_search, DS(x + 104 + br_w + 14), DS(10), DS(se_w), DS(31), TRUE);
            int px = x + 104 + br_w + 14 + se_w + 16;
            const int pos_w = avail - px - 18;
            ShowWindow(lbl_position, pos_w >= 260 ? SW_SHOW : SW_HIDE);
            MoveWindow(lbl_position, DS(px), DS(12), std::max(0, DS(pos_w)), DS(28), TRUE);
        }

        // الإطار المذهّب يحتاج هامشًا يسيرًا فوق الصفحة وتحتها (ويظهر فقط إن اتسعت النافذة للسهمين)
        const bool frame_on = settings.show_mushaf_frame;
        int list_y = tb_h + DS(frame_on ? 24 : 10);
        int list_h = std::max(DS(100), height - list_y - DS(frame_on ? 24 : 12));

        // الفهرس الجانبي يُخفى تلقائيًا إن لم يبق للمصحف عرض مريح (دون تغيير الإعداد نفسه)
        sidebar_fits = (width - DS(330 + 24)) >= DS(kMinWindowWidth - 32);
        bool sb_vis = sidebar_visible();
        apply_sidebar_visibility(sb_vis);
        int sb_w = sb_vis ? DS(330) : 0;
        int sb_x = DS(12);
        int main_x = sb_vis ? (sb_x + sb_w + DS(12)) : DS(12);

        if (sb_vis) {
            MoveWindow(edit_sidebar_filter, sb_x, list_y, sb_w, DS(30), TRUE);
            int tab_y = list_y + DS(36);
            int tab_h = std::max(DS(80), list_h - DS(36));
            MoveWindow(tab_sidebar, sb_x, tab_y, sb_w, tab_h, TRUE);

            int inner_x = sb_x + DS(6);
            int inner_y = tab_y + DS(32);
            int inner_w = sb_w - DS(12);
            int inner_h = std::max(DS(60), tab_h - DS(38));

            MoveWindow(list_sidebar_index, inner_x, inner_y, inner_w, inner_h, TRUE);
            MoveWindow(list_sidebar_bookmarks, inner_x, inner_y, inner_w, std::max(DS(40), inner_h - DS(40)), TRUE);
            int btn_w = (inner_w - DS(8)) / 2;
            MoveWindow(btn_sb_copy_bm, inner_x, inner_y + inner_h - DS(34), btn_w, DS(32), TRUE);
            MoveWindow(btn_sb_del_bm, inner_x + btn_w + DS(8), inner_y + inner_h - DS(34), btn_w, DS(32), TRUE);
        }

        // هامشان على جانبي صفحة المصحف لسهمي التقليب
        invalidate_nav_arrows();
        // من الخارج إلى الداخل: السهم عند حافة النافذة، ثم عمود العلامات، ثم الإطار، ثم الصفحة.
        // إن ضاق المكان (شاشة أصغر من أقل عرض للنافذة) يتنازل السهمان أولًا، ثم يضيق عمود العلامات
        const int arrow_d = DS(kArrowDiameter), gap = DS(kMarkFrameGap), frame_out = frame_on ? DS(16) : DS(4);
        const int lx = sb_vis ? (main_x + DS(2)) : DS(kArrowEdge);
        const int rx = width - DS(kArrowEdge) - arrow_d;
        int col_w = DS(kPageMarginWidth);
        auto room = [&](bool with_arrows, int col) {
            const int l = with_arrows ? (lx + arrow_d + DS(4)) : main_x;
            const int r_edge = with_arrows ? (rx - DS(4)) : (width - DS(12));
            return r_edge - l - 2 * (col + gap + frame_out);
        };
        const bool arrows_on = room(true, col_w) >= DS(kPageMinWidth);
        if (!arrows_on && room(false, col_w) < DS(kPageMinWidth)) {
            col_w = std::max(DS(52), col_w - (DS(kPageMinWidth) - room(false, col_w) + 1) / 2);
        }
        const bool outside = room(arrows_on, col_w) >= DS(200);
        if (outside != marks_outside) {
            marks_outside = outside;
            if (ayah_rows_width != 0) ayah_rows_width = -1;   // يُعاد بناء الصفوف بالعلامات أو بدونها
        }
        const int side = outside ? (col_w + gap + frame_out) : frame_out;
        const int left_edge = arrows_on ? (lx + arrow_d + DS(4)) : main_x;
        const int right_edge = arrows_on ? (rx - DS(4)) : (width - DS(12));
        int page_x = left_edge + side;
        int page_w = std::max(DS(200), right_edge - left_edge - 2 * side);
        // في الشاشة الكبيرة لا تتمدد الصفحة بعرضها كله (سطر طويل يُتعب العين ويباعد كلمات سطور المصحف):
        // للصفحة عرض أقصى يتبع حجم خط القرآن، وتتوسط المساحة ومعها إطارها وعلامات هامشها
        const int em = MulDiv(settings.quran_font_size, ui_dpi(), 72);
        const int max_page_w = std::max(DS(kPageMaxWidth), em * kPageMaxEm);
        if (page_w > max_page_w) {
            page_x += (page_w - max_page_w) / 2;
            page_w = max_page_w;
        }
        {
            const int outer_l = page_x - frame_out, outer_r = page_x + page_w + frame_out;
            mark_col_left = {outer_l - gap - col_w, list_y, outer_l - gap, list_y + list_h};
            mark_col_right = {outer_r + gap, list_y, outer_r + gap + col_w, list_y + list_h};
        }
        int cy = list_y + list_h / 2;
        int r = arrow_d / 2;
        // السهمان عند حافتي النافذة (أو حافة الفهرس الجانبي)، وعلامتا ملء الشاشة والإغلاق في ركنيها
        const int ax_l = lx, ax_r = rx;
        RECT logical_left = {ax_l, cy - r, ax_l + DS(kArrowDiameter), cy + r};
        RECT logical_right = {ax_r, cy - r, ax_r + DS(kArrowDiameter), cy + r};
        // في النافذة المعكوسة يكون الطرف المنطقي الأيمن هو اليسار المرئي
        bool rtl = (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
        arrow_rect_next = rtl ? logical_right : logical_left;
        arrow_rect_prev = rtl ? logical_left : logical_right;
        if (!arrows_on) {
            SetRectEmpty(&arrow_rect_next);
            SetRectEmpty(&arrow_rect_prev);
        }
        // زر الخروج من ملء الشاشة في الركن العلوي: الأيسر في الواجهة العربية (الطرف المنطقي الأيمن)
        // والأيمن في الإنجليزية، كزر الإغلاق في ويندوز، داخل هامش السهم فلا يغطي النص
        // خارج وضع التركيز يكون الزر نفسه للدخول إلى ملء الشاشة (علامة تكبير)، في هامش السهم فلا يغيّر شيئًا في الشاشة
        if (focus_mode || arrows_on) exit_full_rect = {rx, list_y, rx + DS(kArrowDiameter), list_y + DS(kArrowDiameter)};
        else SetRectEmpty(&exit_full_rect);
        // في ملء الشاشة لا شريط عنوان، فزر الإغلاق في الركن العلوي الآخر (لا يُخلط بزر الخروج من ملء الشاشة)
        if (focus_mode) close_app_rect = {lx, list_y, lx + DS(kArrowDiameter), list_y + DS(kArrowDiameter)};
        else SetRectEmpty(&close_app_rect);
        invalidate_nav_arrows();

        if (frame_on) {
            frame_rect = {page_x - DS(16), list_y - DS(16), page_x + page_w + DS(16), list_y + list_h + DS(16)};
        } else {
            SetRectEmpty(&frame_rect);
        }
        InvalidateRect(hwnd, nullptr, TRUE);

        MoveWindow(list_units, page_x, list_y, page_w, list_h, TRUE);
        MoveWindow(list_ayahs, page_x, list_y, page_w, list_h, TRUE);

        // إعادة تقسيم الآيات على العرض الجديد بعد أن يهدأ تغيير الحجم
        if (list_ayahs && ayah_rows_width != 0 && ayah_list_client_width() != ayah_rows_width) {
            SetTimer(hwnd, kRelayoutTimer, 150, nullptr);
        }
    }
};

static MushafAppWindow* g_app = nullptr;
static WNDPROC g_orig_unit_list_proc = nullptr;
static WNDPROC g_orig_sb_index_proc = nullptr;
static WNDPROC g_orig_sb_bm_proc = nullptr;

// موضع قائمة السياق عند فتحها من لوحة المفاتيح: وسط الصف المحدد
static void context_menu_point(HWND list, int& x, int& y) {
    if (x != -1 || y != -1) return;
    int sel = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
    RECT rc = {20, 20, 200, 50};
    if (sel >= 0) SendMessageW(list, LB_GETITEMRECT, sel, (LPARAM)&rc);
    POINT pt = {(rc.left + rc.right) / 2, rc.top + std::min(24L, (rc.bottom - rc.top) / 2)};
    ClientToScreen(list, &pt);
    x = pt.x;
    y = pt.y;
}

// قائمة السورة: معلوماتها ونسخ بيانها أو نصها كاملًا والانتقال لأولها (مع «فتح» في فهرس السور)
static void show_surah_context_menu(int x, int y, int surah_num, bool with_open, int open_unit) {
    if (!g_app || surah_num < 1) return;
    const UINT ID_CTX_OPEN = 9001;
    HMENU menu = CreatePopupMenu();
    if (with_open) {
        AppendMenuW(menu, MF_STRING, ID_CTX_OPEN, g_app->tr(L"فتح السورة\tEnter", L"Open the surah\tEnter").c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    std::wstring sc_si = g_app->sc(IDM_SURAH_INFO);
    AppendMenuW(menu, MF_STRING, IDM_SURAH_INFO,
                (g_app->tr(L"معلومات عن السورة...", L"Surah information...") + (sc_si.empty() ? L"" : (L"\t" + sc_si))).c_str());
    AppendMenuW(menu, MF_STRING, IDM_COPY_SURAH_HEAD, g_app->tr(L"نسخ بيانات السورة...", L"Copy surah details...").c_str());
    AppendMenuW(menu, MF_STRING, IDM_COPY_SURAH_FULL, g_app->tr(L"نسخ السورة كاملة", L"Copy the whole surah").c_str());
    if (!with_open) {
        std::wstring sc_mc = g_app->sc(IDM_MULTI_COPY);
        AppendMenuW(menu, MF_STRING, IDM_MULTI_COPY,
                    (g_app->tr(L"النسخ المتعدد...", L"Multiple copy...") + (sc_mc.empty() ? L"" : (L"\t" + sc_mc))).c_str());
    }
    if (!with_open) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, IDM_SURAH_START, g_app->tr(L"الانتقال إلى أول السورة", L"Go to the start of the surah").c_str());
    }
    UINT chosen = (UINT)TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, x, y, 0, g_app->hwnd, nullptr);
    DestroyMenu(menu);
    if (chosen == 0) return;
    if (chosen == ID_CTX_OPEN) {
        g_app->open_unit_by_number(open_unit);
        return;
    }
    g_app->pending_surah_info_num = surah_num;
    SendMessageW(g_app->hwnd, WM_COMMAND, chosen, 0);
}

// قائمة السياق (زر التطبيقات / Shift+F10 / الزر الأيمن) على قائمة الآيات:
// على الآية تظهر أوامر الآية فقط، وعلى عنوان السورة أو بيانها أو البسملة تظهر أوامر السورة
static void show_ayah_context_menu(HWND /*hwnd*/, int x, int y) {
    if (!g_app) return;
    const AyahInfo* a = g_app->focused_ayah();
    if (!a) return;
    context_menu_point(g_app->list_ayahs, x, y);

    if (!g_app->current_row_is_ayah()) {
        show_surah_context_menu(x, y, a->surah_number, false, 0);
        return;
    }

    bool in_bm = std::find(g_app->settings.bookmarks.begin(), g_app->settings.bookmarks.end(), a->verse_key) != g_app->settings.bookmarks.end();
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_COPY_AYAH,       g_app->tr(L"نسخ\tEnter", L"Copy\tEnter").c_str());
    {
        std::wstring sc_mc = g_app->sc(IDM_MULTI_COPY);
        AppendMenuW(menu, MF_STRING, IDM_MULTI_COPY,
                    (g_app->tr(L"النسخ المتعدد...", L"Multiple copy...") + (sc_mc.empty() ? L"" : (L"\t" + sc_mc))).c_str());
    }
    if (!g_app->marked_ids.empty()) {
        AppendMenuW(menu, MF_STRING, IDM_CLEAR_MARKS, g_app->tr(L"إلغاء التظليل", L"Clear highlighting").c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    std::wstring sc_ai = g_app->sc(IDM_AYAH_INFO);
    AppendMenuW(menu, MF_STRING, IDM_AYAH_INFO,
                (g_app->tr(L"معلومات عن الآية...", L"Ayah information...") + (sc_ai.empty() ? L"" : (L"\t" + sc_ai))).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TOGGLE_BOOKMARK, (in_bm ? g_app->tr(L"إزالة الآية من المفضلة\tB", L"Remove from bookmarks\tB")
                                                             : g_app->tr(L"إضافة الآية إلى المفضلة\tB", L"Add to bookmarks\tB")).c_str());
    const bool on_mark = (a->verse_key == g_app->settings.khatmah_verse_key);
    AppendMenuW(menu, MF_STRING, IDM_SET_KHATMAH, (on_mark ? g_app->tr(L"إزالة فاصل الختمة\tM", L"Remove the reading mark\tM")
                                                         : g_app->tr(L"وضع فاصل الختمة عند الآية\tM", L"Set the reading mark here\tM")).c_str());

    TrackPopupMenu(menu, TPM_RIGHTBUTTON, x, y, 0, g_app->hwnd, nullptr);
    DestroyMenu(menu);
}

static void show_unit_context_menu(HWND list_hwnd, int x, int y, const std::vector<int>& nums) {
    if (!g_app) return;
    int sel = (int)SendMessageW(list_hwnd, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)nums.size()) return;
    context_menu_point(list_hwnd, x, y);
    int unit_num = nums[sel];

    // في فهرس السور: أوامر السورة كلها مع «فتح»
    if (g_app->settings.browse_scope == L"surah") {
        show_surah_context_menu(x, y, unit_num, true, unit_num);
        return;
    }
    const UINT ID_CTX_OPEN = 9001;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_CTX_OPEN, g_app->tr(L"فتح\tEnter", L"Open\tEnter").c_str());
    UINT chosen = (UINT)TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, x, y, 0, g_app->hwnd, nullptr);
    DestroyMenu(menu);
    if (chosen == ID_CTX_OPEN) g_app->open_unit_by_number(unit_num);
}

// موضع زر «نسخ» المضمّن: في نهاية سطر الآية (يسار الآية العربية بصريًا)
static RECT inline_copy_button_rect(const RECT& item_rc, bool rtl_layout) {
    RECT r;
    r.top = item_rc.top + 8;
    r.bottom = item_rc.top + 38;
    if (rtl_layout) {   // في التخطيط المعكوس يكون الطرف المنطقي الأيمن هو اليسار المرئي
        r.right = item_rc.right - 12;
        r.left = r.right - (kInlineBtnWidth - 4);
    } else {
        r.left = item_rc.left + 12;
        r.right = r.left + (kInlineBtnWidth - 4);
    }
    return r;
}

// مقاسات الزخرفة بالنسبة إلى عرضها
struct MarginShapeMetrics { int stem, cap, top, bottom, side; };
static MarginShapeMetrics margin_shape_metrics(int w, MarginShape shape) {
    MarginShapeMetrics m{};
    m.stem = (shape == MarginShape::Small) ? 0 : w * 20 / 100;
    m.cap = (shape == MarginShape::Small) ? w * 30 / 100 : w * 40 / 100;
    m.side = w * 13 / 100;
    // لوحة النص تبدأ بعد الكتفين وحاشيتها، فلا يلامس النص إطارها
    m.top = m.stem + m.cap + w * 16 / 100;
    m.bottom = (shape == MarginShape::Arch) ? w * 14 / 100 : m.top;
    return m;
}

// الكلمة الأساسية في العلامة تُكتب أكبر، كما في حواشي المصحف: الحزب والجزء والسجدة والرقم
static bool margin_line_is_big(const std::wstring& raw) {
    std::wstring ln;
    for (wchar_t ch : raw) if (!((ch >= 0x064B && ch <= 0x0652) || ch == 0x0670)) ln.push_back(ch);
    if (ln.empty()) return false;
    bool digits = true;
    for (wchar_t ch : ln) if (!((ch >= L'0' && ch <= L'9') || (ch >= 0x0660 && ch <= 0x0669))) digits = false;
    if (digits) return true;
    for (const wchar_t* k : {L"الحزب", L"الجزء", L"سجدة", L"سجدة تلاوة", L"۩ سجدة", L"Hizb", L"Juz", L"Sajdah", L"۩ Sajdah"}) {
        const std::wstring key(k);
        if (ln == key || (ln.compare(0, key.size(), key) == 0 && ln.size() > key.size() && ln[key.size()] == L' ' &&
                          (key == L"الجزء" || key == L"الحزب" || key == L"Juz" || key == L"Hizb"))) return true;
    }
    return false;
}

// ارتفاع الزخرفة بعرض عمود الهامش، وارتفاع كل سطر فيها
static int margin_mark_height(HDC hdc, int width, const std::vector<std::wstring>& lines, HFONT font, HFONT big,
                              MarginShape shape, std::vector<int>* heights) {
    const MarginShapeMetrics m = margin_shape_metrics(width, shape);
    HGDIOBJ old_f = SelectObject(hdc, font);
    const int text_w = std::max(10, width - 2 * m.side);
    int text_h = 0;
    for (const auto& ln : lines) {
        SelectObject(hdc, (big && margin_line_is_big(ln)) ? big : font);
        RECT c = {0, 0, text_w, 0};
        DrawTextW(hdc, ln.c_str(), (int)ln.size(), &c, DT_CALCRECT | DT_WORDBREAK | DT_CENTER | DT_NOPREFIX | dt_reading_flag(hdc, true));
        if (heights) heights->push_back(c.bottom - c.top);
        text_h += c.bottom - c.top;
    }
    SelectObject(hdc, old_f);
    return text_h + 2 * DS(4) + m.top + m.bottom;
}

// ترسم الزخرفة مكبّرة أربع مرات ثم تُصغَّر فتنعم حوافها، وتُحفظ لتكرارها دون إعادة رسم
static HBITMAP margin_ornament_bitmap(HDC ref, int w, int h, MarginShape shape, COLORREF bg) {
    static std::map<unsigned long long, HBITMAP> cache;
    const unsigned long long key = ((unsigned long long)bg << 32) | ((unsigned long long)(w & 0x3FFF) << 18) |
                                   ((unsigned long long)(h & 0x3FFF) << 4) | (unsigned long long)shape;
    auto found = cache.find(key);
    if (found != cache.end()) return found->second;

    const int ss = 4, W = w * ss, H = h * ss;
    const MarginShapeMetrics mm = margin_shape_metrics(w, shape);
    const int stem = mm.stem * ss, cap = mm.cap * ss, cx = W / 2;
    const bool both_ends = shape != MarginShape::Arch;
    const COLORREF sage = RGB(166, 186, 164), sage_dark = RGB(84, 108, 88), white = RGB(252, 253, 250),
                   panel = RGB(226, 234, 224), green = RGB(46, 134, 66), green_dark = RGB(24, 84, 42),
                   pink = RGB(232, 84, 168);

    HDC dc = CreateCompatibleDC(ref);
    HBITMAP big = CreateCompatibleBitmap(ref, W, H);
    HGDIOBJ old_big = SelectObject(dc, big);
    RECT all = {0, 0, W, H};
    HBRUSH bg_br = CreateSolidBrush(bg);
    FillRect(dc, &all, bg_br);
    DeleteObject(bg_br);

    auto poly = [&](const std::vector<POINT>& p, COLORREF fill, COLORREF line, int pen_w) {
        HBRUSH b = CreateSolidBrush(fill);
        HPEN n = CreatePen(PS_SOLID, pen_w, line);
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, n);
        Polygon(dc, p.data(), (int)p.size());
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(b); DeleteObject(n);
    };
    // حدود الزخرفة بعد إنقاصها من كل جهة: كتفان، ثم قبة مدببة، ثم مثلها أسفل الشمسة
    auto outline = [&](int inset) {
        const int step = W * 9 / 100, shoulder = stem + cap + inset, peak = stem + inset * 3 / 2;
        const int sx = inset + step;
        std::vector<POINT> top;
        top.push_back({inset, shoulder});
        top.push_back({sx, shoulder});
        const int n = 16;
        for (int i = 1; i < n; ++i) {
            const double t = (double)i / n;
            const double f = (1 - (1 - t) * (1 - t)) * 0.55 + t * t * t * t * 0.45;
            top.push_back({sx + (LONG)((cx - sx) * t), shoulder + (LONG)((peak - shoulder) * f)});
        }
        top.push_back({cx, peak});
        for (int i = (int)top.size() - 2; i >= 0; --i) top.push_back({W - top[i].x, top[i].y});
        std::vector<POINT> all_pts = top;
        if (both_ends) {
            for (int i = (int)top.size() - 1; i >= 0; --i) all_pts.push_back({top[i].x, H - top[i].y});
        } else {
            all_pts.push_back({W - inset, H - inset});
            all_pts.push_back({inset, H - inset});
        }
        return all_pts;
    };
    auto leaf = [&](double x0, double y0, double x1, double y1, double half) {
        const double dx = x1 - x0, dy = y1 - y0, len = std::max(1.0, std::sqrt(dx * dx + dy * dy));
        const double nx = -dy / len, ny = dx / len;
        std::vector<POINT> p;
        const int n = 12;
        for (int side = 1; side >= -1; side -= 2) {
            for (int i = 0; i <= n; ++i) {
                const double t = (side == 1) ? (double)i / n : 1.0 - (double)i / n;
                const double b = std::sin(3.14159265 * std::pow(t, 0.72)) * half * side;
                p.push_back({(LONG)(x0 + dx * t + nx * b), (LONG)(y0 + dy * t + ny * b)});
            }
        }
        poly(p, green, green_dark, std::max(1, ss / 2));
    };
    // زخرفة القبة: ورقة وسطى وورقتان جانبيتان وقاعدة وردية؛ dir = 1 للأعلى و-1 للأسفل
    auto crown = [&](int dir) {
        const double base = (dir > 0) ? stem + cap + W * 0.03 : H - stem - cap - W * 0.03;
        const double r = W * 0.075, tip = cap * 0.70, spread = W * 0.25;
        leaf(cx, base, cx, base - dir * tip, W * 0.075);
        leaf(cx, base, cx - spread, base - dir * cap * 0.32, W * 0.055);
        leaf(cx, base, cx + spread, base - dir * cap * 0.32, W * 0.055);
        HBRUSH b = CreateSolidBrush(pink);
        HPEN n = CreatePen(PS_SOLID, std::max(1, ss / 2), green_dark);
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, n);
        Ellipse(dc, (int)(cx - r), (int)(base - r), (int)(cx + r), (int)(base + r));
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(b); DeleteObject(n);
    };
    auto stem_line = [&](int y0, int y1) {
        HPEN n = CreatePen(PS_SOLID, std::max(2, ss * 3 / 4), sage_dark);
        HGDIOBJ op = SelectObject(dc, n);
        MoveToEx(dc, cx, y0, nullptr);
        LineTo(dc, cx, y1);
        SelectObject(dc, op);
        DeleteObject(n);
        const int my = (y0 + y1) / 2, d = W * 3 / 100;
        poly({{cx, my - d}, {cx + d, my}, {cx, my + d}, {cx - d, my}}, sage_dark, sage_dark, 1);
    };

    if (stem > 0) {
        stem_line(0, stem + W / 50);
        if (both_ends) stem_line(H - stem - W / 50, H);
    }
    const int i1 = W * 4 / 100, i2 = W * 7 / 100, i3 = W * 10 / 100;
    poly(outline(0), sage, sage_dark, ss);
    poly(outline(i1), white, sage_dark, std::max(1, ss / 2));
    crown(1);
    if (both_ends) crown(-1);
    // لوحة النص
    const int panel_top = stem + cap + i2 + W * 6 / 100;
    const int panel_bottom = both_ends ? H - panel_top : H - i3;
    poly({{i2, panel_top}, {W - i2, panel_top}, {W - i2, panel_bottom}, {i2, panel_bottom}}, sage, sage_dark, std::max(1, ss / 2));
    poly({{i3, panel_top + i3 - i2}, {W - i3, panel_top + i3 - i2}, {W - i3, panel_bottom - (i3 - i2)}, {i3, panel_bottom - (i3 - i2)}},
         panel, sage_dark, std::max(1, ss / 2));

    HDC out_dc = CreateCompatibleDC(ref);
    HBITMAP out = CreateCompatibleBitmap(ref, w, h);
    HGDIOBJ old_out = SelectObject(out_dc, out);
    SetStretchBltMode(out_dc, HALFTONE);
    SetBrushOrgEx(out_dc, 0, 0, nullptr);
    StretchBlt(out_dc, 0, 0, w, h, dc, 0, 0, W, H, SRCCOPY);
    SelectObject(out_dc, old_out);
    DeleteDC(out_dc);
    SelectObject(dc, old_big);
    DeleteObject(big);
    DeleteDC(dc);
    cache[key] = out;
    return out;
}

// زخرفة في هامش الصفحة على طراز حواشي المصحف الورقي، وفيها نص العلامة
static void draw_margin_mark(HDC hdc, const RECT& box, const std::vector<std::wstring>& lines,
                             const ThemePalette& pal, HFONT font, HFONT big, MarginShape shape) {
    const int w = box.right - box.left, h = box.bottom - box.top;
    const MarginShapeMetrics m = margin_shape_metrics(w, shape);
    std::vector<int> heights;
    margin_mark_height(hdc, w, lines, font, big, shape, &heights);
    if (HBITMAP bmp = margin_ornament_bitmap(hdc, w, h, shape, pal.window_bg)) {
        HDC mem = CreateCompatibleDC(hdc);
        HGDIOBJ old = SelectObject(mem, bmp);
        BitBlt(hdc, box.left, box.top, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
    }
    HGDIOBJ old_f = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 48, 34));
    int y = box.top + m.top + DS(4);
    for (size_t i = 0; i < lines.size(); ++i) {
        SelectObject(hdc, (big && margin_line_is_big(lines[i])) ? big : font);
        RECT lr = {box.left + m.side, y, box.right - m.side, y + heights[i]};
        DrawTextW(hdc, lines[i].c_str(), (int)lines[i].size(), &lr, DT_WORDBREAK | DT_CENTER | DT_NOPREFIX | dt_reading_flag(hdc, true));
        y += heights[i];
    }
    SelectObject(hdc, old_f);
}

// تفصيل الحروف: كل حرف وعدد مرات وروده، بترتيب الحروف (الألف الوصلية تُعدّ ألفًا)
static std::wstring letter_frequency_text(const std::vector<const std::wstring*>& texts) {
    std::map<wchar_t, int> freq;
    for (const std::wstring* t : texts) {
        for (wchar_t ch : *t) {
            if ((ch >= 0x0621 && ch <= 0x064A) || ch == 0x0671) ++freq[ch == 0x0671 ? (wchar_t)0x0627 : ch];
        }
    }
    std::wstring out;
    for (const auto& kv : freq) {
        if (!out.empty()) out += L"، ";
        out += std::wstring(1, kv.first) + L" (" + std::to_wstring(kv.second) + L")";
    }
    return out;
}

static LRESULT CALLBACK AyahListSubclassProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// كلما تحرّك أول صف ظاهر في قائمة الآيات (تمرير أو انتقال) تُعاد رسم علامات الهامش خارج الإطار معها
static LRESULT CALLBACK AyahListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    const bool track = g_app && g_app->marks_outside && g_orig_ayah_list_proc;
    const LRESULT before = track ? CallWindowProcW(g_orig_ayah_list_proc, hwnd, LB_GETTOPINDEX, 0, 0) : 0;
    LRESULT r = AyahListSubclassProcImpl(hwnd, msg, wParam, lParam);
    if (track && msg != WM_NCDESTROY && IsWindow(hwnd) &&
        CallWindowProcW(g_orig_ayah_list_proc, hwnd, LB_GETTOPINDEX, 0, 0) != before) {
        g_app->invalidate_outside_marks();
    }
    return r;
}

static LRESULT CALLBACK AyahListSubclassProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOUSEMOVE && g_app) g_app->poke_controls();   // حركة الفأرة فوق النص تُظهر العلامات
    if (msg == WM_LBUTTONDOWN && g_app && g_app->settings.show_inline_copy_btn) {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        int sel = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < (int)g_app->ayah_items.size() &&
            g_app->ayah_items[sel].kind == ListItemKind::Ayah && g_app->ayah_items[sel].seg_index == 0) {
            RECT rc{};
            SendMessageW(hwnd, LB_GETITEMRECT, sel, (LPARAM)&rc);
            RECT btn_rc = inline_copy_button_rect(rc, g_app->list_is_rtl(hwnd));
            POINT pt = {mx, my};
            if (PtInRect(&btn_rc, pt)) {
                g_app->copy_current_ayah(false);
                return 0;
            }
        }
    }
    if (msg == WM_LBUTTONDOWN && g_app) {
        // بداية سحب محتمل للتظليل
        LRESULT hit = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, lParam);
        g_app->mark_dragging = (HIWORD(hit) == 0);
        g_app->mark_drag_start = g_app->mark_dragging ? (int)LOWORD(hit) : -1;
        g_app->mark_drag_end = g_app->mark_drag_start;
        g_app->mark_drag_origin = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        // النقر على صف تكميلي من آية طويلة يحدد الآية كلها من أول صف فيها
        LRESULT r = CallWindowProcW(g_orig_ayah_list_proc, hwnd, msg, wParam, lParam);
        g_app->snap_ayah_selection(false);
        return r;
    }
    if (msg == WM_MOUSEMOVE && g_app && g_app->mark_dragging && (wParam & MK_LBUTTON)) {
        // الصف تحت المؤشر يُقرأ قبل أن تمرّر القائمة نفسها أثناء السحب
        LRESULT hit = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, lParam);
        int row = (int)LOWORD(hit);
        LRESULT r = CallWindowProcW(g_orig_ayah_list_proc, hwnd, msg, wParam, lParam);
        int dx = GET_X_LPARAM(lParam) - g_app->mark_drag_origin.x, dy = GET_Y_LPARAM(lParam) - g_app->mark_drag_origin.y;
        bool moved = (dx * dx + dy * dy) > 36;   // أكثر من ٦ بكسل: سحب لا نقرة
        if (moved && row != g_app->mark_drag_end) {
            g_app->mark_drag_end = row;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return r;
    }
    if (msg == WM_LBUTTONUP && g_app && g_app->mark_dragging) {
        LRESULT r = CallWindowProcW(g_orig_ayah_list_proc, hwnd, msg, wParam, lParam);
        int dx = GET_X_LPARAM(lParam) - g_app->mark_drag_origin.x, dy = GET_Y_LPARAM(lParam) - g_app->mark_drag_origin.y;
        bool moved = (dx * dx + dy * dy) > 36;
        int start = g_app->mark_drag_start, end = g_app->mark_drag_end;
        g_app->mark_dragging = false;
        if (moved && start >= 0) {
            g_app->mark_rows(start, end);            // سحب: يضيف النطاق إلى التظليل السابق
        } else if (start >= 0 && (wParam & MK_CONTROL)) {
            g_app->toggle_mark_row(start);           // Ctrl+نقرة: تظليل آية واحدة أو إلغاؤه
        } else {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return r;
    }
    if (msg == WM_CONTEXTMENU && g_app) {
        show_ayah_context_menu(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    }
    if (msg == WM_KEYDOWN && g_app) {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

        if (wParam == VK_APPS || (wParam == VK_F10 && shift)) {
            show_ayah_context_menu(hwnd, -1, -1);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (g_app->focus_mode) {
                SendMessageW(g_app->hwnd, WM_COMMAND, IDM_FOCUS_MODE, 0);
            } else {
                g_app->show_units_view(true);
            }
            return 0;
        }
        if (wParam == VK_NEXT && !ctrl && !alt && !shift) { // PageDown
            g_app->navigate_unit(+1);
            return 0;
        }
        if (wParam == VK_PRIOR && !ctrl && !alt && !shift) { // PageUp
            g_app->navigate_unit(-1);
            return 0;
        }
        if (wParam == VK_RETURN || wParam == VK_F8 || (wParam == 'C' && (ctrl || alt || !shift))) {
            g_app->copy_current_ayah(false);
            return 0;
        }
        if (wParam == 'B' && !ctrl && !alt) {
            g_app->toggle_bookmark_at_current();
            return 0;
        }
        if (wParam == 'M' && ((!ctrl && !alt) || (ctrl && !alt && !shift))) {
            g_app->set_khatmah_at_current();
            return 0;
        }
        if (wParam == 'I' && !ctrl && !alt && !shift) {
            if (g_app->require_ayah_row()) g_app->announce_quick_info();
            return 0;
        }
        if (wParam == VK_F6) {
            g_app->cycle_landmarks_f6();
            return 0;
        }
        if (wParam == VK_DOWN || wParam == VK_END) {
            // في آخر آية بالصفحة أو السورة أو الجزء لا حركة: لو تركنا القائمة تنزل إلى صفها التكميلي
            // ثم أرجعناها لأول صف، لأعاد قارئ الشاشة قراءة الآية نفسها مع كل ضغطة سهم
            int sel = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && MushafAppWindow::next_item_start_row(g_app->ayah_items, sel) < 0) return 0;
        }
        if (wParam == VK_DOWN || wParam == VK_UP || wParam == VK_HOME || wParam == VK_END) {
            LRESULT r = CallWindowProcW(g_orig_ayah_list_proc, hwnd, msg, wParam, lParam);
            g_app->snap_ayah_selection(wParam == VK_DOWN);
            return r;
        }
    }
    if (msg == WM_CHAR) {
        return 0;
    }
    return CallWindowProcW(g_orig_ayah_list_proc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK UnitListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_LBUTTONUP && g_app) {
        LRESULT r = CallWindowProcW(g_orig_unit_list_proc, hwnd, msg, wParam, lParam);
        LRESULT hit = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, lParam);
        if (HIWORD(hit) == 0) {
            int idx = LOWORD(hit);
            if (idx >= 0 && idx < (int)g_app->unit_numbers.size()) {
                g_app->open_unit_by_number(g_app->unit_numbers[idx]);
            }
        }
        return r;
    }
    if (msg == WM_CONTEXTMENU && g_app) {
        show_unit_context_menu(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_app->unit_numbers);
        return 0;
    }
    if (msg == WM_KEYDOWN && g_app) {
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (wParam == VK_APPS || (wParam == VK_F10 && shift)) {
            show_unit_context_menu(hwnd, -1, -1, g_app->unit_numbers);
            return 0;
        }
        if (wParam == VK_RETURN) {
            int sel = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->unit_numbers.size()) {
                g_app->open_unit_by_number(g_app->unit_numbers[sel]);
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            g_app->show_ayahs_view();
            return 0;
        }
        if (wParam == VK_F6) {
            g_app->cycle_landmarks_f6();
            return 0;
        }
    }
    return CallWindowProcW(g_orig_unit_list_proc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK SidebarIndexSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CONTEXTMENU && g_app) {
        show_unit_context_menu(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), g_app->sidebar_unit_numbers);
        return 0;
    }
    if (msg == WM_KEYDOWN && g_app) {
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (wParam == VK_APPS || (wParam == VK_F10 && shift)) {
            show_unit_context_menu(hwnd, -1, -1, g_app->sidebar_unit_numbers);
            return 0;
        }
        if (wParam == VK_RETURN) {
            int sel = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->sidebar_unit_numbers.size()) {
                g_app->open_unit_by_number(g_app->sidebar_unit_numbers[sel]);
            }
            return 0;
        }
        if (wParam == VK_ESCAPE || wParam == VK_F6) {
            g_app->cycle_landmarks_f6();
            return 0;
        }
    }
    return CallWindowProcW(g_orig_sb_index_proc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK SidebarBookmarksSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && g_app) {
        if (wParam == VK_RETURN) {
            int sel = (int)SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->sidebar_bm_ayahs.size()) {
                g_app->jump_to_ayah(g_app->sidebar_bm_ayahs[sel]);
            }
            return 0;
        }
        if (wParam == VK_DELETE) {
            SendMessageW(g_app->hwnd, WM_COMMAND, ID_BTN_SB_DEL_BM, 0);
            return 0;
        }
        if (wParam == VK_F6) {
            g_app->cycle_landmarks_f6();
            return 0;
        }
    }
    return CallWindowProcW(g_orig_sb_bm_proc, hwnd, msg, wParam, lParam);
}

// بيانات السورة سطرًا سطرًا: لنافذة معلوماتها ولنافذة نسخ بياناتها
static std::vector<std::wstring> build_surah_info_lines(int target_surah_num, bool vocalized = false) {
            SurahInfo s = g_app->db->get_surah(target_surah_num);
            std::wstring s_name = s.name_arabic;
            auto s_ayahs = g_app->db->get_surah_ayahs(s.number);
            int total_words = 0, total_letters = 0, total_allah = 0, sajdahs = 0;
            int min_juz = 99, max_juz = 0, min_ruku = 9999, max_ruku = 0;
            int longest_ayah = 0, longest_letters = -1;
            int shortest_ayah = 0, shortest_letters = 999999;
            const bool edition_details = g_app->db->edition_has_details();
            std::vector<const std::wstring*> surah_texts;
            for (const auto& ay : s_ayahs) surah_texts.push_back(&ay.text_simple);
            const std::wstring surah_letter_freq = letter_frequency_text(surah_texts);
            for (const auto& ay : s_ayahs) {
                min_juz = std::min(min_juz, ay.juz_number);
                max_juz = std::max(max_juz, ay.juz_number);
                min_ruku = std::min(min_ruku, ay.ruku_number);
                max_ruku = std::max(max_ruku, ay.ruku_number);
                if (print_sajdah_place(ay.verse_key, edition_details)) ++sajdahs;

                int ay_letters = count_arabic_letters(ay.text_simple);
                if (ay_letters > longest_letters) { longest_letters = ay_letters; longest_ayah = ay.ayah_number; }
                if (ay_letters < shortest_letters) { shortest_letters = ay_letters; shortest_ayah = ay.ayah_number; }

                std::wstring word;
                auto flush_w = [&]() {
                    if (word.empty()) return;
                    bool has_l = false;
                    int lc = 0;
                    std::wstring norm;
                    for (wchar_t ch : word) {
                        if ((ch >= 0x0621 && ch <= 0x064A) || ch == 0x0671) {
                            has_l = true;
                            ++lc;
                            wchar_t n = ch;
                            if (n == 0x0623 || n == 0x0625 || n == 0x0622 || n == 0x0671) n = 0x0627;
                            else if (n == 0x0649) n = 0x064A;
                            else if (n == 0x0629) n = 0x0647;
                            norm.push_back(n);
                        }
                    }
                    if (has_l) {
                        ++total_words;
                        total_letters += lc;
                        if (norm == L"الله" || norm == L"لله" || norm == L"والله" || norm == L"بالله" ||
                            norm == L"فالله" || norm == L"تالله" || norm == L"ولله" || norm == L"فلله" ||
                            norm == L"ابالله" || norm == L"وبالله" || norm == L"فبالله" || norm == L"االله") {
                            ++total_allah;
                        }
                    }
                    word.clear();
                };
                for (wchar_t ch : ay.text_simple) {
                    if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') flush_w();
                    else word.push_back(ch);
                }
                flush_w();
            }
            int ruku_cnt = (max_ruku >= min_ruku) ? (max_ruku - min_ruku + 1) : 1;
            SurahHeadingInfo sh = g_app->db->get_surah_heading_info(s.number);

            std::vector<std::wstring> lines;
            lines.push_back(L"۞ " + s.name_uthmani + L" ۞");   // نص قرآني: بالعربية في الواجهتين
            if (g_app->is_en()) {
                lines.push_back(L"Surah: " + s.name_arabic);
                lines.push_back(L"Order in the Mushaf: surah " + std::to_wstring(s.number) + L" of 114");
                lines.push_back(L"Order of revelation: " + std::to_wstring(s.revelation_order));
                lines.push_back(L"Place of revelation: " + std::wstring((sh.type.empty() || !edition_details ? s.revelation_place : sh.type) == L"مكية" ? L"Meccan" : L"Medinan"));
                if (!edition_details) {
                    // بيان الاستثناء من خصائص طبعة دار المعارف
                } else if (!sh.excepted_verses.empty()) {
                    std::wstring exc_txt = L"Excepted ayahs: " + sh.excepted_verses_en;
                    if (!sh.place_note_en.empty()) exc_txt += L" (" + sh.place_note_en + L")";
                    lines.push_back(exc_txt);
                } else {
                    lines.push_back(L"Excepted ayahs: none");
                    if (!sh.place_note_en.empty()) lines.push_back(L"Revelation note: " + sh.place_note_en);
                }
                lines.push_back(L"Number of ayahs: " + std::to_wstring(s.verses_count));
                lines.push_back(L"Pages (" + g_app->edition_short_label() + L"): " + g_app->page_range_text(s.start_page, s.end_page));
                lines.push_back(L"Juz: " + (min_juz == max_juz ? std::to_wstring(min_juz) : (std::to_wstring(min_juz) + L" - " + std::to_wstring(max_juz))));
                lines.push_back(L"Rukus in the surah: " + std::to_wstring(ruku_cnt) + L" (Ruku " + std::to_wstring(min_ruku) + L" - " + std::to_wstring(max_ruku) + L")");
                lines.push_back(L"Words: " + std::to_wstring(total_words));
                lines.push_back(L"Letters: " + std::to_wstring(total_letters) + L" (without diacritics and spaces)");
                if (!surah_letter_freq.empty()) lines.push_back(L"Letter breakdown in the surah: " + surah_letter_freq);
                if (longest_ayah > 0) {
                    lines.push_back(L"Longest ayah: ayah " + std::to_wstring(longest_ayah) + L" (" + std::to_wstring(longest_letters) + L" letters) • Shortest ayah: ayah " + std::to_wstring(shortest_ayah) + L" (" + std::to_wstring(shortest_letters) + L" letters)");
                }
                lines.push_back(L"The name of Allah (الله) occurs: " + std::to_wstring(total_allah) + ((total_allah) == 1 ? L" time" : L" times") + L" • Sajdah places: " + std::to_wstring(sajdahs));
                if (edition_details) lines.push_back(L"Surah summary: " + build_surah_heading(*g_app->db, s.number, true));
            } else if (vocalized) {
                // النافذة كما يراها المبصر ويسمعها قارئ الشاشة: مشكولة بإعرابها
                const std::wstring ed_key = g_app->db->edition();
                const std::wstring ed_name = ed_key == L"dar_almaarif" ? L"طَبْعَةُ دَارِ المَعَارِفِ"
                                           : ed_key == L"madinah_1405" ? L"طَبْعَةُ المَدِينَةِ القَدِيمَةِ"
                                           : ed_key == L"madinah" ? L"طَبْعَةُ المَدِينَةِ الحَدِيثَةِ"
                                           : g_app->edition_short_label();
                const std::wstring place = sh.type.empty() || !edition_details ? s.revelation_place : sh.type;
                lines.push_back(L"اسْمُ السُّورَةِ: " + s.name_uthmani);
                lines.push_back(L"تَرْتِيبُهَا فِي المُصْحَفِ: السُّورَةُ رَقْمُ " + std::to_wstring(s.number) + L" مِنْ 114");
                lines.push_back(L"تَرْتِيبُ نُزُولِهَا: " + std::to_wstring(s.revelation_order));
                lines.push_back(L"مَكَانُ النُّزُولِ: " + vocalize_marks_text(place));
                if (!edition_details) {
                    // بيان الاستثناء من خصائص طبعة دار المعارف
                } else if (!sh.groups.empty()) {
                    const std::wstring part = shamarly_exception_part(*g_app->db, s.number);
                    lines.push_back(L"الاسْتِثْنَاءُ: " + (part.empty() ? build_exception_clause(sh, true) : part));
                    if (!sh.top_note.empty()) lines.push_back(L"مُلَاحَظَةُ النُّزُولِ: " + vocalize_marks_text(sh.top_note));
                } else {
                    lines.push_back(L"الآيَاتُ المُسْتَثْنَاةُ: لَا تُوجَدُ آيَاتٌ مُسْتَثْنَاةٌ فِي هَذِهِ السُّورَةِ");
                    if (!sh.place_note.empty()) lines.push_back(L"مُلَاحَظَةُ النُّزُولِ: " + vocalize_marks_text(to_arabic_digits(sh.place_note)));
                }
                lines.push_back(L"عَدَدُ آيَاتِهَا (بِالعَدِّ الكُوفِيِّ): " + ayahs_ar_v(s.verses_count));
                lines.push_back(L"صَفَحَاتُهَا (" + ed_name + L"): " + (s.start_page == s.end_page
                    ? (L"الصَّفْحَةُ " + std::to_wstring(s.start_page))
                    : (L"مِنَ الصَّفْحَةِ " + std::to_wstring(s.start_page) + L" إِلَى " + std::to_wstring(s.end_page))));
                lines.push_back(L"نِطَاقُ الأَجْزَاءِ: " + (min_juz == max_juz ? (L"الجُزْءُ " + std::to_wstring(min_juz))
                    : (L"مِنَ الجُزْءِ " + std::to_wstring(min_juz) + L" إِلَى الجُزْءِ " + std::to_wstring(max_juz))));
                lines.push_back(L"الرُّكُوعَاتُ فِي السُّورَةِ: " + rukus_ar_v(ruku_cnt) + (min_ruku == max_ruku
                    ? (L" (الرُّكُوعُ " + std::to_wstring(min_ruku) + L")")
                    : (L" (مِنَ الرُّكُوعِ " + std::to_wstring(min_ruku) + L" إِلَى " + std::to_wstring(max_ruku) + L")")));
                lines.push_back(L"عَدَدُ الكَلِمَاتِ: " + words_ar_v(total_words));
                lines.push_back(L"عَدَدُ الحُرُوفِ فِي السُّورَةِ: " + letters_ar_v(total_letters) + L" (دُونَ التَّشْكِيلِ وَالمَسَافَاتِ)");
                if (!surah_letter_freq.empty()) lines.push_back(L"تَفْصِيلُ الحُرُوفِ فِي السُّورَةِ: " + surah_letter_freq);
                if (longest_ayah > 0) {
                    lines.push_back(L"أَطْوَلُ آيَةٍ: الآيَةُ " + std::to_wstring(longest_ayah) + L" (" + letters_ar_v(longest_letters) +
                                    L") • أَقْصَرُ آيَةٍ: الآيَةُ " + std::to_wstring(shortest_ayah) + L" (" + letters_ar_v(shortest_letters) + L")");
                }
                lines.push_back(L"وُرُودُ لَفْظِ الجَلَالَةِ «اللَّه»: " + (total_allah ? times_ar_v(total_allah) : std::wstring(L"لَمْ يَرِدْ فِي السُّورَةِ")) +
                                L" • مَوَاضِعُ السُّجُودِ: " + (sajdahs ? std::to_wstring(sajdahs) : std::wstring(L"لَا تُوجَدُ")));
                if (edition_details) lines.push_back(L"بَيَانُ السُّورَةِ: " + build_surah_heading(*g_app->db, s.number, false, true));
            } else {
                lines.push_back(L"اسم السورة: سورة " + s.name_arabic);
                lines.push_back(L"ترتيبها في المصحف: السورة رقم " + std::to_wstring(s.number) + L" من 114");
                lines.push_back(L"ترتيب نزولها: " + std::to_wstring(s.revelation_order));
                lines.push_back(L"مكان النزول: " + (sh.type.empty() || !edition_details ? s.revelation_place : sh.type));
                if (!edition_details) {
                    // بيان الاستثناء من خصائص طبعة دار المعارف
                } else if (!sh.groups.empty()) {
                    const std::wstring part = shamarly_exception_part(*g_app->db, s.number);
                    lines.push_back(L"الاستثناء: " + (part.empty() ? build_exception_clause(sh, false) : strip_harakat(part)));
                    if (!sh.top_note.empty()) lines.push_back(L"ملاحظة النزول: " + sh.top_note);
                } else {
                    lines.push_back(L"الآيات المستثناة: لا توجد آيات مستثناة في هذه السورة");
                    if (!sh.place_note.empty()) lines.push_back(L"ملاحظة النزول: " + to_arabic_digits(sh.place_note));
                }
                lines.push_back(L"عدد آياتها (بالعدّ الكوفي): " + ayahs_ar(s.verses_count));
                lines.push_back(L"صفحاتها (" + g_app->edition_short_label() + L"): " + g_app->page_range_text(s.start_page, s.end_page));
                lines.push_back(L"نطاق الأجزاء: " + (min_juz == max_juz ? (L"الجزء " + std::to_wstring(min_juz)) : (L"من الجزء " + std::to_wstring(min_juz) + L" إلى الجزء " + std::to_wstring(max_juz))));
                lines.push_back(L"الركوعات في السورة: " + rukus_ar(ruku_cnt) + (min_ruku == max_ruku ? (L" (الركوع " + std::to_wstring(min_ruku) + L")") : (L" (من الركوع " + std::to_wstring(min_ruku) + L" إلى " + std::to_wstring(max_ruku) + L")")));
                lines.push_back(L"عدد الكلمات: " + words_ar(total_words));
                lines.push_back(L"عدد الحروف في السورة: " + letters_ar(total_letters) + L" (دون التشكيل والمسافات)");
                if (!surah_letter_freq.empty()) lines.push_back(L"تفصيل الحروف في السورة: " + surah_letter_freq);
                if (longest_ayah > 0) {
                    lines.push_back(L"أطول آية: الآية " + std::to_wstring(longest_ayah) + L" (" + letters_ar(longest_letters) + L") • أقصر آية: الآية " + std::to_wstring(shortest_ayah) + L" (" + letters_ar(shortest_letters) + L")");
                }
                lines.push_back(L"ورود لفظ الجلالة «الله»: " + (total_allah ? times_ar(total_allah) : std::wstring(L"لم يرد في السورة")) +
                                L" • مواضع السجود: " + (sajdahs ? std::to_wstring(sajdahs) : std::wstring(L"لا توجد")));
                if (edition_details) lines.push_back(L"بيان السورة: " + build_surah_heading(*g_app->db, s.number, false));
            }
            if (!g_app->is_en()) for (auto& l : lines) l = to_arabic_digits(l);
            return lines;
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    // عند مغادرة النافذة (برنامج آخر أو نافذة فرعية) نحفظ موضع التركيز، وعند العودة نرجعه إليه،
    // فيجد قارئ الشاشة المستخدمَ حيث تركه، وتعمل الأسهم مباشرة
    case WM_ACTIVATE:
        if (g_app) {
            if (LOWORD(wParam) == WA_INACTIVE) {
                HWND f = GetFocus();
                if (f && IsChild(hwnd, f)) {
                    // القائمة المنسدلة تضع التركيز في عنصر داخلي؛ نحفظ القائمة نفسها
                    HWND top = f;
                    while (GetParent(top) && GetParent(top) != hwnd) top = GetParent(top);
                    g_app->last_focus = top;
                }
            } else if (!HIWORD(wParam)) {   // ليست مصغّرة
                SetFocus(g_app->focus_target());
                return 0;
            }
        }
        break;

    case WM_SETFOCUS:
        // إن وصل التركيز إلى النافذة الرئيسية نفسها نمرّره إلى عنصر يقرؤه قارئ الشاشة
        if (g_app && g_app->list_ayahs) {
            SetFocus(g_app->focus_target());
            return 0;
        }
        break;

    case WM_SIZE:
        if (g_app) {
            g_app->layout_children(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    // نُقلت النافذة إلى شاشة بدقة أخرى (أو غيّر المستخدم تكبير النص): نعيد الخطوط والمقاسات بالدقة الجديدة
    case WM_DPICHANGED:
        if (g_app) {
            ui_dpi() = HIWORD(wParam) ? HIWORD(wParam) : 96;
            g_app->recreate_fonts_and_brushes();
            const RECT* r = reinterpret_cast<const RECT*>(lParam);
            if (!g_app->focus_mode) {
                SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            } else {
                MONITORINFO mi{sizeof(mi)};
                GetMonitorInfoW(MonitorFromRect(r, MONITOR_DEFAULTTONEAREST), &mi);
                SetWindowPos(hwnd, nullptr, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_app->layout_children(rc.right, rc.bottom);
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
        }
        return 0;

    // أصغر حجم للنافذة: يبقى فيه المصحف مقروءًا والشريط العلوي غير متراكب
    case WM_GETMINMAXINFO: {
        auto* mm = reinterpret_cast<MINMAXINFO*>(lParam);
        const RECT wa = work_area_of(hwnd);   // لا يتجاوز الحد الأدنى الشاشة نفسها (تكبير كبير على شاشة صغيرة)
        mm->ptMinTrackSize.x = std::min(DS(kMinWindowWidth), (int)(wa.right - wa.left));
        mm->ptMinTrackSize.y = std::min(DS(400), (int)(wa.bottom - wa.top));
        return 0;
    }

    case WM_NOTIFY: {
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm && nm->idFrom == ID_TAB_SIDEBAR && nm->code == TCN_SELCHANGE && g_app) {
            g_app->update_sidebar_visibility();
            return 0;
        }
        break;
    }

    case WM_MEASUREITEM: {
        auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (mis && mis->CtlID == ID_LIST_AYAHS && g_app) {
            int idx = (int)mis->itemID;
            if (idx >= 0 && idx < (int)g_app->ayah_items.size()) {
                mis->itemHeight = (UINT)g_app->ayah_items[idx].height;
                return TRUE;
            }
        }
        break;
    }

    case WM_ERASEBKGND: {
        if (!g_app || !g_app->brush_window) break;
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_app->brush_window);
        if (!g_app->focus_mode) {
            // شريط علوي هادئ يفصله عن صفحة المصحف خط ذهبي رفيع
            ThemePalette pal = get_theme_palette(g_app->effective_theme());
            RECT tb = rc;
            tb.bottom = DS(kToolbarHeight);
            FillRect(hdc, &tb, g_app->brush_toolbar);
            RECT line = tb;
            line.top = tb.bottom - 2;
            HBRUSH gold = CreateSolidBrush(pal.selected_border);
            FillRect(hdc, &line, gold);
            DeleteObject(gold);
        }
        g_app->paint_mushaf_frame(hdc);
        g_app->paint_outside_marks(hdc);
        g_app->paint_nav_arrows(hdc);
        return 1;
    }

    // سهما التقليب: للفأرة فقط (لا عنصر تحكم لهما، فلا يصل إليهما قارئ الشاشة)
    case WM_MOUSEMOVE: {
        if (!g_app) break;
        g_app->poke_controls();
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        int hit = g_app->arrow_hit_test(pt);
        if (hit != g_app->arrow_hover) {
            g_app->arrow_hover = hit;
            g_app->invalidate_nav_arrows();
        }
        if (!g_app->tracking_mouse) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
            g_app->tracking_mouse = TrackMouseEvent(&tme) != FALSE;
        }
        break;
    }
    case WM_MOUSELEAVE:
        if (g_app) {
            g_app->tracking_mouse = false;
            if (g_app->arrow_hover != 0) {
                g_app->arrow_hover = 0;
                g_app->invalidate_nav_arrows();
            }
        }
        return 0;
    case WM_SETCURSOR:
        if (g_app && (HWND)wParam == hwnd && LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            int hit = g_app->arrow_hit_test(pt);
            if (g_app->hit_enabled(hit)) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_LBUTTONDOWN:
        if (g_app) {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            int hit = g_app->arrow_hit_test(pt);
            if (hit == MushafAppWindow::kHitClose) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (hit == MushafAppWindow::kHitExitFull) {
                SendMessageW(hwnd, WM_COMMAND, IDM_FOCUS_MODE, 0);
                return 0;
            }
            if (hit != 0 && g_app->can_navigate(hit)) {
                g_app->navigate_unit(hit);
                return 0;
            }
        }
        break;

    case WM_CTLCOLORSTATIC: {
        if (!g_app) break;
        HWND ctl = (HWND)lParam;
        if (ctl == g_app->lbl_browse || ctl == g_app->lbl_position || ctl == g_app->lbl_edition) {
            ThemePalette pal = get_theme_palette(g_app->effective_theme());
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, pal.toolbar_bg);
            SetTextColor(hdc, ctl == g_app->lbl_position ? pal.text_header : pal.text_quran);
            return (LRESULT)g_app->brush_toolbar;
        }
        break;
    }

    case WM_CTLCOLORLISTBOX: {
        if (!g_app) break;
        HWND ctl = (HWND)lParam;
        if (ctl == g_app->list_ayahs || ctl == g_app->list_units) {
            ThemePalette pal = get_theme_palette(g_app->effective_theme());
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, pal.paper_bg);
            SetTextColor(hdc, pal.text_quran);
            return (LRESULT)g_app->brush_paper;
        }
        break;
    }

    case WM_UPDATE_RESULT:
        if (g_app) g_app->on_update_result(reinterpret_cast<UpdateCheckResult*>(lParam));
        else delete reinterpret_cast<UpdateCheckResult*>(lParam);
        return 0;

    case WM_TIMER:
        if (wParam == kUpdateTimer && g_app) {
            KillTimer(hwnd, kUpdateTimer);
            g_app->start_update_check(false);
            return 0;
        }
        if (wParam == kControlsTimer && g_app) {
            g_app->hide_controls_if_idle();
            return 0;
        }
        if (wParam == kRelayoutTimer && g_app) {
            KillTimer(hwnd, kRelayoutTimer);
            g_app->relayout_ayah_rows();
            return 0;
        }
        break;

    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis && dis->CtlID == ID_LIST_AYAHS && g_app && dis->itemID != (UINT)-1) {
            int idx = (int)dis->itemID;
            if (idx >= 0 && idx < (int)g_app->ayah_items.size()) {
                const auto& item = g_app->ayah_items[idx];
                ThemePalette pal = get_theme_palette(g_app->effective_theme());
                const bool rtl = g_app->list_is_rtl(dis->hwndItem);
                const bool is_ayah = (item.kind == ListItemKind::Ayah);
                const bool is_banner = (item.kind == ListItemKind::SurahHeader || item.kind == ListItemKind::SurahDetails);

                // الآية المقسّمة على عدة صفوف تُظلَّل كلها حين يُحدَّد صفها الأول
                int cur_sel = (int)SendMessageW(dis->hwndItem, LB_GETCURSEL, 0, 0);
                bool selected = (dis->itemState & ODS_SELECTED) != 0;
                if (!selected && cur_sel >= 0 && cur_sel < (int)g_app->ayah_items.size()) {
                    selected = (g_app->ayah_items[cur_sel].first_row == item.first_row);
                }

                // الآية المظللة بالفأرة: لون ذهبي خفيف يبقى ظاهرًا حتى وهي محددة (والشريط الذهبي يدل على التحديد)
                const bool marked = g_app->row_is_marked(idx);
                COLORREF marked_bg = RGB((GetRValue(pal.paper_bg) * 2 + GetRValue(pal.selected_border)) / 3,
                                         (GetGValue(pal.paper_bg) * 2 + GetGValue(pal.selected_border)) / 3,
                                         (GetBValue(pal.paper_bg) * 2 + GetBValue(pal.selected_border)) / 3);
                COLORREF bg = marked ? marked_bg
                            : selected ? pal.selected_bg : (is_banner ? pal.surah_banner_bg : pal.paper_bg);
                COLORREF fg = is_banner ? pal.text_header : pal.text_quran;

                HBRUSH br = CreateSolidBrush(bg);
                FillRect(dis->hDC, &dis->rcItem, br);
                DeleteObject(br);

                // في التخطيط المعكوس (الواجهة العربية) يكون اليسار المنطقي هو اليمين المرئي
                RECT start_bar = dis->rcItem;
                if (rtl) start_bar.right = start_bar.left + DS(kAccentWidth);
                else     start_bar.left = start_bar.right - DS(kAccentWidth);

                if (selected) {
                    HBRUSH sbr = CreateSolidBrush(pal.selected_border);
                    FillRect(dis->hDC, &start_bar, sbr);
                    DeleteObject(sbr);

                    if (is_ayah && !item.mushaf_line && item.seg_index == 0 && g_app->settings.show_inline_copy_btn) {
                        RECT btn_rc = inline_copy_button_rect(dis->rcItem, rtl);
                        HBRUSH bbr = CreateSolidBrush(pal.toolbar_bg);
                        FillRect(dis->hDC, &btn_rc, bbr);
                        DeleteObject(bbr);
                        HBRUSH frame = CreateSolidBrush(pal.selected_border);
                        FrameRect(dis->hDC, &btn_rc, frame);
                        DeleteObject(frame);
                        HGDIOBJ old_bf = SelectObject(dis->hDC, g_app->font_ui);
                        SetBkMode(dis->hDC, TRANSPARENT);
                        SetTextColor(dis->hDC, pal.text_header);
                        std::wstring copy_lbl = g_app->tr(L"نسخ", L"Copy");
                        DrawTextW(dis->hDC, copy_lbl.c_str(), (int)copy_lbl.size(), &btn_rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        SelectObject(dis->hDC, old_bf);
                    }
                }

                RECT text_rc = dis->rcItem;
                text_rc.left += DS(kRowPadX);
                text_rc.right -= DS(kRowPadX);
                if (rtl) text_rc.left += DS(kAccentWidth); else text_rc.right -= DS(kAccentWidth);
                if (is_ayah && !item.mushaf_line && g_app->settings.show_inline_copy_btn) {
                    if (rtl) text_rc.right -= DS(kInlineBtnWidth); else text_rc.left += DS(kInlineBtnWidth);
                }

                if (item.mushaf_line && g_app->font_line) {
                    // سطر الطبعة: الكلمات من اليمين المرئي إلى اليسار، والمسافة الزائدة تتوزع بينها بالتساوي
                    HGDIOBJ old_lf = SelectObject(dis->hDC, g_app->font_line);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, fg);
                    SIZE sp{};
                    GetTextExtentPoint32W(dis->hDC, L" ", 1, &sp);
                    const int n = (int)std::min(item.words.size(), item.word_w.size());
                    int words_w = 0;
                    for (int k = 0; k < n; ++k) words_w += item.word_w[k];
                    const int avail = (int)(text_rc.right - text_rc.left) - 8;
                    double gap = sp.cx;
                    int used = words_w + (n > 1 ? (int)(gap * (n - 1)) : 0);
                    if (item.justify && n > 1 && avail > words_w) {
                        gap = (double)(avail - words_w) / (n - 1);
                        used = avail;
                    }
                    // في التخطيط المعكوس اليسار المنطقي هو اليمين المرئي، فنتقدم بالإحداثي المنطقي موجبًا
                    const int extra = std::max(0, avail - used);
                    double x = rtl ? (text_rc.left + 4 + extra / 2) : (text_rc.right - 4 - extra / 2);
                    const int top = text_rc.top + std::max(0, (int)((text_rc.bottom - text_rc.top) - item.line_h) / 2);
                    for (int k = 0; k < n; ++k) {
                        const int w = item.word_w[k];
                        RECT wr;
                        if (rtl) { wr = {(LONG)x, top, (LONG)x + w, top + item.line_h}; x += w + gap; }
                        else     { wr = {(LONG)(x - w), top, (LONG)x, top + item.line_h}; x -= w + gap; }
                        DrawTextW(dis->hDC, item.words[k].c_str(), (int)item.words[k].size(), &wr,
                                  DT_CENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX | dt_reading_flag(dis->hDC, true));
                    }
                    SelectObject(dis->hDC, old_lf);
                    return TRUE;
                }

                if (item.kind == ListItemKind::SurahHeader) {
                    // إطار مزخرف لاسم السورة: حدّ ذهبي مزدوج كترويسات المصاحف
                    RECT frame = dis->rcItem;
                    InflateRect(&frame, -DS(kHeaderInsetX), -DS(kHeaderInsetY));
                    frame.bottom = std::max(frame.top + 10, (LONG)frame.bottom);
                    HBRUSH fill = CreateSolidBrush(selected ? pal.selected_bg : pal.paper_bg);
                    FillRect(dis->hDC, &frame, fill);
                    DeleteObject(fill);
                    HBRUSH gold = CreateSolidBrush(pal.selected_border);
                    FrameRect(dis->hDC, &frame, gold);
                    RECT outer2 = frame;
                    InflateRect(&outer2, -1, -1);
                    FrameRect(dis->hDC, &outer2, gold);
                    RECT inner = frame;
                    InflateRect(&inner, -5, -5);
                    FrameRect(dis->hDC, &inner, gold);
                    // زخرفة صغيرة في منتصف الحافتين الجانبيتين
                    int mid = (frame.top + frame.bottom) / 2;
                    RECT dot1 = {frame.left - 4, mid - 4, frame.left + 4, mid + 4};
                    RECT dot2 = {frame.right - 4, mid - 4, frame.right + 4, mid + 4};
                    FillRect(dis->hDC, &dot1, gold);
                    FillRect(dis->hDC, &dot2, gold);
                    DeleteObject(gold);
                    text_rc = frame;
                    InflateRect(&text_rc, -14, -6);
                } else {
                    text_rc.top += (is_ayah && item.seg_index > 0) ? 0 : DS(kRowPadY);
                    text_rc.bottom -= (is_ayah && item.seg_index + 1 < item.seg_count) ? 0 : DS(kRowPadY);
                }

                HGDIOBJ old_f = SelectObject(dis->hDC, g_app->font_for_item(item));
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, fg);

                // نص الآية يبدأ دائمًا من اليمين المرئي: DT_LEFT في التخطيط المعكوس و DT_RIGHT في غيره
                UINT align;
                if (!is_ayah || g_app->settings.doc_alignment == L"center") align = DT_CENTER;
                else align = rtl ? DT_LEFT : DT_RIGHT;
                if (is_ayah && !item.lines.empty() && item.line_h > 0) {
                    // نرسم السطور التي حُسبت عند البناء سطرًا سطرًا، فلا يُقصّ شيء من الآية
                    int y = text_rc.top;
                    for (const auto& ln : item.lines) {
                        RECT lr = {text_rc.left, y, text_rc.right, y + item.line_h};
                        DrawTextW(dis->hDC, ln.c_str(), (int)ln.size(), &lr,
                                  align | DT_SINGLELINE | dt_reading_flag(dis->hDC, true) | DT_NOPREFIX | DT_NOCLIP);
                        y += item.line_h;
                    }
                } else if (item.kind == ListItemKind::SurahHeader && !item.sub_text.empty()) {
                    // الاسم ثم بيانه في سطر واحد، متوسطان أفقيًا ومتحاذيان على خط الكتابة نفسه.
                    // في القائمة المعكوسة يبدأ الإحداثي المنطقي من اليمين، فيقع الاسم يمين البيان
                    HFONT sub_f = g_app->details_font(item.sub_px);
                    const int name_w = g_app->single_line_width(dis->hDC, g_app->font_header, item.visual_text);
                    const int sub_w = g_app->single_line_width(dis->hDC, sub_f, item.sub_text);
                    const int gap = g_app->header_gap();
                    TEXTMETRICW tn{}, ts{};
                    SelectObject(dis->hDC, g_app->font_header);
                    GetTextMetricsW(dis->hDC, &tn);
                    SelectObject(dis->hDC, sub_f);
                    GetTextMetricsW(dis->hDC, &ts);
                    const int asc = std::max(tn.tmAscent, ts.tmAscent);
                    const int line_h = asc + std::max(tn.tmDescent, ts.tmDescent);
                    const int top = text_rc.top + std::max(0, (int)(text_rc.bottom - text_rc.top) - line_h) / 2;
                    const int total = name_w + gap + sub_w;
                    int x = text_rc.left + std::max(0, (int)(text_rc.right - text_rc.left) - total) / 2;
                    const UINT f1 = DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | dt_reading_flag(dis->hDC, true);
                    RECT nr = {x, top + asc - tn.tmAscent, x + name_w, top + asc - tn.tmAscent + tn.tmHeight};
                    SelectObject(dis->hDC, g_app->font_header);
                    DrawTextW(dis->hDC, item.visual_text.c_str(), (int)item.visual_text.size(), &nr, f1 | DT_CENTER);
                    // فاصل ذهبي صغير بين الاسم والبيان
                    const int cx = x + name_w + gap / 2, cy = top + line_h / 2;
                    HBRUSH gold = CreateSolidBrush(pal.selected_border);
                    RECT dot = {cx - DS(3), cy - DS(3), cx + DS(3), cy + DS(3)};
                    FillRect(dis->hDC, &dot, gold);
                    DeleteObject(gold);
                    x += name_w + gap;
                    RECT sr = {x, top + asc - ts.tmAscent, x + sub_w, top + asc - ts.tmAscent + ts.tmHeight};
                    SelectObject(dis->hDC, sub_f);
                    DrawTextW(dis->hDC, item.sub_text.c_str(), (int)item.sub_text.size(), &sr,
                              f1 | DT_CENTER);
                } else {
                    if (item.kind == ListItemKind::SurahHeader) {
                        // توسيط رأسي لاسم السورة داخل الإطار
                        RECT calc = {0, 0, text_rc.right - text_rc.left, 0};
                        DrawTextW(dis->hDC, item.visual_text.c_str(), (int)item.visual_text.size(), &calc,
                                  DT_CALCRECT | DT_WORDBREAK | dt_reading_flag(dis->hDC, true) | DT_NOPREFIX);
                        int extra = (text_rc.bottom - text_rc.top) - (calc.bottom - calc.top);
                        if (extra > 0) text_rc.top += extra / 2;
                    }
                    const UINT wrap = (item.kind == ListItemKind::SurahDetails) ? (DT_SINGLELINE | DT_VCENTER) : DT_WORDBREAK;
                    DrawTextW(dis->hDC, item.visual_text.c_str(), (int)item.visual_text.size(), &text_rc,
                              align | wrap | dt_reading_flag(dis->hDC, true) | DT_NOPREFIX);
                }
                SelectObject(dis->hDC, old_f);
                return TRUE;
            }
        }
        break;
    }

    case WM_COMMAND: {
        if (!g_app) break;
        UINT id = LOWORD(wParam);
        UINT code = HIWORD(wParam);

        if (id == ID_LIST_AYAHS && code == LBN_SELCHANGE) {
            g_app->update_current_ayah_from_selection(false);
            // إعادة رسم صفوف الآية المقسّمة السابقة والحالية حتى يكتمل تظليلها
            InvalidateRect(g_app->list_ayahs, nullptr, FALSE);
            return 0;
        }
        if (id == ID_LIST_UNITS && code == LBN_DBLCLK) {
            int sel = (int)SendMessageW(g_app->list_units, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->unit_numbers.size()) {
                g_app->open_unit_by_number(g_app->unit_numbers[sel]);
            }
            return 0;
        }
        if (id == ID_EDIT_SIDEBAR_FILTER && code == EN_CHANGE) {
            g_app->populate_sidebar_index_list();
            return 0;
        }
        if (id == ID_LIST_SIDEBAR_INDEX && code == LBN_DBLCLK) {
            int sel = (int)SendMessageW(g_app->list_sidebar_index, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->sidebar_unit_numbers.size()) {
                g_app->open_unit_by_number(g_app->sidebar_unit_numbers[sel]);
            }
            return 0;
        }
        if (id == ID_LIST_SIDEBAR_BOOKMARKS && code == LBN_DBLCLK) {
            int sel = (int)SendMessageW(g_app->list_sidebar_bookmarks, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->sidebar_bm_ayahs.size()) {
                g_app->jump_to_ayah(g_app->sidebar_bm_ayahs[sel]);
            }
            return 0;
        }
        if (id == ID_BTN_SB_COPY_BM) {
            int sel = (int)SendMessageW(g_app->list_sidebar_bookmarks, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->sidebar_bm_ayahs.size()) {
                std::wstring formatted = format_ayah_custom(
                    g_app->sidebar_bm_ayahs[sel], g_app->settings.copy_script,
                    g_app->settings.copy_brackets, g_app->settings.copy_reference,
                    g_app->settings.copy_arabic_digits
                );
                set_clipboard_text(hwnd, formatted);
                g_app->say(g_app->tr(L"نُسخت الآية المحددة من المفضلة", L"Copied selected bookmarked ayah"));
            }
            return 0;
        }
        if (id == ID_BTN_SB_DEL_BM) {
            int sel = (int)SendMessageW(g_app->list_sidebar_bookmarks, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_app->sidebar_bm_ayahs.size()) {
                auto it = std::find(g_app->settings.bookmarks.begin(), g_app->settings.bookmarks.end(), g_app->sidebar_bm_ayahs[sel].verse_key);
                if (it != g_app->settings.bookmarks.end()) g_app->settings.bookmarks.erase(it);
                g_app->settings.save();
                g_app->populate_sidebar_bookmarks_list();
                g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            }
            return 0;
        }

        if (id == ID_COMBO_EDITION && code == CBN_SELCHANGE && !g_app->updating_combos) {
            int sel = (int)SendMessageW(g_app->combo_edition, CB_GETCURSEL, 0, 0);
            const auto& eds = g_app->db->editions();
            if (sel >= 0 && sel < (int)eds.size()) g_app->switch_edition(eds[sel].key);
            return 0;
        }
        if (id >= IDM_EDITION_FIRST && id < IDM_EDITION_FIRST + 40) {
            const auto& eds = g_app->db->editions();
            size_t i = id - IDM_EDITION_FIRST;
            if (i < eds.size()) g_app->switch_edition(eds[i].key);
            return 0;
        }
        if (id == ID_COMBO_BROWSE && code == CBN_SELCHANGE && !g_app->updating_combos) {
            int sel = (int)SendMessageW(g_app->combo_browse, CB_GETCURSEL, 0, 0);
            static const wchar_t* scopes[7] = {L"surah", L"page", L"juz", L"hizb", L"rub", L"manzil", L"ruku"};
            if (sel >= 0 && sel < 7) {
                g_app->settings.browse_scope = scopes[sel];
                g_app->settings.save();
                g_app->update_sidebar_tabs_text();
                g_app->populate_sidebar_index_list();
                g_app->show_units_view(false);
            }
            return 0;
        }

        switch (id) {
        case IDM_BROWSE_SURAH:  g_app->settings.browse_scope = L"surah";  g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); g_app->announce_browse_mode(); return 0;
        case IDM_BROWSE_PAGE:   g_app->settings.browse_scope = L"page";   g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); g_app->announce_browse_mode(); return 0;
        case IDM_BROWSE_JUZ:    g_app->settings.browse_scope = L"juz";    g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); g_app->announce_browse_mode(); return 0;
        case IDM_BROWSE_RUB:    g_app->settings.browse_scope = L"rub";    g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); g_app->announce_browse_mode(); return 0;
        case IDM_BROWSE_HIZB:   g_app->settings.browse_scope = L"hizb";   g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); g_app->announce_browse_mode(); return 0;
        case IDM_BROWSE_MANZIL: g_app->settings.browse_scope = L"manzil"; g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); g_app->announce_browse_mode(); return 0;
        case IDM_BROWSE_RUKU:   g_app->settings.browse_scope = L"ruku";   g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); g_app->announce_browse_mode(); return 0;


        case IDM_SHOW_UNITS:       g_app->show_units_view(true); return 0;
        case IDM_FOCUS_AYAHS:      g_app->show_ayahs_view(); return 0;
        case IDM_FOCUS_INDEX:
            if (!g_app->settings.show_sidebar) {
                g_app->settings.show_sidebar = true;
                g_app->update_sidebar_visibility();
                g_app->build_menus();
            }
            SetFocus(g_app->list_sidebar_index);
            return 0;
        case IDM_CYCLE_LANDMARKS:  g_app->cycle_landmarks_f6(); return 0;
        case IDM_NEXT_UNIT:        g_app->navigate_unit(+1); return 0;
        case IDM_PREV_UNIT:        g_app->navigate_unit(-1); return 0;
        case IDM_COPY_AYAH:        g_app->copy_current_ayah(false); return 0;
        case IDM_CLEAR_MARKS:      g_app->clear_marks(); return 0;
        case IDM_MULTI_COPY: {
            // آيات الوحدة المعروضة (سورة أو صفحة أو جزء...) في قائمة بمربعات اختيار
            if (g_app->current_ayahs.empty()) return 0;
            if (g_app->showing_units) g_app->show_ayahs_view();
            const AyahInfo* fa = g_app->focused_ayah();
            show_multi_copy_dialog(hwnd, *g_app->db, g_app->settings, g_app->current_ayahs, g_app->marked_ids,
                                   fa ? fa->id : 0, g_app->is_en(), g_app->font_ui, g_app->font_units,
                                   &ScreenReaderEngine::instance());
            g_app->on_marks_changed();
            SetFocus(g_app->list_ayahs);
            return 0;
        }
        case IDM_TOGGLE_BOOKMARK:  g_app->toggle_bookmark_at_current(); return 0;
        case IDM_SET_KHATMAH:      g_app->set_khatmah_at_current(); return 0;
        case IDM_JUMP_KHATMAH:
            g_app->push_history(g_app->current_surah, g_app->current_ayah_num);
            g_app->jump_to_khatmah();
            return 0;
        case IDM_SAJDAH_LIST: {
            std::wstring vk;
            if (show_sajdah_navigator_dialog(hwnd, *g_app->db, g_app->settings, vk, g_app->font_ui) && !vk.empty()) {
                AyahInfo target;
                if (g_app->db->get_ayah_by_key(vk, target)) {
                    g_app->push_history(g_app->current_surah, g_app->current_ayah_num);
                    g_app->settings.browse_scope = L"surah";
                    g_app->populate_toolbar_combos();
                    g_app->load_surah(target.surah_number, target.ayah_number);
                    g_app->push_history(target.surah_number, target.ayah_number);
                }
            }
            return 0;
        }
        case IDM_HISTORY_BACK: {
            if (g_app->nav_history_pos > 0) {
                --g_app->nav_history_pos;
                auto pt = g_app->nav_history[g_app->nav_history_pos];
                g_app->navigating_history = true;
                g_app->settings.browse_scope = L"surah";
                g_app->populate_toolbar_combos();
                g_app->load_surah(pt.first, pt.second);
                g_app->navigating_history = false;
                SurahInfo s = g_app->db->get_surah(pt.first);
                g_app->say(g_app->tr(
                    L"الرجوع إلى سورة " + s.name_arabic + L" الآية " + std::to_wstring(pt.second),
                    L"Back to Surah " + s.name_arabic + L", ayah " + std::to_wstring(pt.second)
                ));
            } else {
                g_app->say(g_app->tr(L"لا يوجد موضع سابق في سجل التنقل", L"No previous position in history"));
            }
            return 0;
        }
        case IDM_HISTORY_FORWARD: {
            if (g_app->nav_history_pos >= 0 && g_app->nav_history_pos + 1 < (int)g_app->nav_history.size()) {
                ++g_app->nav_history_pos;
                auto pt = g_app->nav_history[g_app->nav_history_pos];
                g_app->navigating_history = true;
                g_app->settings.browse_scope = L"surah";
                g_app->populate_toolbar_combos();
                g_app->load_surah(pt.first, pt.second);
                g_app->navigating_history = false;
                SurahInfo s = g_app->db->get_surah(pt.first);
                g_app->say(g_app->tr(
                    L"التقدم إلى سورة " + s.name_arabic + L" الآية " + std::to_wstring(pt.second),
                    L"Forward to Surah " + s.name_arabic + L", ayah " + std::to_wstring(pt.second)
                ));
            } else {
                g_app->say(g_app->tr(L"لا يوجد موضع تالٍ في سجل التنقل", L"No next position in history"));
            }
            return 0;
        }
        case IDM_QUICK_INFO:       if (g_app->require_ayah_row()) g_app->announce_quick_info(); return 0;

        case IDM_COPY_SURAH_HEAD: {
            const int n = g_app->take_context_surah();
            const std::wstring name = g_app->db->get_surah(n).name_arabic;
            auto items = build_surah_info_lines(n);
            for (auto& l : items) { replace_all(l, L"۞ ", L""); replace_all(l, L" ۞", L""); }
            show_check_copy_dialog(hwnd, g_app->settings,
                g_app->tr(L"نسخ بيانات سورة " + name, L"Copy details of Surah " + name),
                g_app->tr(L"البيانات كلها محددة. اضغط المسافة لإلغاء تحديد ما لا تريده، ثم Enter للنسخ:",
                          L"All details are selected. Press Space to unselect what you do not want, then Enter to copy:"),
                items, g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance());
            return 0;
        }
        case IDM_COPY_SURAH_FULL:
            g_app->copy_whole_surah(g_app->take_context_surah());
            return 0;
        case IDM_SURAH_START:
            g_app->go_to_surah_start(g_app->take_context_surah());
            return 0;

        case IDM_COPY_ALL: {
            std::wstring all;
            for (size_t i = 0; i < g_app->current_ayahs.size(); ++i) {
                if (i > 0) all += L"\r\n";
                all += format_ayah_custom(
                    g_app->current_ayahs[i],
                    g_app->settings.copy_script,
                    g_app->settings.copy_brackets,
                    g_app->settings.copy_reference,
                    g_app->settings.copy_arabic_digits
                );
            }
            set_clipboard_text(hwnd, all);
            g_app->say(g_app->tr(L"نُسخت الآيات المعروضة كلها", L"Copied all displayed ayahs"));
            return 0;
        }

        case IDM_OPTIONS: {
            std::wstring lang_before = g_app->settings.ui_language;
            if (show_options_dialog(hwnd, g_app->settings, g_app->focused_ayah(), g_app->is_en(), g_app->font_ui,
                                    g_app->db ? g_app->db->page_count() : 604)) {
                if (g_app->settings.ui_language != lang_before) {
                    g_app->relaunch_for_language();
                    return 0;
                }
                g_app->recreate_fonts_and_brushes();
                g_app->rebuild_accelerators();
                g_app->build_menus();
                g_app->populate_toolbar_combos();
                g_app->update_sidebar_visibility();
                g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            }
            return 0;
        }

        case IDM_TOGGLE_SIDEBAR:
            g_app->settings.show_sidebar = !g_app->settings.show_sidebar;
            g_app->settings.save();
            g_app->update_sidebar_visibility();
            g_app->build_menus();
            if (g_app->settings.show_sidebar && !g_app->sidebar_visible() && !g_app->focus_mode) {
                g_app->say(g_app->tr(L"الفهرس الجانبي مفعّل، ويظهر حين تتسع النافذة له",
                                     L"The side index is on, and appears when the window is wide enough"));
            } else {
                g_app->say(g_app->settings.show_sidebar ? g_app->tr(L"إظهار الفهرس الجانبي", L"Side index shown")
                                                        : g_app->tr(L"إخفاء الفهرس الجانبي", L"Side index hidden"));
            }
            return 0;

        case IDM_FOCUS_MODE:
            g_app->focus_mode = !g_app->focus_mode;
            g_app->apply_fullscreen(g_app->focus_mode);
            g_app->build_menus();
            g_app->update_sidebar_visibility();
            SetFocus(g_app->showing_units ? g_app->list_units : g_app->list_ayahs);
            g_app->say(g_app->focus_mode ? g_app->tr(L"ملء الشاشة، وللخروج اضغط Escape", L"Full screen. Press Escape to leave")
                                         : g_app->tr(L"الخروج من ملء الشاشة", L"Left full screen"));
            return 0;

        case IDM_GOTO: {
            int p_cnt = g_app->db ? g_app->db->page_count() : 604;
            GoToResult r = show_goto_dialog(
                hwnd, *g_app->db, g_app->current_surah, g_app->current_ayah_num,
                g_app->current_page, g_app->current_juz, g_app->current_hizb,
                g_app->current_rub, g_app->current_manzil, g_app->current_ruku, p_cnt,
                g_app->is_en(), g_app->font_ui
            );
            if (r.accepted) {
                g_app->push_history(g_app->current_surah, g_app->current_ayah_num);
                if (r.mode == L"ayah") {
                    g_app->settings.browse_scope = L"surah";
                    g_app->populate_toolbar_combos();
                    g_app->load_surah(r.surah, r.value);
                } else if (r.mode == L"page") {
                    g_app->settings.browse_scope = L"page";
                    g_app->populate_toolbar_combos();
                    g_app->load_page(r.value);
                } else if (r.mode == L"juz") {
                    g_app->settings.browse_scope = L"juz";
                    g_app->populate_toolbar_combos();
                    g_app->load_juz(r.value);
                } else if (r.mode == L"hizb") {
                    g_app->settings.browse_scope = L"hizb";
                    g_app->populate_toolbar_combos();
                    g_app->load_hizb(r.value);
                } else if (r.mode == L"rub") {
                    g_app->settings.browse_scope = L"rub";
                    g_app->populate_toolbar_combos();
                    g_app->load_rub(r.value);
                } else if (r.mode == L"manzil") {
                    g_app->settings.browse_scope = L"manzil";
                    g_app->populate_toolbar_combos();
                    g_app->load_manzil(r.value);
                } else if (r.mode == L"ruku") {
                    g_app->settings.browse_scope = L"ruku";
                    g_app->populate_toolbar_combos();
                    g_app->load_ruku(r.value);
                }
                g_app->push_history(g_app->current_surah, g_app->current_ayah_num);
            }
            return 0;
        }

        case IDM_SEARCH: {
            SearchDlgResult sr = show_search_dialog(
                hwnd, *g_app->db, g_app->settings, g_app->current_surah,
                g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance()
            );
            if (sr.jump) {
                g_app->jump_to_ayah(sr.target);
            }
            return 0;
        }

        case IDM_BOOKMARKS_DLG: {
            SearchDlgResult br = show_bookmarks_dialog(
                hwnd, *g_app->db, g_app->settings, g_app->focused_ayah(),
                g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance()
            );
            g_app->populate_sidebar_bookmarks_list();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            if (br.jump) {
                g_app->jump_to_ayah(br.target);
            }
            return 0;
        }

        case IDM_AYAH_INFO: {
            if (!g_app->require_ayah_row()) return 0;
            const AyahInfo* a = g_app->focused_ayah();
            if (!a) return 0;
            SurahInfo s = g_app->db->get_surah(a->surah_number);
            std::wstring s_name = s.name_arabic;
            std::vector<std::wstring> lines;

            auto count_stats = [](const std::wstring& simple_txt) {
                struct Stats { int words = 0; int letters = 0; int allah = 0; };
                Stats st;
                std::wstring word;
                auto flush_word = [&]() {
                    if (word.empty()) return;
                    bool has_letter = false;
                    int l_cnt = 0;
                    std::wstring norm;
                    for (wchar_t ch : word) {
                        if ((ch >= 0x0621 && ch <= 0x064A) || ch == 0x0671) {
                            has_letter = true;
                            ++l_cnt;
                            wchar_t n = ch;
                            if (n == 0x0623 || n == 0x0625 || n == 0x0622 || n == 0x0671) n = 0x0627;
                            else if (n == 0x0649) n = 0x064A;
                            else if (n == 0x0629) n = 0x0647;
                            norm.push_back(n);
                        }
                    }
                    if (has_letter) {
                        ++st.words;
                        st.letters += l_cnt;
                        if (norm == L"الله" || norm == L"لله" || norm == L"والله" || norm == L"بالله" ||
                            norm == L"فالله" || norm == L"تالله" || norm == L"ولله" || norm == L"فلله" ||
                            norm == L"ابالله" || norm == L"وبالله" || norm == L"فبالله" || norm == L"االله") {
                            ++st.allah;
                        }
                    }
                    word.clear();
                };
                for (wchar_t ch : simple_txt) {
                    if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') flush_word();
                    else word.push_back(ch);
                }
                flush_word();
                return st;
            };

            auto st = count_stats(a->text_simple);
            std::wstring letter_freq = letter_frequency_text({&a->text_simple});
            auto same_keys = g_app->db->find_same_text(a->text_simple);
            std::vector<std::wstring> other_places;
            for (const auto& k : same_keys) {
                if (k == a->verse_key) continue;
                size_t col = k.find(L':');
                if (col != std::wstring::npos) {
                    int os = _wtoi(k.substr(0, col).c_str());
                    int oa = _wtoi(k.substr(col + 1).c_str());
                    SurahInfo osi = g_app->db->get_surah(os);
                    other_places.push_back((g_app->is_en() ? osi.name_arabic : (L"سورة " + osi.name_arabic)) + L" " + std::to_wstring(oa));
                }
            }

            static const wchar_t* q_ar[4] = {L"الربع الأول", L"الربع الثاني", L"الربع الثالث", L"الربع الرابع"};
            static const wchar_t* q_en[4] = {L"first quarter", L"second quarter", L"third quarter", L"fourth quarter"};
            int q_idx = (a->rub_el_hizb_number - 1) % 4;
            std::wstring rev_place = g_app->is_en() ? (s.revelation_place == L"مكية" ? L"Meccan" : L"Medinan") : s.revelation_place;
            SurahHeadingInfo sh = g_app->db->get_surah_heading_info(a->surah_number);
            auto is_ayah_excepted = [](int ay_num, const std::wstring& exc) -> bool {
                if (exc.empty()) return false;
                std::wstring norm = to_western_digits(exc);
                size_t i = 0;
                while (i < norm.size()) {
                    while (i < norm.size() && !iswdigit(norm[i])) ++i;
                    if (i >= norm.size()) break;
                    int a1 = 0;
                    while (i < norm.size() && iswdigit(norm[i])) { a1 = a1 * 10 + (norm[i] - L'0'); ++i; }
                    if (i < norm.size() && norm[i] == L'-') {
                        ++i;
                        int a2 = 0;
                        while (i < norm.size() && iswdigit(norm[i])) { a2 = a2 * 10 + (norm[i] - L'0'); ++i; }
                        if (ay_num >= a1 && ay_num <= a2) return true;
                    } else if (ay_num == a1) {
                        return true;
                    }
                }
                return false;
            };
            const bool edition_details = g_app->db->edition_has_details();
            bool ay_excepted = edition_details && is_ayah_excepted(a->ayah_number, sh.excepted_verses);

            if (g_app->is_en()) {
                lines.push_back(L"﴿ " + a->text_uthmani + L" ﴾");
                lines.push_back(L"Surah: " + s_name + L" (Surah #" + std::to_wstring(a->surah_number) + L" of 114, " + rev_place + L")");
                if (ay_excepted) {
                    std::wstring exc_msg = L"Revelation exception: this ayah differs from the rest of the surah in its place of revelation";
                    if (!sh.place_note_en.empty()) exc_msg += L" (" + sh.place_note_en + L")";
                    lines.push_back(exc_msg);
                }
                lines.push_back(L"Ayah number: " + std::to_wstring(a->ayah_number) + L" of " + std::to_wstring(s.verses_count));
                if (a->ayah_number == 1) lines.push_back(L"Position in the surah: first ayah");
                else if (a->ayah_number == s.verses_count) lines.push_back(L"Position in the surah: last ayah");
                lines.push_back(L"Order in the Quran: " + std::to_wstring(a->id) + L" of 6,236");
                lines.push_back(L"Words: " + std::to_wstring(st.words));
                lines.push_back(L"Letters: " + std::to_wstring(st.letters) + L" (without diacritics and spaces)");
                if (!letter_freq.empty()) lines.push_back(L"Letter breakdown: " + letter_freq);
                lines.push_back(L"The name of Allah (الله) occurs: " + (st.allah ? std::to_wstring(st.allah) + ((st.allah) == 1 ? L" time" : L" times") : L"does not occur"));
                if (other_places.empty()) {
                    lines.push_back(L"Repetition: this text occurs nowhere else in the Quran");
                } else {
                    std::wstring joined;
                    for (size_t i = 0; i < other_places.size() && i < 15; ++i) {
                        if (i > 0) joined += L", ";
                        joined += other_places[i];
                    }
                    if (other_places.size() > 15) joined += L" ...";
                    lines.push_back(L"Same text elsewhere (" + std::to_wstring(other_places.size()) + L"): " + joined);
                }
                lines.push_back(L"Page (" + g_app->edition_short_label() + L"): " + std::to_wstring(a->page_number) + L" of " + std::to_wstring(g_app->db->page_count()));
                lines.push_back(L"Juz: " + std::to_wstring(a->juz_number) + L" of 30 • Hizb: " + std::to_wstring(a->hizb_number) + L" of 60");
                lines.push_back(L"Quarter: " + std::to_wstring(a->rub_el_hizb_number) + L" of 240 (" + std::wstring(q_en[q_idx]) + L" of Hizb " + std::to_wstring(a->hizb_number) + L")");
                lines.push_back(L"Ruku: " + std::to_wstring(a->ruku_number) + L" of 558");
                lines.push_back(L"Manzil: " + std::to_wstring(a->manzil_number) + L" of 7 (" + manzil_day_label(a->manzil_number, true) + L")");
            } else {
                lines.push_back(L"﴿ " + a->text_uthmani + L" ﴾");
                lines.push_back(L"السورة: سورة " + s_name + L" (رقم " + std::to_wstring(a->surah_number) + L" من 114 — " + rev_place + L")");
                if (ay_excepted) {
                    std::wstring exc_msg = L"استثناء النزول: هذه الآية مستثناة من مكان نزول السورة";
                    for (const auto& grp : sh.groups) {
                        if (std::find(grp.verses.begin(), grp.verses.end(), a->ayah_number) == grp.verses.end()) continue;
                        std::wstring why = grp.place;
                        if (!why.empty()) {
                            if (why[0] == L'ف') why.erase(0, 1);              // «فبالجحفة…» ← «بالجحفة…»
                            if (why.rfind(L"نزلت", 0) != 0) why = L"نزلت " + why;
                        } else {
                            why = !grp.kind.empty() ? grp.kind : ((sh.type == L"مكية") ? L"مدنية" : L"مكية");
                        }
                        exc_msg += L" (" + why + L")";
                        break;
                    }
                    lines.push_back(exc_msg);
                }
                lines.push_back(L"رقم الآية: " + std::to_wstring(a->ayah_number) + L" من أصل " + std::to_wstring(s.verses_count) + L" آية");
                if (a->ayah_number == 1) lines.push_back(L"موقعها في السورة: أول آية في السورة");
                else if (a->ayah_number == s.verses_count) lines.push_back(L"موقعها في السورة: آخر آية في السورة");
                lines.push_back(L"الترتيب العام للآية في المصحف: " + std::to_wstring(a->id) + L" من 6236");
                lines.push_back(L"عدد الكلمات: " + words_ar(st.words));
                lines.push_back(L"عدد الحروف: " + letters_ar(st.letters) + L" (دون التشكيل والمسافات)");
                if (!letter_freq.empty()) lines.push_back(L"تفصيل الحروف في الآية: " + letter_freq);
                lines.push_back(L"ورود لفظ الجلالة «الله»: " + (st.allah ? times_ar(st.allah) : std::wstring(L"لم يرد في هذه الآية")));
                if (other_places.empty()) {
                    lines.push_back(L"تكرار نص الآية: لا يتكرر نصها في موضع آخر من القرآن");
                } else {
                    std::wstring joined;
                    for (size_t i = 0; i < other_places.size() && i < 15; ++i) {
                        if (i > 0) joined += L"، ";
                        joined += other_places[i];
                    }
                    if (other_places.size() > 15) joined += L" ...";
                    lines.push_back(L"تكرار نص الآية في مواضع أخرى (" + to_arabic_digits((int)other_places.size()) + L"): " + joined);
                }
                lines.push_back(L"الصفحة (" + g_app->edition_short_label() + L"): " + std::to_wstring(a->page_number) + L" من " + std::to_wstring(g_app->db->page_count()));
                lines.push_back(L"الجزء: " + std::to_wstring(a->juz_number) + L" من 30 • الحزب: " + std::to_wstring(a->hizb_number) + L" من 60");
                lines.push_back(L"الربع العام: " + std::to_wstring(a->rub_el_hizb_number) + L" من 240 (" + std::wstring(q_ar[q_idx]) + L" من الحزب " + std::to_wstring(a->hizb_number) + L")");
                lines.push_back(L"الركوع: " + std::to_wstring(a->ruku_number) + L" من 558");
                lines.push_back(L"التحزيب السباعي: المنزلة " + std::to_wstring(a->manzil_number) + L" من 7 (" + manzil_day_label(a->manzil_number, false) + L")");
            }
            if (const SajdahPlace* sp = print_sajdah_place(a->verse_key, edition_details)) {
                if (edition_details) {
                    lines.push_back(g_app->is_en() ? (L"Sajdah: " + std::wstring(sp->note_en)) : (L"بيان السجدة: " + std::wstring(sp->note_ar)));
                } else {
                    lines.push_back(g_app->tr(L"السجدة: موضع سجدة تلاوة", L"Sajdah: place of a prostration of recitation"));
                }
            }
            // في الواجهة العربية تُكتب الأرقام كلها بالأرقام العربية المشرقية، كسائر البرنامج
            if (!g_app->is_en()) for (auto& l : lines) l = to_arabic_digits(l);
            show_line_reader_dialog(
                hwnd,
                g_app->tr(L"معلومات عن الآية " + g_app->num_str(a->ayah_number) + L" من سورة " + s_name,
                          L"Ayah " + std::to_wstring(a->ayah_number) + L" of Surah " + s_name + L" — information"),
                lines, g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance()
            );
            return 0;
        }

        case IDM_SURAH_INFO: {
            const int n = g_app->take_context_surah();
            const std::wstring name = g_app->db->get_surah(n).name_arabic;
            auto lines = build_surah_info_lines(n, !g_app->is_en());
            // عنوان النافذة مشكول: «مَعْلُومَاتٌ عَنْ سُورَةِ البَقَرَةِ»
            std::wstring title_name = g_app->db->get_surah(n).name_uthmani;
            const std::wstring nom = L"سُورَةُ";
            if (title_name.compare(0, nom.size(), nom) == 0) title_name = L"سُورَةِ" + title_name.substr(nom.size());
            show_line_reader_dialog(
                hwnd,
                g_app->tr(L"مَعْلُومَاتٌ عَنْ " + title_name, L"Surah " + name + L" — information"),
                lines, g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance()
            );
            return 0;
        }

        case IDM_SHORTCUTS_HELP: {
            bool en = g_app->is_en();
            std::vector<std::wstring> lines;
            if (en) {
                lines.push_back(L"Up / Down Arrow: Move between ayahs and hear them immediately");
                lines.push_back(L"PageDown / PageUp: Next / Previous unit (Surah, Page, Juz, Quarter, Hizb, Manzil or Ruku)");
                lines.push_back(L"Escape: Leave the ayahs and return to the list of surahs (or pages, juz...)");
                lines.push_back(L"Applications key or Shift+F10 on an ayah: ayah menu (copy, ayah information, bookmark, reading mark)");
                lines.push_back(L"Applications key or Shift+F10 on a surah heading or in the surah list: surah menu (surah information, copy heading, copy whole surah)");
                lines.push_back(L"Enter / Ctrl+C / F8: Copy current ayah to clipboard");
                lines.push_back(L"Ctrl+Alt+C: Multiple copy — the current unit's ayahs with checkboxes; Space selects, Enter copies");
                lines.push_back(L"Mouse: drag over ayahs to highlight them (each drag adds to the highlight, Ctrl+click toggles one ayah); Copy then copies all highlighted ayahs");
                lines.push_back(L"M / Ctrl+M: Set the reading mark at the current ayah, or remove it if it is already there");
                lines.push_back(L"B: Add or remove current ayah from bookmarks");
                lines.push_back(L"I: Speak the position of the ayah");
            } else {
                lines.push_back(L"سهم لأعلى / سهم لأسفل: التنقل بين الآيات وقراءتها فورًا");
                lines.push_back(L"PageDown / PageUp: الانتقال إلى الوحدة التالية أو السابقة (سورة / صفحة / جزء / ربع / حزب / منزلة / ركوع)");
                lines.push_back(L"Escape: الخروج من الآيات والرجوع إلى قائمة السور (أو الصفحات والأجزاء...)");
                lines.push_back(L"زر التطبيقات أو Shift+F10 على الآية: قائمة الآية (النسخ، ومعلومات الآية، والمفضلة، وفاصل الختمة)");
                lines.push_back(L"زر التطبيقات أو Shift+F10 على عنوان السورة أو في فهرس السور: قائمة السورة (معلومات السورة، ونسخ بيانها، ونسخها كاملة)");
                lines.push_back(L"Enter أو Ctrl+C أو F8: نسخ الآية الحالية إلى الحافظة");
                lines.push_back(L"Ctrl+Alt+C: النسخ المتعدد — آيات الوحدة الحالية بمربعات اختيار، المسافة للتحديد و Enter للنسخ");
                lines.push_back(L"بالفأرة: اسحب على الآيات لتظليلها (كل سحب يضيف إلى التظليل السابق، و Ctrl مع النقر يظلل آية واحدة أو يلغيها)، ثم «نسخ» ينسخ كل المظلل");
                lines.push_back(L"M أو Ctrl+M: وضع فاصل الختمة (علامة التوقف في القراءة) عند الآية الحالية، أو إزالته إن كان عندها");
                lines.push_back(L"B: إضافة أو إزالة الآية الحالية من المفضلة");
                lines.push_back(L"حرف I: نطق سريع لموضع الآية (السورة، الصفحة، الجزء، الحزب، الربع، المنزلة)");
            }
            for (const auto& def : shortcut_defs()) {
                std::wstring spec = effective_shortcut_spec(g_app->settings.custom_shortcuts, def);
                lines.push_back(spec + L": " + (en ? def.label_en : def.label_ar));
            }
            show_line_reader_dialog(
                hwnd,
                g_app->tr(L"دليل اختصارات لوحة المفاتيح وقارئات الشاشة", L"Keyboard and screen reader guide"),
                lines, g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance()
            );
            return 0;
        }

        case IDM_FONT_INC:
            { const int before = g_app->settings.quran_font_size;
              g_app->settings.quran_font_size = std::min(52, g_app->settings.quran_font_size + 2);
              g_app->settings.save();
              g_app->recreate_fonts_and_brushes();
              g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
              g_app->announce_font_size(before); }
            return 0;
        case IDM_FONT_DEC:
            { const int before = g_app->settings.quran_font_size;
              g_app->settings.quran_font_size = std::max(16, g_app->settings.quran_font_size - 2);
              g_app->settings.save();
              g_app->recreate_fonts_and_brushes();
              g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
              g_app->announce_font_size(before); }
            return 0;
        case IDM_FONT_RESET:
            { const int before = g_app->settings.quran_font_size;
              g_app->settings.quran_font_size = 26;
              g_app->settings.save();
              g_app->recreate_fonts_and_brushes();
              g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
              g_app->announce_font_size(before); }
            return 0;

        case IDM_ALIGN_CENTER:
        case IDM_ALIGN_RIGHT:
            g_app->settings.doc_alignment = (id == IDM_ALIGN_RIGHT) ? L"right" : L"center";
            g_app->settings.save();
            g_app->build_menus();
            InvalidateRect(g_app->list_ayahs, nullptr, TRUE);
            g_app->say(id == IDM_ALIGN_RIGHT ? g_app->tr(L"محاذاة السطور لليمين", L"Lines right-aligned")
                                             : g_app->tr(L"توسيط السطور كالمصحف", L"Lines centered like a Mushaf"));
            return 0;

        case IDM_MUSHAF_FRAME:
            g_app->settings.show_mushaf_frame = !g_app->settings.show_mushaf_frame;
            g_app->settings.save();
            g_app->build_menus();
            g_app->update_sidebar_visibility();   // يعيد التخطيط والرسم
            g_app->say(g_app->settings.show_mushaf_frame ? g_app->tr(L"إظهار إطار المصحف", L"Mushaf frame shown")
                                                         : g_app->tr(L"إخفاء إطار المصحف", L"Mushaf frame hidden"));
            return 0;

        case IDM_MUSHAF_LINES:
            g_app->settings.mushaf_lines = !g_app->settings.mushaf_lines;
            g_app->settings.save();
            g_app->build_menus();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            g_app->say(g_app->settings.mushaf_lines
                           ? g_app->tr(L"سطور الطبعة كما في المصحف", L"Page lines as in the Mushaf")
                           : g_app->tr(L"الآيات آيةً آية", L"One ayah per row"));
            return 0;

        case IDM_DISPLAY_TEXT:
        case IDM_DISPLAY_NUM:
        case IDM_DISPLAY_SURAH:
            g_app->settings.ayah_display = (id == IDM_DISPLAY_TEXT) ? L"text"
                                         : (id == IDM_DISPLAY_NUM)  ? L"text_number"
                                         : L"text_number_surah";
            g_app->settings.save();
            g_app->build_menus();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            g_app->say(id == IDM_DISPLAY_TEXT ? g_app->tr(L"نص الآية فقط", L"Ayah text only")
                       : id == IDM_DISPLAY_NUM ? g_app->tr(L"نص الآية مع رقمها", L"Ayah text with its number")
                       : g_app->tr(L"نص الآية مع رقمها واسم السورة", L"Ayah text with its number and surah name"));
            return 0;

        case IDM_CYCLE_THEME: {
            static const wchar_t* th[4] = {L"madinah_cream", L"royal_emerald", L"night_gold", L"high_contrast"};
            int idx = 0;
            for (int i = 0; i < 4; ++i) if (g_app->settings.theme == th[i]) idx = (i + 1) % 4;
            g_app->settings.theme = th[idx];
            g_app->settings.save();
            g_app->recreate_fonts_and_brushes();
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            static const wchar_t* th_ar[4] = {L"كريمي المدينة", L"أخضر زمردي ملكي", L"ليلي ذهبي هادئ", L"تباين عالٍ"};
            static const wchar_t* th_en[4] = {L"Madinah Cream", L"Royal Emerald", L"Night Gold", L"High Contrast"};
            std::wstring msg = g_app->tr(std::wstring(L"المظهر: ") + th_ar[idx], std::wstring(L"Theme: ") + th_en[idx]);
            // التباين العالي في ويندوز يغلب على اختيار البرنامج، فلا يتغير شيء على الشاشة حتى يُطفأ
            if (g_app->system_high_contrast())
                msg += g_app->tr(L"، ويظهر بعد إيقاف التباين العالي في ويندوز", L", shown once Windows high contrast is off");
            g_app->say(msg);
            return 0;
        }

        case IDM_LANG_AR:
        case IDM_LANG_EN: {
            std::wstring lang = (id == IDM_LANG_EN) ? L"en" : L"ar";
            if (lang == g_app->settings.ui_language) return 0;
            g_app->settings.ui_language = lang;
            g_app->relaunch_for_language();
            return 0;
        }

        case IDM_USER_GUIDE:
            g_app->open_user_guide();
            return 0;

        case IDM_CHECK_UPDATES:
            g_app->start_update_check(true);
            return 0;

        case IDM_CYCLE_EDITION: {
            const auto& eds = g_app->db->editions();
            if (eds.size() < 2) {
                g_app->say(g_app->tr(L"لا تتوفر إلا طبعة واحدة", L"Only one print is available"));
                return 0;
            }
            size_t cur = 0;
            for (size_t i = 0; i < eds.size(); ++i) if (eds[i].key == g_app->db->edition()) cur = i;
            g_app->switch_edition(eds[(cur + 1) % eds.size()].key);
            return 0;
        }

        case IDM_CYCLE_DISPLAY: {
            const std::wstring& d = g_app->settings.ayah_display;
            UINT next = (d == L"text") ? IDM_DISPLAY_NUM : (d == L"text_number") ? IDM_DISPLAY_SURAH : IDM_DISPLAY_TEXT;
            SendMessageW(hwnd, WM_COMMAND, next, 0);
            return 0;
        }

        case IDM_TOGGLE_ALIGN:
            SendMessageW(hwnd, WM_COMMAND, g_app->settings.doc_alignment == L"right" ? IDM_ALIGN_CENTER : IDM_ALIGN_RIGHT, 0);
            return 0;

        case IDM_TOGGLE_SOUND:
            g_app->settings.page_turn_sound = !g_app->settings.page_turn_sound;
            g_app->settings.save();
            g_app->build_menus();
            g_app->say(g_app->settings.page_turn_sound ? g_app->tr(L"تشغيل صوت تقليب الصفحة", L"Page-turn sound on")
                                                       : g_app->tr(L"إيقاف صوت تقليب الصفحة", L"Page-turn sound off"));
            return 0;

        case IDM_TOGGLE_SR_WAQF:
        case IDM_TOGGLE_MUQATTAAT: {
            const bool waqf = (id == IDM_TOGGLE_SR_WAQF);
            bool& flag = waqf ? g_app->settings.sr_speak_waqf : g_app->settings.sr_spell_muqattaat;
            flag = !flag;
            g_app->settings.save();
            g_app->build_menus();
            // نص الآيات المنطوق يُبنى مع القائمة، فتُعاد بناؤها على الآية نفسها
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            g_app->say(waqf ? (flag ? g_app->tr(L"نطق علامات الوقف", L"Pause marks are spoken")
                                    : g_app->tr(L"إيقاف نطق علامات الوقف", L"Pause marks are not spoken"))
                            : (flag ? g_app->tr(L"تفكيك الحروف المقطعة", L"Disjointed letters are spelled out")
                                    : g_app->tr(L"إيقاف تفكيك الحروف المقطعة", L"Disjointed letters are read as written")));
            return 0;
        }

        case IDM_ABOUT: {
            // آية الحفظ تُقرأ من قاعدة البيانات نفسها حتى يطابق رسمها المصحف حرفًا بحرف
            AyahInfo hifz;
            std::wstring hifz_txt;
            if (g_app->db->get_ayah(15, 9, hifz)) {
                // بخط الواجهة لا بالخط القرآني: بلا علامات المصحف الصغيرة (الواو الصغيرة والصفر المستدير
                // وعلامات الوقف) ولا المدّة ولا التطويل، مع بقاء الحروف والحركات والألف الخنجرية
                std::wstring clean;
                for (wchar_t ch : hifz.text_uthmani) {
                    if ((ch >= 0x06D6 && ch <= 0x06ED) || ch == 0x0653 || ch == 0x0640) continue;
                    clean.push_back(ch);
                }
                hifz_txt = L"﴿" + join_words(split_words(clean), L" ") + L"﴾";
            }
            // الآية في رأس النافذة، ثم التعريف بالبرنامج
            std::vector<std::wstring> lines = g_app->is_en() ? std::vector<std::wstring>{
                hifz_txt,
                std::wstring(L"Al-Mushaf Al-Sharif — Version ") + kAppVersion,
                L"The Noble Quran in the narration of Hafs from 'Asim, in the Uthmani script. "
                L"Its pages can be browsed as laid out in three prints: the new and classic prints of the King Fahd "
                L"Complex for the Printing of the Holy Quran in Madinah, and the Dar al-Maarif print in Egypt.",
                L"Built to be a calm companion for reciting, reviewing and reflecting on the Book of Allah, "
                L"with nothing to draw the heart away from its verses.",
                L"Developed by the Omnya Technology team and offered, seeking only the pleasure of Allah, "
                L"as a gift to researchers and all Muslims.",
                L"Runs on Windows 7 and later, and serves sighted readers and screen reader users alike.",
                L"© 2026 Omnya Technology"
            } : std::vector<std::wstring>{
                hifz_txt,
                std::wstring(L"المصحف الشريف — الإصدار ") + kAppVersion,
                L"القرآن الكريم برواية حفص عن عاصم، بالرسم العثماني، "
                L"ويُتصفَّح بصفحات ثلاث طبعات: طبعتي مجمع الملك فهد لطباعة المصحف الشريف بالمدينة المنورة "
                L"الحديثة والقديمة، وطبعة دار المعارف بمصر.",
                L"صُمِّم ليكون رفيقًا هادئًا لتلاوة كتاب الله ومراجعته وتدبّره، "
                L"خاليًا مما يشغل القلب عن آياته.",
                L"طوّره فريق (Omnya Technology)، وقدّمه هديةً خالصةً لوجه الله تعالى "
                L"إلى الباحثين وعموم المسلمين.",
                L"يعمل على ويندوز ٧ فأحدث، ويخدم المبصرين ومستخدمي قارئات الشاشة على حدٍّ سواء.",
                L"حقوق النشر © ٢٠٢٦ لفريق Omnya Technology"
            };
            lines.erase(std::remove(lines.begin(), lines.end(), std::wstring()), lines.end());
            show_line_reader_dialog(
                hwnd,
                g_app->tr(L"حول برنامج المصحف الشريف", L"About Al-Mushaf Al-Sharif"),
                lines, g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance()
            );
            return 0;
        }

        case IDM_EXIT:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
        // تشغيل التباين العالي في ويندوز أو إيقافه أثناء عمل البرنامج
        if (g_app && g_app->list_ayahs) {
            g_app->recreate_fonts_and_brushes();
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }
        break;

    case WM_DESTROY:
        if (g_app && !g_app->restarting) g_app->save_state();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int g_selftest_failures = 0;
#define SELFTEST_CHECK(cond) do { \
    if (!(cond)) { ++g_selftest_failures; std::fprintf(stderr, "FAIL (line %d): %s\n", __LINE__, #cond); } \
} while (0)

static int run_self_test() {
    MushafAppWindow app;
    app.resolve_paths();
    app.db = std::make_unique<QuranDatabase>(
        app.assets_dir + L"\\data\\quran.sqlite3",
        app.assets_dir + L"\\data\\surah_info_egyptian.json"
    );
    if (!app.db->is_open()) {
        std::fprintf(stderr, "FAIL: Could not open quran.sqlite3\n");
        return 1;
    }
    SELFTEST_CHECK(app.db->total_ayahs() == 6236);
    SELFTEST_CHECK(app.db->get_all_surahs().size() == 114);
    SELFTEST_CHECK(app.db->page_count() == 604);
    SELFTEST_CHECK(app.db->get_page_info_list().size() == 604);
    SELFTEST_CHECK(app.db->get_juz_info_list().size() == 30);
    SELFTEST_CHECK(app.db->get_hizb_info_list().size() == 60);
    SELFTEST_CHECK(app.db->get_rub_info_list().size() == 240);
    SELFTEST_CHECK(app.db->get_manzil_info_list().size() == 7);
    SELFTEST_CHECK(app.db->get_ruku_info_list().size() == 558);
    SELFTEST_CHECK(app.db->rub_start_ids().size() == 240);

    // صفحات طبعة المدينة: البقرة تبدأ من ٢ والناس في ٦٠٤
    SELFTEST_CHECK(app.db->get_surah(2).start_page == 2);
    SELFTEST_CHECK(app.db->get_surah(114).end_page == 604);
    SELFTEST_CHECK(app.db->get_surah(78).name_arabic == L"النبأ" && app.db->get_surah(34).name_arabic == L"سبأ");
    SELFTEST_CHECK(build_surah_brief(app.db->get_surah(4)) == L"مدنية وآياتها ١٧٦");
    SELFTEST_CHECK(app.db->get_surah(82).name_arabic == L"الانفطار" && app.db->get_surah(84).name_arabic == L"الانشقاق");
    SELFTEST_CHECK(build_surah_brief(app.db->get_surah(1), true) == L"Meccan • 7 verses");

    AyahInfo kursi;
    SELFTEST_CHECK(app.db->get_ayah(2, 255, kursi));
    SELFTEST_CHECK(kursi.page_number == 42);

    // كل آية تقع في ركوع واحد بالضبط، وآخر ركوع ينتهي بآخر آية في المصحف
    {
        size_t total = 0;
        for (int r = 1; r <= 558; ++r) total += app.db->get_ruku_ayahs(r).size();
        SELFTEST_CHECK(total == 6236);
        auto last = app.db->get_ruku_ayahs(558);
        SELFTEST_CHECK(!last.empty() && last.back().verse_key == L"114:6");
    }

    AyahInfo alm;
    SELFTEST_CHECK(app.db->get_ayah(2, 1, alm));
    SELFTEST_CHECK(prepare_ayah_for_screen_reader(alm, true, false) == L"الم");
    {
        // العدد والمعدود
        // ملف قياسات الكلمات: يُقرأ كما حُفظ، ويُرفض إن اختلف الخط أو نقص الملف أو تلف
        {
            wchar_t tdir[MAX_PATH] = {0};
            GetTempPathW(MAX_PATH, tdir);
            const std::wstring tf = std::wstring(tdir) + L"mushaf_word_widths_test.bin";
            MushafAppWindow::WordWidths in{{L"كلمة", 42}, {L"ٱلْحَمْدُ", 97}}, got;
            SELFTEST_CHECK(MushafAppWindow::save_word_widths(tf, L"Font|-35|400", in));
            SELFTEST_CHECK(MushafAppWindow::load_word_widths(tf, L"Font|-35|400", got) && got == in);
            got.clear();
            SELFTEST_CHECK(!MushafAppWindow::load_word_widths(tf, L"Font|-36|400", got) && got.empty());
            std::string raw;
            if (FILE* f = _wfopen(tf.c_str(), L"rb")) { char c[4096]; size_t n; while ((n = std::fread(c, 1, sizeof(c), f)) > 0) raw.append(c, n); std::fclose(f); }
            SELFTEST_CHECK(raw.size() > 20);
            for (int mode = 0; mode < 2; ++mode) {
                std::string bad = raw;
                if (mode == 0) bad.resize(bad.size() / 2); else bad[bad.size() / 2] ^= 0x55;
                if (FILE* f = _wfopen(tf.c_str(), L"wb")) { std::fwrite(bad.data(), 1, bad.size(), f); std::fclose(f); }
                got.clear();
                SELFTEST_CHECK(!MushafAppWindow::load_word_widths(tf, L"Font|-35|400", got) && got.empty());
            }
            DeleteFileW(tf.c_str());
        }
        SELFTEST_CHECK(letters_ar(1) == L"حرف واحد");
        SELFTEST_CHECK(letters_ar(2) == L"حرفان");
        SELFTEST_CHECK(letters_ar(5) == L"٥ أحرف");
        SELFTEST_CHECK(letters_ar(15) == L"١٥ حرفًا");
        SELFTEST_CHECK(letters_ar(100) == L"١٠٠ حرف");
        SELFTEST_CHECK(letters_ar(103) == L"١٠٣ أحرف");
        SELFTEST_CHECK(letters_ar(113) == L"١١٣ حرفًا");
        SELFTEST_CHECK(ayahs_ar(286) == L"٢٨٦ آيةً");
        // نسخ السورة كاملة: رقم الآية وحده بعدها، بلا اسم السورة
        {
            AyahInfo sa;
            SELFTEST_CHECK(app.db->get_ayah(2, 1, sa));
            const std::wstring t = format_ayah_in_surah(sa, L"uthmani", true, true, true);
            SELFTEST_CHECK(t.size() > 4 && t.compare(t.size() - 4, 4, L" [\u0661]") == 0);
            SELFTEST_CHECK(t.find(sa.surah_name) == std::wstring::npos);
            const std::wstring plain = format_ayah_in_surah(sa, L"uthmani", true, false, true);
            SELFTEST_CHECK(plain.find(L'[') == std::wstring::npos);
        }
        // النسخ بعلامات الوقف أو بدونها: الحروف والتشكيل كما هي، وتسقط العلامات وحدها
        AyahInfo dhalika;
        SELFTEST_CHECK(app.db->get_ayah(2, 2, dhalika));
        const std::wstring with_w = format_ayah_custom(dhalika, L"uthmani", false, false, true, 1);
        const std::wstring no_w = format_ayah_custom(dhalika, L"uthmani", false, false, true, 0);
        SELFTEST_CHECK(with_w.find(L'ۛ') != std::wstring::npos);
        SELFTEST_CHECK(no_w.find(L'ۛ') == std::wstring::npos);
        SELFTEST_CHECK(no_w.find(L"  ") == std::wstring::npos);
        std::wstring stripped = with_w;
        stripped.erase(std::remove(stripped.begin(), stripped.end(), L'ۛ'), stripped.end());
        SELFTEST_CHECK(join_words(split_words(stripped), L" ") == no_w);
    }
    SELFTEST_CHECK(prepare_ayah_for_screen_reader(alm, true, true) == L"أَلِفْ لَامْ مِيمْ");

    // علامات الوقف: مُسقطة افتراضيًا، وتُنطق بأسمائها عند تفعيل الخيار (البقرة ٢: لا ريب ۛ فيه ۛ)
    {
        AyahInfo b2;
        SELFTEST_CHECK(app.db->get_ayah(2, 2, b2));
        std::wstring plain = prepare_ayah_for_screen_reader(b2, true, false, false);
        std::wstring named = prepare_ayah_for_screen_reader(b2, true, false, true);
        SELFTEST_CHECK(plain.find(L"تعانق") == std::wstring::npos);
        SELFTEST_CHECK(named.find(L"رَيْبَ (تعانق) فِيهِ (تعانق)") != std::wstring::npos);
        // السين الصغيرة داخل كلمة تنبيه على النطق لا علامة وقف (البقرة ٢٤٥: وَيَبْصُۜطُ)
        AyahInfo b245;
        SELFTEST_CHECK(app.db->get_ayah(2, 245, b245));
        SELFTEST_CHECK(prepare_ayah_for_screen_reader(b245, true, false, true).find(L"(س)") == std::wstring::npos);
    }

    AyahInfo a26, a75, a142;
    SELFTEST_CHECK(app.db->get_ayah(2, 26, a26));
    SELFTEST_CHECK(division_badge_text(a26, false) == L"[۞ ربع الحزب ١]");
    SELFTEST_CHECK(division_spoken_text(a26, false) == L"ربع الحزب 1");

    SELFTEST_CHECK(app.db->get_ayah(2, 75, a75));
    SELFTEST_CHECK(division_badge_text(a75, false) == L"[۞ الحزب ٢ • الجزء ١]");
    SELFTEST_CHECK(division_spoken_text(a75, false) == L"بداية الحزب 2 من الجزء 1");

    SELFTEST_CHECK(app.db->get_ayah(2, 142, a142));
    SELFTEST_CHECK(division_badge_text(a142, false) == L"[۞ الجزء ٢ • الحزب ٣]");
    SELFTEST_CHECK(division_spoken_text(a142, false) == L"بداية الجزء 2، الحزب 3");

    const auto& juz_list = app.db->get_juz_info_list();
    SELFTEST_CHECK(app.division_ayah_range_text(juz_list[0]).find(L"من الآية ١ في سورة الفاتحة إلى الآية ١٤١ في سورة البقرة") != std::wstring::npos);
    SELFTEST_CHECK(app.division_ayah_range_text(juz_list[1]).find(L"من الآية ١٤٢ إلى الآية ٢٥٢ في سورة البقرة") != std::wstring::npos);

    SELFTEST_CHECK(count_arabic_letters(alm.text_simple) == 3);
    SELFTEST_CHECK(all_sajdah_places().size() == 15);
    for (const auto& def : shortcut_defs()) {
        ACCEL ac{};
        std::wstring rar, ren;
        SELFTEST_CHECK(parse_shortcut_spec(def.default_spec, ac, def.cmd_id));
        SELFTEST_CHECK(is_screen_reader_safe_shortcut(ac.fVirt, ac.key, rar, ren));
    }
    {
        std::wstring rar, ren;
        SELFTEST_CHECK(!is_screen_reader_safe_shortcut(FVIRTKEY, 'A', rar, ren));
        SELFTEST_CHECK(!is_screen_reader_safe_shortcut(FVIRTKEY | FCONTROL, VK_INSERT, rar, ren));
    }

    // الأعداد الترتيبية في بيانات الأرباع مع خط مجمع الملك فهد
    SELFTEST_CHECK(MushafAppWindow::ordinal_ar(1) == L"الأول");
    SELFTEST_CHECK(MushafAppWindow::ordinal_ar(11) == L"الحادي عشر");
    SELFTEST_CHECK(MushafAppWindow::ordinal_ar(21) == L"الحادي والعشرون");
    SELFTEST_CHECK(MushafAppWindow::ordinal_ar(30) == L"الثلاثون");
    SELFTEST_CHECK(MushafAppWindow::ordinal_ar(58) == L"الثامن والخمسون");
    SELFTEST_CHECK(MushafAppWindow::ordinals_for_quran_font(L"[۞ الحزب ٢ • الجزء ١]") == L"[۞ الحزب الثاني • الجزء الأول]");

    // الإعدادات: أوضاع التصفح السبعة مقبولة، وأي قيمة غريبة مرفوضة
    for (const wchar_t* s : {L"surah", L"page", L"juz", L"hizb", L"rub", L"manzil", L"ruku"}) {
        SELFTEST_CHECK(is_valid_browse_scope(s));
    }
    SELFTEST_CHECK(!is_valid_browse_scope(L"shamarly"));

    // آية الدَّين (البقرة ٢٨٢) أطول آية: لا بد أن تُقسَّم على صفوف كلٌّ منها في حدود الارتفاع،
    // وأن يبقى نصها كاملًا بلا نقص ولا زيادة بعد التقسيم.
    {
        app.load_private_fonts();

        // خط «كتاب»: يُحمَّل مع البرنامج، وفيه كل حروف آية الكرسي وعلاماتها بالرسم العثماني
        {
            AyahInfo kursi;
            SELFTEST_CHECK(app.db->get_ayah(2, 255, kursi));
            HDC kdc = CreateCompatibleDC(nullptr);
            HFONT kf = CreateFontW(-32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Kitab");
            HGDIOBJ kold = SelectObject(kdc, kf);
            wchar_t face[LF_FACESIZE] = {0};
            GetTextFaceW(kdc, LF_FACESIZE, face);
            SELFTEST_CHECK(std::wstring(face) == L"Kitab");
            std::vector<WORD> gi(kursi.text_uthmani.size());
            DWORD n = GetGlyphIndicesW(kdc, kursi.text_uthmani.c_str(), (int)kursi.text_uthmani.size(), gi.data(),
                                       GGI_MARK_NONEXISTING_GLYPHS);
            SELFTEST_CHECK(n == kursi.text_uthmani.size() && std::find(gi.begin(), gi.end(), (WORD)0xFFFF) == gi.end());
            SelectObject(kdc, kold);
            DeleteObject(kf);
            DeleteDC(kdc);
        }

        AyahInfo dayn;
        SELFTEST_CHECK(app.db->get_ayah(2, 282, dayn));
        HDC hdc = CreateCompatibleDC(nullptr);
        HFONT f = CreateFontW(-MulDiv(26, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"KFGQPC HAFS Uthmanic Script");
        HGDIOBJ old = SelectObject(hdc, f);
        const int width = 700;
        const int max_h = kRowMaxHeight - 2 * DS(kRowPadY);
        const int lh = MushafAppWindow::quran_line_height(hdc);
        auto lines = MushafAppWindow::wrap_text_lines(hdc, dayn.text_qpc_hafs, width);
        // أكثر من صف واحد لازم: كل سطورها لا تتسع لها حدود ارتفاع الصف الواحد
        SELFTEST_CHECK((int)lines.size() * lh > max_h);
        SELFTEST_CHECK(lh > 0 && lh <= max_h);
        std::wstring joined;
        for (const auto& ln : lines) {
            // كل سطر يتسع لعرضه، إلا كلمة مفردة أعرض من السطر (لا تحدث عمليًا)
            SIZE sz{};
            GetTextExtentPoint32W(hdc, ln.c_str(), (int)ln.size(), &sz);
            SELFTEST_CHECK(sz.cx <= width + 4 || ln.find(L' ') == std::wstring::npos);
            if (!joined.empty()) joined += L" ";
            joined += ln;
        }
        std::wstring squeezed;
        for (wchar_t ch : dayn.text_qpc_hafs) {
            if (ch == L' ' && !squeezed.empty() && squeezed.back() == L' ') continue;
            squeezed.push_back(ch);
        }
        while (!squeezed.empty() && squeezed.back() == L' ') squeezed.pop_back();
        while (!squeezed.empty() && squeezed.front() == L' ') squeezed.erase(squeezed.begin());
        SELFTEST_CHECK(joined == squeezed);

        // الآية القصيرة تبقى صفًا واحدًا
        SELFTEST_CHECK(MushafAppWindow::wrap_text_lines(hdc, alm.text_qpc_hafs, width).size() == 1);

        // آية الدَّين بكل خط قرآني وبأكبر حجم (٥٢) على شاشة عادية وعالية الدقة (٢٠٠٪) وفي نافذة ضيقة:
        // سطر واحد يتسع لصفه دائمًا، ولا يضيع من النص شيء ولا يتجاوز سطرٌ عرضه
        {
            const wchar_t* families[] = {L"KFGQPC HAFS Uthmanic Script", L"Amiri Quran", L"Amiri",
                                         L"Scheherazade New", L"Kitab"};
            for (const wchar_t* fam : families) {
                const std::wstring& txt = std::wstring(fam).find(L"KFGQPC") != std::wstring::npos
                                              ? dayn.text_qpc_hafs : dayn.text_uthmani;
                std::wstring want;
                for (wchar_t ch : txt) {
                    if (ch == L' ' && !want.empty() && want.back() == L' ') continue;
                    want.push_back(ch);
                }
                while (!want.empty() && want.back() == L' ') want.pop_back();
                while (!want.empty() && want.front() == L' ') want.erase(want.begin());
                for (int dpi : {96, 192}) {
                    HFONT big = MushafAppWindow::create_quran_font(-MulDiv(52, dpi, 72), fam);
                    HGDIOBJ prev = SelectObject(hdc, big);
                    const int blh = MushafAppWindow::quran_line_height(hdc);
                    SELFTEST_CHECK(blh > 0 && blh <= max_h);
                    for (int w : {700, 380}) {
                        auto blines = MushafAppWindow::wrap_text_lines(hdc, txt, w);
                        std::wstring got;
                        for (const auto& ln : blines) {
                            SIZE sz{};
                            GetTextExtentPoint32W(hdc, ln.c_str(), (int)ln.size(), &sz);
                            SELFTEST_CHECK(sz.cx <= w + 4 || ln.find(L' ') == std::wstring::npos);
                            if (!got.empty()) got += L" ";
                            got += ln;
                        }
                        SELFTEST_CHECK(got == want);
                    }
                    SelectObject(hdc, prev);
                    DeleteObject(big);
                }
            }
        }

        // السهم لأسفل في آخر آية لا يتحرك (فلا يعيد قارئ الشاشة قراءتها)، ولو كانت مقسّمة على صفوف
        {
            std::vector<AyahListItemData> rows(6);
            const int segs[] = {0, 0, 1, 0, 1, 2};   // ترويسة، آية من صفين، آية أخيرة من ثلاثة صفوف
            for (int i = 0; i < 6; ++i) rows[i].seg_index = segs[i];
            SELFTEST_CHECK(MushafAppWindow::next_item_start_row(rows, 0) == 1);
            SELFTEST_CHECK(MushafAppWindow::next_item_start_row(rows, 1) == 3);
            SELFTEST_CHECK(MushafAppWindow::next_item_start_row(rows, 3) == -1);
            SELFTEST_CHECK(MushafAppWindow::next_item_start_row(rows, 5) == -1);
            rows.resize(4);   // آية أخيرة من صف واحد
            SELFTEST_CHECK(MushafAppWindow::next_item_start_row(rows, 3) == -1);
            SELFTEST_CHECK(MushafAppWindow::next_item_start_row(rows, 1) == 3);
        }

        // السكتات الخمس: كلمة كل سكتة في سطر واحد بعينه من الآية، بالرسمين وبعرضين مختلفين
        {
            const int places[][2] = {{18, 1}, {36, 52}, {69, 28}, {75, 27}, {83, 14}};
            for (const auto& pl : places) {
                AyahInfo sa;
                SELFTEST_CHECK(app.db->get_ayah(pl[0], pl[1], sa));
                const SaktaPlace* sk = find_sakta_place(sa.verse_key);
                SELFTEST_CHECK(sk != nullptr);
                if (!sk) continue;
                for (const std::wstring* txt : {&sa.text_qpc_hafs, &sa.text_uthmani}) {
                    for (int w : {700, 260}) {
                        int hits = 0;
                        for (const auto& ln : MushafAppWindow::wrap_text_lines(hdc, *txt, w)) {
                            if (line_has_arabic_word(ln, sk->word)) ++hits;
                        }
                        SELFTEST_CHECK(hits == 1);
                    }
                }
                const std::wstring label = sakta_spoken_label(*sk);
                SELFTEST_CHECK(build_ayah_accessible_name(sa, false, false, L"text", false, L"", false, 0, false, true).find(label) != std::wstring::npos);
                SELFTEST_CHECK(build_ayah_accessible_name(sa, false, false, L"text", false, L"", false, 0, false, false).find(label) == std::wstring::npos);
            }
            SELFTEST_CHECK(find_sakta_place(L"2:245") == nullptr);
        }
        SelectObject(hdc, old);
        DeleteObject(f);
        DeleteDC(hdc);
    }

    // الطبعات: ملف بدايات صفحات المدينة نفسها يجب أن يُقبل ويعطي ٦٠٤ صفحات بالتقسيم نفسه
    {
        std::string csv = "# key: selftest\n# name_ar: طبعة اختبار\n# name_en: Test print\nالصفحة,السورة,الآية\n";
        const auto& pages = app.db->get_page_info_list();
        int prev_surah = 0;
        for (const auto& d : pages) {
            // السورة تُكتب بالاسم مرة وبالرقم مرة، وتُترك فارغة إذا لم تتغير
            std::string sname;
            if (d.start_surah_number != prev_surah) {
                sname = (d.unit_number % 2) ? to_utf8(L"سورة " + d.start_surah_name) : std::to_string(d.start_surah_number);
            }
            prev_surah = d.start_surah_number;
            csv += std::to_string(d.unit_number) + "," + sname + "," + std::to_string(d.start_ayah_number) + "\n";
        }
        EditionCheck chk = app.db->check_edition_text(csv);
        SELFTEST_CHECK(chk.ok());
        SELFTEST_CHECK(chk.info.page_count == 604);
        SELFTEST_CHECK(chk.info.name_ar == L"طبعة اختبار");
        SELFTEST_CHECK(app.db->add_edition(chk));
        SELFTEST_CHECK(app.db->set_edition(L"selftest") == L"selftest");
        SELFTEST_CHECK(app.db->page_count() == 604);
        AyahInfo k2;
        SELFTEST_CHECK(app.db->get_ayah(2, 255, k2) && k2.page_number == 42);
        SELFTEST_CHECK(app.db->set_edition(L"madinah") == L"madinah");
        SELFTEST_CHECK(app.db->set_edition(L"no-such-print") == L"madinah");

        // طبعة دار المعارف المرفقة: ٤٨٥ صفحة، الصفحة ٣٨ تبدأ بالآية ٣ من آل عمران، والأخيرة بالإخلاص
        app.db->load_editions_folder(app.assets_dir + L"\\data\\editions");
        if (app.db->set_edition(L"dar_almaarif") == L"dar_almaarif") {
            SELFTEST_CHECK(app.db->page_count() == 485);
            AyahInfo d1;
            SELFTEST_CHECK(app.db->get_ayah(3, 3, d1) && d1.page_number == 38);
            SELFTEST_CHECK(app.db->get_ayah(114, 6, d1) && d1.page_number == 485);
            // تفاصيل دار المعارف: سجدة المالكية في فصلت ٣٧ وبيان السورة
            AyahInfo f37, f38;
            SELFTEST_CHECK(app.db->get_ayah(41, 37, f37) && app.db->get_ayah(41, 38, f38));
            SELFTEST_CHECK(app.db->edition_has_details());
            // دار المعارف: أقوال المذاهب في السجدة، وسجدة فصلت عند الآية ٣٨ ومعها موضعها عند المالكية
            SELFTEST_CHECK(app.format_visual_ayah(f37).find(L"۩") == std::wstring::npos);
            SELFTEST_CHECK(app.margin_sajdah_lines(f37).empty());
            {
                const auto m38 = app.margin_sajdah_lines(f38);
                SELFTEST_CHECK(m38.size() == 3 && m38[0] == L"سَجْدَةٌ" &&
                               m38[1] == L"عِنْدَ الحَنَفِيَّةِ وَالشَّافِعِيَّةِ وَالحَنَابِلَةِ" &&
                               m38[2] == L"(وَعِنْدَ المَالِكِيَّةِ عِنْدَ الآيَةِ ٣٧)");
                SELFTEST_CHECK(app.format_spoken_ayah(f38).find(L"سَجْدَةٌ عِنْدَ الحَنَفِيَّةِ وَالشَّافِعِيَّةِ وَالحَنَابِلَةِ (وَعِنْدَ المَالِكِيَّةِ عِنْدَ الآيَةِ ٣٧)") != std::wstring::npos);
                AyahInfo h77;
                SELFTEST_CHECK(app.db->get_ayah(22, 77, h77));
                const auto ml = app.margin_sajdah_lines(h77);
                SELFTEST_CHECK(ml.size() == 2 && ml[0] == L"سَجْدَةٌ" && ml[1] == L"عِنْدَ الشَّافِعِيَّةِ وَالحَنَابِلَةِ");
                SELFTEST_CHECK(app.format_spoken_ayah(h77).find(L"سَجْدَةٌ عِنْدَ الشَّافِعِيَّةِ وَالحَنَابِلَةِ") != std::wstring::npos);
                AyahInfo sad24, alaq19, araf206;
                SELFTEST_CHECK(app.db->get_ayah(38, 24, sad24) && app.db->get_ayah(96, 19, alaq19) && app.db->get_ayah(7, 206, araf206));
                SELFTEST_CHECK(app.margin_sajdah_lines(sad24).back() == L"عِنْدَ الحَنَفِيَّةِ وَالمَالِكِيَّةِ");
                SELFTEST_CHECK(app.margin_sajdah_lines(alaq19).back() == L"عِنْدَ الحَنَفِيَّةِ وَالشَّافِعِيَّةِ وَالحَنَابِلَةِ");
                const auto ma = app.margin_sajdah_lines(araf206);
                SELFTEST_CHECK(ma.size() == 2 && ma[0] == L"سَجْدَةٌ" && ma[1] == L"(مُتَّفَقٌ عَلَيْهَا عِنْدَ الأَئِمَّةِ الأَرْبَعَةِ)");
                SELFTEST_CHECK(app.format_spoken_ayah(araf206).find(L"سَجْدَةٌ (مُتَّفَقٌ عَلَيْهَا عِنْدَ الأَئِمَّةِ الأَرْبَعَةِ)") != std::wstring::npos);
            }
            {
                AyahListItemData mid_hdr;
                app.fill_surah_header(mid_hdr, app.db->get_surah(2), false);
                SELFTEST_CHECK(mid_hdr.sub_text.empty());
            }
            {
                const std::wstring saved_scope = app.settings.browse_scope;
                app.settings.browse_scope = L"surah";
                std::vector<std::pair<int, std::wstring>> items; int tgt = 0;
                app.populate_units_and_index_items(items, tgt);
                SELFTEST_CHECK(items.size() == 114 && items[1].second.find(L"سورة البقرة — ٢٨٦ آية (") != std::wstring::npos);
                SELFTEST_CHECK(items[0].second.find(L"— ٧ آيات (") != std::wstring::npos);
                SELFTEST_CHECK(items[1].second.find(L"من الآية") == std::wstring::npos);
                app.settings.browse_scope = saved_scope;
            }
            // البحث عن التحديثات: مقارنة الإصدارات وقراءة رد GitHub
            SELFTEST_CHECK(compare_versions(L"v1.0.1", L"1.0.0") > 0);
            SELFTEST_CHECK(compare_versions(L"1.0", L"1.0.0") == 0);
            SELFTEST_CHECK(compare_versions(L"v1.2.0", L"1.10.0") < 0);
            SELFTEST_CHECK(compare_versions(L"2.0.0-beta", L"1.9.9") > 0);
            SELFTEST_CHECK(strip_version_prefix(L"v1.1.0") == L"1.1.0");
            {
                const std::string j = "{\"url\":\"x\",\"html_url\": \"https://github.com/omnya-tech/AlMushafAlSharif/releases/tag/v1.1.0\","
                                      "\"tag_name\" : \"v1.1.0\",\"name\":\"\\u0627\\u0644\\u0645\"}";
                std::wstring tag, url, name;
                SELFTEST_CHECK(json_string_value(j, "tag_name", tag) && tag == L"v1.1.0");
                SELFTEST_CHECK(json_string_value(j, "html_url", url) && url.find(L"/releases/tag/v1.1.0") != std::wstring::npos);
                SELFTEST_CHECK(json_string_value(j, "name", name) && name == L"\u0627\u0644\u0645");
                SELFTEST_CHECK(!json_string_value(j, "missing", name));
            }
            // ترويسات دار المعارف بنص الشمرلي
            SELFTEST_CHECK(build_surah_heading(*app.db, 28) ==
                L"مكية إلا من آية ٥٢ إلى آية ٥٥ فمدنية وآية ٨٥ فنزلت بالجحفة أثناء الهجرة وآياتها ٨٨ نزلت بعد النمل");
            SELFTEST_CHECK(build_surah_heading(*app.db, 18) ==
                L"مكية إلا الآية ٢٨ ومن آية ٨٣ إلى آية ١٠١ فمدنية وآياتها ١١٠ نزلت بعد الغاشية");
            SELFTEST_CHECK(build_surah_heading(*app.db, 14) ==
                L"مكية إلا آيتي ٢٨، ٢٩ فمدنيتان وآياتها ٥٢ نزلت بعد نوح");
            SELFTEST_CHECK(build_surah_heading(*app.db, 3) == L"مدنية وآياتها ٢٠٠ نزلت بعد الأنفال");
            SELFTEST_CHECK(build_surah_heading(*app.db, 96) == L"مكية وآياتها ١٩ وهي أول ما نزل من القرآن");
            SELFTEST_CHECK(build_surah_heading(*app.db, 9).find(L"فمكيتان") != std::wstring::npos);

            // بيان كل سورة مشكول كله في الطبعات الثلاث: لا كلمة عربية بلا حركة
            {
                auto unvocalized = [](const std::wstring& t) {
                    std::wstring bad, word;
                    auto flush = [&]() {
                        bool letters = false, marks = false;
                        for (wchar_t ch : word) {
                            if ((ch >= 0x0621 && ch <= 0x064A) || ch == 0x0671) letters = true;
                            if ((ch >= 0x064B && ch <= 0x0655) || ch == 0x0670 || ch == 0x06E1) marks = true;
                        }
                        // «طه» اسم سورة يُكتب في المصحف بلا حركات، كسائر أسماء الحروف المقطعة
                        if (letters && !marks && word != L"طه") bad += word + L" ";
                        word.clear();
                    };
                    for (wchar_t ch : t) {
                        if (ch == L' ' || ch == L'\u060C' || ch == L'(' || ch == L')' || ch == L':' || ch == L'\u061B' || ch == L'-') flush();
                        else word.push_back(ch);
                    }
                    flush();
                    return bad;
                };
                int bad_count = 0;
                for (const wchar_t* ed : {L"dar_almaarif", L"madinah", L"madinah_1405"}) {
                    app.db->set_edition(ed);
                    for (int n = 1; n <= 114; ++n) {
                        const std::wstring shown = app.db->edition_has_details()
                            ? build_surah_heading(*app.db, n, false, true)
                            : vocalize_marks_text(build_surah_brief(app.db->get_surah(n)));
                        const std::wstring bad = unvocalized(shown);
                        if (!bad.empty()) {
                            ++bad_count;
                            std::printf("unvocalized heading: %ls surah %d: %s\n", ed, n, to_utf8(bad).c_str());
                        }
                    }
                }
                // ونافذة بيانات السورة كذلك، إلا سطر تفصيل الحروف فهو حروف مفردة
                MushafAppWindow* saved_app = g_app;
                g_app = &app;
                for (const wchar_t* ed : {L"dar_almaarif", L"madinah", L"madinah_1405"}) {
                    app.db->set_edition(ed);
                    for (int n = 1; n <= 114; ++n) {
                        for (const auto& ln : build_surah_info_lines(n, true)) {
                            if (ln.rfind(L"تَفْصِيلُ", 0) == 0 || ln.rfind(L"۞ ", 0) == 0) continue;
                            const std::wstring bad = unvocalized(ln);
                            if (!bad.empty()) {
                                ++bad_count;
                                std::printf("unvocalized info: %ls surah %d: %s\n", ed, n, to_utf8(bad).c_str());
                            }
                        }
                    }
                }
                g_app = saved_app;
                app.db->set_edition(L"dar_almaarif");
                SELFTEST_CHECK(bad_count == 0);
            }

            // طبعة المدينة القديمة: ترتيبها الأول، ومطابقة للمصحف بلا تفاصيل المذاهب
            SELFTEST_CHECK(app.db->editions().size() >= 3 && app.db->editions()[1].key == L"madinah_1405");
            SELFTEST_CHECK(app.db->set_edition(L"madinah_1405") == L"madinah_1405");
            SELFTEST_CHECK(app.db->page_count() == 604 && !app.db->edition_has_details());
            AyahInfo m1;
            SELFTEST_CHECK(app.db->get_ayah(5, 77, m1) && m1.page_number == 121);
            SELFTEST_CHECK(app.db->get_ayah(96, 19, m1) && m1.page_number == 597);
            SELFTEST_CHECK(app.db->get_ayah(100, 9, m1) && m1.page_number == 599);
            SELFTEST_CHECK(app.format_visual_ayah(f37).find(L"۩") == std::wstring::npos);
            // المدينة القديمة وحدها تكتب «سَجْدَةُ تِلَاوَةٍ» وتنطقها
            SELFTEST_CHECK(app.margin_sajdah_lines(f38).size() == 1 && app.margin_sajdah_lines(f38)[0] == L"سَجْدَةُ تِلَاوَةٍ");
            SELFTEST_CHECK(app.format_spoken_ayah(f38).find(L"سَجْدَةُ تِلَاوَةٍ") != std::wstring::npos);

            // طبعة المدينة الحديثة
            SELFTEST_CHECK(app.db->set_edition(L"madinah") == L"madinah");
            SELFTEST_CHECK(app.db->get_ayah(5, 77, m1) && m1.page_number == 120);
            SELFTEST_CHECK(app.db->get_ayah(96, 19, m1) && m1.page_number == 598);
            SELFTEST_CHECK(app.db->get_ayah(100, 9, m1) && m1.page_number == 600);
            SELFTEST_CHECK(app.format_visual_ayah(f37).find(L"۩") == std::wstring::npos);
            SELFTEST_CHECK(app.format_visual_ayah(f38).find(L"۩") != std::wstring::npos);
            SELFTEST_CHECK(app.format_spoken_ayah(f37).find(L"مَالِكٍ") == std::wstring::npos);
            SELFTEST_CHECK(app.format_spoken_ayah(f38).find(L"سَجْدَة") != std::wstring::npos);
            SELFTEST_CHECK(app.format_spoken_ayah(f38).find(L"تِلَاوَةٍ") == std::wstring::npos);
            SELFTEST_CHECK(app.margin_sajdah_lines(f37).empty());
            SELFTEST_CHECK(app.margin_sajdah_lines(f38).size() == 1 && app.margin_sajdah_lines(f38)[0] == L"سَجْدَة");
            // الربع الذي يبدأ بأول السورة: علامته في الهامش بلا ۞ في سطر الآية
            AyahInfo nisa1, baq26;
            SELFTEST_CHECK(app.db->get_ayah(4, 1, nisa1) && app.db->get_ayah(2, 26, baq26));
            SELFTEST_CHECK(app.format_visual_ayah(nisa1).find(L"۞") != 0);
            SELFTEST_CHECK(!app.margin_division_lines(nisa1).empty());
            SELFTEST_CHECK(app.format_visual_ayah(baq26).find(L"۞") == 0);
            SELFTEST_CHECK(app.format_spoken_ayah(baq26).find(L"رُبْعُ الحِزْبِ 1") != std::wstring::npos);
        } else {
            SELFTEST_CHECK(false && "dar_almaarif edition did not load");
        }

        // أخطاء يجب أن تُكتشف: صفحة مكررة، وآية غير موجودة، وسورة غير معروفة، وترتيب معكوس
        EditionCheck bad1 = app.db->check_edition_text("1,1,1\n2,2,1\n2,2,6\n");
        SELFTEST_CHECK(!bad1.ok());
        EditionCheck bad2 = app.db->check_edition_text("1,1,1\n2,2,300\n");
        SELFTEST_CHECK(!bad2.ok());
        EditionCheck bad3 = app.db->check_edition_text(to_utf8(L"1,1,1\n2,سورة ليست موجودة,1\n"));
        SELFTEST_CHECK(!bad3.ok());
        EditionCheck bad4 = app.db->check_edition_text("1,1,1\n2,2,10\n3,2,5\n");
        SELFTEST_CHECK(!bad4.ok());
        // الأرقام العربية والفاصلة العربية مقبولة
        EditionCheck ar = app.db->check_edition_text(to_utf8(L"١،الفاتحة،١\n٢،البقرة،١\n"));
        SELFTEST_CHECK(ar.page_starts.size() == 2 && ar.errors.size() == 1);   // سليم لكنه ناقص (صفحتان فقط)
    }

    // دليل الاستخدام موجود مع البرنامج، وفيه كل اختصار أصلي حتى لا يتأخر الدليل عن البرنامج
    {
        const std::wstring guide = app.user_guide_path();
        std::string raw;
        if (FILE* f = _wfopen(guide.c_str(), L"rb")) {
            char buf[8192];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) raw.append(buf, n);
            std::fclose(f);
        }
        SELFTEST_CHECK(raw.size() > 10000);
        const std::wstring html = to_wide(raw);
        SELFTEST_CHECK(html.find(L"lang=\"ar\" dir=\"rtl\"") != std::wstring::npos);
        SELFTEST_CHECK(html.find(L"{{") == std::wstring::npos);   // لا عناصر نائبة لم تُملأ
        for (const auto& d : shortcut_defs()) {
            std::wstring spec = d.default_spec;
            replace_all(spec, L"Left", L"\u2190");
            replace_all(spec, L"Right", L"\u2192");
            const bool listed = html.find(L"<kbd>" + spec + L"</kbd>") != std::wstring::npos;
            if (!listed) std::fwprintf(stderr, L"guide is missing shortcut: %ls\n", spec.c_str());
            SELFTEST_CHECK(listed);
        }
    }

    if (g_selftest_failures > 0) {
        std::fprintf(stderr, "%d SELF-TEST CHECK(S) FAILED\n", g_selftest_failures);
        return 1;
    }
    std::printf("ALL C++ SELF-TESTS PASSED (6236 ayahs, 114 surahs, Madinah 604 pages, 7 browse modes, "
                "558 rukus, long-ayah row splitting, list-end arrows, user guide, divisions, shortcuts & screen reader)\n");
    return 0;
}

} // namespace mushaf

namespace mushaf {

// فحص ملف بدايات صفحات طبعة جديدة قبل اعتمادها: يكتب تقريرًا بجوار الملف ويعرضه
static int run_check_edition(const std::wstring& path, bool open_report) {
    MushafAppWindow app;
    app.resolve_paths();
    app.db = std::make_unique<QuranDatabase>(
        app.assets_dir + L"\\data\\quran.sqlite3",
        app.assets_dir + L"\\data\\surah_info_egyptian.json"
    );
    if (!app.db->is_open()) return 2;
    EditionCheck c = app.db->check_edition_file(path);

    std::wstring r;
    r += L"تقرير فحص ملف بدايات الصفحات\r\n";
    r += L"الملف: " + path + L"\r\n";
    r += L"الطبعة: " + c.info.name_ar + L" (" + c.info.key + L")\r\n";
    r += L"عدد الصفحات المقروءة: " + std::to_wstring(c.page_starts.size()) + L"\r\n\r\n";
    if (c.ok()) {
        r += L"النتيجة: الملف سليم ويمكن اعتماده. ضعه في مجلد assets\\data\\editions ثم شغّل البرنامج.\r\n";
    } else {
        r += L"النتيجة: في الملف " + std::to_wstring(c.errors.size()) + L" خطأ يجب تصحيحه قبل اعتماده:\r\n";
        for (const auto& e : c.errors) r += L"  - " + e + L"\r\n";
    }
    if (!c.warnings.empty()) {
        r += L"\r\nملاحظات تستحق المراجعة على المصحف الورقي (" + std::to_wstring(c.warnings.size()) + L"):\r\n";
        for (const auto& w : c.warnings) r += L"  - " + w + L"\r\n";
    }
    std::wstring report_path = path + L".تقرير.txt";
    write_file_utf8(report_path, std::string("\xEF\xBB\xBF") + to_utf8(r));
    // يفتح التقرير في المفكرة حتى يقرأه المستخدم مباشرة (ويقرؤه قارئ الشاشة)
    if (open_report) {
        ShellExecuteW(nullptr, L"open", L"notepad.exe", (L"\"" + report_path + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    }
    return c.ok() ? 0 : 1;
}

} // namespace mushaf

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    if (lpCmdLine && std::wcsstr(lpCmdLine, L"--check-edition") != nullptr) {
        // --quiet: يكتب التقرير دون فتحه (للاختبار الآلي)
        bool quiet = std::wcsstr(lpCmdLine, L"--quiet") != nullptr;
        std::wstring arg = std::wcsstr(lpCmdLine, L"--check-edition") + 15;
        size_t qpos = arg.find(L"--quiet");
        if (qpos != std::wstring::npos) arg.erase(qpos, 7);
        while (!arg.empty() && (arg.front() == L' ' || arg.front() == L'"')) arg.erase(arg.begin());
        while (!arg.empty() && (arg.back() == L' ' || arg.back() == L'"')) arg.pop_back();
        return mushaf::run_check_edition(arg, !quiet);
    }
    if (lpCmdLine && std::wcsstr(lpCmdLine, L"--sync-shortcuts") != nullptr) {
        return mushaf::run_sync_shortcuts(std::wcsstr(lpCmdLine, L"--sync-shortcuts en") != nullptr);
    }
    if (lpCmdLine && std::wcsstr(lpCmdLine, L"--self-test") != nullptr) {
        // إن كان الخرج موجّهًا إلى ملف أو أنبوب نكتب فيه مباشرة، وإلا نلتحق بنافذة الأوامر الأم
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        bool redirected = out && out != INVALID_HANDLE_VALUE && GetFileType(out) != FILE_TYPE_CHAR;
        if (!redirected && AttachConsole(ATTACH_PARENT_PROCESS)) {
            std::freopen("CONOUT$", "w", stdout);
            std::freopen("CONOUT$", "w", stderr);
        }
        int rc = mushaf::run_self_test();
        std::fflush(stdout);
        std::fflush(stderr);
        return rc;
    }

    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES};
    InitCommonControlsEx(&icc);

    mushaf::MushafAppWindow app;
    mushaf::g_app = &app;
    app.resolve_paths();
    app.settings.load();
    mushaf::copy_waqf_marks_enabled() = app.settings.copy_waqf_marks;
    {
        std::wstring dir = mushaf::AppSettings::settings_file_path();
        const size_t cut = dir.find_last_of(L"\\/");
        dir = (cut == std::wstring::npos) ? std::wstring() : dir.substr(0, cut + 1);
        mushaf::MushafAppWindow::width_store().path = dir + L"word_widths.bin";
    }
    app.load_private_fonts();
    mushaf::ScreenReaderEngine::instance().init(app.app_dir);

    app.db = std::make_unique<mushaf::QuranDatabase>(
        app.assets_dir + L"\\data\\quran.sqlite3",
        app.assets_dir + L"\\data\\surah_info_egyptian.json"
    );
    // الطبعات الإضافية (مثل دار المعارف) تُقرأ من ملفات بدايات الصفحات إن وُجدت
    app.db->load_editions_folder(app.assets_dir + L"\\data\\editions");
    app.settings.mushaf_edition = app.db->set_edition(app.settings.mushaf_edition);
    app.current_surah = app.settings.last_surah;
    app.current_page = app.settings.last_page;
    app.current_juz = app.settings.last_juz;
    app.current_hizb = app.settings.last_hizb;
    app.current_rub = app.settings.last_rub;
    app.current_manzil = app.settings.last_manzil;
    app.current_ruku = std::max(1, std::min(558, app.settings.last_ruku));
    app.current_ayah_num = app.settings.last_ayah;

    // الأيقونة المضمّنة في الملف التنفيذي احتياطًا إن لم يوجد ملف assets\icon.ico
    if (!app.app_icon) app.app_icon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = mushaf::MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = app.app_icon;
    wc.hIconSm = app.app_icon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;   // الخلفية والشريط العلوي يُرسمان في WM_ERASEBKGND حسب المظهر
    wc.lpszClassName = L"AlMushafAlSharifCppWnd";
    RegisterClassExW(&wc);

    DWORD ex_style = app.is_en() ? 0 : WS_EX_LAYOUTRTL;
    // استعادة موضع النافذة وحجمها السابقين إن كانا ما زالا داخل إحدى الشاشات
    // الحجم الافتراضي بمقاس الشاشة: ١٢٨٠×٨٢٠ عند ١٠٠٪، لا يتجاوز ٩٢٪ من منطقة العمل، وفي وسطها
    mushaf::ui_dpi() = mushaf::window_dpi(nullptr);
    MONITORINFO pmi{sizeof(pmi)};
    POINT origin{0, 0};
    GetMonitorInfoW(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &pmi);
    const RECT pw = pmi.rcWork;
    const int pw_w = pw.right - pw.left, pw_h = pw.bottom - pw.top;
    int win_w = std::min(mushaf::DS(1280), pw_w * 92 / 100), win_h = std::min(mushaf::DS(820), pw_h * 92 / 100);
    int win_x = pw.left + (pw_w - win_w) / 2, win_y = pw.top + (pw_h - win_h) / 2;
    if (app.settings.window_w >= 400 && app.settings.window_h >= 300) {
        RECT saved = {app.settings.window_x, app.settings.window_y,
                      app.settings.window_x + app.settings.window_w,
                      app.settings.window_y + app.settings.window_h};
        // الموضع المحفوظ يُستعاد فقط إن كان على شاشة موجودة الآن، ويُقصّ على مساحتها
        // (قد تكون الشاشة تغيّرت: شاشة أصغر، أو فُصلت شاشة ثانية)
        if (HMONITOR mon = MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
            MONITORINFO mi{sizeof(mi)};
            GetMonitorInfoW(mon, &mi);
            const RECT w = mi.rcWork;
            win_w = std::min(app.settings.window_w, (int)(w.right - w.left));
            win_h = std::min(app.settings.window_h, (int)(w.bottom - w.top));
            win_x = std::max((int)w.left, std::min(app.settings.window_x, (int)w.right - win_w));
            win_y = std::max((int)w.top, std::min(app.settings.window_y, (int)w.bottom - win_h));
        }
    }
    app.hwnd = CreateWindowExW(
        ex_style,
        wc.lpszClassName,
        app.is_en() ? L"Al-Mushaf Al-Sharif" : L"المصحف الشريف",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        win_x, win_y, win_w, win_h,
        nullptr, nullptr, hInstance, nullptr
    );
    if (app.app_icon) {
        SendMessageW(app.hwnd, WM_SETICON, ICON_BIG, (LPARAM)app.app_icon);
        SendMessageW(app.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)app.app_icon);
    }

    mushaf::ui_dpi() = mushaf::window_dpi(app.hwnd);   // دقة الشاشة التي فُتحت عليها النافذة فعلًا
    app.recreate_fonts_and_brushes();
    app.build_menus();

    // 1) الشريط العلوي: «تصفح حسب» فقط، وفي طرفه الآخر سطر يبيّن موضع القراءة الحالي
    app.lbl_edition = CreateWindowExW(0, L"STATIC", app.tr(L"ال&طبعة:", L"&Print:").c_str(), WS_CHILD | SS_CENTERIMAGE, 16, 13, 62, 26, app.hwnd, nullptr, hInstance, nullptr);
    app.combo_edition = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST, 82, 10, 220, 320, app.hwnd, (HMENU)mushaf::ID_COMBO_EDITION, hInstance, nullptr);
    app.lbl_browse = CreateWindowExW(0, L"STATIC", app.tr(L"&تصفح حسب:", L"&Browse by:").c_str(), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 16, 13, 100, 26, app.hwnd, nullptr, hInstance, nullptr);
    app.combo_browse = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 120, 10, 340, 320, app.hwnd, (HMENU)mushaf::ID_COMBO_BROWSE, hInstance, nullptr);
    app.btn_search = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        474, 10, 210, 31, app.hwnd, (HMENU)(INT_PTR)mushaf::IDM_SEARCH, hInstance, nullptr);
    app.lbl_position = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE | SS_NOPREFIX | SS_ENDELLIPSIS,
        476, 12, 780, 28, app.hwnd, nullptr, hInstance, nullptr);

    // 2) الفهرس الجانبي (مربع التصفية + تبويبا «الفهرس» و«المفضلة»)
    app.edit_sidebar_filter = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        12, 54, 330, 30, app.hwnd, (HMENU)mushaf::ID_EDIT_SIDEBAR_FILTER, hInstance, nullptr
    );
    app.tab_sidebar = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        12, 90, 330, 700, app.hwnd, (HMENU)mushaf::ID_TAB_SIDEBAR, hInstance, nullptr
    );
    TCITEMW tie{};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<wchar_t*>(L"الفهرس");
    SendMessageW(app.tab_sidebar, TCM_INSERTITEMW, 0, (LPARAM)&tie);
    tie.pszText = const_cast<wchar_t*>(L"المفضلة");
    SendMessageW(app.tab_sidebar, TCM_INSERTITEMW, 1, (LPARAM)&tie);

    app.list_sidebar_index = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        18, 122, 318, 660, app.hwnd, (HMENU)mushaf::ID_LIST_SIDEBAR_INDEX, hInstance, nullptr
    );
    app.list_sidebar_bookmarks = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        18, 122, 318, 620, app.hwnd, (HMENU)mushaf::ID_LIST_SIDEBAR_BOOKMARKS, hInstance, nullptr
    );
    app.btn_sb_copy_bm = CreateWindowExW(
        0, L"BUTTON", app.tr(L"نسخ الآية", L"Copy ayah").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        18, 748, 155, 32, app.hwnd, (HMENU)mushaf::ID_BTN_SB_COPY_BM, hInstance, nullptr
    );
    app.btn_sb_del_bm = CreateWindowExW(
        0, L"BUTTON", app.tr(L"حذف", L"Remove").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        181, 748, 155, 32, app.hwnd, (HMENU)mushaf::ID_BTN_SB_DEL_BM, hInstance, nullptr
    );

    // 3) منطقة المصحف (مكدس قائمة الوحدات وقائمة الآيات)
    // بلا حافة ثلاثية الأبعاد: حدود الصفحة يرسمها الإطار المذهّب، والحافة الفاتحة كانت تزاحمه في المظهر الليلي
    app.list_units = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        354, 54, 914, 720, app.hwnd, (HMENU)mushaf::ID_LIST_UNITS, hInstance, nullptr
    );
    app.list_ayahs = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        354, 54, 914, 720, app.hwnd, (HMENU)mushaf::ID_LIST_AYAHS, hInstance, nullptr
    );

    for (HWND w : {app.lbl_edition, app.combo_edition, app.lbl_browse, app.combo_browse, app.btn_search, app.lbl_position,
                   app.edit_sidebar_filter, app.tab_sidebar, app.list_sidebar_index,
                   app.list_sidebar_bookmarks, app.btn_sb_copy_bm, app.btn_sb_del_bm}) {
        SendMessageW(w, WM_SETFONT, (WPARAM)app.font_ui, TRUE);
    }
    SendMessageW(app.list_units, WM_SETFONT, (WPARAM)app.font_units, TRUE);
    SendMessageW(app.list_ayahs, WM_SETFONT, (WPARAM)app.font_quran, TRUE);
    mushaf::send_tab_to_back(app.tab_sidebar);   // حتى لا يغطي جسم التبويب قائمتي الفهرس والمفضلة

    mushaf::g_orig_ayah_list_proc = (WNDPROC)SetWindowLongPtrW(app.list_ayahs, GWLP_WNDPROC, (LONG_PTR)mushaf::AyahListSubclassProc);
    mushaf::g_orig_unit_list_proc = (WNDPROC)SetWindowLongPtrW(app.list_units, GWLP_WNDPROC, (LONG_PTR)mushaf::UnitListSubclassProc);
    mushaf::g_orig_sb_index_proc  = (WNDPROC)SetWindowLongPtrW(app.list_sidebar_index, GWLP_WNDPROC, (LONG_PTR)mushaf::SidebarIndexSubclassProc);
    mushaf::g_orig_sb_bm_proc     = (WNDPROC)SetWindowLongPtrW(app.list_sidebar_bookmarks, GWLP_WNDPROC, (LONG_PTR)mushaf::SidebarBookmarksSubclassProc);

    app.populate_toolbar_combos();
    app.update_sidebar_visibility();   // ترتيب العناصر أولًا حتى تُقسَّم الآيات على عرضها الحقيقي
    app.load_current_browse_unit(app.current_ayah_num, app.current_surah);
    app.push_history(app.current_surah, app.current_ayah_num);
    app.rebuild_accelerators();

    ShowWindow(app.hwnd, app.settings.window_maximized ? SW_SHOWMAXIMIZED : nCmdShow);
    UpdateWindow(app.hwnd);
    if (app.settings.startup_focus == L"units") {
        app.show_units_view(true);
    } else if (app.settings.startup_focus == L"ayahs") {
        SetFocus(app.showing_units ? app.list_units : app.list_ayahs);
    } else if (app.sidebar_visible() && IsWindowVisible(app.list_sidebar_index)) {
        SetFocus(app.list_sidebar_index);
    } else {
        SetFocus(app.showing_units ? app.list_units : app.list_ayahs);
    }

    app.offer_user_guide_once();
    app.maybe_auto_check_updates();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (app.haccel && TranslateAcceleratorW(app.hwnd, app.haccel, &msg)) {
            continue;
        }
        if (mushaf::handle_enter_on_focused_button(app.hwnd, msg)) {
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            HWND foc = GetFocus();
            if (foc) {
                if ((foc == app.combo_browse || foc == app.combo_edition) &&
                    SendMessageW(foc, CB_GETDROPPEDSTATE, 0, 0) == 0) {
                    SetFocus(app.showing_units ? app.list_units : app.list_ayahs);
                    continue;
                }
                if (foc == app.edit_sidebar_filter) {
                    if (SendMessageW(app.list_sidebar_index, LB_GETCOUNT, 0, 0) > 0) {
                        SendMessageW(app.list_sidebar_index, LB_SETCURSEL, 0, 0);
                    }
                    SetFocus(app.list_sidebar_index);
                    continue;
                }
                if (foc == app.tab_sidebar) {
                    int tab = (int)SendMessageW(app.tab_sidebar, TCM_GETCURSEL, 0, 0);
                    SetFocus(tab == 1 ? app.list_sidebar_bookmarks : app.list_sidebar_index);
                    continue;
                }
            }
        }
        bool is_return_or_esc = (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE));
        if (!is_return_or_esc && IsDialogMessageW(app.hwnd, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (app.haccel) DestroyAcceleratorTable(app.haccel);
    return (int)msg.wParam;
}
