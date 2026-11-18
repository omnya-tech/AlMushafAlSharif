#pragma once
// -*- coding: utf-8 -*-
// sajdah_and_formatter.hpp — مواضع السجدات عند الأئمة الأربعة وتنسيق الآيات والنطق الصوتي لقارئات الشاشة.

#include "database.hpp"

#include <map>
#include <string>
#include <vector>

namespace mushaf {

struct SajdahPlace {
    std::wstring verse_key;
    int surah = 0;
    int ayah = 0;
    bool hanafi = false;
    bool maliki = false;
    bool shafii = false;
    bool hanbali = false;
    std::wstring note_ar;
    std::wstring note_en;

    bool is_agreed() const {
        return hanafi && maliki && shafii && hanbali;
    }
};

inline const std::vector<SajdahPlace>& all_sajdah_places() {
    static const std::vector<SajdahPlace> places = {
        {L"7:206",  7, 206, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة (خاتمة سورة الأعراف)", L"Agreed upon by the four imams (end of Al-A'raf)"},
        {L"13:15", 13,  15, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة", L"Agreed upon by the four imams"},
        {L"16:50", 16,  50, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة (وموضع السجود آخر الآية ٥٠)", L"Agreed upon by the four imams (prostration is at the end of ayah 50)"},
        {L"17:109",17, 109, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة", L"Agreed upon by the four imams"},
        {L"19:58", 19,  58, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة", L"Agreed upon by the four imams"},
        {L"22:18", 22,  18, true,  true,  true,  true,  L"السجدة الأولى من سورة الحج، متفق عليها بين الأئمة الأربعة", L"First prostration in Al-Hajj, agreed upon by the four imams"},
        {L"22:77", 22,  77, false, false, true,  true,  L"السجدة الثانية من سورة الحج: عزيمة عند الشافعي وأحمد، وليست سجدة تلاوة عند أبي حنيفة ومالك", L"Second prostration in Al-Hajj: observed by Ash-Shafi'i and Ahmad, not by Abu Hanifah and Malik"},
        {L"25:60", 25,  60, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة", L"Agreed upon by the four imams"},
        {L"27:26", 27,  26, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة", L"Agreed upon by the four imams"},
        {L"32:15", 32,  15, true,  true,  true,  true,  L"متفق عليها بين الأئمة الأربعة", L"Agreed upon by the four imams"},
        {L"38:24", 38,  24, true,  true,  false, false, L"سورة ص: سجدة تلاوة عند أبي حنيفة ومالك، وسجدة شكر لا تلاوة عند الشافعي وأحمد", L"Surah Sad: prostration of recitation for Abu Hanifah and Malik; prostration of gratitude for Ash-Shafi'i and Ahmad"},
        {L"41:38", 41,  38, true,  true,  true,  true,  L"متفق على أصل السجدة؛ وعند مالك الموضع آخر الآية ٣٧، وعند الجمهور آخر الآية ٣٨", L"Agreed upon; Malik places it at the end of ayah 37, and the majority at the end of ayah 38"},
        {L"53:62", 53,  62, true,  false, true,  true,  L"من مفصّل السور: سجدة عند أبي حنيفة والشافعي وأحمد، ولا سجود في المفصّل عند مالك على المشهور", L"In the Mufassal: observed by Abu Hanifah, Ash-Shafi'i and Ahmad; not by Malik"},
        {L"84:21", 84,  21, true,  false, true,  true,  L"من مفصّل السور: سجدة عند أبي حنيفة والشافعي وأحمد، ولا سجود فيها عند مالك", L"In the Mufassal: observed by Abu Hanifah, Ash-Shafi'i and Ahmad; not by Malik"},
        {L"96:19", 96,  19, true,  false, true,  true,  L"من مفصّل السور: سجدة عند أبي حنيفة والشافعي وأحمد، ولا سجود فيها عند مالك", L"In the Mufassal: observed by Abu Hanifah, Ash-Shafi'i and Ahmad; not by Malik"},
    };
    return places;
}

inline const SajdahPlace* find_sajdah_place(const std::wstring& verse_key) {
    if (verse_key == L"41:37") {
        for (const auto& p : all_sajdah_places()) {
            if (p.verse_key == L"41:38") return &p;
        }
    }
    for (const auto& p : all_sajdah_places()) {
        if (p.verse_key == verse_key) return &p;
    }
    return nullptr;
}

// موضع السجدة كما في الطبعة المختارة: طبعتا المدينة فيهما سجدة فصلت عند الآية ٣٨ وحدها
inline const SajdahPlace* print_sajdah_place(const std::wstring& verse_key, bool edition_details) {
    if (!edition_details && verse_key == L"41:37") return nullptr;
    return find_sajdah_place(verse_key);
}

// مواضع «سكتة لطيفة» المكتوبة في حواشي مصحف المدينة
struct SaktaPlace {
    const wchar_t* verse_key;
    const wchar_t* word;       // الكلمة التي عليها السكتة، بلا تشكيل
    const wchar_t* where_ar;
    const wchar_t* where_en;
};

inline const SaktaPlace* find_sakta_place(const std::wstring& verse_key) {
    static const SaktaPlace places[] = {
        {L"18:1",  L"عوجا",   L"على ألف عوجا",  L"on the alif of 'iwaja"},
        {L"36:52", L"مرقدنا", L"على الألف",     L"on the alif"},
        {L"69:28", L"ماليه",  L"على هاء ماليه", L"on the ha of maliyah"},
        {L"75:27", L"من",     L"على النون",     L"on the nun"},
        {L"83:14", L"بل",     L"على اللام",     L"on the lam"},
    };
    for (const auto& p : places) {
        if (verse_key == p.verse_key) return &p;
    }
    return nullptr;
}

// حروف الكلمة وحدها، ليطابق الرسمُ العثماني الكلمةَ المجردة أيًّا كان خط العرض
inline std::wstring bare_arabic_word(const std::wstring& word) {
    std::wstring out;
    for (wchar_t ch : word) {
        if (ch == 0x0671 || ch == 0x0622 || ch == 0x0623 || ch == 0x0625) out.push_back(L'\u0627');
        else if (ch == 0x0649) out.push_back(L'\u064A');
        else if (ch == 0x0629) out.push_back(L'\u0647');
        else if ((ch >= 0x0621 && ch <= 0x063A) || (ch >= 0x0641 && ch <= 0x064A)) out.push_back(ch);
    }
    return out;
}

inline bool line_has_arabic_word(const std::wstring& line, const std::wstring& word) {
    size_t i = 0;
    while (i <= line.size()) {
        size_t j = line.find(L' ', i);
        if (j == std::wstring::npos) j = line.size();
        if (bare_arabic_word(line.substr(i, j - i)) == word) return true;
        i = j + 1;
    }
    return false;
}

inline std::wstring sakta_spoken_label(const SaktaPlace& p, bool en = false) {
    return en ? (std::wstring(L"slight pause ") + p.where_en) : (std::wstring(L"سكتة لطيفة ") + p.where_ar);
}

inline std::wstring sajdah_short_label(const std::wstring& verse_key, bool en = false) {
    if (verse_key == L"41:37") {
        return en ? L"prostration (Maliki school)" : L"موضع سجدة تلاوة عند المالكية";
    }
    const SajdahPlace* p = find_sajdah_place(verse_key);
    if (!p) return L"";
    if (verse_key == L"41:38") {
        return en ? L"prostration (Hanafi, Shafi'i and Hanbali; for Malik at ayah 37)"
                  : L"سجدة عند الحنفية والشافعية والحنابلة (وعند المالكية عند الآية ٣٧)";
    }
    if (p->is_agreed()) {
        return en ? L"prostration (agreed upon by the four schools)"
                  : L"سجدة تلاوة (متفق عليها عند الأئمة الأربعة)";
    }
    std::vector<std::wstring> names;
    if (p->hanafi)  names.push_back(en ? L"Hanafi"  : L"الحنفية");
    if (p->maliki)  names.push_back(en ? L"Maliki"  : L"المالكية");
    if (p->shafii)  names.push_back(en ? L"Shafi'i" : L"الشافعية");
    if (p->hanbali) names.push_back(en ? L"Hanbali" : L"الحنابلة");
    if (en) {
        std::wstring joined;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) joined += (i + 1 == names.size()) ? L" and " : L", ";
            joined += names[i];
        }
        return L"prostration (" + joined + L")";
    } else {
        std::wstring joined;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) joined += L" و";
            joined += names[i];
        }
        return L"سجدة عند " + joined;
    }
}

inline std::wstring clean_muqattaat_word(const std::wstring& w) {
    std::wstring out;
    for (wchar_t c : w) {
        if ((c >= 0x064B && c <= 0x065F) || c == 0x0670 || c == 0x0640 ||
            c == L'،' || c == L'.' || c == L'؟' || c == L'!') {
            continue;
        }
        out.push_back(c);
    }
    return out;
}

// اسم علامة الوقف كما تُكتب في المصحف، يُنطق بين قوسين حتى لا تلتبس «لا» العلامة بـ«لا» الكلمة
inline const wchar_t* waqf_mark_name(wchar_t c) {
    switch (c) {
    case 0x06D6: return L"صلى";
    case 0x06D7: return L"قلى";
    case 0x06DA: return L"ج";
    case 0x06D8: return L"م";
    case 0x06D9: return L"لا";
    case 0x06DB: return L"تعانق";
    case 0x06DC: return L"س";
    default: return nullptr;
    }
}

// علامات الوقف المنفصلة («رَيْبَ ۛ فِيهِ») تُستبدل بأسمائها؛ والسين الصغيرة داخل الكلمة (تنبيه على النطق) تُحذف
inline std::wstring speak_waqf_marks(const std::wstring& text) {
    std::wstring out;
    const size_t n = text.size();
    for (size_t i = 0; i < n; ++i) {
        wchar_t c = text[i];
        const wchar_t* name = waqf_mark_name(c);
        if (!name) { out.push_back(c); continue; }
        bool alone = (i == 0 || text[i - 1] == L' ') && (i + 1 >= n || text[i + 1] == L' ');
        if (alone) { out += L"("; out += name; out += L")"; }
    }
    return out;
}

inline std::wstring prepare_ayah_for_screen_reader(
    const AyahInfo& ayah,
    bool phonetic_norm = true,
    bool spell_muqattaat = false,
    bool speak_waqf = false
) {
    if (!phonetic_norm) return speak_waqf ? speak_waqf_marks(ayah.text_uthmani) : ayah.text_uthmani;

    static const std::map<std::wstring, std::wstring> muqattaat = {
        {L"الم", L"أَلِفْ لَامْ مِيمْ"},
        {L"المص", L"أَلِفْ لَامْ مِيمْ صَادْ"},
        {L"الر", L"أَلِفْ لَامْ رَا"},
        {L"المر", L"أَلِفْ لَامْ مِيمْ رَا"},
        {L"كهيعص", L"كَافْ هَا يَا عَيْنْ صَادْ"},
        {L"طه", L"طَا هَا"},
        {L"طسم", L"طَا سِينْ مِيمْ"},
        {L"طس", L"طَا سِينْ"},
        {L"يس", L"يَا سِينْ"},
        {L"ص", L"صَادْ"},
        {L"حم", L"حَا مِيمْ"},
        {L"عسق", L"عَيْنْ سِينْ قَافْ"},
        {L"ق", L"قَافْ"},
        {L"ن", L"نُونْ"},
    };

    std::wstring text = speak_waqf ? speak_waqf_marks(ayah.text_imlaei) : ayah.text_imlaei;
    for (wchar_t& c : text) {
        if ((c >= 0x06D6 && c <= 0x06ED) || c == 0x06DD || c == 0x06DE) c = L' ';
    }
    replace_all(text, L"الرَّحْمَٰنِ", L"الرَّحْمَنِ");
    replace_all(text, L"الْعَٰلَمِينَ", L"الْعَالَمِينَ");
    replace_all(text, L"ذَٰلِكَ", L"ذَلِكَ");
    replace_all(text, L"هَٰذَا", L"هَذَا");
    replace_all(text, L"هَٰذِهِ", L"هَذِهِ");
    replace_all(text, L"هَٰؤُلَاءِ", L"هَؤُلَاءِ");
    replace_all(text, L"لَٰكِن", L"لَكِن");
    replace_all(text, L"إِلَٰه", L"إِلَه");
    replace_all(text, L"\x0670", L"");

    auto words = split_words(text);
    if (spell_muqattaat && (ayah.ayah_number == 1 || ayah.ayah_number == 2)) {
        for (auto& w : words) {
            std::wstring cw = clean_muqattaat_word(w);
            auto it = muqattaat.find(cw);
            if (it != muqattaat.end()) w = it->second;
        }
    }
    return join_words(words, L" ");
}

inline std::wstring division_badge_text(const AyahInfo& ayah, bool en = false) {
    int rub = ayah.rub_el_hizb_number;
    if (rub < 2) return L"";
    int hizb = ayah.hizb_number;
    int juz = ayah.juz_number;
    int pos = (rub - 1) % 4;
    if ((rub - 1) % 8 == 0) {
        if (!en) return L"[۞ الجزء " + to_arabic_digits(juz) + L" • الحزب " + to_arabic_digits(hizb) + L"]";
        return L"[۞ Juz " + std::to_wstring(juz) + L" • Hizb " + std::to_wstring(hizb) + L"]";
    }
    if (pos == 0) {
        if (!en) return L"[۞ الحزب " + to_arabic_digits(hizb) + L" • الجزء " + to_arabic_digits(juz) + L"]";
        return L"[۞ Hizb " + std::to_wstring(hizb) + L" • Juz " + std::to_wstring(juz) + L"]";
    }
    if (pos == 1) {
        if (!en) return L"[۞ ربع الحزب " + to_arabic_digits(hizb) + L"]";
        return L"[۞ Quarter of hizb " + std::to_wstring(hizb) + L"]";
    }
    if (pos == 2) {
        if (!en) return L"[۞ نصف الحزب " + to_arabic_digits(hizb) + L"]";
        return L"[۞ Half of hizb " + std::to_wstring(hizb) + L"]";
    }
    if (!en) return L"[۞ ثلاثة أرباع الحزب " + to_arabic_digits(hizb) + L"]";
    return L"[۞ Three-quarters of hizb " + std::to_wstring(hizb) + L"]";
}

inline std::wstring division_spoken_text(const AyahInfo& ayah, bool en = false) {
    int rub = ayah.rub_el_hizb_number;
    if (rub < 2) return L"";
    int hizb = ayah.hizb_number;
    int juz = ayah.juz_number;
    int pos = (rub - 1) % 4;
    if ((rub - 1) % 8 == 0) {
        return en ? (L"start of juz " + std::to_wstring(juz) + L", hizb " + std::to_wstring(hizb))
                  : (L"بداية الجزء " + std::to_wstring(juz) + L"، الحزب " + std::to_wstring(hizb));
    }
    if (pos == 0) {
        return en ? (L"start of hizb " + std::to_wstring(hizb) + L", juz " + std::to_wstring(juz))
                  : (L"بداية الحزب " + std::to_wstring(hizb) + L" من الجزء " + std::to_wstring(juz));
    }
    if (pos == 1) {
        return en ? (L"quarter of hizb " + std::to_wstring(hizb)) : (L"ربع الحزب " + std::to_wstring(hizb));
    }
    if (pos == 2) {
        return en ? (L"half of hizb " + std::to_wstring(hizb)) : (L"نصف الحزب " + std::to_wstring(hizb));
    }
    return en ? (L"three-quarters of hizb " + std::to_wstring(hizb)) : (L"ثلاثة أرباع الحزب " + std::to_wstring(hizb));
}

inline std::wstring build_ayah_accessible_name(
    const AyahInfo& ayah,
    bool phonetic_norm,
    bool spell_muqattaat,
    const std::wstring& display_mode,
    bool is_rub_start,
    const std::wstring& bookmark_spoken,
    bool en = false,
    int letter_count = 0,
    bool speak_waqf = false,
    bool speak_marks = true,  // نطق علامات الأجزاء والأحزاب والأرباع والسجدات والسكتات مع الآية
    bool edition_details = true   // أقوال المذاهب في السجدة تُذكر في طبعة دار المعارف وحدها
) {
    std::wstring spoken = prepare_ayah_for_screen_reader(ayah, phonetic_norm, spell_muqattaat, speak_waqf);
    std::wstring tail;
    if (display_mode == L"text") {
        tail = L"";
    } else if (display_mode == L"text_number") {
        tail = en ? (L"Ayah " + std::to_wstring(ayah.ayah_number))
                  : (L"الآية " + std::to_wstring(ayah.ayah_number));
    } else {
        tail = en ? (L"Ayah " + std::to_wstring(ayah.ayah_number) + L" of Surah " + ayah.surah_name)
                  : (L"الآية " + std::to_wstring(ayah.ayah_number) + L" من سورة " + ayah.surah_name);
    }

    std::wstring sajdah;
    if (speak_marks) {
        if (edition_details) sajdah = sajdah_short_label(ayah.verse_key, en);
        else if (print_sajdah_place(ayah.verse_key, false)) sajdah = en ? L"prostration of recitation" : L"سجدة تلاوة";
    }
    std::wstring div = (speak_marks && is_rub_start) ? division_spoken_text(ayah, en) : L"";
    // إذا كانت علامات الوقف تُنطق فالسكتة مذكورة في موضعها من الآية
    std::wstring sakta;
    if (speak_marks && !speak_waqf) {
        if (const SaktaPlace* sp = find_sakta_place(ayah.verse_key)) sakta = sakta_spoken_label(*sp, en);
    }
    std::wstring letters_txt;
    if (letter_count > 0) {
        letters_txt = en ? (std::to_wstring(letter_count) + L" letters")
                         : letters_ar(letter_count);
    }
    std::wstring sep = en ? L", " : L"، ";

    for (const auto& extra : {letters_txt, sajdah, sakta, div, bookmark_spoken}) {
        if (!extra.empty()) {
            tail = tail.empty() ? extra : (tail + sep + extra);
        }
    }
    return tail.empty() ? spoken : (spoken + L" — " + tail);
}

// نسخ علامات الوقف (صلى، قلى، ج، م، لا، التعانق، السكتة) مع الآية: إعداد عام يضبطه البرنامج من الخيارات
inline bool& copy_waqf_marks_enabled() {
    static bool on = true;
    return on;
}

inline std::wstring format_ayah_custom(
    const AyahInfo& ayah,
    const std::wstring& script = L"uthmani",
    bool brackets = true,
    bool reference = true,
    bool arabic_digits = true,
    int waqf_marks = -1   // -1: بحسب الإعداد العام، 0: بدونها، 1: معها (لمعاينة الخيارات قبل حفظها)
) {
    std::wstring core;
    if (script == L"qpc") core = ayah.text_qpc_clean;
    else if (script == L"imlaei") core = ayah.text_imlaei;
    else if (script == L"plain") core = ayah.text_simple;
    else core = ayah.text_uthmani;

    replace_all(core, L"۞", L"");
    const bool keep_waqf = (waqf_marks < 0) ? copy_waqf_marks_enabled() : (waqf_marks != 0);
    if (!keep_waqf) {
        std::wstring kept;
        kept.reserve(core.size());
        for (wchar_t ch : core) {
            if (ch >= 0x06D6 && ch <= 0x06DC) continue;   // علامات الوقف وحدها، دون الحروف والتشكيل
            kept.push_back(ch);
        }
        core.swap(kept);
    }
    core = join_words(split_words(core), L" ");

    std::wstring body = brackets ? (L"﴿ " + core + L" ﴾") : core;
    if (reference) {
        std::wstring n_str = arabic_digits ? to_arabic_digits(ayah.ayah_number) : std::to_wstring(ayah.ayah_number);
        return body + L" [" + ayah.surah_name + L": " + n_str + L"]";
    }
    return body;
}

// آية داخل نسخ السورة كاملة: اسم السورة مكتوب في أول النص، فيكفي بعد الآية رقمها
inline std::wstring format_ayah_in_surah(
    const AyahInfo& ayah,
    const std::wstring& script,
    bool brackets,
    bool with_number,
    bool arabic_digits
) {
    std::wstring out = format_ayah_custom(ayah, script, brackets, false, arabic_digits);
    if (with_number) {
        out += L" [" + (arabic_digits ? to_arabic_digits(ayah.ayah_number) : std::to_wstring(ayah.ayah_number)) + L"]";
    }
    return out;
}

inline std::wstring quarter_label(int rub_number, int hizb_number, bool en = false) {
    int pos = (rub_number - 1) % 4;
    if (en) {
        if (pos == 0) return L"Start of hizb " + std::to_wstring(hizb_number);
        if (pos == 1) return L"Quarter of hizb " + std::to_wstring(hizb_number);
        if (pos == 2) return L"Half of hizb " + std::to_wstring(hizb_number);
        return L"Three-quarters of hizb " + std::to_wstring(hizb_number);
    } else {
        std::wstring h = to_arabic_digits(hizb_number);
        if (pos == 0) return L"أول الحزب " + h;
        if (pos == 1) return L"ربع الحزب " + h;
        if (pos == 2) return L"نصف الحزب " + h;
        return L"ثلاثة أرباع الحزب " + h;
    }
}

inline std::wstring manzil_day_label(int manzil_number, bool en = false) {
    static const wchar_t* ar[7] = {
        L"اليوم الأول (من الفاتحة إلى النساء)",
        L"اليوم الثاني (من المائدة إلى التوبة)",
        L"اليوم الثالث (من يونس إلى النحل)",
        L"اليوم الرابع (من الإسراء إلى الفرقان)",
        L"اليوم الخامس (من الشعراء إلى يس)",
        L"اليوم السادس (من الصافات إلى الحجرات)",
        L"اليوم السابع — حزب المفصّل (من ق إلى الناس)",
    };
    static const wchar_t* en_lbl[7] = {
        L"Day 1 (Al-Fatihah to An-Nisa)",
        L"Day 2 (Al-Ma'idah to At-Tawbah)",
        L"Day 3 (Yunus to An-Nahl)",
        L"Day 4 (Al-Isra to Al-Furqan)",
        L"Day 5 (Ash-Shu'ara to Ya-Sin)",
        L"Day 6 (As-Saffat to Al-Hujurat)",
        L"Day 7 — Al-Mufassal (Qaf to An-Nas)",
    };
    int idx = std::max(1, std::min(7, manzil_number)) - 1;
    return en ? en_lbl[idx] : ar[idx];
}

// ما يظهر تحت اسم السورة في طبعتي المدينة: مكية أو مدنية، وعدد آياتها
inline std::wstring build_surah_brief(const SurahInfo& s, bool en = false, bool spoken = false) {
    const std::wstring sep = spoken ? (en ? L", " : L"، ") : L" • ";
    if (en) return std::wstring(s.revelation_place == L"مكية" ? L"Meccan" : L"Medinan") + sep + std::to_wstring(s.verses_count) + L" verses";
    return s.revelation_place + sep + L"آياتها " + to_arabic_digits(s.verses_count);
}

inline std::wstring build_surah_heading(const QuranDatabase& db, int surah_number, bool en = false) {
    SurahInfo s = db.get_surah(surah_number);
    SurahHeadingInfo h = db.get_surah_heading_info(surah_number);
    if (h.type.empty()) h.type = s.revelation_place;

    if (en) {
        std::wstring place = (h.type == L"مكية") ? L"Meccan" : L"Medinan";
        std::wstring res = place;
        if (!h.excepted_verses.empty()) {
            const bool multi = h.excepted_verses_en.find_first_of(L",-") != std::wstring::npos;
            res += (multi ? L", except ayahs " : L", except ayah ") + h.excepted_verses_en;
        }
        if (!h.place_note_en.empty()) res += L" (" + h.place_note_en + L")";
        res += L" • " + std::to_wstring(s.verses_count) + L" verses";
        if (h.after_surah > 0) {
            SurahInfo prev = db.get_surah(h.after_surah);
            res += L" • revealed after Surah " + prev.name_arabic;
        }
        return res;
    } else {
        std::wstring res = h.type;
        if (!h.excepted_verses.empty()) {
            bool multi = (h.excepted_verses.find(L'،') != std::wstring::npos || h.excepted_verses.find(L'-') != std::wstring::npos);
            res += (multi ? L"، إلا الآيات " : L"، إلا الآية ") + to_arabic_digits(h.excepted_verses);
        }
        if (!h.place_note.empty()) {
            res += L" (" + to_arabic_digits(h.place_note) + L")";
        }
        res += L"، وآياتها " + to_arabic_digits(s.verses_count);
        if (h.after_surah > 0) {
            SurahInfo prev = db.get_surah(h.after_surah);
            res += L"، نزلت بعد سورة " + prev.name_arabic;
        }
        return res;
    }
}

} // namespace mushaf
