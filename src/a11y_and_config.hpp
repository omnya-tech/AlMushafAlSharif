#pragma once
// -*- coding: utf-8 -*-
// a11y_and_config.hpp — محرك قارئات الشاشة المباشر (NVDA / JAWS / SAPI5) والإعدادات والمظاهر اللونية.

#include "utf8_util.hpp"
#include <mmsystem.h>
#include <objbase.h>
#include <oleauto.h>
#include <shlobj.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace mushaf {

class ScreenReaderEngine {
public:
    static ScreenReaderEngine& instance() {
        static ScreenReaderEngine inst;
        return inst;
    }

    void init(const std::wstring& app_dir) {
        if (initialized_) return;
        initialized_ = true;
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

#ifdef _WIN64
        const wchar_t* nvda_dll = L"nvdaControllerClient64.dll";
#else
        const wchar_t* nvda_dll = L"nvdaControllerClient32.dll";
#endif
        std::wstring p1 = app_dir + L"\\" + nvda_dll;
        nvda_mod_ = LoadLibraryW(p1.c_str());
        if (!nvda_mod_) {
            nvda_mod_ = LoadLibraryW(nvda_dll);
        }
        if (nvda_mod_) {
            nvda_test_ = reinterpret_cast<fn_test_t>(GetProcAddress(nvda_mod_, "nvdaController_testIfRunning"));
            nvda_speak_ = reinterpret_cast<fn_speak_t>(GetProcAddress(nvda_mod_, "nvdaController_speakText"));
            nvda_cancel_ = reinterpret_cast<fn_cancel_t>(GetProcAddress(nvda_mod_, "nvdaController_cancelSpeech"));
        }
    }

    void speak(const std::wstring& text, bool interrupt = true, bool allow_sapi_fallback = false) {
        if (text.empty()) return;

        // ١. المحاولة عبر NVDA المباشر
        if (nvda_test_ && nvda_speak_ && nvda_test_() == 0) {
            if (interrupt && nvda_cancel_) nvda_cancel_();
            if (nvda_speak_(text.c_str()) == 0) return;
        }

        // ٢. المحاولة عبر JAWS COM API
        if (speak_jaws(text, interrupt)) return;

        // ٣. النطق المدمج SAPI5 عند تفعيله
        if (allow_sapi_fallback) {
            speak_sapi(text, interrupt);
        }
    }

private:
    using fn_test_t = long(__stdcall*)();
    using fn_speak_t = long(__stdcall*)(const wchar_t*);
    using fn_cancel_t = long(__stdcall*)();

    bool initialized_ = false;
    HMODULE nvda_mod_ = nullptr;
    fn_test_t nvda_test_ = nullptr;
    fn_speak_t nvda_speak_ = nullptr;
    fn_cancel_t nvda_cancel_ = nullptr;
    IDispatch* jaws_disp_ = nullptr;
    bool jaws_checked_ = false;
    IDispatch* sapi_disp_ = nullptr;
    bool sapi_checked_ = false;

    bool speak_jaws(const std::wstring& text, bool interrupt) {
        if (!jaws_checked_) {
            jaws_checked_ = true;
            CLSID clsid;
            if (SUCCEEDED(CLSIDFromProgID(L"FreedomSci.JawsApi", &clsid))) {
                CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IDispatch, (void**)&jaws_disp_);
            }
        }
        if (!jaws_disp_) return false;
        OLECHAR* member = const_cast<OLECHAR*>(L"SayString");
        DISPID dispid;
        if (FAILED(jaws_disp_->GetIDsOfNames(IID_NULL, &member, 1, LOCALE_USER_DEFAULT, &dispid))) return false;

        VARIANT args[2];
        VariantInit(&args[0]);
        VariantInit(&args[1]);
        args[0].vt = VT_BOOL;
        args[0].boolVal = interrupt ? VARIANT_TRUE : VARIANT_FALSE;
        args[1].vt = VT_BSTR;
        args[1].bstrVal = SysAllocString(text.c_str());

        DISPPARAMS dp = {args, nullptr, 2, 0};
        VARIANT res;
        VariantInit(&res);
        HRESULT hr = jaws_disp_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dp, &res, nullptr, nullptr);
        SysFreeString(args[1].bstrVal);
        return SUCCEEDED(hr) && res.vt == VT_BOOL && res.boolVal == VARIANT_TRUE;
    }

    void speak_sapi(const std::wstring& text, bool interrupt) {
        if (!sapi_checked_) {
            sapi_checked_ = true;
            CLSID clsid;
            if (SUCCEEDED(CLSIDFromProgID(L"SAPI.SpVoice", &clsid))) {
                CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IDispatch, (void**)&sapi_disp_);
            }
        }
        if (!sapi_disp_) return;
        OLECHAR* member = const_cast<OLECHAR*>(L"Speak");
        DISPID dispid;
        if (FAILED(sapi_disp_->GetIDsOfNames(IID_NULL, &member, 1, LOCALE_USER_DEFAULT, &dispid))) return;

        VARIANT args[2];
        VariantInit(&args[0]);
        VariantInit(&args[1]);
        args[0].vt = VT_I4;
        args[0].lVal = 1 | (interrupt ? 2 : 0); // SPF_ASYNC | SPF_PURGEBEFORESPEAK
        args[1].vt = VT_BSTR;
        args[1].bstrVal = SysAllocString(text.c_str());

        DISPPARAMS dp = {args, nullptr, 2, 0};
        sapi_disp_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dp, nullptr, nullptr, nullptr);
        SysFreeString(args[1].bstrVal);
    }
};

inline void set_clipboard_text(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* ptr = GlobalLock(hMem);
        if (ptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
}

inline void play_page_turn_sound(const std::wstring& wav_path, bool enabled) {
    if (!enabled || wav_path.empty()) return;
    PlaySoundW(wav_path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

struct ThemePalette {
    COLORREF window_bg;
    COLORREF paper_bg;
    COLORREF toolbar_bg;
    COLORREF text_quran;
    COLORREF text_header;
    COLORREF selected_bg;
    COLORREF selected_border;
    COLORREF surah_banner_bg;
};

inline ThemePalette get_theme_palette(const std::wstring& key) {
    if (key == L"royal_emerald") {
        return {
            RGB(13, 43, 34),    // window_bg
            RGB(244, 249, 245), // paper_bg
            RGB(19, 59, 46),    // toolbar_bg
            RGB(16, 36, 28),    // text_quran
            RGB(15, 82, 58),    // text_header
            RGB(214, 236, 222), // selected_bg
            RGB(184, 138, 42),  // selected_border
            RGB(230, 242, 234), // surah_banner_bg
        };
    }
    if (key == L"night_gold") {
        return {
            RGB(15, 22, 34),    // window_bg
            RGB(20, 29, 43),    // paper_bg
            RGB(25, 36, 54),    // toolbar_bg
            RGB(242, 234, 214), // text_quran
            RGB(218, 179, 84),  // text_header
            RGB(38, 54, 78),    // selected_bg
            RGB(218, 179, 84),  // selected_border
            RGB(29, 42, 62),    // surah_banner_bg
        };
    }
    if (key == L"high_contrast") {
        return {
            RGB(0, 0, 0),       // window_bg
            RGB(0, 0, 0),       // paper_bg
            RGB(18, 18, 18),    // toolbar_bg
            RGB(255, 255, 102), // text_quran
            RGB(102, 255, 255), // text_header
            RGB(48, 48, 0),     // selected_bg
            RGB(255, 255, 0),   // selected_border
            RGB(28, 28, 28),    // surah_banner_bg
        };
    }
    // madinah_cream (الافتراضي)
    return {
        RGB(238, 229, 209), // window_bg
        RGB(253, 249, 238), // paper_bg
        RGB(245, 237, 218), // toolbar_bg
        RGB(28, 22, 14),    // text_quran
        RGB(20, 82, 56),    // text_header
        RGB(243, 230, 195), // selected_bg
        RGB(186, 142, 46),  // selected_border
        RGB(246, 238, 217), // surah_banner_bg
    };
}

inline int count_arabic_letters(const std::wstring& simple_txt) {
    int count = 0;
    for (wchar_t ch : simple_txt) {
        if ((ch >= 0x0621 && ch <= 0x064A) || ch == 0x0671) {
            ++count;
        }
    }
    return count;
}

inline bool handle_enter_on_focused_button(HWND owner, const MSG& msg) {
    if (msg.message != WM_KEYDOWN || msg.wParam != VK_RETURN) return false;
    HWND foc = GetFocus();
    if (!foc || (foc != owner && !IsChild(owner, foc))) return false;
    wchar_t cls[32] = {0};
    GetClassNameW(foc, cls, 31);
    if (_wcsicmp(cls, L"Button") == 0) {
        LONG btype = GetWindowLongW(foc, GWL_STYLE) & BS_TYPEMASK;
        if (btype == BS_PUSHBUTTON || btype == BS_DEFPUSHBUTTON) {
            SendMessageW(owner, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(foc), BN_CLICKED), (LPARAM)foc);
        } else {
            SendMessageW(foc, BM_CLICK, 0, 0);
        }
        return true;
    }
    return false;
}

inline bool is_valid_browse_scope(const std::wstring& s) {
    return s == L"surah" || s == L"page" || s == L"juz" || s == L"hizb" ||
           s == L"rub" || s == L"manzil" || s == L"ruku";
}

struct AppSettings {
    std::wstring ui_language = L"ar";       // ar | en
    std::wstring mushaf_edition = L"madinah"; // madinah أو مفتاح طبعة من assets\data\editions
    std::wstring theme = L"madinah_cream";
    std::wstring quran_font_family = L"KFGQPC HAFS Uthmanic Script";
    int quran_font_size = 26;
    std::wstring copy_script = L"uthmani";   // uthmani | qpc | imlaei | plain
    bool copy_brackets = true;
    bool copy_reference = true;
    bool copy_arabic_digits = true;
    bool copy_waqf_marks = true;     // نسخ علامات الوقف مع الآية
    std::wstring browse_scope = L"surah";    // surah | page | juz | hizb | rub | manzil | ruku
    std::wstring default_browse_mode = L"surah";
    std::wstring doc_alignment = L"center";  // center (كالمصحف، الافتراضي) | right
    std::wstring ayah_display = L"text_number"; // text | text_number | text_number_surah
    std::wstring startup_focus = L"surah_index"; // surah_index | ayahs | units
    bool show_sidebar = false;              // الواجهة النظيفة: «تصفح حسب» وقائمة الآيات فقط
    bool show_inline_copy_btn = false;
    bool show_mushaf_frame = true;   // الإطار المذهّب حول صفحة المصحف (للمبصر، ويُخفى في التباين العالي)
    bool mushaf_lines = true;   // تصفح الصفحات بسطور الطبعة نفسها إن كان لها ملف سطور (كالشمرلي)
    bool show_ayah_letter_count = false;
    int ui_layout_version = 4;
    bool page_turn_sound = true;
    bool sr_auto_speak_ayah = true;
    bool sr_phonetic_norm = true;
    bool sr_spell_muqattaat = false;
    bool sr_speak_waqf = false;        // نطق علامات الوقف بأسمائها (صلى، قلى، ج...) لقارئ الشاشة
    bool sr_speak_marks = true;        // نطق علامات الأجزاء والأحزاب والأرباع والسجدات مع الآية
    bool sapi_fallback = false;
    int last_surah = 1;
    int last_page = 1;
    int last_juz = 1;
    int last_hizb = 1;
    int last_rub = 1;
    int last_manzil = 1;
    int last_ruku = 1;
    int last_ayah = 1;
    // موضع النافذة وحجمها عند آخر إغلاق (0 = يحدده ويندوز)
    int window_x = 0;
    int window_y = 0;
    int window_w = 0;
    int window_h = 0;
    bool window_maximized = false;
    std::wstring khatmah_verse_key;
    std::vector<std::wstring> bookmarks;
    std::map<std::wstring, std::wstring> custom_shortcuts;

    static std::wstring settings_file_path() {
        wchar_t appdata[MAX_PATH] = {0};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
            std::wstring dir = std::wstring(appdata) + L"\\AlMushafAlSharif";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir + L"\\settings.json";
        }
        return L"settings.json";
    }

    void load() {
        std::string raw = read_file_utf8(settings_file_path());
        if (raw.empty()) {
            HKEY hk = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\AlMushafAlSharif", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
                wchar_t val[64] = {0};
                DWORD cb = sizeof(val);
                if (RegQueryValueExW(hk, L"InstallerLanguage", nullptr, nullptr, (LPBYTE)val, &cb) == ERROR_SUCCESS) {
                    if (std::wstring(val).find(L"english") != std::wstring::npos) ui_language = L"en";
                }
                RegCloseKey(hk);
            }
            return;
        }
        auto get_str = [&](const char* k, std::wstring& dst) {
            std::string key = std::string("\"") + k + "\"";
            size_t p = raw.find(key);
            if (p == std::string::npos) return;
            p = raw.find(':', p + key.size());
            if (p == std::string::npos) return;
            size_t q1 = raw.find('"', p + 1);
            if (q1 == std::string::npos) return;
            size_t q2 = raw.find('"', q1 + 1);
            if (q2 == std::string::npos) return;
            dst = to_wide(raw.substr(q1 + 1, q2 - q1 - 1));
        };
        auto get_int = [&](const char* k, int& dst) {
            std::string key = std::string("\"") + k + "\"";
            size_t p = raw.find(key);
            if (p == std::string::npos) return;
            p = raw.find(':', p + key.size());
            if (p == std::string::npos) return;
            while (p + 1 < raw.size() && (raw[p + 1] == ' ' || raw[p + 1] == '\t')) ++p;
            if (p + 1 < raw.size() && (std::isdigit((unsigned char)raw[p + 1]) || raw[p + 1] == '-')) {
                dst = std::atoi(raw.c_str() + p + 1);
            }
        };
        auto get_bool = [&](const char* k, bool& dst) {
            std::string key = std::string("\"") + k + "\"";
            size_t p = raw.find(key);
            if (p == std::string::npos) return;
            p = raw.find(':', p + key.size());
            if (p == std::string::npos) return;
            size_t t = raw.find("true", p);
            size_t f = raw.find("false", p);
            if (t != std::string::npos && (f == std::string::npos || t < f) && t - p < 12) dst = true;
            else if (f != std::string::npos && f - p < 12) dst = false;
        };

        get_str("ui_language", ui_language);
        if (ui_language != L"ar" && ui_language != L"en") ui_language = L"ar";
        // كانت طبعة الشمرلي تغيّر الخط تلقائياً إلى «Amiri Quran»؛ بعد إزالتها نعيد خط طبعة المدينة.
        std::wstring legacy_edition;
        get_str("mushaf_edition", legacy_edition);
        // الشمرلي أُزيلت؛ وأي طبعة غير موجودة تعود إلى المدينة عند تشغيل البرنامج
        mushaf_edition = (legacy_edition.empty() || legacy_edition == L"shamarly") ? L"madinah" : legacy_edition;
        get_str("theme", theme);
        get_str("quran_font_family", quran_font_family);
        if (legacy_edition == L"shamarly" && quran_font_family == L"Amiri Quran") {
            quran_font_family = L"KFGQPC HAFS Uthmanic Script";
        }
        get_int("quran_font_size", quran_font_size);
        get_str("copy_script", copy_script);
        // أُزيل النسخ بلا تشكيل: من اختاره سابقاً يُنسخ له الرسم الإملائي المشكول
        if (copy_script != L"uthmani" && copy_script != L"qpc" && copy_script != L"imlaei") copy_script = L"imlaei";
        get_bool("copy_brackets", copy_brackets);
        get_bool("copy_reference", copy_reference);
        get_bool("copy_arabic_digits", copy_arabic_digits);
        get_bool("copy_waqf_marks", copy_waqf_marks);
        get_str("browse_scope", browse_scope);
        if (!is_valid_browse_scope(browse_scope)) browse_scope = L"surah";
        default_browse_mode = browse_scope;
        get_str("default_browse_mode", default_browse_mode);
        if (!is_valid_browse_scope(default_browse_mode)) default_browse_mode = browse_scope;
        get_str("doc_alignment", doc_alignment);
        get_str("ayah_display", ayah_display);
        get_str("startup_focus", startup_focus);
        if (startup_focus != L"surah_index" && startup_focus != L"ayahs" && startup_focus != L"units") {
            startup_focus = L"surah_index";
        }
        get_bool("show_sidebar", show_sidebar);
        get_bool("show_inline_copy_btn", show_inline_copy_btn);
        get_bool("mushaf_lines", mushaf_lines);
        get_bool("show_mushaf_frame", show_mushaf_frame);
        get_bool("show_ayah_letter_count", show_ayah_letter_count);
        int layout_ver = 1;
        get_int("ui_layout_version", layout_ver);
        get_bool("page_turn_sound", page_turn_sound);
        get_bool("sr_auto_speak_ayah", sr_auto_speak_ayah);
        get_bool("sr_phonetic_norm", sr_phonetic_norm);
        get_bool("sr_spell_muqattaat", sr_spell_muqattaat);
        get_bool("sr_speak_waqf", sr_speak_waqf);
        get_bool("sr_speak_marks", sr_speak_marks);
        get_bool("sapi_fallback", sapi_fallback);
        get_int("last_surah", last_surah);
        get_int("last_page", last_page);
        get_int("last_juz", last_juz);
        get_int("last_hizb", last_hizb);
        get_int("last_rub", last_rub);
        get_int("last_manzil", last_manzil);
        get_int("last_ruku", last_ruku);
        get_int("last_ayah", last_ayah);
        get_int("window_x", window_x);
        get_int("window_y", window_y);
        get_int("window_w", window_w);
        get_int("window_h", window_h);
        get_bool("window_maximized", window_maximized);
        get_str("khatmah_verse_key", khatmah_verse_key);

        bookmarks.clear();
        size_t bm_pos = raw.find("\"bookmarks\"");
        if (bm_pos != std::string::npos) {
            size_t arr_start = raw.find('[', bm_pos);
            size_t arr_end = raw.find(']', arr_start);
            if (arr_start != std::string::npos && arr_end != std::string::npos) {
                size_t cur = arr_start;
                while ((cur = raw.find("\"verse_key\"", cur)) != std::string::npos && cur < arr_end) {
                    size_t col = raw.find(':', cur);
                    size_t q1 = raw.find('"', col + 1);
                    size_t q2 = raw.find('"', q1 + 1);
                    if (q1 < arr_end && q2 < arr_end) {
                        std::wstring vk = to_wide(raw.substr(q1 + 1, q2 - q1 - 1));
                        if (!vk.empty()) bookmarks.push_back(vk);
                    }
                    cur = q2 + 1;
                }
            }
        }
        custom_shortcuts.clear();
        size_t sc_pos = raw.find("\"custom_shortcuts\"");
        if (sc_pos != std::string::npos) {
            size_t obj_start = raw.find('{', sc_pos);
            size_t obj_end = raw.find('}', obj_start);
            if (obj_start != std::string::npos && obj_end != std::string::npos) {
                size_t cur = obj_start + 1;
                while (cur < obj_end) {
                    size_t k1 = raw.find('"', cur);
                    if (k1 == std::string::npos || k1 >= obj_end) break;
                    size_t k2 = raw.find('"', k1 + 1);
                    if (k2 == std::string::npos || k2 >= obj_end) break;
                    size_t col = raw.find(':', k2 + 1);
                    if (col == std::string::npos || col >= obj_end) break;
                    size_t v1 = raw.find('"', col + 1);
                    if (v1 == std::string::npos || v1 >= obj_end) break;
                    size_t v2 = raw.find('"', v1 + 1);
                    if (v2 == std::string::npos || v2 >= obj_end) break;
                    std::wstring k = to_wide(raw.substr(k1 + 1, k2 - k1 - 1));
                    std::wstring v = to_wide(raw.substr(v1 + 1, v2 - v1 - 1));
                    if (!k.empty() && !v.empty()) custom_shortcuts[k] = v;
                    cur = v2 + 1;
                }
            }
        }
        if (layout_ver < 4) {
            // الإصدار ٣ من الواجهة: إخفاء الفهرس الجانبي وزر النسخ المضمّن افتراضياً
            // (يبقى الفهرس متاحاً بـ F9 لمن يريده).
            show_inline_copy_btn = false;
            show_sidebar = false;
            doc_alignment = L"center";   // الإصدار ٤: توسيط السطور كالمصحف هو الافتراضي
            ui_layout_version = 4;
            save();
        } else {
            ui_layout_version = layout_ver;
        }
    }

    void save() const {
        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"ui_language\": \"" << json_escape(ui_language) << "\",\n";
        oss << "  \"mushaf_edition\": \"" << json_escape(mushaf_edition) << "\",\n";
        oss << "  \"theme\": \"" << json_escape(theme) << "\",\n";
        oss << "  \"quran_font_family\": \"" << json_escape(quran_font_family) << "\",\n";
        oss << "  \"quran_font_size\": " << quran_font_size << ",\n";
        oss << "  \"copy_script\": \"" << json_escape(copy_script) << "\",\n";
        oss << "  \"copy_brackets\": " << (copy_brackets ? "true" : "false") << ",\n";
        oss << "  \"copy_reference\": " << (copy_reference ? "true" : "false") << ",\n";
        oss << "  \"copy_arabic_digits\": " << (copy_arabic_digits ? "true" : "false") << ",\n";
        oss << "  \"copy_waqf_marks\": " << (copy_waqf_marks ? "true" : "false") << ",\n";
        oss << "  \"browse_scope\": \"" << json_escape(browse_scope) << "\",\n";
        oss << "  \"default_browse_mode\": \"" << json_escape(default_browse_mode) << "\",\n";
        oss << "  \"doc_alignment\": \"" << json_escape(doc_alignment) << "\",\n";
        oss << "  \"ayah_display\": \"" << json_escape(ayah_display) << "\",\n";
        oss << "  \"startup_focus\": \"" << json_escape(startup_focus) << "\",\n";
        oss << "  \"show_sidebar\": " << (show_sidebar ? "true" : "false") << ",\n";
        oss << "  \"show_inline_copy_btn\": " << (show_inline_copy_btn ? "true" : "false") << ",\n";
        oss << "  \"mushaf_lines\": " << (mushaf_lines ? "true" : "false") << ",\n";
        oss << "  \"show_mushaf_frame\": " << (show_mushaf_frame ? "true" : "false") << ",\n";
        oss << "  \"show_ayah_letter_count\": " << (show_ayah_letter_count ? "true" : "false") << ",\n";
        oss << "  \"ui_layout_version\": " << ui_layout_version << ",\n";
        oss << "  \"page_turn_sound\": " << (page_turn_sound ? "true" : "false") << ",\n";
        oss << "  \"sr_auto_speak_ayah\": " << (sr_auto_speak_ayah ? "true" : "false") << ",\n";
        oss << "  \"sr_phonetic_norm\": " << (sr_phonetic_norm ? "true" : "false") << ",\n";
        oss << "  \"sr_spell_muqattaat\": " << (sr_spell_muqattaat ? "true" : "false") << ",\n";
        oss << "  \"sr_speak_waqf\": " << (sr_speak_waqf ? "true" : "false") << ",\n";
        oss << "  \"sr_speak_marks\": " << (sr_speak_marks ? "true" : "false") << ",\n";
        oss << "  \"sapi_fallback\": " << (sapi_fallback ? "true" : "false") << ",\n";
        oss << "  \"last_surah\": " << last_surah << ",\n";
        oss << "  \"last_page\": " << last_page << ",\n";
        oss << "  \"last_juz\": " << last_juz << ",\n";
        oss << "  \"last_hizb\": " << last_hizb << ",\n";
        oss << "  \"last_rub\": " << last_rub << ",\n";
        oss << "  \"last_manzil\": " << last_manzil << ",\n";
        oss << "  \"last_ruku\": " << last_ruku << ",\n";
        oss << "  \"last_ayah\": " << last_ayah << ",\n";
        oss << "  \"window_x\": " << window_x << ",\n";
        oss << "  \"window_y\": " << window_y << ",\n";
        oss << "  \"window_w\": " << window_w << ",\n";
        oss << "  \"window_h\": " << window_h << ",\n";
        oss << "  \"window_maximized\": " << (window_maximized ? "true" : "false") << ",\n";
        oss << "  \"khatmah_verse_key\": \"" << json_escape(khatmah_verse_key) << "\",\n";
        oss << "  \"bookmarks\": [";
        for (size_t i = 0; i < bookmarks.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "{\"verse_key\": \"" << json_escape(bookmarks[i]) << "\"}";
        }
        oss << "],\n";
        oss << "  \"custom_shortcuts\": {";
        bool first_sc = true;
        for (const auto& kv : custom_shortcuts) {
            if (!first_sc) oss << ", ";
            first_sc = false;
            oss << "\"" << json_escape(kv.first) << "\": \"" << json_escape(kv.second) << "\"";
        }
        oss << "}\n}\n";
        write_file_utf8(settings_file_path(), oss.str());
    }
};

} // namespace mushaf
