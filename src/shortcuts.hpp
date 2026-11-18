#pragma once
// -*- coding: utf-8 -*-
// shortcuts.hpp — تعريف وتخصيص اختصارات لوحة المفاتيح المتوافقة مع قارئات الشاشة.

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace mushaf {

struct ShortcutDef {
    std::wstring key;
    UINT cmd_id;
    std::wstring label_ar;
    std::wstring label_en;
    std::wstring default_spec;
};

inline const std::vector<ShortcutDef>& shortcut_defs() {
    static const std::vector<ShortcutDef> defs = {
        {L"browse_surah",    1001, L"التصفح بالسور",               L"Browse by Surahs",            L"Alt+1"},
        {L"browse_page",     1002, L"التصفح بالصفحات",             L"Browse by Pages",             L"Alt+2"},
        {L"browse_juz",      1003, L"التصفح بالأجزاء",             L"Browse by Juz",               L"Alt+3"},
        {L"browse_rub",      1004, L"التصفح بالأرباع",             L"Browse by Quarters",    L"Alt+4"},
        {L"browse_hizb",     1005, L"التصفح بالأحزاب",             L"Browse by Hizb",              L"Alt+5"},
        {L"browse_manzil",   1006, L"التصفح بالمنازل السبعة",      L"Browse by Manzil",            L"Alt+6"},
        {L"browse_ruku",     1007, L"التصفح بالركوعات",            L"Browse by Ruku",              L"Alt+7"},
        {L"goto",            1020, L"الانتقال السريع",             L"Go To",      L"Ctrl+G"},
        {L"search",          1021, L"البحث في القرآن",             L"Search Quran",                L"Ctrl+F"},
        {L"sajdah_list",     1030, L"مواضع سجدات التلاوة",         L"Sajdah Places",     L"Ctrl+Shift+S"},
        {L"history_back",    1031, L"الرجوع إلى الموضع السابق",        L"Back to Previous Position",   L"Alt+Left"},
        {L"history_forward", 1032, L"التقدم إلى الموضع التالي",        L"Forward to Next Position",    L"Alt+Right"},
        {L"focus_ayahs",     1022, L"التركيز على قائمة الآيات",    L"Focus Ayah List",            L"Ctrl+D"},
        {L"show_units",      1023, L"عرض قائمة الوحدات",           L"Show Index List",             L"Ctrl+L"},
        {L"cycle_landmarks", 1025, L"التنقل بين عناصر النافذة",    L"Move Between Window Areas",      L"F6"},
        {L"bookmarks",       1113, L"المفضلة والعلامات المرجعية",  L"Bookmarks and Reading Mark",           L"Ctrl+B"},
        {L"jump_khatmah",    1112, L"الانتقال إلى فاصل الختمة",       L"Go to Reading Mark",    L"Alt+M"},
        {L"ayah_info",       1104, L"تفاصيل وإحصائيات الآية",      L"Ayah Information",      L"Ctrl+I"},
        {L"surah_info",      1105, L"تفاصيل وإحصائيات السورة",     L"Surah Information",     L"Ctrl+Shift+I"},
        {L"multi_copy",      1115, L"النسخ المتعدد",               L"Multiple Copy",               L"Ctrl+Alt+C"},
        {L"copy_all",        1103, L"نسخ الوحدة الحالية كاملة",    L"Copy Current Unit",    L"Ctrl+Shift+A"},
        {L"options",         1200, L"نافذة الخيارات",              L"Options",              L"Ctrl+O"},
        {L"cycle_theme",     1234, L"تبديل المظهر اللوني",         L"Switch Color Theme",           L"Ctrl+T"},
        {L"font_inc",        1210, L"تكبير الخط",                  L"Increase Font Size",          L"Ctrl+="},
        {L"font_dec",        1211, L"تصغير الخط",                  L"Decrease Font Size",          L"Ctrl+-"},
        {L"font_reset",      1212, L"إعادة الخط إلى حجمه الأصلي",          L"Reset Font Size",             L"Ctrl+0"},
        {L"toggle_sidebar",  1213, L"إظهار الفهرس الجانبي أو إخفاؤه",  L"Show or Hide Side Index",           L"F9"},
        {L"focus_mode",      1214, L"وضع القراءة التركيزي (ملء الشاشة)", L"Full Screen",    L"F11"},
        {L"shortcuts_help",  1301, L"دليل اختصارات لوحة المفاتيح", L"Keyboard Shortcuts Help",     L"F1"},
        {L"user_guide",      1303, L"دليل استخدام البرنامج",       L"User Guide",                  L"Shift+F1"},
        {L"focus_index",     1024, L"التركيز على الفهرس الجانبي",  L"Focus Side Index",            L"Ctrl+Shift+L"},
        {L"cycle_edition",   1242, L"تبديل طبعة المصحف",           L"Switch Mushaf Print",         L"Ctrl+E"},
        {L"copy_surah",      1108, L"نسخ السورة كاملة",            L"Copy Whole Surah",            L"Ctrl+Shift+C"},
        {L"cycle_display",   1243, L"تبديل طريقة عرض الآية",       L"Switch Ayah Display",         L"Ctrl+Shift+N"},
        {L"toggle_align",    1244, L"تبديل محاذاة السطور",         L"Switch Line Alignment",       L"Ctrl+J"},
        {L"mushaf_frame",    1226, L"إظهار إطار المصحف أو إخفاؤه",     L"Show or Hide Mushaf Frame",      L"Ctrl+Shift+F"},
        {L"page_sound",      1241, L"تشغيل صوت تقليب الصفحة أو إيقافه", L"Page-Turn Sound",     L"Ctrl+Shift+P"},
        {L"sr_waqf",         1245, L"نطق علامات الوقف",            L"Speak Pause Marks",           L"Ctrl+Shift+W"},
        {L"muqattaat",       1240, L"تفكيك الحروف المقطعة",        L"Spell Disjointed Letters",    L"Ctrl+Shift+M"},
    };
    return defs;
}

inline std::wstring to_upper_ascii(std::wstring s) {
    for (wchar_t& ch : s) {
        if (ch >= L'a' && ch <= L'z') ch = (wchar_t)(ch - L'a' + L'A');
    }
    return s;
}

inline std::wstring key_name_from_vk(WORD vk) {
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
    if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
    if (vk >= VK_F1 && vk <= VK_F12) return L"F" + std::to_wstring((int)(vk - VK_F1 + 1));
    switch (vk) {
        case VK_LEFT:      return L"Left";
        case VK_RIGHT:     return L"Right";
        case VK_UP:        return L"Up";
        case VK_DOWN:      return L"Down";
        case VK_HOME:      return L"Home";
        case VK_END:       return L"End";
        case VK_PRIOR:     return L"PgUp";
        case VK_NEXT:      return L"PgDn";
        case VK_OEM_PLUS:  return L"=";
        case VK_OEM_MINUS: return L"-";
        case VK_OEM_4:     return L"[";
        case VK_OEM_6:     return L"]";
        case VK_OEM_1:     return L";";
        case VK_OEM_7:     return L"'";
        case VK_OEM_COMMA: return L",";
        case VK_OEM_PERIOD:return L".";
        case VK_OEM_2:     return L"/";
        default:           return L"";
    }
}

inline WORD vk_from_key_name(const std::wstring& name) {
    std::wstring up = to_upper_ascii(name);
    if (up.size() == 1) {
        wchar_t ch = up[0];
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) return (WORD)ch;
        if (ch == L'=' || ch == L'+') return VK_OEM_PLUS;
        if (ch == L'-') return VK_OEM_MINUS;
        if (ch == L'[') return VK_OEM_4;
        if (ch == L']') return VK_OEM_6;
        if (ch == L';') return VK_OEM_1;
        if (ch == L'\'') return VK_OEM_7;
        if (ch == L',') return VK_OEM_COMMA;
        if (ch == L'.') return VK_OEM_PERIOD;
        if (ch == L'/') return VK_OEM_2;
    }
    if (up.size() >= 2 && up[0] == L'F') {
        int fn = _wtoi(up.c_str() + 1);
        if (fn >= 1 && fn <= 12) return (WORD)(VK_F1 + fn - 1);
    }
    if (up == L"LEFT") return VK_LEFT;
    if (up == L"RIGHT") return VK_RIGHT;
    if (up == L"UP") return VK_UP;
    if (up == L"DOWN") return VK_DOWN;
    if (up == L"HOME") return VK_HOME;
    if (up == L"END") return VK_END;
    if (up == L"PGUP" || up == L"PAGEUP") return VK_PRIOR;
    if (up == L"PGDN" || up == L"PAGEDOWN") return VK_NEXT;
    return 0;
}

inline std::wstring format_shortcut_spec(BYTE fVirt, WORD vk) {
    std::wstring kn = key_name_from_vk(vk);
    if (kn.empty()) return L"";
    std::wstring out;
    if (fVirt & FCONTROL) out += L"Ctrl+";
    if (fVirt & FALT)     out += L"Alt+";
    if (fVirt & FSHIFT)   out += L"Shift+";
    out += kn;
    return out;
}

inline bool parse_shortcut_spec(const std::wstring& spec, ACCEL& out, UINT cmd_id) {
    if (spec.empty()) return false;
    BYTE fVirt = FVIRTKEY;
    std::wstring key_part;
    size_t start = 0;
    while (start <= spec.size()) {
        size_t plus = spec.find(L'+', start);
        std::wstring tok = (plus == std::wstring::npos) ? spec.substr(start) : spec.substr(start, plus - start);
        std::wstring up = to_upper_ascii(tok);
        if (up == L"CTRL" || up == L"CONTROL") fVirt |= FCONTROL;
        else if (up == L"ALT") fVirt |= FALT;
        else if (up == L"SHIFT") fVirt |= FSHIFT;
        else if (!tok.empty()) key_part = tok;
        if (plus == std::wstring::npos) break;
        start = plus + 1;
    }
    WORD vk = vk_from_key_name(key_part);
    if (vk == 0) return false;
    out.fVirt = fVirt;
    out.key = vk;
    out.cmd = (WORD)cmd_id;
    return true;
}

inline bool is_screen_reader_safe_shortcut(BYTE fVirt, WORD vk, std::wstring& reason_ar, std::wstring& reason_en) {
    if (vk == 0 || key_name_from_vk(vk).empty()) {
        reason_ar = L"هذا المفتاح غير مدعوم كاختصار.";
        reason_en = L"This key is not supported as a shortcut.";
        return false;
    }
    if (vk == VK_INSERT || vk == VK_CAPITAL) {
        reason_ar = L"مفتاحا Insert و CapsLock محجوزان لقارئات الشاشة.";
        reason_en = L"Insert and CapsLock are reserved for screen readers.";
        return false;
    }
    bool has_ctrl = (fVirt & FCONTROL) != 0;
    bool has_alt = (fVirt & FALT) != 0;
    bool has_shift = (fVirt & FSHIFT) != 0;
    bool is_fn = (vk >= VK_F1 && vk <= VK_F12);

    if (!has_ctrl && !has_alt && !is_fn) {
        reason_ar = L"لتجنب التعارض مع قارئ الشاشة، يجب استخدام Ctrl أو Alt أو مفاتيح F1–F12.";
        reason_en = L"To avoid screen-reader conflicts, use Ctrl, Alt, or F1–F12.";
        return false;
    }
    if (has_alt && has_shift && !has_ctrl) {
        reason_ar = L"اختصار Alt+Shift محجوز لتبديل لغة لوحة المفاتيح في ويندوز.";
        reason_en = L"Alt+Shift is reserved for Windows keyboard language switching.";
        return false;
    }
    if (has_alt && !has_ctrl && vk == VK_F4) {
        reason_ar = L"اختصار Alt+F4 محجوز لإغلاق النافذة.";
        reason_en = L"Alt+F4 is reserved for closing the window.";
        return false;
    }
    if (has_ctrl && !has_alt && !has_shift && vk == 'C') {
        reason_ar = L"اختصار Ctrl+C محجوز لنسخ الآية المحددة.";
        reason_en = L"Ctrl+C is reserved for copying the focused ayah.";
        return false;
    }
    return true;
}

inline std::wstring effective_shortcut_spec(
    const std::map<std::wstring, std::wstring>& custom,
    const ShortcutDef& def
) {
    auto it = custom.find(def.key);
    if (it != custom.end() && !it->second.empty()) {
        ACCEL tmp{};
        std::wstring rar, ren;
        if (parse_shortcut_spec(it->second, tmp, def.cmd_id) &&
            is_screen_reader_safe_shortcut(tmp.fVirt, tmp.key, rar, ren)) {
            return format_shortcut_spec(tmp.fVirt, tmp.key);
        }
    }
    return def.default_spec;
}

inline std::wstring shortcut_for_cmd(const std::map<std::wstring, std::wstring>& custom, UINT cmd_id) {
    for (const auto& d : shortcut_defs()) {
        if (d.cmd_id == cmd_id) {
            return effective_shortcut_spec(custom, d);
        }
    }
    return L"";
}

} // namespace mushaf
