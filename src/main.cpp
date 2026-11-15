// -*- coding: utf-8 -*-
// main.cpp — النسخة الأصلية بلغة C++ (Win32 API) من برنامج «المصحف الشريف»
// مطابقة تماماً في الواجهة والتجربة لنسخة بايثون، ومتوافقة من ويندوز 7 (32-bit / 64-bit) حتى ويندوز 11.

#include "a11y_and_config.hpp"
#include "database.hpp"
#include "sajdah_and_formatter.hpp"
#include "shortcuts.hpp"
#include "dialogs.hpp"
#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

namespace mushaf {

// أبعاد صفوف قائمة الآيات. صناديق القوائم في ويندوز لا تقبل صفاً أطول من ٢٥٥ بكسل،
// فالآية الطويلة تُقسَّم على أكثر من صف متصل حتى لا ينقطع شيء من نصها أبداً.
constexpr int kRowMaxHeight   = 250;
constexpr int kRowPadX        = 22;
constexpr int kRowPadY        = 10;
constexpr int kAccentWidth    = 5;
constexpr int kInlineBtnWidth = 72;
constexpr int kHeaderInsetX   = 28;
constexpr int kHeaderInsetY   = 8;
constexpr int kToolbarHeight  = 52;
constexpr UINT_PTR kRelayoutTimer = 1;
// سهما التقليب للمبصر على جانبي صفحة المصحف: يُرسمان على خلفية النافذة دون أي عنصر تحكم،
// فلا يراهما قارئ الشاشة ولا يدخلان في التنقل بلوحة المفاتيح.
constexpr int kArrowGutter = 56;
constexpr int kArrowDiameter = 42;

enum MenuIds : UINT {
    ID_COMBO_BROWSE           = 102,
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
    IDM_CYCLE_THEME      = 1234,
    IDM_TOGGLE_MUQATTAAT = 1240,
    IDM_TOGGLE_SOUND     = 1241,

    // 4) قائمة المساعدة
    IDM_SHORTCUTS_HELP   = 1301,
    IDM_ABOUT            = 1302,
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
    std::vector<std::wstring> lines;  // سطور الآية بعد اللفّ، تُرسم كما قيست تماماً
    int line_h = 0;
};

// إجراء قائمة الآيات الأصلي (قبل الاستبدال) — يُستخدم لإكمال حركة الأسهم فوق الصفوف التكميلية
static WNDPROC g_orig_ayah_list_proc = nullptr;

class MushafAppWindow {
public:
    HWND hwnd = nullptr;
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
    HBRUSH brush_paper = nullptr;
    HBRUSH brush_toolbar = nullptr;
    HBRUSH brush_window = nullptr;
    int ayah_rows_width = 0;       // عرض القائمة الذي قُسّمت عليه صفوف الآيات آخر مرة
    RECT arrow_rect_next{};        // سهم «التالي» (على اليسار المرئي كتقليب المصحف)
    RECT arrow_rect_prev{};        // سهم «السابق» (على اليمين المرئي)
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
        if (brush_paper) DeleteObject(brush_paper);
        if (brush_toolbar) DeleteObject(brush_toolbar);
        if (brush_window) DeleteObject(brush_window);

        HDC hdc = GetDC(hwnd);
        int dpi_y = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
        if (hdc) ReleaseDC(hwnd, hdc);

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
        font_quran = CreateFontW(
            pt_to_px(settings.quran_font_size), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, settings.quran_font_family.c_str()
        );
        font_header = CreateFontW(
            pt_to_px(std::max(16, (int)(settings.quran_font_size * 0.8))), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Amiri"
        );
        font_details = CreateFontW(
            pt_to_px(std::max(13, (int)(settings.quran_font_size * 0.55))), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Amiri"
        );

        ThemePalette pal = get_theme_palette(settings.theme);
        brush_paper = CreateSolidBrush(pal.paper_bg);
        brush_toolbar = CreateSolidBrush(pal.toolbar_bg);
        brush_window = CreateSolidBrush(pal.window_bg);

        // الخطوط القديمة حُذفت للتو، فلا بد من ربط الجديدة بكل عنصر وإلا رجع لخط النظام
        for (HWND w : {lbl_browse, combo_browse, btn_search, lbl_position, edit_sidebar_filter, tab_sidebar,
                       list_sidebar_index, list_sidebar_bookmarks, btn_sb_copy_bm, btn_sb_del_bm}) {
            if (w) SendMessageW(w, WM_SETFONT, (WPARAM)font_ui, TRUE);
        }
        if (list_ayahs) SendMessageW(list_ayahs, WM_SETFONT, (WPARAM)font_quran, FALSE);
        if (list_units) SendMessageW(list_units, WM_SETFONT, (WPARAM)font_units, TRUE);
    }

    std::wstring edition_short_label() const {
        return tr(L"طبعة المدينة", L"Madinah print");
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
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_PAGE,   lbl(L"التصفح بالصفحات (٦٠٤ صفحات)", L"Browse by page (604)", IDM_BROWSE_PAGE).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_JUZ,    lbl(L"التصفح بالأجزاء (٣٠ جزءاً)", L"Browse by juz (30)", IDM_BROWSE_JUZ).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_RUB,    lbl(L"التصفح بالأرباع (٢٤٠ ربعاً)", L"Browse by quarter (240)", IDM_BROWSE_RUB).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_HIZB,   lbl(L"التصفح بالأحزاب (٦٠ حزباً)", L"Browse by hizb (60)", IDM_BROWSE_HIZB).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_MANZIL, lbl(L"التصفح بالمنازل السبعة (ختمة الأسبوع — فمي بشوق)", L"Browse by the Seven Manzils (weekly reading)", IDM_BROWSE_MANZIL).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BROWSE_RUKU,   lbl(L"التصفح بالركوعات (٥٥٨ ركوعاً)", L"Browse by ruku (558)", IDM_BROWSE_RUKU).c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_GOTO,            lbl(L"انتقال سريع إلى سورة / آية / صفحة / جزء / ربع...", L"Go to surah / ayah / page / juz / quarter...", IDM_GOTO).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_SEARCH,          lbl(L"البحث الشامل في آيات القرآن الكريم...", L"Search the Quran...", IDM_SEARCH).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_SAJDAH_LIST,     lbl(L"مواضع سجدات التلاوة (١٥ موضعاً)...", L"Sajdah places in the Quran (15)...", IDM_SAJDAH_LIST).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_HISTORY_BACK,    lbl(L"الرجوع للموضع السابق في السجل", L"Back to previous reading position", IDM_HISTORY_BACK).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_HISTORY_FORWARD, lbl(L"التقدم للموضع التالي في السجل", L"Forward to next reading position", IDM_HISTORY_FORWARD).c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_JUMP_KHATMAH,    lbl(L"الانتقال إلى فاصل الختمة (متابعة القراءة)", L"Go to reading mark (resume reading)", IDM_JUMP_KHATMAH).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_BOOKMARKS_DLG,   lbl(L"العلامات المرجعية وفاصل الختمة...", L"Bookmarks and reading mark...", IDM_BOOKMARKS_DLG).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_COPY_ALL,        lbl(L"نسخ كل الآيات المعروضة (الوحدة الحالية)", L"Copy all displayed ayahs (current unit)", IDM_COPY_ALL).c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_FOCUS_AYAHS,     lbl(L"التركيز على قائمة الآيات", L"Focus the ayah list", IDM_FOCUS_AYAHS).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_SHOW_UNITS,      lbl(L"فهرس السور / الصفحات / الأجزاء / الأرباع", L"Surah / page / juz / quarter list", IDM_SHOW_UNITS).c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_FOCUS_INDEX,     tr(L"التركيز على قائمة الفهرس الجانبي", L"Focus the side index").c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_CYCLE_LANDMARKS, lbl(L"التنقل الدائري بين مناطق النافذة", L"Cycle between window areas", IDM_CYCLE_LANDMARKS).c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_NEXT_UNIT, tr(L"الوحدة التالية (سورة / صفحة / جزء / ربع)\tPageDown", L"Next (surah / page / juz / quarter)\tPageDown").c_str());
        AppendMenuW(m_nav, MF_STRING, IDM_PREV_UNIT, tr(L"الوحدة السابقة (سورة / صفحة / جزء / ربع)\tPageUp", L"Previous (surah / page / juz / quarter)\tPageUp").c_str());
        AppendMenuW(m_nav, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m_nav, MF_STRING, IDM_EXIT, tr(L"خروج من البرنامج\tAlt+F4", L"Exit\tAlt+F4").c_str());
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
        AppendMenuW(m_opts, MF_POPUP, (UINT_PTR)m_align, tr(L"محاذاة السطور", L"Line alignment").c_str());

        HMENU m_lang = CreatePopupMenu();
        AppendMenuW(m_lang, MF_STRING | (!is_en() ? MF_CHECKED : 0), IDM_LANG_AR, tr(L"العربية", L"Arabic").c_str());
        AppendMenuW(m_lang, MF_STRING | (is_en() ? MF_CHECKED : 0),  IDM_LANG_EN, tr(L"الإنجليزية", L"English").c_str());
        AppendMenuW(m_opts, MF_POPUP, (UINT_PTR)m_lang, tr(L"لغة الواجهة", L"Interface language").c_str());
        AppendMenuW(m_opts, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(m_opts, MF_STRING, IDM_FONT_INC,       lbl(L"تكبير خط الآيات", L"Larger Quran font", IDM_FONT_INC).c_str());
        AppendMenuW(m_opts, MF_STRING, IDM_FONT_DEC,       lbl(L"تصغير خط الآيات", L"Smaller Quran font", IDM_FONT_DEC).c_str());
        AppendMenuW(m_opts, MF_STRING, IDM_FONT_RESET,     lbl(L"الحجم الافتراضي لخط الآيات", L"Default Quran font size", IDM_FONT_RESET).c_str());
        AppendMenuW(m_opts, MF_STRING | (settings.show_sidebar ? MF_CHECKED : 0),
                    IDM_TOGGLE_SIDEBAR, lbl(L"إظهار / إخفاء الفهرس الجانبي", L"Show / hide the index", IDM_TOGGLE_SIDEBAR).c_str());
        AppendMenuW(m_opts, MF_STRING | (focus_mode ? MF_CHECKED : 0),
                    IDM_FOCUS_MODE, lbl(L"وضع التركيز: المصحف بملء الشاشة", L"Focus mode: full-screen Mushaf", IDM_FOCUS_MODE).c_str());

        HMENU m_disp = CreatePopupMenu();
        AppendMenuW(m_disp, MF_STRING | (settings.ayah_display == L"text" ? MF_CHECKED : 0),             IDM_DISPLAY_TEXT,  tr(L"نص الآية فقط", L"Ayah text only").c_str());
        AppendMenuW(m_disp, MF_STRING | (settings.ayah_display == L"text_number" ? MF_CHECKED : 0),      IDM_DISPLAY_NUM,   tr(L"نص الآية مع رقمها", L"Ayah text with its number").c_str());
        AppendMenuW(m_disp, MF_STRING | (settings.ayah_display == L"text_number_surah" ? MF_CHECKED : 0),IDM_DISPLAY_SURAH, tr(L"نص الآية مع رقمها واسم السورة", L"Ayah text with its number and surah name").c_str());
        AppendMenuW(m_opts, MF_POPUP, (UINT_PTR)m_disp, tr(L"طريقة عرض الآيات", L"Ayah display").c_str());

        AppendMenuW(m_opts, MF_STRING, IDM_CYCLE_THEME, lbl(L"تبديل المظهر اللوني", L"Switch colour theme", IDM_CYCLE_THEME).c_str());
        AppendMenuW(mb, MF_POPUP, (UINT_PTR)m_opts, tr(L"ال&خيارات والعرض", L"&Options").c_str());

        // 3) المساعدة
        HMENU m_help = CreatePopupMenu();
        AppendMenuW(m_help, MF_STRING, IDM_SHORTCUTS_HELP, lbl(L"دليل اختصارات لوحة المفاتيح وقارئات الشاشة...", L"Keyboard and screen reader guide...", IDM_SHORTCUTS_HELP).c_str());
        AppendMenuW(m_help, MF_STRING, IDM_ABOUT,          tr(L"حول برنامج المصحف الشريف...", L"About Al-Mushaf Al-Sharif...").c_str());
        AppendMenuW(mb, MF_POPUP, (UINT_PTR)m_help,        tr(L"الم&ساعدة", L"&Help").c_str());

        HMENU old_mb = GetMenu(hwnd);
        SetMenu(hwnd, mb);
        if (old_mb) DestroyMenu(old_mb);
    }

    void populate_toolbar_combos() {
        updating_combos = true;
        SetWindowTextW(lbl_browse, tr(L"&تصفح حسب:", L"&Browse by:").c_str());
        if (btn_search) {
            std::wstring s_sc = sc(IDM_SEARCH);
            std::wstring s_lbl = tr(L"بحث في القرآن...", L"Search the Quran...");
            if (!s_sc.empty()) s_lbl += L" (" + s_sc + L")";
            SetWindowTextW(btn_search, s_lbl.c_str());
        }
        SetWindowTextW(btn_sb_copy_bm, tr(L"نسخ المحددة", L"Copy selected").c_str());
        SetWindowTextW(btn_sb_del_bm, tr(L"حذف", L"Remove").c_str());
        SendMessageW(
            edit_sidebar_filter, EM_SETCUEBANNER, TRUE,
            (LPARAM)tr(L"تصفية القائمة بالاسم أو الرقم...", L"Filter by name or number...").c_str()
        );

        SendMessageW(combo_browse, CB_RESETCONTENT, 0, 0);
        std::wstring pages_lbl = tr(
            L"الصفحات (" + num_str(db->page_count()) + L" صفحات)",
            L"Pages (" + std::to_wstring(db->page_count()) + L")"
        );
        const std::pair<std::wstring, std::wstring> modes[7] = {
            {L"surah",  tr(L"السور (١١٤ سورة)", L"Surahs (114)")},
            {L"page",   pages_lbl},
            {L"juz",    tr(L"الأجزاء (٣٠ جزءاً)", L"Juz (30)")},
            {L"hizb",   tr(L"الأحزاب (٦٠ حزباً)", L"Hizb (60)")},
            {L"rub",    tr(L"الأرباع (٢٤٠ ربعاً / ٦٠ حزباً)", L"Quarters (240 / 60 hizb)")},
            {L"manzil", tr(L"المنازل السبعة (ختمة الأسبوع — فمي بشوق)", L"Seven Manzils (weekly reading)")},
            {L"ruku",   tr(L"الركوعات (٥٥٨ ركوعاً)", L"Rukus (558)")},
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
                    ? (std::to_wstring(s.number) + L". Surah " + s_name + L" — from ayah 1 to ayah " + std::to_wstring(s.verses_count) + L" (" + place_txt + L" • " + pages + L")")
                    : (num_str(s.number) + L" - سورة " + s_name + L" — من الآية ١ إلى الآية " + num_str(s.verses_count) + L" (" + place_txt + L" • " + pages + L")");
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
        std::wstring key = settings.browse_scope + L"|" + settings.ui_language;
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
        if (is_rub && text.find(L"۞") != 0) {
            text = L"۞ " + text;
        }

        const SajdahPlace* sp = find_sajdah_place(a.verse_key);
        if (sp) {
            if (text.find(L"۩") == std::wstring::npos) text += L" ۩";
            if (!sp->is_agreed() || a.verse_key == L"41:37" || a.verse_key == L"41:38") {
                text += L"  [" + sajdah_short_label(a.verse_key, is_en()) + L"]";
            }
        }
        if (is_rub) {
            std::wstring badge = division_badge_text(a, is_en());
            if (!badge.empty()) text += L"  " + badge;
        }
        if (settings.ayah_display == L"text_number_surah") {
            text += is_en() ? (L"   [Surah " + a.surah_name + L"]")
                            : (L"   [سورة " + a.surah_name + L"]");
        }
        if (!settings.khatmah_verse_key.empty() && a.verse_key == settings.khatmah_verse_key) {
            text += is_en() ? L"  [Reading bookmark]" : L"  [فاصل الختمة]";
        } else if (std::find(settings.bookmarks.begin(), settings.bookmarks.end(), a.verse_key) != settings.bookmarks.end()) {
            text += is_en() ? L"  [Bookmarked]" : L"  [في المفضلة]";
        }
        if (settings.show_ayah_letter_count) {
            int lcnt = count_arabic_letters(a.text_simple);
            text += is_en() ? (L"  [" + std::to_wstring(lcnt) + L" letters]")
                            : (L"  〔" + to_arabic_digits(lcnt) + L" حرفاً〕");
        }
        return text;
    }

    std::wstring format_spoken_ayah(const AyahInfo& a) const {
        bool is_rub = (a.rub_el_hizb_number >= 2 && db->rub_start_ids().count(a.id) > 0);
        std::wstring bm;
        if (!settings.khatmah_verse_key.empty() && a.verse_key == settings.khatmah_verse_key) {
            bm = tr(L"فاصل الختمة (موضع التوقف)", L"reading bookmark (stop position)");
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
            lcnt
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

    // العرض المتاح لنص الآية بعد الهوامش وشريط التحديد (وزر النسخ المضمّن إن كان مفعّلاً)
    int ayah_text_width(ListItemKind kind) const {
        int w = ayah_list_client_width() - 2 * kRowPadX - kAccentWidth;
        if (kind == ListItemKind::SurahHeader) w -= 2 * kHeaderInsetX + 28;
        if (kind == ListItemKind::Ayah && settings.show_inline_copy_btn) w -= kInlineBtnWidth;
        return std::max(120, w);
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

    // لفّ النص على سطور بعرض محدد، كلمةً كلمة. قياس الكلمات منفردة أسرع بعشر مرات تقريباً
    // من DrawText مع DT_CALCRECT للنص العربي، والسطور الناتجة تُرسم كما هي فلا يختلف العرض عن القياس.
    static std::vector<std::wstring> wrap_text_lines(HDC hdc, const std::wstring& text, int width) {
        SIZE space{};
        GetTextExtentPoint32W(hdc, L" ", 1, &space);
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
            GetTextExtentPoint32W(hdc, text.c_str() + i, (int)(j - i), &sz);
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
    static int quran_line_height(HDC hdc) {
        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);
        return tm.tmHeight + tm.tmExternalLeading + tm.tmHeight / 12;
    }

    // يضيف عنصراً إلى القائمة، ويقسّم الآية الطويلة على صفوف متصلة؛ يعيد رقم صفها الأول
    int add_ayah_list_row(HDC hdc, AyahListItemData item) {
        const int first = (int)ayah_items.size();
        HGDIOBJ old_f = SelectObject(hdc, font_for_kind(item.kind));
        const int width = ayah_text_width(item.kind);

        if (item.kind == ListItemKind::Ayah) {
            // هامش أمان صغير حتى لا يلامس آخر حرف حافة السطر
            auto lines = wrap_text_lines(hdc, item.visual_text, width - 6);
            const int lh = quran_line_height(hdc);
            const int per_row = std::max(1, (kRowMaxHeight - 2 * kRowPadY) / std::max(1, lh));
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
                int pad_top = (i == 0) ? kRowPadY : 0;
                int pad_bottom = (i == n - 1) ? kRowPadY : 0;
                int h = (int)row.lines.size() * lh + pad_top + pad_bottom;
                row.height = std::min(kRowMaxHeight, std::max(n == 1 ? 48 : 8, h));
                ayah_items.push_back(row);
                // كل صفوف الآية تحمل نصها المنطوق كاملاً، فأي صف يقف عليه قارئ الشاشة يقرأ الآية كلها
                SendMessageW(list_ayahs, LB_ADDSTRING, 0, (LPARAM)row.spoken_text.c_str());
            }
        } else {
            // الترويسة: هامش الإطار + حشوة النص داخله + مسافة تنفّس
            int pad = (item.kind == ListItemKind::SurahHeader) ? (2 * kHeaderInsetY + 12 + 18) : kRowPadY * 2;
            int h = measure_text_height(hdc, item.visual_text, width) + pad;
            item.first_row = first;
            item.height = std::min(kRowMaxHeight, std::max(40, h));
            ayah_items.push_back(item);
            SendMessageW(list_ayahs, LB_ADDSTRING, 0, (LPARAM)item.spoken_text.c_str());
        }
        SelectObject(hdc, old_f);
        return first;
    }

    void set_ayahs_in_list(const std::vector<AyahInfo>& ayahs, int target_ayah, int target_surah) {
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
                if (a.ayah_number == 1) {
                    hdr.visual_text = s.name_uthmani;   // «سُورَةُ ...» داخل إطار مزخرف
                    hdr.spoken_text = is_en() ? (L"Surah " + s_name) : (L"سورة " + s_name);
                } else {
                    hdr.visual_text = s.name_uthmani + L"  —  " + place_txt + L" • " +
                        (is_en() ? (std::to_wstring(s.verses_count) + L" verses")
                                 : (L"آياتها " + num_str(s.verses_count)));
                    hdr.spoken_text = is_en()
                        ? (L"Surah " + s_name + L", " + place_txt + L", " + std::to_wstring(s.verses_count) + L" verses")
                        : (L"سورة " + s_name + L"، " + s.revelation_place + L"، آياتها " + std::to_wstring(s.verses_count));
                }
                add_ayah_list_row(hdc, hdr);

                if (a.ayah_number == 1) {
                    AyahListItemData det;
                    det.kind = ListItemKind::SurahDetails;
                    det.ayah = a;
                    det.visual_text = build_surah_heading(*db, a.surah_number, is_en());
                    det.spoken_text = det.visual_text;
                    add_ayah_list_row(hdc, det);
                }

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
    }

    // بعد أي حركة بالأسهم أو بالفأرة: إن وقع التحديد على صف تكميلي من آية مقسّمة
    // نكمل الحركة بمفاتيح القائمة نفسها حتى يقف على أول صف لآية، فيبقى نطق قارئ الشاشة طبيعياً.
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
                               : (L"  •  المظلل: " + ayah_count_ar((int)marked_ids.size(), false));
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
        InvalidateRect(hwnd, &arrow_rect_next, TRUE);
        InvalidateRect(hwnd, &arrow_rect_prev, TRUE);
    }

    // +1 فوق سهم التالي، -1 فوق السابق، 0 خارجهما (بإحداثيات منطقة العميل)
    int arrow_hit_test(POINT pt) const {
        if (PtInRect(&arrow_rect_next, pt)) return +1;
        if (PtInRect(&arrow_rect_prev, pt)) return -1;
        return 0;
    }

    // دائرة بإطار ذهبي وفي وسطها شارة اتجاه؛ visual_dir: -1 تشير لليسار المرئي، +1 لليمين المرئي
    void draw_nav_arrow(HDC hdc, const RECT& rc, int visual_dir, bool enabled, bool hover) const {
        ThemePalette pal = get_theme_palette(settings.theme);
        auto blend = [](COLORREF a, COLORREF b) {
            return RGB((GetRValue(a) + GetRValue(b)) / 2, (GetGValue(a) + GetGValue(b)) / 2, (GetBValue(a) + GetBValue(b)) / 2);
        };
        COLORREF ring = enabled ? pal.selected_border : blend(pal.selected_border, pal.window_bg);
        COLORREF fill = (enabled && hover) ? pal.selected_bg : pal.toolbar_bg;
        COLORREF mark = enabled ? pal.text_header : blend(pal.text_header, pal.window_bg);

        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 2, ring);
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
        HPEN cp = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, 4, &lb, 0, nullptr);
        HGDIOBJ old = SelectObject(hdc, cp);
        Polyline(hdc, pts, 3);
        SelectObject(hdc, old);
        DeleteObject(cp);
    }

    void paint_nav_arrows(HDC hdc) const {
        if (IsRectEmpty(&arrow_rect_next)) return;
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
        draw_nav_arrow(hdc, to_visual(arrow_rect_next), -1, can_navigate(+1), arrow_hover == +1);
        draw_nav_arrow(hdc, to_visual(arrow_rect_prev), +1, can_navigate(-1), arrow_hover == -1);
        SetLayout(hdc, old_layout);
    }

    void navigate_unit(int delta) {
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
        if (moved) {
            play_page_turn_sound(assets_dir + L"\\sounds\\page_turn.wav", settings.page_turn_sound);
            update_current_ayah_from_selection(true);
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
        say(tr(L"تم إلغاء التظليل", L"Highlighting cleared"));
    }

    // تمييز العدد بالعربية: آية واحدة، آيتان، ٣ آيات، ١١ آية
    std::wstring ayah_count_ar(int n, bool spoken) const {
        std::wstring d = spoken ? std::to_wstring(n) : num_str(n);
        if (n == 1) return L"آية واحدة";
        if (n == 2) return L"آيتان";
        if (n >= 3 && n <= 10) return d + L" آيات";
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

    // أوامر الآية لا تعمل على صفوف السورة؛ نرشد المستخدم إلى قائمة السورة بدلاً منها
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

    void copy_whole_surah(int surah_num) {
        auto ayahs = db->get_surah_ayahs(surah_num);
        std::wstring all;
        for (size_t i = 0; i < ayahs.size(); ++i) {
            if (i > 0) all += L"\r\n";
            all += format_ayah_custom(ayahs[i], settings.copy_script, settings.copy_brackets,
                                      settings.copy_reference, settings.copy_arabic_digits);
        }
        set_clipboard_text(hwnd, all);
        SurahInfo s = db->get_surah(surah_num);
        say(tr(
            L"تم نسخ سورة " + s.name_arabic + L" كاملة، " + std::to_wstring(ayahs.size()) + L" آية",
            L"Copied the whole of Surah " + s.name_arabic + L", " + std::to_wstring(ayahs.size()) + L" ayahs"
        ));
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
            say(tr(L"تم نسخ المظلل: " + ayah_count_ar(n, true), L"Copied " + std::to_wstring(n) + L" highlighted ayahs"));
            return;
        }
        // على عنوان السورة أو بيانها يُنسخ بيان السورة، وعلى البسملة تُنسخ البسملة
        if (const AyahListItemData* r = current_row()) {
            if (r->kind == ListItemKind::SurahHeader || r->kind == ListItemKind::SurahDetails) {
                set_clipboard_text(hwnd, build_surah_heading(*db, r->ayah.surah_number, is_en()));
                say(tr(L"تم نسخ بيان السورة", L"Copied the surah heading"));
                return;
            }
            if (r->kind == ListItemKind::Basmala) {
                set_clipboard_text(hwnd, L"بِسْمِ ٱللَّهِ ٱلرَّحْمَـٰنِ ٱلرَّحِيمِ");
                say(tr(L"تم نسخ البسملة", L"Copied the Basmala"));
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
            L"تم نسخ الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name,
            L"Copied ayah " + std::to_wstring(a->ayah_number) + L" of Surah " + s_name
        ));
    }

    void set_khatmah_at_current() {
        if (!require_ayah_row()) return;
        const AyahInfo* a = focused_ayah();
        if (!a) return;
        settings.khatmah_verse_key = a->verse_key;
        settings.save();
        int s = a->surah_number, ay = a->ayah_number;
        load_current_browse_unit(ay, s);
        int pct = (int)((a->id * 100.0) / std::max(1, db->total_ayahs()) + 0.5);
        std::wstring s_name = a->surah_name;   // اسم السورة بالعربية في الواجهتين
        say(tr(
            L"تم وضع فاصل الختمة عند الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name +
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
                L"لم يتم تحديد فاصل الختمة بعد. اضغط M أو Ctrl+M على أي آية لوضع علامة التوقف عندها",
                L"No reading mark set yet. Press M or Ctrl+M on any ayah to set your stop mark"
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
                L"تمت إزالة الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name + L" من المفضلة",
                L"Removed ayah " + std::to_wstring(a->ayah_number) + L" of " + s_name + L" from bookmarks"
            ));
        } else {
            settings.bookmarks.insert(settings.bookmarks.begin(), a->verse_key);
            say(tr(
                L"تمت إضافة الآية " + std::to_wstring(a->ayah_number) + L" من سورة " + s_name + L" إلى المفضلة",
                L"Added ayah " + std::to_wstring(a->ayah_number) + L" of " + s_name + L" to bookmarks"
            ));
        }
        settings.save();
        populate_sidebar_bookmarks_list();
        int s = a->surah_number, ay = a->ayah_number;
        load_current_browse_unit(ay, s);
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

    void update_sidebar_visibility() {
        bool sb_vis = settings.show_sidebar && !focus_mode;
        if (sb_vis) populate_sidebar_index_list();
        int active_tab = tab_sidebar ? (int)SendMessageW(tab_sidebar, TCM_GETCURSEL, 0, 0) : 0;

        ShowWindow(lbl_browse,    focus_mode ? SW_HIDE : SW_SHOW);
        ShowWindow(combo_browse,  focus_mode ? SW_HIDE : SW_SHOW);
        ShowWindow(btn_search,    focus_mode ? SW_HIDE : SW_SHOW);
        ShowWindow(lbl_position,  focus_mode ? SW_HIDE : SW_SHOW);

        ShowWindow(edit_sidebar_filter,    sb_vis ? SW_SHOW : SW_HIDE);
        ShowWindow(tab_sidebar,            sb_vis ? SW_SHOW : SW_HIDE);
        ShowWindow(list_sidebar_index,     (sb_vis && active_tab == 0) ? SW_SHOW : SW_HIDE);
        ShowWindow(list_sidebar_bookmarks, (sb_vis && active_tab == 1) ? SW_SHOW : SW_HIDE);
        ShowWindow(btn_sb_copy_bm,         (sb_vis && active_tab == 1) ? SW_SHOW : SW_HIDE);
        ShowWindow(btn_sb_del_bm,          (sb_vis && active_tab == 1) ? SW_SHOW : SW_HIDE);

        RECT rc;
        GetClientRect(hwnd, &rc);
        layout_children(rc.right - rc.left, rc.bottom - rc.top);
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    void cycle_landmarks_f6() {
        HWND foc = GetFocus();
        bool sb_vis = settings.show_sidebar && !focus_mode;
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
        int tb_h = focus_mode ? 0 : kToolbarHeight;
        if (!focus_mode) {
            // الشريط العلوي: «تصفح حسب» في البداية، وسطر الموضع الحالي في الطرف الآخر
            MoveWindow(lbl_browse, 16, 13, 100, 26, TRUE);
            MoveWindow(combo_browse, 120, 10, 340, 320, TRUE);
            MoveWindow(btn_search, 474, 10, 210, 31, TRUE);
            MoveWindow(lbl_position, 700, 12, std::max(0, width - 700 - 18), 28, TRUE);
        }

        int list_y = tb_h + 10;
        int list_h = std::max(100, height - list_y - 12);

        bool sb_vis = settings.show_sidebar && !focus_mode;
        int sb_w = sb_vis ? 330 : 0;
        int sb_x = 12;
        int main_x = sb_vis ? (sb_x + sb_w + 12) : 12;
        int main_w = std::max(200, width - main_x - 12);

        if (sb_vis) {
            MoveWindow(edit_sidebar_filter, sb_x, list_y, sb_w, 30, TRUE);
            int tab_y = list_y + 36;
            int tab_h = std::max(80, list_h - 36);
            MoveWindow(tab_sidebar, sb_x, tab_y, sb_w, tab_h, TRUE);

            int inner_x = sb_x + 6;
            int inner_y = tab_y + 32;
            int inner_w = sb_w - 12;
            int inner_h = std::max(60, tab_h - 38);

            MoveWindow(list_sidebar_index, inner_x, inner_y, inner_w, inner_h, TRUE);
            MoveWindow(list_sidebar_bookmarks, inner_x, inner_y, inner_w, std::max(40, inner_h - 40), TRUE);
            int btn_w = (inner_w - 8) / 2;
            MoveWindow(btn_sb_copy_bm, inner_x, inner_y + inner_h - 34, btn_w, 32, TRUE);
            MoveWindow(btn_sb_del_bm, inner_x + btn_w + 8, inner_y + inner_h - 34, btn_w, 32, TRUE);
        }

        // هامشان على جانبي صفحة المصحف لسهمي التقليب
        invalidate_nav_arrows();
        int page_x = main_x + kArrowGutter;
        int page_w = std::max(200, main_w - 2 * kArrowGutter);
        int cy = list_y + list_h / 2;
        int r = kArrowDiameter / 2;
        RECT logical_left = {main_x + kArrowGutter / 2 - r, cy - r, main_x + kArrowGutter / 2 + r, cy + r};
        RECT logical_right = {page_x + page_w + kArrowGutter / 2 - r, cy - r, page_x + page_w + kArrowGutter / 2 + r, cy + r};
        // في النافذة المعكوسة يكون الطرف المنطقي الأيمن هو اليسار المرئي
        bool rtl = (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
        arrow_rect_next = rtl ? logical_right : logical_left;
        arrow_rect_prev = rtl ? logical_left : logical_right;
        invalidate_nav_arrows();

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

// قائمة السورة: معلوماتها ونسخ بيانها أو نصها كاملاً والانتقال لأولها (مع «فتح» في فهرس السور)
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
    AppendMenuW(menu, MF_STRING, IDM_COPY_SURAH_HEAD, g_app->tr(L"نسخ بيان السورة", L"Copy the surah heading").c_str());
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
    AppendMenuW(menu, MF_STRING, IDM_COPY_PLAIN,      g_app->tr(L"نسخ بدون تشكيل\tShift+C", L"Copy without diacritics\tShift+C").c_str());
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
    AppendMenuW(menu, MF_STRING, IDM_SET_KHATMAH,     g_app->tr(L"وضع فاصل الختمة عند الآية\tM", L"Set the reading mark here\tM").c_str());

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

// موضع زر «نسخ» المضمّن: في نهاية سطر الآية (يسار الآية العربية بصرياً)
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

static LRESULT CALLBACK AyahListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
        if (wParam == 'C' && shift && !ctrl && !alt) {
            g_app->copy_current_ayah(true);
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

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (g_app) {
            g_app->layout_children(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

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
            ThemePalette pal = get_theme_palette(g_app->settings.theme);
            RECT tb = rc;
            tb.bottom = kToolbarHeight;
            FillRect(hdc, &tb, g_app->brush_toolbar);
            RECT line = tb;
            line.top = tb.bottom - 2;
            HBRUSH gold = CreateSolidBrush(pal.selected_border);
            FillRect(hdc, &line, gold);
            DeleteObject(gold);
        }
        g_app->paint_nav_arrows(hdc);
        return 1;
    }

    // سهما التقليب: للفأرة فقط (لا عنصر تحكم لهما، فلا يصل إليهما قارئ الشاشة)
    case WM_MOUSEMOVE: {
        if (!g_app) break;
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
            if (hit != 0 && g_app->can_navigate(hit)) {
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
            if (hit != 0 && g_app->can_navigate(hit)) {
                g_app->navigate_unit(hit);
                return 0;
            }
        }
        break;

    case WM_CTLCOLORSTATIC: {
        if (!g_app) break;
        HWND ctl = (HWND)lParam;
        if (ctl == g_app->lbl_browse || ctl == g_app->lbl_position) {
            ThemePalette pal = get_theme_palette(g_app->settings.theme);
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
            ThemePalette pal = get_theme_palette(g_app->settings.theme);
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, pal.paper_bg);
            SetTextColor(hdc, pal.text_quran);
            return (LRESULT)g_app->brush_paper;
        }
        break;
    }

    case WM_TIMER:
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
                ThemePalette pal = get_theme_palette(g_app->settings.theme);
                const bool rtl = g_app->list_is_rtl(dis->hwndItem);
                const bool is_ayah = (item.kind == ListItemKind::Ayah);
                const bool is_banner = (item.kind == ListItemKind::SurahHeader || item.kind == ListItemKind::SurahDetails);

                // الآية المقسّمة على عدة صفوف تُظلَّل كلها حين يُحدَّد صفها الأول
                int cur_sel = (int)SendMessageW(dis->hwndItem, LB_GETCURSEL, 0, 0);
                bool selected = (dis->itemState & ODS_SELECTED) != 0;
                if (!selected && cur_sel >= 0 && cur_sel < (int)g_app->ayah_items.size()) {
                    selected = (g_app->ayah_items[cur_sel].first_row == item.first_row);
                }

                // الآية المظللة بالفأرة: لون ذهبي خفيف يبقى ظاهراً حتى وهي محددة (والشريط الذهبي يدل على التحديد)
                const bool marked = g_app->row_is_marked(idx);
                COLORREF marked_bg = RGB((GetRValue(pal.paper_bg) * 2 + GetRValue(pal.selected_border)) / 3,
                                         (GetGValue(pal.paper_bg) * 2 + GetGValue(pal.selected_border)) / 3,
                                         (GetBValue(pal.paper_bg) * 2 + GetBValue(pal.selected_border)) / 3);
                COLORREF bg = marked ? marked_bg
                            : selected ? pal.selected_bg : (is_banner ? pal.surah_banner_bg : pal.paper_bg);
                COLORREF fg = (item.kind == ListItemKind::SurahHeader) ? pal.text_header : pal.text_quran;

                HBRUSH br = CreateSolidBrush(bg);
                FillRect(dis->hDC, &dis->rcItem, br);
                DeleteObject(br);

                // في التخطيط المعكوس (الواجهة العربية) يكون اليسار المنطقي هو اليمين المرئي
                RECT start_bar = dis->rcItem;
                if (rtl) start_bar.right = start_bar.left + kAccentWidth;
                else     start_bar.left = start_bar.right - kAccentWidth;

                if (selected) {
                    HBRUSH sbr = CreateSolidBrush(pal.selected_border);
                    FillRect(dis->hDC, &start_bar, sbr);
                    DeleteObject(sbr);

                    if (is_ayah && item.seg_index == 0 && g_app->settings.show_inline_copy_btn) {
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
                text_rc.left += kRowPadX;
                text_rc.right -= kRowPadX;
                if (rtl) text_rc.left += kAccentWidth; else text_rc.right -= kAccentWidth;
                if (is_ayah && g_app->settings.show_inline_copy_btn) {
                    if (rtl) text_rc.right -= kInlineBtnWidth; else text_rc.left += kInlineBtnWidth;
                }

                if (item.kind == ListItemKind::SurahHeader) {
                    // إطار مزخرف لاسم السورة: حدّ ذهبي مزدوج كترويسات المصاحف
                    RECT frame = dis->rcItem;
                    InflateRect(&frame, -kHeaderInsetX, -kHeaderInsetY);
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
                    text_rc.top += (is_ayah && item.seg_index > 0) ? 0 : kRowPadY;
                    text_rc.bottom -= (is_ayah && item.seg_index + 1 < item.seg_count) ? 0 : kRowPadY;
                }

                HGDIOBJ old_f = SelectObject(dis->hDC, g_app->font_for_kind(item.kind));
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, fg);

                // نص الآية يبدأ دائماً من اليمين المرئي: DT_LEFT في التخطيط المعكوس و DT_RIGHT في غيره
                UINT align;
                if (!is_ayah || g_app->settings.doc_alignment == L"center") align = DT_CENTER;
                else align = rtl ? DT_LEFT : DT_RIGHT;
                if (is_ayah && !item.lines.empty() && item.line_h > 0) {
                    // نرسم السطور التي حُسبت عند البناء سطراً سطراً، فلا يُقصّ شيء من الآية
                    int y = text_rc.top;
                    for (const auto& ln : item.lines) {
                        RECT lr = {text_rc.left, y, text_rc.right, y + item.line_h};
                        DrawTextW(dis->hDC, ln.c_str(), (int)ln.size(), &lr,
                                  align | DT_SINGLELINE | dt_reading_flag(dis->hDC, true) | DT_NOPREFIX | DT_NOCLIP);
                        y += item.line_h;
                    }
                } else {
                    if (item.kind == ListItemKind::SurahHeader) {
                        // توسيط رأسي لاسم السورة داخل الإطار
                        RECT calc = {0, 0, text_rc.right - text_rc.left, 0};
                        DrawTextW(dis->hDC, item.visual_text.c_str(), (int)item.visual_text.size(), &calc,
                                  DT_CALCRECT | DT_WORDBREAK | dt_reading_flag(dis->hDC, true) | DT_NOPREFIX);
                        int extra = (text_rc.bottom - text_rc.top) - (calc.bottom - calc.top);
                        if (extra > 0) text_rc.top += extra / 2;
                    }
                    DrawTextW(dis->hDC, item.visual_text.c_str(), (int)item.visual_text.size(), &text_rc,
                              align | DT_WORDBREAK | dt_reading_flag(dis->hDC, true) | DT_NOPREFIX);
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
                g_app->say(g_app->tr(L"تم نسخ الآية المحددة من المفضلة", L"Copied selected bookmarked ayah"));
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
        case IDM_BROWSE_SURAH:  g_app->settings.browse_scope = L"surah";  g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); return 0;
        case IDM_BROWSE_PAGE:   g_app->settings.browse_scope = L"page";   g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); return 0;
        case IDM_BROWSE_JUZ:    g_app->settings.browse_scope = L"juz";    g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); return 0;
        case IDM_BROWSE_RUB:    g_app->settings.browse_scope = L"rub";    g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); return 0;
        case IDM_BROWSE_HIZB:   g_app->settings.browse_scope = L"hizb";   g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); return 0;
        case IDM_BROWSE_MANZIL: g_app->settings.browse_scope = L"manzil"; g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); return 0;
        case IDM_BROWSE_RUKU:   g_app->settings.browse_scope = L"ruku";   g_app->populate_toolbar_combos(); g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah); return 0;


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
        case IDM_COPY_PLAIN:       g_app->copy_current_ayah(true); return 0;
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
            std::wstring htxt = build_surah_heading(*g_app->db, g_app->take_context_surah(), g_app->is_en());
            set_clipboard_text(hwnd, htxt);
            g_app->say(g_app->tr(L"تم نسخ بيان السورة إلى الحافظة", L"Copied surah heading to clipboard"));
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
            g_app->say(g_app->tr(L"تم نسخ جميع الآيات المعروضة إلى الحافظة", L"Copied all displayed ayahs to clipboard"));
            return 0;
        }

        case IDM_OPTIONS: {
            if (show_options_dialog(hwnd, g_app->settings, g_app->focused_ayah(), g_app->is_en(), g_app->font_ui)) {
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
            return 0;

        case IDM_FOCUS_MODE:
            g_app->focus_mode = !g_app->focus_mode;
            g_app->build_menus();
            g_app->update_sidebar_visibility();
            SetFocus(g_app->showing_units ? g_app->list_units : g_app->list_ayahs);
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
            auto build_letter_freq = [](const std::wstring& simple_txt) -> std::wstring {
                std::map<wchar_t, int> freq;
                for (wchar_t ch : simple_txt) {
                    if ((ch >= 0x0621 && ch <= 0x064A) || ch == 0x0671) {
                        wchar_t n = ch;
                        if (n == 0x0671) n = 0x0627;
                        ++freq[n];
                    }
                }
                std::wstring out;
                for (const auto& kv : freq) {
                    if (!out.empty()) out += L"، ";
                    out += std::wstring(1, kv.first) + L" (" + std::to_wstring(kv.second) + L")";
                }
                return out;
            };
            std::wstring letter_freq = build_letter_freq(a->text_simple);
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
            bool ay_excepted = is_ayah_excepted(a->ayah_number, sh.excepted_verses);

            if (g_app->is_en()) {
                lines.push_back(L"﴿ " + a->text_uthmani + L" ﴾");
                lines.push_back(L"Surah: " + s_name + L" (Surah #" + std::to_wstring(a->surah_number) + L" of 114, " + rev_place + L")");
                if (ay_excepted) {
                    std::wstring exc_msg = L"Revelation Exception: This ayah is excepted from the surah's revelation type";
                    if (!sh.place_note.empty()) exc_msg += L" (" + sh.place_note + L")";
                    lines.push_back(exc_msg);
                }
                lines.push_back(L"Ayah Number: " + std::to_wstring(a->ayah_number) + L" of " + std::to_wstring(s.verses_count));
                if (a->ayah_number == 1) lines.push_back(L"Position in Surah: First ayah of the surah");
                else if (a->ayah_number == s.verses_count) lines.push_back(L"Position in Surah: Last ayah of the surah");
                lines.push_back(L"Global Ayah Order: " + std::to_wstring(a->id) + L" of 6,236");
                lines.push_back(L"Words: " + std::to_wstring(st.words));
                lines.push_back(L"Letter Counter: " + std::to_wstring(st.letters) + L" letters (without diacritics/spaces)");
                if (!letter_freq.empty()) lines.push_back(L"Letter Breakdown: " + letter_freq);
                lines.push_back(L"Occurrences of the name of Allah (الله): " + (st.allah ? std::to_wstring(st.allah) + L" time(s)" : L"Does not occur"));
                if (other_places.empty()) {
                    lines.push_back(L"Repetition: Does not occur elsewhere in the Quran");
                } else {
                    std::wstring joined;
                    for (size_t i = 0; i < other_places.size() && i < 15; ++i) {
                        if (i > 0) joined += L", ";
                        joined += other_places[i];
                    }
                    if (other_places.size() > 15) joined += L" ...";
                    lines.push_back(L"Repetition (" + std::to_wstring(other_places.size()) + L" other places): " + joined);
                }
                lines.push_back(L"Page (" + g_app->edition_short_label() + L"): " + std::to_wstring(a->page_number) + L" of " + std::to_wstring(g_app->db->page_count()));
                lines.push_back(L"Juz: " + std::to_wstring(a->juz_number) + L" of 30 • Hizb: " + std::to_wstring(a->hizb_number) + L" of 60");
                lines.push_back(L"Quarter: " + std::to_wstring(a->rub_el_hizb_number) + L" of 240 (" + std::wstring(q_en[q_idx]) + L" of Hizb " + std::to_wstring(a->hizb_number) + L")");
                lines.push_back(L"Ruku: " + std::to_wstring(a->ruku_number) + L" of 558");
                lines.push_back(L"Seven Manzil: " + std::to_wstring(a->manzil_number) + L" of 7 (" + manzil_day_label(a->manzil_number, true) + L")");
            } else {
                lines.push_back(L"﴿ " + a->text_uthmani + L" ﴾");
                lines.push_back(L"السورة: سورة " + s_name + L" (رقم " + std::to_wstring(a->surah_number) + L" من 114 — " + rev_place + L")");
                if (ay_excepted) {
                    std::wstring exc_msg = L"استثناء النزول: هذه الآية مستثناة من نوع تنزيل السورة";
                    if (!sh.place_note.empty()) exc_msg += L" (" + sh.place_note + L")";
                    lines.push_back(exc_msg);
                }
                lines.push_back(L"رقم الآية: " + std::to_wstring(a->ayah_number) + L" من أصل " + std::to_wstring(s.verses_count) + L" آية");
                if (a->ayah_number == 1) lines.push_back(L"موقعها في السورة: أول آية في السورة");
                else if (a->ayah_number == s.verses_count) lines.push_back(L"موقعها في السورة: آخر آية في السورة");
                lines.push_back(L"الترتيب العام للآية في المصحف: " + std::to_wstring(a->id) + L" من 6236");
                lines.push_back(L"عدد الكلمات: " + std::to_wstring(st.words) + L" كلمة");
                lines.push_back(L"عداد الحروف: " + std::to_wstring(st.letters) + L" حرفاً (دون التشكيل والمسافات)");
                if (!letter_freq.empty()) lines.push_back(L"تفصيل الحروف في الآية: " + letter_freq);
                lines.push_back(L"ورود لفظ الجلالة «الله»: " + (st.allah ? (std::to_wstring(st.allah) + L" مرة") : L"لم يرد في هذه الآية"));
                if (other_places.empty()) {
                    lines.push_back(L"تكرار نص الآية: لا يتكرر نصها في موضع آخر من القرآن");
                } else {
                    std::wstring joined;
                    for (size_t i = 0; i < other_places.size() && i < 15; ++i) {
                        if (i > 0) joined += L"، ";
                        joined += other_places[i];
                    }
                    if (other_places.size() > 15) joined += L" ...";
                    lines.push_back(L"تكرار نص الآية (" + std::to_wstring(other_places.size()) + L" موضعاً آخر): " + joined);
                }
                lines.push_back(L"الصفحة (" + g_app->edition_short_label() + L"): " + std::to_wstring(a->page_number) + L" من " + std::to_wstring(g_app->db->page_count()));
                lines.push_back(L"الجزء: " + std::to_wstring(a->juz_number) + L" من 30 • الحزب: " + std::to_wstring(a->hizb_number) + L" من 60");
                lines.push_back(L"الربع العام: " + std::to_wstring(a->rub_el_hizb_number) + L" من 240 (" + std::wstring(q_ar[q_idx]) + L" من الحزب " + std::to_wstring(a->hizb_number) + L")");
                lines.push_back(L"الركوع: " + std::to_wstring(a->ruku_number) + L" من 558");
                lines.push_back(L"التحزيب السباعي: المنزلة " + std::to_wstring(a->manzil_number) + L" من 7 (" + manzil_day_label(a->manzil_number, false) + L")");
            }
            if (const SajdahPlace* sp = find_sajdah_place(a->verse_key)) {
                lines.push_back(g_app->is_en() ? (L"Sajdah: " + std::wstring(sp->note_en)) : (L"بيان السجدة: " + std::wstring(sp->note_ar)));
            }
            show_line_reader_dialog(
                hwnd,
                g_app->tr(L"معلومات عن الآية " + g_app->num_str(a->ayah_number) + L" من سورة " + s_name,
                          L"Ayah " + std::to_wstring(a->ayah_number) + L" of Surah " + s_name + L" — information"),
                lines, g_app->is_en(), g_app->font_ui, &ScreenReaderEngine::instance()
            );
            return 0;
        }

        case IDM_SURAH_INFO: {
            int target_surah_num = (g_app->pending_surah_info_num > 0) ? g_app->pending_surah_info_num : g_app->current_surah;
            g_app->pending_surah_info_num = 0;
            SurahInfo s = g_app->db->get_surah(target_surah_num);
            std::wstring s_name = s.name_arabic;
            auto s_ayahs = g_app->db->get_surah_ayahs(s.number);
            int total_words = 0, total_letters = 0, total_allah = 0, sajdahs = 0;
            int min_juz = 99, max_juz = 0, min_ruku = 9999, max_ruku = 0;
            int longest_ayah = 0, longest_letters = -1;
            int shortest_ayah = 0, shortest_letters = 999999;
            for (const auto& ay : s_ayahs) {
                min_juz = std::min(min_juz, ay.juz_number);
                max_juz = std::max(max_juz, ay.juz_number);
                min_ruku = std::min(min_ruku, ay.ruku_number);
                max_ruku = std::max(max_ruku, ay.ruku_number);
                if (find_sajdah_place(ay.verse_key)) ++sajdahs;

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
                lines.push_back(L"Surah Name: " + s.name_arabic);
                lines.push_back(L"Mushaf Order: Surah #" + std::to_wstring(s.number) + L" of 114");
                lines.push_back(L"Revelation Order: #" + std::to_wstring(s.revelation_order));
                lines.push_back(L"Revelation Place: " + std::wstring((sh.type.empty() ? s.revelation_place : sh.type) == L"مكية" ? L"Meccan" : L"Medinan"));
                if (!sh.excepted_verses.empty()) {
                    std::wstring exc_txt = L"Excepted Ayahs: Ayah " + to_western_digits(sh.excepted_verses);
                    if (!sh.place_note.empty()) exc_txt += L" (" + to_western_digits(sh.place_note) + L")";
                    lines.push_back(exc_txt);
                } else {
                    lines.push_back(L"Excepted Ayahs: None");
                    if (!sh.place_note.empty()) lines.push_back(L"Revelation Note: " + to_western_digits(sh.place_note));
                }
                lines.push_back(L"Verses Count: " + std::to_wstring(s.verses_count) + L" verses");
                lines.push_back(L"Pages (" + g_app->edition_short_label() + L"): " + g_app->page_range_text(s.start_page, s.end_page));
                lines.push_back(L"Juz Span: " + (min_juz == max_juz ? std::to_wstring(min_juz) : (std::to_wstring(min_juz) + L" - " + std::to_wstring(max_juz))));
                lines.push_back(L"Rukus in Surah: " + std::to_wstring(ruku_cnt) + L" (Ruku " + std::to_wstring(min_ruku) + L" - " + std::to_wstring(max_ruku) + L")");
                lines.push_back(L"Words: " + std::to_wstring(total_words));
                lines.push_back(L"Letter Counter: " + std::to_wstring(total_letters) + L" letters (without diacritics/spaces)");
                if (longest_ayah > 0) {
                    lines.push_back(L"Longest Ayah: Ayah " + std::to_wstring(longest_ayah) + L" (" + std::to_wstring(longest_letters) + L" letters) • Shortest Ayah: Ayah " + std::to_wstring(shortest_ayah) + L" (" + std::to_wstring(shortest_letters) + L" letters)");
                }
                lines.push_back(L"Occurrences of the name of Allah (الله): " + std::to_wstring(total_allah) + L" time(s) • Sajdahs: " + std::to_wstring(sajdahs));
                lines.push_back(L"Heading Summary: " + build_surah_heading(*g_app->db, s.number, true));
            } else {
                lines.push_back(L"اسم السورة: سورة " + s.name_arabic);
                lines.push_back(L"ترتيبها في المصحف: السورة رقم " + std::to_wstring(s.number) + L" من 114");
                lines.push_back(L"ترتيب نزولها: " + std::to_wstring(s.revelation_order));
                lines.push_back(L"نوع التنزيل: " + (sh.type.empty() ? s.revelation_place : sh.type));
                if (!sh.excepted_verses.empty()) {
                    bool multi_exc = (sh.excepted_verses.find(L'،') != std::wstring::npos || sh.excepted_verses.find(L'-') != std::wstring::npos);
                    std::wstring exc_txt = std::wstring(multi_exc ? L"الآيات المستثناة: الآيات " : L"الآيات المستثناة: الآية ") + to_arabic_digits(sh.excepted_verses);
                    if (!sh.place_note.empty()) exc_txt += L" (" + to_arabic_digits(sh.place_note) + L")";
                    lines.push_back(exc_txt);
                } else {
                    lines.push_back(L"الآيات المستثناة: لا توجد آيات مستثناة في هذه السورة");
                    if (!sh.place_note.empty()) lines.push_back(L"ملاحظة النزول: " + to_arabic_digits(sh.place_note));
                }
                lines.push_back(L"عدد آياتها (بالكوفي): " + std::to_wstring(s.verses_count) + L" آية");
                lines.push_back(L"صفحاتها (" + g_app->edition_short_label() + L"): " + g_app->page_range_text(s.start_page, s.end_page));
                lines.push_back(L"نطاق الأجزاء: " + (min_juz == max_juz ? (L"الجزء " + std::to_wstring(min_juz)) : (L"من الجزء " + std::to_wstring(min_juz) + L" إلى الجزء " + std::to_wstring(max_juz))));
                lines.push_back(L"الركوعات في السورة: " + std::to_wstring(ruku_cnt) + L" ركوعاً (من الركوع " + std::to_wstring(min_ruku) + L" إلى " + std::to_wstring(max_ruku) + L")");
                lines.push_back(L"إجمالي الكلمات: " + std::to_wstring(total_words) + L" كلمة");
                lines.push_back(L"عداد الحروف في السورة: " + std::to_wstring(total_letters) + L" حرفاً (دون التشكيل والمسافات)");
                if (longest_ayah > 0) {
                    lines.push_back(L"أطول آية: الآية " + std::to_wstring(longest_ayah) + L" (" + std::to_wstring(longest_letters) + L" حرفاً) • أقصر آية: الآية " + std::to_wstring(shortest_ayah) + L" (" + std::to_wstring(shortest_letters) + L" حرفاً)");
                }
                lines.push_back(L"ورود لفظ الجلالة «الله»: " + std::to_wstring(total_allah) + L" مرة • مواضع السجود: " + std::to_wstring(sajdahs));
                lines.push_back(L"بيان السورة: " + build_surah_heading(*g_app->db, s.number, false));
            }
            show_line_reader_dialog(
                hwnd,
                g_app->tr(L"معلومات عن سورة " + s_name, L"Surah " + s_name + L" — information"),
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
                lines.push_back(L"Shift+C: Copy current ayah as plain text without diacritics");
                lines.push_back(L"Ctrl+Alt+C: Multiple copy — the current unit's ayahs with checkboxes; Space selects, Enter copies");
                lines.push_back(L"Mouse: drag over ayahs to highlight them (each drag adds to the highlight, Ctrl+click toggles one ayah); Copy then copies all highlighted ayahs");
                lines.push_back(L"M / Ctrl+M: Set Reading Stop Mark (Khatmah) at current ayah");
                lines.push_back(L"B: Add or remove current ayah from bookmarks");
                lines.push_back(L"I: Speak quick ayah position info");
            } else {
                lines.push_back(L"سهم لأعلى / سهم لأسفل: التنقل بين الآيات وقراءتها فوراً");
                lines.push_back(L"PageDown / PageUp: الانتقال للوحدة التالية أو السابقة (سورة / صفحة / جزء / ربع / حزب / منزلة / ركوع)");
                lines.push_back(L"Escape: الخروج من الآيات والرجوع لقائمة السور (أو الصفحات والأجزاء...)");
                lines.push_back(L"زر التطبيقات أو Shift+F10 على الآية: قائمة الآية (النسخ، ومعلومات الآية، والمفضلة، وفاصل الختمة)");
                lines.push_back(L"زر التطبيقات أو Shift+F10 على عنوان السورة أو في فهرس السور: قائمة السورة (معلومات السورة، ونسخ بيانها، ونسخها كاملة)");
                lines.push_back(L"Enter أو Ctrl+C أو F8: نسخ الآية الحالية إلى الحافظة");
                lines.push_back(L"Shift+C: نسخ الآية الحالية بنص إملائي مجرد بدون تشكيل");
                lines.push_back(L"Ctrl+Alt+C: النسخ المتعدد — آيات الوحدة الحالية بمربعات اختيار، المسافة للتحديد و Enter للنسخ");
                lines.push_back(L"بالفأرة: اسحب على الآيات لتظليلها (كل سحب يضيف إلى التظليل السابق، و Ctrl مع النقر يظلل آية واحدة أو يلغيها)، ثم «نسخ» ينسخ كل المظلل");
                lines.push_back(L"M أو Ctrl+M: وضع فاصل الختمة (علامة التوقف للقراءة) عند الآية الحالية");
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
            g_app->settings.quran_font_size = std::min(52, g_app->settings.quran_font_size + 2);
            g_app->settings.save();
            g_app->recreate_fonts_and_brushes();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            return 0;
        case IDM_FONT_DEC:
            g_app->settings.quran_font_size = std::max(16, g_app->settings.quran_font_size - 2);
            g_app->settings.save();
            g_app->recreate_fonts_and_brushes();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            return 0;
        case IDM_FONT_RESET:
            g_app->settings.quran_font_size = 26;
            g_app->settings.save();
            g_app->recreate_fonts_and_brushes();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
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

        case IDM_DISPLAY_TEXT:
        case IDM_DISPLAY_NUM:
        case IDM_DISPLAY_SURAH:
            g_app->settings.ayah_display = (id == IDM_DISPLAY_TEXT) ? L"text"
                                         : (id == IDM_DISPLAY_NUM)  ? L"text_number"
                                         : L"text_number_surah";
            g_app->settings.save();
            g_app->build_menus();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            return 0;

        case IDM_CYCLE_THEME: {
            static const wchar_t* th[4] = {L"madinah_cream", L"royal_emerald", L"night_gold", L"high_contrast"};
            int idx = 0;
            for (int i = 0; i < 4; ++i) if (g_app->settings.theme == th[i]) idx = (i + 1) % 4;
            g_app->settings.theme = th[idx];
            g_app->settings.save();
            g_app->recreate_fonts_and_brushes();
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return 0;
        }

        case IDM_LANG_AR:
        case IDM_LANG_EN:
            g_app->settings.ui_language = (id == IDM_LANG_EN) ? L"en" : L"ar";
            g_app->settings.save();
            g_app->build_menus();
            g_app->populate_toolbar_combos();
            g_app->load_current_browse_unit(g_app->current_ayah_num, g_app->current_surah);
            return 0;

        case IDM_ABOUT: {
            // آية الحفظ تُقرأ من قاعدة البيانات نفسها حتى يطابق رسمها المصحف حرفاً بحرف
            AyahInfo hifz;
            std::wstring hifz_txt;
            if (g_app->db->get_ayah(15, 9, hifz)) {
                hifz_txt = L"﴿ " + hifz.text_uthmani + (g_app->is_en() ? L" ﴾ [Al-Hijr: 9]" : L" ﴾ [الحجر: ٩]");
            }
            std::vector<std::wstring> lines = g_app->is_en() ? std::vector<std::wstring>{
                L"Al-Mushaf Al-Sharif — Version 1.0.0",
                L"The Noble Quran in the narration of Hafs from 'Asim, following the Madinah Mushaf script "
                L"issued by the King Fahd Complex for the Printing of the Holy Quran (604 pages).",
                L"Built to be a calm companion for reciting, reviewing and reflecting on the Book of Allah, "
                L"with nothing to draw the heart away from its verses.",
                L"Developed by the Omnya Technology team and offered, seeking only the pleasure of Allah, "
                L"as a gift to researchers, master's and doctoral students, and all Muslims.",
                L"Runs on Windows 7, 8.1, 10 and 11, and serves sighted readers and screen reader users "
                L"(NVDA and JAWS) alike.",
                hifz_txt,
                L"© 2026 Omnya Technology"
            } : std::vector<std::wstring>{
                L"المصحف الشريف — الإصدار ١٫٠٫٠",
                L"القرآن الكريم برواية حفص عن عاصم، على رسم مصحف المدينة النبوية الصادر عن "
                L"مجمع الملك فهد لطباعة المصحف الشريف (٦٠٤ صفحات).",
                L"صُمِّم ليكون رفيقاً هادئاً لتلاوة كتاب الله ومراجعته وتدبّره، "
                L"خالياً مما يشغل القلب عن آياته.",
                L"طوّره فريق أمنية تكنولوجي (Omnya Technology)، وقدّمه هديةً خالصةً لوجه الله تعالى "
                L"إلى الباحثين وطلاب الماجستير والدكتوراه وعموم المسلمين.",
                L"يعمل على ويندوز ٧ و٨٫١ و١٠ و١١، ويخدم المبصرين ومستخدمي قارئات الشاشة على حدٍّ سواء.",
                hifz_txt,
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

    case WM_DESTROY:
        if (g_app) {
            g_app->settings.last_surah = g_app->current_surah;
            g_app->settings.last_page = g_app->current_page;
            g_app->settings.last_juz = g_app->current_juz;
            g_app->settings.last_hizb = g_app->current_hizb;
            g_app->settings.last_rub = g_app->current_rub;
            g_app->settings.last_manzil = g_app->current_manzil;
            g_app->settings.last_ruku = g_app->current_ruku;
            g_app->settings.last_ayah = g_app->current_ayah_num;
            WINDOWPLACEMENT wp{};
            wp.length = sizeof(wp);
            if (GetWindowPlacement(hwnd, &wp)) {
                const RECT& r = wp.rcNormalPosition;
                g_app->settings.window_x = r.left;
                g_app->settings.window_y = r.top;
                g_app->settings.window_w = r.right - r.left;
                g_app->settings.window_h = r.bottom - r.top;
                g_app->settings.window_maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
            }
            g_app->settings.save();
        }
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
    SELFTEST_CHECK(prepare_ayah_for_screen_reader(alm, true, true) == L"أَلِفْ لَامْ مِيمْ");

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

    // الإعدادات: أوضاع التصفح السبعة مقبولة، وأي قيمة غريبة مرفوضة
    for (const wchar_t* s : {L"surah", L"page", L"juz", L"hizb", L"rub", L"manzil", L"ruku"}) {
        SELFTEST_CHECK(is_valid_browse_scope(s));
    }
    SELFTEST_CHECK(!is_valid_browse_scope(L"shamarly"));

    // آية الدَّين (البقرة ٢٨٢) أطول آية: لا بد أن تُقسَّم على صفوف كلٌّ منها في حدود الارتفاع،
    // وأن يبقى نصها كاملاً بلا نقص ولا زيادة بعد التقسيم.
    {
        app.load_private_fonts();
        AyahInfo dayn;
        SELFTEST_CHECK(app.db->get_ayah(2, 282, dayn));
        HDC hdc = CreateCompatibleDC(nullptr);
        HFONT f = CreateFontW(-MulDiv(26, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"KFGQPC HAFS Uthmanic Script");
        HGDIOBJ old = SelectObject(hdc, f);
        const int width = 700;
        const int max_h = kRowMaxHeight - 2 * kRowPadY;
        const int lh = MushafAppWindow::quran_line_height(hdc);
        auto lines = MushafAppWindow::wrap_text_lines(hdc, dayn.text_qpc_hafs, width);
        // أكثر من صف واحد لازم: كل سطورها لا تتسع لها حدود ارتفاع الصف الواحد
        SELFTEST_CHECK((int)lines.size() * lh > max_h);
        SELFTEST_CHECK(lh > 0 && lh <= max_h);
        std::wstring joined;
        for (const auto& ln : lines) {
            // كل سطر يتسع لعرضه، إلا كلمة مفردة أعرض من السطر (لا تحدث عملياً)
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

        // الآية القصيرة تبقى صفاً واحداً
        SELFTEST_CHECK(MushafAppWindow::wrap_text_lines(hdc, alm.text_qpc_hafs, width).size() == 1);
        SelectObject(hdc, old);
        DeleteObject(f);
        DeleteDC(hdc);
    }

    if (g_selftest_failures > 0) {
        std::fprintf(stderr, "%d SELF-TEST CHECK(S) FAILED\n", g_selftest_failures);
        return 1;
    }
    std::printf("ALL C++ SELF-TESTS PASSED (6236 ayahs, 114 surahs, Madinah 604 pages, 7 browse modes, "
                "558 rukus, long-ayah row splitting, divisions, shortcuts & screen reader)\n");
    return 0;
}

} // namespace mushaf

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    if (lpCmdLine && std::wcsstr(lpCmdLine, L"--self-test") != nullptr) {
        // إن كان الخرج موجّهاً إلى ملف أو أنبوب نكتب فيه مباشرة، وإلا نلتحق بنافذة الأوامر الأم
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
    app.load_private_fonts();
    mushaf::ScreenReaderEngine::instance().init(app.app_dir);

    app.db = std::make_unique<mushaf::QuranDatabase>(
        app.assets_dir + L"\\data\\quran.sqlite3",
        app.assets_dir + L"\\data\\surah_info_egyptian.json"
    );
    app.current_surah = app.settings.last_surah;
    app.current_page = app.settings.last_page;
    app.current_juz = app.settings.last_juz;
    app.current_hizb = app.settings.last_hizb;
    app.current_rub = app.settings.last_rub;
    app.current_manzil = app.settings.last_manzil;
    app.current_ruku = std::max(1, std::min(558, app.settings.last_ruku));
    app.current_ayah_num = app.settings.last_ayah;

    // الأيقونة المضمّنة في الملف التنفيذي احتياطاً إن لم يوجد ملف assets\icon.ico
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
    int win_x = CW_USEDEFAULT, win_y = CW_USEDEFAULT, win_w = 1280, win_h = 820;
    if (app.settings.window_w >= 640 && app.settings.window_h >= 480) {
        RECT saved = {app.settings.window_x, app.settings.window_y,
                      app.settings.window_x + app.settings.window_w,
                      app.settings.window_y + app.settings.window_h};
        if (MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
            win_x = saved.left;
            win_y = saved.top;
            win_w = app.settings.window_w;
            win_h = app.settings.window_h;
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

    app.recreate_fonts_and_brushes();
    app.build_menus();

    // 1) الشريط العلوي: «تصفح حسب» فقط، وفي طرفه الآخر سطر يبيّن موضع القراءة الحالي
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
        0, L"BUTTON", app.tr(L"نسخ المحددة", L"Copy selected").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        18, 748, 155, 32, app.hwnd, (HMENU)mushaf::ID_BTN_SB_COPY_BM, hInstance, nullptr
    );
    app.btn_sb_del_bm = CreateWindowExW(
        0, L"BUTTON", app.tr(L"حذف", L"Remove").c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        181, 748, 155, 32, app.hwnd, (HMENU)mushaf::ID_BTN_SB_DEL_BM, hInstance, nullptr
    );

    // 3) منطقة المصحف (مكدس قائمة الوحدات وقائمة الآيات)
    app.list_units = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        354, 54, 914, 720, app.hwnd, (HMENU)mushaf::ID_LIST_UNITS, hInstance, nullptr
    );
    app.list_ayahs = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWVARIABLE | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        354, 54, 914, 720, app.hwnd, (HMENU)mushaf::ID_LIST_AYAHS, hInstance, nullptr
    );

    for (HWND w : {app.lbl_browse, app.combo_browse, app.btn_search, app.lbl_position,
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
    app.update_sidebar_visibility();   // ترتيب العناصر أولاً حتى تُقسَّم الآيات على عرضها الحقيقي
    app.load_current_browse_unit(app.current_ayah_num, app.current_surah);
    app.push_history(app.current_surah, app.current_ayah_num);
    app.rebuild_accelerators();

    ShowWindow(app.hwnd, app.settings.window_maximized ? SW_SHOWMAXIMIZED : nCmdShow);
    UpdateWindow(app.hwnd);
    if (app.settings.startup_focus == L"units") {
        app.show_units_view(true);
    } else if (app.settings.startup_focus == L"ayahs") {
        SetFocus(app.showing_units ? app.list_units : app.list_ayahs);
    } else if (app.settings.show_sidebar && !app.focus_mode && IsWindowVisible(app.list_sidebar_index)) {
        SetFocus(app.list_sidebar_index);
    } else {
        SetFocus(app.showing_units ? app.list_units : app.list_ayahs);
    }

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
                if (foc == app.combo_browse &&
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
