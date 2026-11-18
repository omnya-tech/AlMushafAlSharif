#pragma once
// -*- coding: utf-8 -*-
// database.hpp — محرك قاعدة البيانات القرآنية بلغة C++ (SQLite3).
// طبعة المدينة المنورة (٦٠٤ صفحات) مدمجة، وأي طبعة أخرى تُضاف بملف «بدايات الصفحات»
// في assets\data\editions\ (انظر tools\editions\اقرأني.txt).

#include "utf8_util.hpp"
#include "../third_party/sqlite3/sqlite3.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mushaf {

struct SurahInfo {
    int number = 1;
    std::wstring name_arabic;
    std::wstring name_uthmani;
    std::wstring name_simple;
    std::wstring revelation_place;
    int revelation_order = 1;
    bool bismillah_pre = true;
    int verses_count = 7;
    int start_page = 1;
    int end_page = 1;
    int start_juz = 1;
};

struct AyahInfo {
    int id = 1;
    int surah_number = 1;
    std::wstring surah_name;
    std::wstring surah_name_uthmani;
    std::wstring surah_name_simple;
    int ayah_number = 1;
    std::wstring verse_key;
    std::wstring text_qpc_hafs;
    std::wstring text_qpc_clean;
    std::wstring text_uthmani;
    std::wstring text_imlaei;
    std::wstring text_simple;
    int page_number = 1;
    int juz_number = 1;
    int hizb_number = 1;
    int rub_el_hizb_number = 1;
    int manzil_number = 1;
    int ruku_number = 1;
    int sajdah_number = 0; // 0 = لا يوجد
};

struct DivisionRangeInfo {
    int unit_number = 1;
    int hizb_number = 1;
    int juz_number = 1;
    int start_page = 1;
    int end_page = 1;
    int start_surah_number = 1;
    std::wstring start_surah_name;
    std::wstring start_surah_simple;
    int start_ayah_number = 1;
    int end_surah_number = 1;
    std::wstring end_surah_name;
    std::wstring end_surah_simple;
    int end_ayah_number = 1;
    int verses_count = 1;
    int surahs_count = 1;
    std::wstring first_words;
};

// طبعة مصحف: تختلف الطبعات في تقسيم الصفحات فقط، والنص والأجزاء والأحزاب واحدة
struct EditionInfo {
    std::wstring key;        // مثل madinah أو dar_almaarif
    std::wstring name_ar;    // مثل «طبعة دار المعارف»
    std::wstring name_en;
    std::wstring source;     // مسار الملف (فارغ للطبعة المدمجة)
    int page_count = 0;
};

// نتيجة فحص ملف بدايات الصفحات
struct EditionCheck {
    EditionInfo info;
    std::vector<int> page_starts;          // رقم أول آية (الترتيب العام) في كل صفحة
    std::vector<std::wstring> errors;      // أخطاء تمنع اعتماد الملف
    std::vector<std::wstring> warnings;    // ملاحظات تستحق المراجعة على المصحف الورقي
    bool ok() const { return errors.empty() && !page_starts.empty(); }
};

// سطور طبعة مصورة (مثل الشمرلي): كل سطر كما في المصحف الورقي، بكلماته
// first_word = 0 يعني أن السطر يبدأ بعلامة نهاية الآية وحدها (كلماتها كلها في السطر السابق)
struct EditionLineSeg {
    int ayah_id = 0;
    int first_word = 1;
    int last_word = 0;
    bool ends = false;   // تنتهي الآية في هذا السطر (تُرسم علامتها بعده)
};

enum class EditionLineKind { Text, SurahBanner, Basmala };

struct EditionLine {
    EditionLineKind kind = EditionLineKind::Text;
    int surah = 1;
    std::vector<EditionLineSeg> segs;
};

// كلمات الآية بالرسم العثماني كما تُعدّ في ملف السطور: علامات الوقف تلحق بالكلمة قبلها، و«۞» بالتي بعدها
std::vector<std::wstring> uthmani_words(const std::wstring& text_uthmani);

struct SurahHeadingInfo {
    std::wstring type;
    std::wstring excepted_verses;
    std::wstring place_note;
    std::wstring excepted_verses_en;   // الآيات المستثناة بفواصل إنجليزية
    std::wstring place_note_en;        // ملاحظة النزول بالإنجليزية
    int after_surah = 0;
};

class QuranDatabase {
public:
    QuranDatabase(
        const std::wstring& db_path,
        const std::wstring& headings_path
    );
    ~QuranDatabase();

    bool is_open() const { return db_ != nullptr; }

    // الطبعات: المدينة مدمجة، والباقي من ملفات بدايات الصفحات
    const std::vector<EditionInfo>& editions() const { return editions_; }
    const std::wstring& edition() const { return edition_; }
    const EditionInfo* edition_info(const std::wstring& key) const;
    std::wstring set_edition(const std::wstring& key);              // يعيد الطبعة المعتمدة فعلاً
    int load_editions_folder(const std::wstring& folder);           // يعيد عدد الطبعات المضافة
    EditionCheck check_edition_file(const std::wstring& path) const;
    EditionCheck check_edition_text(const std::string& utf8_text) const;
    bool add_edition(const EditionCheck& checked);

    int page_count() const;
    int total_ayahs() const { return max_id_; }

    // سطور الطبعة الحالية (إن كان لها ملف سطور)، صفحةً صفحة
    bool has_edition_lines() const { return edition_lines_.count(edition_) > 0; }
    // بيان السور وأقوال المذاهب في السجدات من خصائص طبعة دار المعارف؛ وطبعتا المدينة كما طبعهما المجمع
    bool edition_has_details() const { return edition_ == L"dar_almaarif"; }
    std::vector<EditionLine> get_page_lines(int page) const;
    // يقرأ ملف السطور ويتحقق أنه يغطي كل كلمات القرآن بترتيبها؛ يعيد رسالة خطأ أو نصاً فارغاً
    std::wstring load_edition_lines(const std::wstring& key, const std::wstring& path);

    int page_of_id(int ayah_id) const;
    std::pair<int, int> page_id_bounds(int page) const;

    const std::vector<SurahInfo>& get_all_surahs() const { return surahs_cache_; }
    SurahInfo get_surah(int surah_number) const;
    SurahHeadingInfo get_surah_heading_info(int surah_number) const;

    std::vector<AyahInfo> get_surah_ayahs(int surah_number) const;
    std::vector<AyahInfo> get_page_ayahs(int page_number) const;
    std::vector<AyahInfo> get_juz_ayahs(int juz_number) const;
    std::vector<AyahInfo> get_hizb_ayahs(int hizb_number) const;
    std::vector<AyahInfo> get_rub_ayahs(int rub_number) const;
    std::vector<AyahInfo> get_manzil_ayahs(int manzil_number) const;
    std::vector<AyahInfo> get_ruku_ayahs(int ruku_number) const;

    bool get_ayah(int surah_number, int ayah_number, AyahInfo& out) const;
    bool get_ayah_by_key(const std::wstring& verse_key, AyahInfo& out) const;
    bool get_ayah_by_id(int id, AyahInfo& out) const;
    std::vector<std::wstring> find_same_text(const std::wstring& text_simple) const;

    const std::vector<DivisionRangeInfo>& get_page_info_list() const;
    const std::vector<DivisionRangeInfo>& get_juz_info_list() const;
    const std::vector<DivisionRangeInfo>& get_hizb_info_list() const;
    const std::vector<DivisionRangeInfo>& get_rub_info_list() const;
    const std::vector<DivisionRangeInfo>& get_manzil_info_list() const;
    const std::vector<DivisionRangeInfo>& get_ruku_info_list() const;

    std::vector<AyahInfo> search_ayahs(const std::wstring& query, int surah_filter = 0, int limit = 250) const;
    const std::set<int>& rub_start_ids() const { return rub_start_ids_; }

private:
    sqlite3* db_ = nullptr;
    std::wstring edition_ = L"madinah";
    int max_id_ = 6236;

    std::vector<SurahInfo> base_surahs_;
    std::vector<SurahInfo> surahs_cache_;
    std::map<int, SurahInfo> surah_map_;
    std::map<int, std::pair<int, int>> surah_id_bounds_;
    std::map<std::pair<int, int>, int> key_to_id_;
    std::map<int, std::pair<int, int>> id_to_key_;
    std::map<std::wstring, std::vector<int>> page_first_ids_;
    std::set<int> rub_start_ids_;
    std::map<int, SurahHeadingInfo> headings_map_;
    std::vector<EditionInfo> editions_;
    std::map<std::wstring, std::vector<std::vector<EditionLine>>> edition_lines_;   // [صفحة-١] → سطورها
    mutable std::vector<int> ayah_text_len_;   // طول نص كل آية، لفحص كمية النص في الصفحة

    mutable std::map<std::wstring, std::vector<DivisionRangeInfo>> page_info_cache_;
    mutable std::map<std::wstring, std::vector<DivisionRangeInfo>> juz_info_cache_;
    mutable std::map<std::wstring, std::vector<DivisionRangeInfo>> hizb_info_cache_;
    mutable std::map<std::wstring, std::vector<DivisionRangeInfo>> rub_info_cache_;
    mutable std::map<std::wstring, std::vector<DivisionRangeInfo>> manzil_info_cache_;
    mutable std::map<std::wstring, std::vector<DivisionRangeInfo>> ruku_info_cache_;

    void load_surahs();
    void apply_page_mapping();
    int surah_from_text(const std::wstring& token) const;
    void load_headings(const std::wstring& path);
    AyahInfo row_to_ayah(sqlite3_stmt* stmt) const;
    std::vector<DivisionRangeInfo> build_division_list(const char* col_name, int expected_max) const;
};

} // namespace mushaf
