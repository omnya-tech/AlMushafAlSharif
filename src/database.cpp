// -*- coding: utf-8 -*-
// database.cpp — تنفيذ محرك قاعدة البيانات القرآنية بلغة C++.

#include "database.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace mushaf {

static std::wstring col_wstr(sqlite3_stmt* stmt, int idx) {
    const unsigned char* txt = sqlite3_column_text(stmt, idx);
    if (!txt) return L"";
    return to_wide(reinterpret_cast<const char*>(txt));
}

QuranDatabase::QuranDatabase(
    const std::wstring& db_path,
    const std::wstring& headings_path
) {
    if (sqlite3_open_v2(to_utf8(db_path).c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        return;
    }

    load_surahs();

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT id, surah_number, ayah_number FROM ayahs ORDER BY id", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            int s = sqlite3_column_int(stmt, 1);
            int a = sqlite3_column_int(stmt, 2);
            key_to_id_[{s, a}] = id;
            id_to_key_[id] = {s, a};
            if (id > max_id_) max_id_ = id;
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(db_, "SELECT surah_number, MIN(id), MAX(id) FROM ayahs GROUP BY surah_number", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int s = sqlite3_column_int(stmt, 0);
            int mn = sqlite3_column_int(stmt, 1);
            int mx = sqlite3_column_int(stmt, 2);
            surah_id_bounds_[s] = {mn, mx};
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(db_, "SELECT MIN(id) FROM ayahs GROUP BY rub_el_hizb_number", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            rub_start_ids_.insert(sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    std::vector<int> madinah_starts;
    if (sqlite3_prepare_v2(db_, "SELECT MIN(id) FROM ayahs GROUP BY page_number ORDER BY page_number", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            madinah_starts.push_back(sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    page_first_ids_[L"madinah"] = madinah_starts;

    load_headings(headings_path);
    apply_page_mapping();
}

QuranDatabase::~QuranDatabase() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void QuranDatabase::load_surahs() {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT number, name_arabic, name_uthmani, name_simple, revelation_place, "
                      "revelation_order, bismillah_pre, verses_count, start_page, end_page, start_juz "
                      "FROM surahs ORDER BY number";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SurahInfo s;
            s.number = sqlite3_column_int(stmt, 0);
            s.name_arabic = col_wstr(stmt, 1);
            s.name_uthmani = col_wstr(stmt, 2);
            s.name_simple = col_wstr(stmt, 3);
            s.revelation_place = col_wstr(stmt, 4);
            s.revelation_order = sqlite3_column_int(stmt, 5);
            s.bismillah_pre = sqlite3_column_int(stmt, 6) != 0;
            s.verses_count = sqlite3_column_int(stmt, 7);
            s.start_page = sqlite3_column_int(stmt, 8);
            s.end_page = sqlite3_column_int(stmt, 9);
            s.start_juz = sqlite3_column_int(stmt, 10);
            base_surahs_.push_back(s);
            surah_map_[s.number] = s;
        }
        sqlite3_finalize(stmt);
    }
    surahs_cache_ = base_surahs_;
}

static std::string extract_json_string(const std::string& block, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = block.find(k);
    if (p == std::string::npos) return "";
    p = block.find(':', p + k.size());
    if (p == std::string::npos) return "";
    while (p + 1 < block.size() && (block[p + 1] == ' ' || block[p + 1] == '\t')) ++p;
    if (p + 1 < block.size() && block[p + 1] != '"') return "";
    size_t q1 = block.find('"', p + 1);
    if (q1 == std::string::npos) return "";
    size_t q2 = q1 + 1;
    while (q2 < block.size() && !(block[q2] == '"' && block[q2 - 1] != '\\')) ++q2;
    if (q2 >= block.size()) return "";
    return block.substr(q1 + 1, q2 - q1 - 1);
}

static int extract_json_int(const std::string& block, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = block.find(k);
    if (p == std::string::npos) return 0;
    p = block.find(':', p + k.size());
    if (p == std::string::npos) return 0;
    while (p + 1 < block.size() && (block[p + 1] == ' ' || block[p + 1] == '\t')) ++p;
    if (p + 1 < block.size() && std::isdigit((unsigned char)block[p + 1])) {
        return std::atoi(block.c_str() + p + 1);
    }
    return 0;
}

static size_t find_matching_delim(const std::string& s, size_t open_pos, char open_ch, char close_ch) {
    int depth = 0;
    bool in_str = false;
    for (size_t i = open_pos; i < s.size(); ++i) {
        char c = s[i];
        if (in_str) {
            if (c == '"' && s[i - 1] != '\\') in_str = false;
            continue;
        }
        if (c == '"') {
            in_str = true;
        } else if (c == open_ch) {
            ++depth;
        } else if (c == close_ch) {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

static std::wstring format_verse_ranges(const std::vector<int>& verses) {
    if (verses.empty()) return L"";
    std::wstring out;
    size_t i = 0;
    while (i < verses.size()) {
        size_t j = i;
        while (j + 1 < verses.size() && verses[j + 1] == verses[j] + 1) ++j;
        if (!out.empty()) out += L"، ";
        if (j - i >= 2) {
            out += std::to_wstring(verses[i]) + L"-" + std::to_wstring(verses[j]);
        } else if (j == i + 1) {
            out += std::to_wstring(verses[i]) + L"، " + std::to_wstring(verses[j]);
        } else {
            out += std::to_wstring(verses[i]);
        }
        i = j + 1;
    }
    return out;
}

void QuranDatabase::load_headings(const std::wstring& path) {
    std::string raw = read_file_utf8(path);
    if (raw.empty()) return;

    size_t arr_key = raw.find("\"surahs\"");
    size_t arr_start = (arr_key != std::string::npos) ? raw.find('[', arr_key) : raw.find('[');
    if (arr_start == std::string::npos) return;
    size_t arr_end = find_matching_delim(raw, arr_start, '[', ']');
    if (arr_end == std::string::npos) return;

    size_t pos = arr_start + 1;
    while (pos < arr_end) {
        size_t brace1 = raw.find('{', pos);
        if (brace1 == std::string::npos || brace1 >= arr_end) break;
        size_t brace2 = find_matching_delim(raw, brace1, '{', '}');
        if (brace2 == std::string::npos || brace2 >= arr_end) break;

        std::string block = raw.substr(brace1, brace2 - brace1 + 1);
        pos = brace2 + 1;

        int s = extract_json_int(block, "number");
        if (s < 1 || s > 114) continue;

        SurahHeadingInfo h;
        h.type = to_wide(extract_json_string(block, "type"));
        h.after_surah = extract_json_int(block, "revealed_after");
        std::wstring top_note = to_wide(extract_json_string(block, "note"));

        std::vector<int> all_verses;
        std::vector<std::wstring> exc_details;

        size_t ex_key = block.find("\"exceptions\"");
        if (ex_key != std::string::npos) {
            size_t ex_arr1 = block.find('[', ex_key);
            size_t ex_arr2 = (ex_arr1 != std::string::npos) ? find_matching_delim(block, ex_arr1, '[', ']') : std::string::npos;
            if (ex_arr1 != std::string::npos && ex_arr2 != std::string::npos) {
                size_t epos = ex_arr1 + 1;
                while (epos < ex_arr2) {
                    size_t eb1 = block.find('{', epos);
                    if (eb1 == std::string::npos || eb1 >= ex_arr2) break;
                    size_t eb2 = find_matching_delim(block, eb1, '{', '}');
                    if (eb2 == std::string::npos || eb2 >= ex_arr2) break;
                    std::string eblock = block.substr(eb1, eb2 - eb1 + 1);
                    epos = eb2 + 1;

                    std::vector<int> everses;
                    size_t vk = eblock.find("\"verses\"");
                    if (vk != std::string::npos) {
                        size_t va1 = eblock.find('[', vk);
                        size_t va2 = (va1 != std::string::npos) ? find_matching_delim(eblock, va1, '[', ']') : std::string::npos;
                        if (va1 != std::string::npos && va2 != std::string::npos) {
                            size_t vi = va1 + 1;
                            while (vi < va2) {
                                while (vi < va2 && !std::isdigit((unsigned char)eblock[vi])) ++vi;
                                if (vi >= va2) break;
                                int vnum = 0;
                                while (vi < va2 && std::isdigit((unsigned char)eblock[vi])) {
                                    vnum = vnum * 10 + (eblock[vi] - '0');
                                    ++vi;
                                }
                                if (vnum > 0) {
                                    everses.push_back(vnum);
                                    all_verses.push_back(vnum);
                                }
                            }
                        }
                    }

                    std::wstring kind = to_wide(extract_json_string(eblock, "kind"));
                    std::wstring place = to_wide(extract_json_string(eblock, "place"));
                    std::wstring detail;
                    if (!kind.empty() && !place.empty()) detail = kind + L" — " + place;
                    else if (!kind.empty()) detail = kind;
                    else if (!place.empty()) detail = place;

                    if (!detail.empty()) {
                        std::wstring vr = format_verse_ranges(everses);
                        if (!vr.empty()) {
                            exc_details.push_back(vr + L": " + detail);
                        } else {
                            exc_details.push_back(detail);
                        }
                    }
                }
            }
        }

        h.excepted_verses = format_verse_ranges(all_verses);
        std::wstring exc_note;
        if (exc_details.size() == 1) {
            // Single exception group: drop the redundant "verses: " prefix since excepted_verses is shown right before it
            size_t colon = exc_details[0].find(L": ");
            exc_note = (colon != std::wstring::npos) ? exc_details[0].substr(colon + 2) : exc_details[0];
        } else if (exc_details.size() > 1) {
            for (size_t k = 0; k < exc_details.size(); ++k) {
                if (k > 0) exc_note += L"؛ ";
                exc_note += exc_details[k];
            }
        }

        if (!exc_note.empty() && !top_note.empty()) {
            h.place_note = exc_note + L"، " + top_note;
        } else if (!exc_note.empty()) {
            h.place_note = exc_note;
        } else {
            h.place_note = top_note;
        }

        headings_map_[s] = h;
    }
}

// البرنامج يعتمد طبعة المدينة المنورة وحدها (٦٠٤ صفحات)، فتُحسب صفحات السور منها مرة واحدة.
void QuranDatabase::apply_page_mapping() {
    surahs_cache_ = base_surahs_;
    for (auto& s : surahs_cache_) {
        auto it = surah_id_bounds_.find(s.number);
        if (it != surah_id_bounds_.end()) {
            s.start_page = page_of_id(it->second.first);
            s.end_page = page_of_id(it->second.second);
        }
        surah_map_[s.number] = s;
    }
}

int QuranDatabase::page_count() const {
    auto it = page_first_ids_.find(edition_);
    if (it == page_first_ids_.end()) return 604;
    return (int)it->second.size();
}

int QuranDatabase::page_of_id(int ayah_id) const {
    auto it = page_first_ids_.find(edition_);
    if (it == page_first_ids_.end() || it->second.empty()) return 1;
    const auto& vec = it->second;
    auto up = std::upper_bound(vec.begin(), vec.end(), ayah_id);
    int idx = (int)(up - vec.begin());
    return std::max(1, idx);
}

std::pair<int, int> QuranDatabase::page_id_bounds(int page) const {
    auto it = page_first_ids_.find(edition_);
    if (it == page_first_ids_.end() || it->second.empty()) return {1, max_id_};
    const auto& vec = it->second;
    int p = std::max(1, std::min((int)vec.size(), page));
    int first_id = vec[p - 1];
    int last_id = (p < (int)vec.size()) ? (vec[p] - 1) : max_id_;
    return {first_id, last_id};
}

SurahInfo QuranDatabase::get_surah(int surah_number) const {
    auto it = surah_map_.find(surah_number);
    if (it != surah_map_.end()) return it->second;
    return surahs_cache_.empty() ? SurahInfo{} : surahs_cache_[0];
}

SurahHeadingInfo QuranDatabase::get_surah_heading_info(int surah_number) const {
    auto it = headings_map_.find(surah_number);
    if (it != headings_map_.end()) return it->second;
    return SurahHeadingInfo{};
}

static const char* kAyahSelectCols =
    "SELECT id, surah_number, ayah_number, verse_key, text_qpc_hafs, text_qpc_clean, "
    "text_uthmani, text_imlaei, text_simple, juz_number, hizb_number, rub_el_hizb_number, "
    "manzil_number, ruku_number, sajdah_number FROM ayahs ";

AyahInfo QuranDatabase::row_to_ayah(sqlite3_stmt* stmt) const {
    AyahInfo a;
    a.id = sqlite3_column_int(stmt, 0);
    a.surah_number = sqlite3_column_int(stmt, 1);
    auto sit = surah_map_.find(a.surah_number);
    if (sit != surah_map_.end()) {
        a.surah_name = sit->second.name_arabic;
        a.surah_name_uthmani = sit->second.name_uthmani;
        a.surah_name_simple = sit->second.name_simple;
    }
    a.ayah_number = sqlite3_column_int(stmt, 2);
    a.verse_key = col_wstr(stmt, 3);
    a.text_qpc_hafs = col_wstr(stmt, 4);
    a.text_qpc_clean = col_wstr(stmt, 5);
    a.text_uthmani = col_wstr(stmt, 6);
    a.text_imlaei = col_wstr(stmt, 7);
    a.text_simple = col_wstr(stmt, 8);
    a.page_number = page_of_id(a.id);
    a.juz_number = sqlite3_column_int(stmt, 9);
    a.hizb_number = sqlite3_column_int(stmt, 10);
    a.rub_el_hizb_number = sqlite3_column_int(stmt, 11);
    a.manzil_number = sqlite3_column_int(stmt, 12);
    a.ruku_number = sqlite3_column_int(stmt, 13);
    a.sajdah_number = (sqlite3_column_type(stmt, 14) == SQLITE_NULL) ? 0 : sqlite3_column_int(stmt, 14);
    return a;
}

std::vector<AyahInfo> QuranDatabase::get_surah_ayahs(int surah_number) const {
    std::vector<AyahInfo> res;
    std::string sql = std::string(kAyahSelectCols) + "WHERE surah_number = ? ORDER BY ayah_number";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, surah_number);
        while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back(row_to_ayah(stmt));
        sqlite3_finalize(stmt);
    }
    return res;
}

std::vector<AyahInfo> QuranDatabase::get_page_ayahs(int page_number) const {
    std::vector<AyahInfo> res;
    auto bounds = page_id_bounds(page_number);
    std::string sql = std::string(kAyahSelectCols) + "WHERE id BETWEEN ? AND ? ORDER BY id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, bounds.first);
        sqlite3_bind_int(stmt, 2, bounds.second);
        while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back(row_to_ayah(stmt));
        sqlite3_finalize(stmt);
    }
    return res;
}

std::vector<AyahInfo> QuranDatabase::get_juz_ayahs(int juz_number) const {
    std::vector<AyahInfo> res;
    std::string sql = std::string(kAyahSelectCols) + "WHERE juz_number = ? ORDER BY id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, juz_number);
        while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back(row_to_ayah(stmt));
        sqlite3_finalize(stmt);
    }
    return res;
}

std::vector<AyahInfo> QuranDatabase::get_hizb_ayahs(int hizb_number) const {
    std::vector<AyahInfo> res;
    std::string sql = std::string(kAyahSelectCols) + "WHERE hizb_number = ? ORDER BY id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, hizb_number);
        while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back(row_to_ayah(stmt));
        sqlite3_finalize(stmt);
    }
    return res;
}

std::vector<AyahInfo> QuranDatabase::get_rub_ayahs(int rub_number) const {
    std::vector<AyahInfo> res;
    std::string sql = std::string(kAyahSelectCols) + "WHERE rub_el_hizb_number = ? ORDER BY id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, rub_number);
        while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back(row_to_ayah(stmt));
        sqlite3_finalize(stmt);
    }
    return res;
}

std::vector<AyahInfo> QuranDatabase::get_manzil_ayahs(int manzil_number) const {
    std::vector<AyahInfo> res;
    std::string sql = std::string(kAyahSelectCols) + "WHERE manzil_number = ? ORDER BY id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, manzil_number);
        while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back(row_to_ayah(stmt));
        sqlite3_finalize(stmt);
    }
    return res;
}

std::vector<AyahInfo> QuranDatabase::get_ruku_ayahs(int ruku_number) const {
    std::vector<AyahInfo> res;
    std::string sql = std::string(kAyahSelectCols) + "WHERE ruku_number = ? ORDER BY id";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, ruku_number);
        while (sqlite3_step(stmt) == SQLITE_ROW) res.push_back(row_to_ayah(stmt));
        sqlite3_finalize(stmt);
    }
    return res;
}

std::vector<std::wstring> QuranDatabase::find_same_text(const std::wstring& text_simple) const {
    std::vector<std::wstring> res;
    if (text_simple.empty()) return res;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT verse_key FROM ayahs WHERE text_simple = ? ORDER BY id", -1, &stmt, nullptr) == SQLITE_OK) {
        std::string u8 = to_utf8(text_simple);
        sqlite3_bind_text(stmt, 1, u8.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            res.push_back(col_wstr(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return res;
}

bool QuranDatabase::get_ayah(int surah_number, int ayah_number, AyahInfo& out) const {
    std::string sql = std::string(kAyahSelectCols) + "WHERE surah_number = ? AND ayah_number = ?";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, surah_number);
        sqlite3_bind_int(stmt, 2, ayah_number);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out = row_to_ayah(stmt);
            ok = true;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool QuranDatabase::get_ayah_by_key(const std::wstring& verse_key, AyahInfo& out) const {
    std::string sql = std::string(kAyahSelectCols) + "WHERE verse_key = ?";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    std::string utf8_key = to_utf8(verse_key);
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, utf8_key.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out = row_to_ayah(stmt);
            ok = true;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool QuranDatabase::get_ayah_by_id(int id, AyahInfo& out) const {
    std::string sql = std::string(kAyahSelectCols) + "WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out = row_to_ayah(stmt);
            ok = true;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

std::vector<DivisionRangeInfo> QuranDatabase::build_division_list(const char* col_name, int /*expected_max*/) const {
    std::vector<DivisionRangeInfo> list;
    std::string sql = std::string(
        "SELECT ") + col_name + ", MIN(id), MAX(id), MIN(hizb_number), MIN(juz_number), "
        "COUNT(*), COUNT(DISTINCT surah_number), text_uthmani "
        "FROM ayahs GROUP BY " + col_name + " HAVING id = MIN(id) ORDER BY " + col_name;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DivisionRangeInfo d;
            d.unit_number = sqlite3_column_int(stmt, 0);
            int first_id = sqlite3_column_int(stmt, 1);
            int last_id = sqlite3_column_int(stmt, 2);
            d.hizb_number = sqlite3_column_int(stmt, 3);
            d.juz_number = sqlite3_column_int(stmt, 4);
            d.verses_count = sqlite3_column_int(stmt, 5);
            d.surahs_count = sqlite3_column_int(stmt, 6);
            std::wstring txt = col_wstr(stmt, 7);
            auto words = split_words(txt);
            if (words.size() > 5) words.resize(5);
            d.first_words = join_words(words, L" ") + L"...";

            d.start_page = page_of_id(first_id);
            d.end_page = page_of_id(last_id);
            auto fk = id_to_key_.find(first_id);
            auto lk = id_to_key_.find(last_id);
            if (fk != id_to_key_.end()) {
                d.start_surah_number = fk->second.first;
                d.start_ayah_number = fk->second.second;
                auto s = get_surah(d.start_surah_number);
                d.start_surah_name = s.name_arabic;
                d.start_surah_simple = s.name_simple;
            }
            if (lk != id_to_key_.end()) {
                d.end_surah_number = lk->second.first;
                d.end_ayah_number = lk->second.second;
                auto s = get_surah(d.end_surah_number);
                d.end_surah_name = s.name_arabic;
                d.end_surah_simple = s.name_simple;
            }
            list.push_back(d);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

const std::vector<DivisionRangeInfo>& QuranDatabase::get_page_info_list() const {
    auto it = page_info_cache_.find(edition_);
    if (it != page_info_cache_.end()) return it->second;

    std::vector<DivisionRangeInfo> list;
    const auto& starts = page_first_ids_.at(edition_);
    int count = (int)starts.size();
    list.reserve(count);

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT juz_number FROM ayahs WHERE id = ?", -1, &stmt, nullptr);
    for (int i = 0; i < count; ++i) {
        int first_id = starts[i];
        int last_id = (i + 1 < count) ? (starts[i + 1] - 1) : max_id_;
        DivisionRangeInfo d;
        d.unit_number = i + 1;
        d.start_page = i + 1;
        d.end_page = i + 1;
        d.verses_count = last_id - first_id + 1;
        if (stmt) {
            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, first_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) d.juz_number = sqlite3_column_int(stmt, 0);
        }
        auto fk = id_to_key_.find(first_id);
        auto lk = id_to_key_.find(last_id);
        if (fk != id_to_key_.end()) {
            d.start_surah_number = fk->second.first;
            d.start_ayah_number = fk->second.second;
            auto s = get_surah(d.start_surah_number);
            d.start_surah_name = s.name_arabic;
            d.start_surah_simple = s.name_simple;
        }
        if (lk != id_to_key_.end()) {
            d.end_surah_number = lk->second.first;
            d.end_ayah_number = lk->second.second;
            auto s = get_surah(d.end_surah_number);
            d.end_surah_name = s.name_arabic;
            d.end_surah_simple = s.name_simple;
        }
        list.push_back(d);
    }
    if (stmt) sqlite3_finalize(stmt);
    return page_info_cache_[edition_] = std::move(list);
}

const std::vector<DivisionRangeInfo>& QuranDatabase::get_juz_info_list() const {
    auto it = juz_info_cache_.find(edition_);
    if (it != juz_info_cache_.end()) return it->second;
    return juz_info_cache_[edition_] = build_division_list("juz_number", 30);
}

const std::vector<DivisionRangeInfo>& QuranDatabase::get_hizb_info_list() const {
    auto it = hizb_info_cache_.find(edition_);
    if (it != hizb_info_cache_.end()) return it->second;
    return hizb_info_cache_[edition_] = build_division_list("hizb_number", 60);
}

const std::vector<DivisionRangeInfo>& QuranDatabase::get_rub_info_list() const {
    auto it = rub_info_cache_.find(edition_);
    if (it != rub_info_cache_.end()) return it->second;
    return rub_info_cache_[edition_] = build_division_list("rub_el_hizb_number", 240);
}

const std::vector<DivisionRangeInfo>& QuranDatabase::get_manzil_info_list() const {
    auto it = manzil_info_cache_.find(edition_);
    if (it != manzil_info_cache_.end()) return it->second;
    return manzil_info_cache_[edition_] = build_division_list("manzil_number", 7);
}

const std::vector<DivisionRangeInfo>& QuranDatabase::get_ruku_info_list() const {
    auto it = ruku_info_cache_.find(edition_);
    if (it != ruku_info_cache_.end()) return it->second;
    return ruku_info_cache_[edition_] = build_division_list("ruku_number", 558);
}

std::vector<AyahInfo> QuranDatabase::search_ayahs(const std::wstring& query, int surah_filter, int limit) const {
    std::vector<AyahInfo> res;
    std::wstring q = trim_wide(to_western_digits(query));
    if (q.empty()) return res;

    std::wstring norm_q = normalize_arabic_search(q);
    if (norm_q == L"ايه الكرسي" || norm_q == L"الكرسي") {
        AyahInfo kursi;
        if (get_ayah(2, 255, kursi)) res.push_back(kursi);
        return res;
    }

    size_t colon = q.find(L':');
    if (colon != std::wstring::npos) {
        int s = _wtoi(q.substr(0, colon).c_str());
        int a = _wtoi(q.substr(colon + 1).c_str());
        AyahInfo item;
        if (s >= 1 && s <= 114 && a >= 1 && get_ayah(s, a, item)) {
            res.push_back(item);
            return res;
        }
    }

    if (norm_q.empty()) return res;
    std::string pattern = "%" + to_utf8(norm_q) + "%";
    std::string sql = std::string(kAyahSelectCols) + "WHERE text_search LIKE ? ";
    if (surah_filter >= 1 && surah_filter <= 114) {
        sql += "AND surah_number = ? ";
    }
    sql += "ORDER BY id LIMIT ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        int idx = 1;
        sqlite3_bind_text(stmt, idx++, pattern.c_str(), -1, SQLITE_TRANSIENT);
        if (surah_filter >= 1 && surah_filter <= 114) {
            sqlite3_bind_int(stmt, idx++, surah_filter);
        }
        sqlite3_bind_int(stmt, idx++, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            res.push_back(row_to_ayah(stmt));
        }
        sqlite3_finalize(stmt);
    }
    return res;
}

} // namespace mushaf
