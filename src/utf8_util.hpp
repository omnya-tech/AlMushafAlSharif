#pragma once
// -*- coding: utf-8 -*-
// utf8_util.hpp — أدوات النصوص العربية (UTF-8 / UTF-16) وتطبيع البحث وقراءة/كتابة JSON.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS   // _wfopen و freopen مستخدمة عمداً ومتوافقة مع كل المترجمات
#endif
#ifndef NOMINMAX
#define NOMINMAX                  // نستخدم std::min و std::max، فلا نريد ماكرو windows.h
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mushaf {

inline std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &out[0], len);
    return out;
}

inline std::string to_utf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), &out[0], len, nullptr, nullptr);
    return out;
}

// علامة القراءة من اليمين لـ DrawText بحسب اتجاه سطح الرسم:
// في السطح المعكوس (النوافذ العربية) يكون الاتجاه من اليمين أصلاً، وإضافة DT_RTLREADING تقلبه لليسار.
inline UINT dt_reading_flag(HDC hdc, bool text_is_rtl) {
    bool mirrored = (GetLayout(hdc) & LAYOUT_RTL) != 0;
    return (text_is_rtl != mirrored) ? DT_RTLREADING : 0;
}

inline std::wstring to_arabic_digits(const std::wstring& s) {
    std::wstring out = s;
    for (wchar_t& c : out) {
        if (c >= L'0' && c <= L'9') {
            c = static_cast<wchar_t>(0x0660 + (c - L'0'));
        }
    }
    return out;
}

inline std::wstring to_arabic_digits(int val) {
    return to_arabic_digits(std::to_wstring(val));
}

// العدد مع معدوده على قواعد العربية: «حرف واحد»، «حرفان»، «٥ أحرف»، «١٥ حرفًا»، «١٠٠ حرف»
inline std::wstring count_ar(int n, const std::wstring& one, const std::wstring& two,
                             const std::wstring& plural, const std::wstring& acc, const std::wstring& sing) {
    if (n == 1) return one;
    if (n == 2) return two;
    const std::wstring num = to_arabic_digits(n);
    const int r = n % 100;
    if (r >= 3 && r <= 10) return num + L" " + plural;
    if (r >= 11) return num + L" " + acc;
    return num + L" " + sing;
}
inline std::wstring letters_ar(int n) { return count_ar(n, L"حرف واحد", L"حرفان", L"أحرف", L"حرفًا", L"حرف"); }
inline std::wstring words_ar(int n)   { return count_ar(n, L"كلمة واحدة", L"كلمتان", L"كلمات", L"كلمةً", L"كلمة"); }
inline std::wstring times_ar(int n)   { return count_ar(n, L"مرة واحدة", L"مرتان", L"مرات", L"مرةً", L"مرة"); }
inline std::wstring ayahs_ar(int n)   { return count_ar(n, L"آية واحدة", L"آيتان", L"آيات", L"آيةً", L"آية"); }
inline std::wstring rukus_ar(int n)   { return count_ar(n, L"ركوع واحد", L"ركوعان", L"ركوعات", L"ركوعًا", L"ركوع"); }

inline std::wstring to_western_digits(const std::wstring& s) {
    std::wstring out = s;
    for (wchar_t& c : out) {
        if (c >= 0x0660 && c <= 0x0669) {
            c = static_cast<wchar_t>(L'0' + (c - 0x0660));
        }
    }
    return out;
}

inline std::wstring trim_wide(const std::wstring& s) {
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b])) ++b;
    while (e > b && iswspace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

inline void replace_all(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::wstring::npos) {
        str.replace(pos, from.size(), to);
        pos += to.size();
    }
}

inline std::vector<std::wstring> split_words(const std::wstring& s) {
    std::vector<std::wstring> out;
    std::wistringstream iss(s);
    std::wstring w;
    while (iss >> w) out.push_back(w);
    return out;
}

inline std::wstring join_words(const std::vector<std::wstring>& words, const std::wstring& sep = L" ") {
    std::wstring out;
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) out += sep;
        out += words[i];
    }
    return out;
}

inline bool is_quran_diacritic_or_mark(wchar_t c) {
    return (c >= 0x0610 && c <= 0x061A) ||
           (c >= 0x064B && c <= 0x065F) ||
           c == 0x0670 ||
           (c >= 0x06D6 && c <= 0x06ED) ||
           c == 0x0640;
}

inline std::wstring normalize_arabic_search(const std::wstring& text) {
    std::wstring w = to_western_digits(text);
    std::wstring out;
    out.reserve(w.size());
    bool last_space = true;
    for (wchar_t c : w) {
        if (is_quran_diacritic_or_mark(c)) continue;
        if (c == L'أ' || c == L'إ' || c == L'آ' || c == L'ٱ') c = L'ا';
        else if (c == L'ى') c = L'ي';
        else if (c == L'ؤ') c = L'و';
        else if (c == L'ئ') c = L'ي';
        else if (c == L'ة') c = L'ه';
        else if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');

        if (iswspace(c)) {
            if (!last_space) {
                out.push_back(L' ');
                last_space = true;
            }
        } else {
            out.push_back(c);
            last_space = false;
        }
    }
    if (!out.empty() && out.back() == L' ') out.pop_back();
    return out;
}

inline std::string read_file_utf8(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return "";
    }
    std::string buf((size_t)sz, '\0');
    fread(&buf[0], 1, (size_t)sz, f);
    fclose(f);
    return buf;
}

inline bool write_file_utf8(const std::wstring& path, const std::string& content) {
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

inline std::string json_escape(const std::wstring& ws) {
    std::string s = to_utf8(ws);
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back((char)c);
    }
    return out;
}

} // namespace mushaf
