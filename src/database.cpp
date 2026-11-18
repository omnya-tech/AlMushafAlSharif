// -*- coding: utf-8 -*-
// database.cpp — تنفيذ محرك قاعدة البيانات القرآنية بلغة C++.

#include "database.hpp"

#include <algorithm>
#include <cstdio>
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
    EditionInfo madinah;
    madinah.key = L"madinah";
    madinah.name_ar = L"طبعة المدينة الحديثة";
    madinah.name_en = L"New Madinah print";
    madinah.page_count = (int)madinah_starts.size();
    editions_.push_back(madinah);

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

// ترجمة عبارات النزول في بيانات السور (النص العربي هو المرجع)
static std::wstring surah_note_en(const std::wstring& ar) {
    if (ar == L"مكية") return L"Meccan";
    if (ar == L"مدنية") return L"Medinan";
    if (ar == L"فنزلت بمنى في حجة الوداع") return L"revealed at Mina during the Farewell Pilgrimage";
    if (ar == L"فنزلت بعرفات في حجة الوداع") return L"revealed at Arafat during the Farewell Pilgrimage";
    if (ar == L"فبين مكة والمدينة") return L"revealed between Makkah and Madinah";
    if (ar == L"فبالجحفة أثناء الهجرة") return L"revealed at Al-Juhfah during the Hijrah";
    if (ar == L"فنزلت في الطريق أثناء الهجرة") return L"revealed on the way during the Hijrah";
    if (ar == L"وقد نزلت في الطريق عند الانصراف من الحديبية") return L"revealed on the way back from Al-Hudaybiyah";
    if (ar == L"وهي آخر سورة نزلت بمكة") return L"the last surah revealed in Makkah";
    if (ar == L"وقد نزلت بمنى في حجة الوداع") return L"revealed at Mina during the Farewell Pilgrimage";
    return L"";
}

static std::wstring latin_commas(std::wstring s) {
    for (wchar_t& ch : s) if (ch == L'\u060C') ch = L',';
    return s;
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
        std::vector<std::wstring> exc_details, exc_details_en;

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
                        const std::wstring kind_en = surah_note_en(kind), place_en = surah_note_en(place);
                        std::wstring detail_en = kind_en;
                        if (!place_en.empty()) detail_en += (detail_en.empty() ? L"" : L" \u2014 ") + place_en;
                        if (!vr.empty()) {
                            exc_details.push_back(vr + L": " + detail);
                            exc_details_en.push_back(latin_commas(vr) + L": " + detail_en);
                        } else {
                            exc_details.push_back(detail);
                            exc_details_en.push_back(detail_en);
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

        h.excepted_verses_en = latin_commas(h.excepted_verses);
        std::wstring exc_en;
        if (exc_details_en.size() == 1) {
            size_t colon = exc_details_en[0].find(L": ");
            exc_en = (colon != std::wstring::npos) ? exc_details_en[0].substr(colon + 2) : exc_details_en[0];
        } else {
            for (size_t k = 0; k < exc_details_en.size(); ++k) exc_en += (k ? L"; " : L"") + exc_details_en[k];
        }
        const std::wstring top_en = surah_note_en(top_note);
        h.place_note_en = exc_en;
        if (!top_en.empty()) h.place_note_en += (h.place_note_en.empty() ? L"" : L", ") + top_en;

        headings_map_[s] = h;
    }
}

// صفحات السور تُحسب من تقسيم الطبعة الحالية
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

const EditionInfo* QuranDatabase::edition_info(const std::wstring& key) const {
    for (const auto& e : editions_) if (e.key == key) return &e;
    return nullptr;
}

std::wstring QuranDatabase::set_edition(const std::wstring& key) {
    edition_ = (edition_info(key) && page_first_ids_.count(key)) ? key : L"madinah";
    apply_page_mapping();
    return edition_;
}

bool QuranDatabase::add_edition(const EditionCheck& checked) {
    if (!checked.ok() || checked.info.key.empty() || checked.info.key == L"madinah") return false;
    for (auto& e : editions_) {
        if (e.key == checked.info.key) { e = checked.info; page_first_ids_[e.key] = checked.page_starts; return true; }
    }
    editions_.push_back(checked.info);
    page_first_ids_[checked.info.key] = checked.page_starts;
    page_info_cache_.erase(checked.info.key);
    return true;
}

int QuranDatabase::load_editions_folder(const std::wstring& folder) {
    int added = 0;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((folder + L"\\*.csv").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const std::wstring path = folder + L"\\" + fd.cFileName;
        EditionCheck c = check_edition_file(path);
        if (add_edition(c)) {   // الملف الذي فيه أخطاء لا يُعتمد، ويُفحص بأمر --check-edition
            ++added;
            // ملف السطور اختياري، باسم ملف البدايات نفسه: shamarly.csv ← shamarly.lines.tsv
            std::wstring lines_path = path.substr(0, path.size() - 4) + L".lines.tsv";
            if (GetFileAttributesW(lines_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                load_edition_lines(c.info.key, lines_path);
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    // طبعتا المدينة متجاورتان في أول القائمة، أيًّا كان ترتيب الملفات في المجلد
    std::stable_partition(editions_.begin(), editions_.end(), [](const EditionInfo& e) {
        return e.key.compare(0, 7, L"madinah") == 0;
    });
    return added;
}

std::vector<std::wstring> uthmani_words(const std::wstring& text) {
    auto is_mark_token = [](const std::wstring& t) {
        for (wchar_t ch : t) {
            bool mark = (ch >= 0x06D6 && ch <= 0x06DC) || ch == 0x06DE || ch == 0x06E9;
            if (!mark) return false;
        }
        return !t.empty();
    };
    std::vector<std::wstring> out;
    std::wstring pending;
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        while (i < n && text[i] == L' ') ++i;
        if (i >= n) break;
        size_t j = text.find(L' ', i);
        if (j == std::wstring::npos) j = n;
        std::wstring tok = text.substr(i, j - i);
        i = j;
        if (is_mark_token(tok)) {
            if (tok == L"۞") pending = tok + L" ";
            else if (!out.empty()) out.back() += L" " + tok;
            else pending += tok + L" ";
        } else {
            out.push_back(pending + tok);
            pending.clear();
        }
    }
    return out;
}

std::wstring QuranDatabase::load_edition_lines(const std::wstring& key, const std::wstring& path) {
    const EditionInfo* info = edition_info(key);
    if (!info || !db_) return L"الطبعة غير معروفة";
    std::string raw = read_file_utf8(path);
    if (raw.empty()) return L"ملف السطور فارغ";

    // عدد كلمات كل آية، للتحقق من أن الملف يغطيها كاملة بترتيبها
    std::vector<int> word_count(max_id_ + 1, 0);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT id, text_uthmani FROM ayahs", -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            if (id >= 1 && id <= max_id_) word_count[id] = (int)uthmani_words(col_wstr(stmt, 1)).size();
        }
        sqlite3_finalize(stmt);
    }

    std::vector<std::vector<EditionLine>> pages(info->page_count);
    std::vector<int> seen(max_id_ + 1, 0);
    size_t pos = 0;
    int line_no = 0;
    while (pos < raw.size()) {
        size_t eol = raw.find('\n', pos);
        if (eol == std::string::npos) eol = raw.size();
        std::string ln = raw.substr(pos, eol - pos);
        pos = eol + 1;
        ++line_no;
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        if (ln.empty() || ln[0] == '#') continue;
        // صفحة \t رقم السطر \t النوع \t السورة \t المقاطع
        std::vector<std::string> f;
        size_t a = 0;
        while (true) {
            size_t t = ln.find('\t', a);
            f.push_back(ln.substr(a, t == std::string::npos ? std::string::npos : t - a));
            if (t == std::string::npos) break;
            a = t + 1;
        }
        if (f.size() < 4) return L"سطر غير مكتمل في ملف السطور: " + std::to_wstring(line_no);
        int page = std::atoi(f[0].c_str());
        if (page < 1 || page > (int)pages.size()) return L"رقم صفحة خارج الطبعة في السطر " + std::to_wstring(line_no);
        EditionLine el;
        el.kind = (f[2] == "banner") ? EditionLineKind::SurahBanner
                : (f[2] == "basmala") ? EditionLineKind::Basmala : EditionLineKind::Text;
        el.surah = std::atoi(f[3].c_str());
        if (f.size() >= 5) {
            const std::string& sg = f[4];
            size_t b = 0;
            while (b < sg.size()) {
                size_t e = sg.find(' ', b);
                if (e == std::string::npos) e = sg.size();
                std::string tok = sg.substr(b, e - b);
                b = e + 1;
                if (tok.empty()) continue;
                EditionLineSeg s;
                s.ends = (tok.back() == '*');
                int id = 0, w1 = 0, w2 = 0;
                if (std::sscanf(tok.c_str(), "%d:%d-%d", &id, &w1, &w2) != 3 || id < 1 || id > max_id_)
                    return L"مقطع غير مفهوم في السطر " + std::to_wstring(line_no);
                s.ayah_id = id;
                s.first_word = w1;
                s.last_word = w2;
                if (w1 > 0) {
                    if (w1 != seen[id] + 1 || w2 < w1 || w2 > word_count[id])
                        return L"ترتيب الكلمات لا يطابق نص الآية في السطر " + std::to_wstring(line_no);
                    seen[id] = w2;
                }
                el.segs.push_back(s);
            }
        }
        pages[page - 1].push_back(el);
    }
    for (int id = 1; id <= max_id_; ++id) {
        if (seen[id] != word_count[id]) return L"الآية رقم " + std::to_wstring(id) + L" غير مكتملة في ملف السطور";
    }
    edition_lines_[key] = std::move(pages);
    return L"";
}

std::vector<EditionLine> QuranDatabase::get_page_lines(int page) const {
    auto it = edition_lines_.find(edition_);
    if (it == edition_lines_.end() || page < 1 || page > (int)it->second.size()) return {};
    return it->second[page - 1];
}

// اسم السورة أو رقمها (بالأرقام العربية أو الإنجليزية، مع «سورة» أو بدونها) → رقمها، أو 0
int QuranDatabase::surah_from_text(const std::wstring& token) const {
    std::wstring t = to_western_digits(token);
    bool digits = !t.empty();
    for (wchar_t ch : t) if (!iswdigit(ch)) { digits = false; break; }
    if (digits) {
        int n = _wtoi(t.c_str());
        return (n >= 1 && n <= 114) ? n : 0;
    }
    std::wstring q = normalize_arabic_search(token);
    const std::wstring prefix = normalize_arabic_search(L"سورة ");
    if (q.rfind(prefix, 0) == 0) q = q.substr(prefix.size());
    while (!q.empty() && q.front() == L' ') q.erase(q.begin());
    while (!q.empty() && q.back() == L' ') q.pop_back();
    for (const auto& s : base_surahs_) {
        if (normalize_arabic_search(s.name_arabic) == q) return s.number;
    }
    return 0;
}

EditionCheck QuranDatabase::check_edition_file(const std::wstring& path) const {
    EditionCheck c = check_edition_text(read_file_utf8(path));
    c.info.source = path;
    if (c.info.key.empty()) {
        // المفتاح من اسم الملف إن لم يُكتب في رأسه
        size_t slash = path.find_last_of(L"\\/");
        std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
        size_t dot = name.find_last_of(L'.');
        c.info.key = (dot == std::wstring::npos) ? name : name.substr(0, dot);
    }
    if (c.info.name_ar.empty()) c.info.name_ar = c.info.key;
    if (c.info.name_en.empty()) c.info.name_en = c.info.key;
    return c;
}

// صيغة الملف (يفتحه Excel أو المفكرة):
//   # key: dar_almaarif
//   # name_ar: طبعة دار المعارف
//   # name_en: Dar al-Maarif print
//   الصفحة,السورة,الآية
//   1,الفاتحة,1
//   2,البقرة,1
//   3,,6          ← السورة الفارغة تعني نفس سورة السطر السابق
EditionCheck QuranDatabase::check_edition_text(const std::string& utf8_text) const {
    EditionCheck c;
    std::wstring text = to_wide(utf8_text);
    if (!text.empty() && text[0] == 0xFEFF) text.erase(text.begin());
    if (text.empty()) {
        c.errors.push_back(L"الملف فارغ أو لا يمكن قراءته.");
        return c;
    }

    std::vector<std::pair<int, int>> rows;   // (رقم الصفحة، رقم الآية العام)
    int line_no = 0, last_surah = 0, expected_page = 1;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find(L'\n', pos);
        std::wstring line = text.substr(pos, (nl == std::wstring::npos) ? std::wstring::npos : nl - pos);
        pos = (nl == std::wstring::npos) ? text.size() + 1 : nl + 1;
        ++line_no;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ' || line.back() == L'\t')) line.pop_back();
        while (!line.empty() && (line.front() == L' ' || line.front() == L'\t')) line.erase(line.begin());
        if (line.empty()) continue;
        const std::wstring ln = L"السطر " + std::to_wstring(line_no) + L": ";

        if (line[0] == L'#') {   // بيانات الطبعة
            size_t colon = line.find(L':');
            if (colon != std::wstring::npos) {
                std::wstring k = line.substr(1, colon - 1), v = line.substr(colon + 1);
                auto trim = [](std::wstring& s) {
                    while (!s.empty() && s.front() == L' ') s.erase(s.begin());
                    while (!s.empty() && s.back() == L' ') s.pop_back();
                };
                trim(k); trim(v);
                if (k == L"key") c.info.key = v;
                else if (k == L"name_ar") c.info.name_ar = v;
                else if (k == L"name_en") c.info.name_en = v;
            }
            continue;
        }

        // تقسيم الحقول: فاصلة أو فاصلة عربية أو فاصلة منقوطة أو جدولة
        std::vector<std::wstring> f;
        std::wstring cur;
        for (wchar_t ch : line) {
            if (ch == L',' || ch == 0x060C || ch == L';' || ch == 0x061B || ch == L'\t') { f.push_back(cur); cur.clear(); }
            else cur.push_back(ch);
        }
        f.push_back(cur);
        for (auto& x : f) {
            while (!x.empty() && x.front() == L' ') x.erase(x.begin());
            while (!x.empty() && x.back() == L' ') x.pop_back();
        }
        if (f.size() < 3) { c.errors.push_back(ln + L"يجب أن يحتوي على الصفحة والسورة والآية."); continue; }

        std::wstring p_txt = to_western_digits(f[0]), a_txt = to_western_digits(f[2]);
        bool p_num = !p_txt.empty(), a_num = !a_txt.empty();
        for (wchar_t ch : p_txt) if (!iswdigit(ch)) p_num = false;
        for (wchar_t ch : a_txt) if (!iswdigit(ch)) a_num = false;
        if (!p_num && rows.empty() && line_no <= 5) continue;   // سطر العناوين
        if (!p_num) { c.errors.push_back(ln + L"رقم الصفحة غير صحيح: " + f[0]); continue; }

        int page = _wtoi(p_txt.c_str());
        int surah = f[1].empty() ? last_surah : surah_from_text(f[1]);
        if (surah == 0) { c.errors.push_back(ln + L"تعذّر التعرّف على السورة: " + (f[1].empty() ? L"(فارغة في أول سطر)" : f[1])); continue; }
        if (!a_num) { c.errors.push_back(ln + L"رقم الآية غير صحيح: " + f[2]); continue; }
        int ayah = _wtoi(a_txt.c_str());
        last_surah = surah;

        auto it = key_to_id_.find({surah, ayah});
        if (it == key_to_id_.end()) {
            auto sm = surah_map_.find(surah);
            int cnt = (sm != surah_map_.end()) ? sm->second.verses_count : 0;
            c.errors.push_back(ln + L"لا توجد الآية " + std::to_wstring(ayah) + L" في سورة " +
                               (sm != surah_map_.end() ? sm->second.name_arabic : std::to_wstring(surah)) +
                               L" (عدد آياتها " + std::to_wstring(cnt) + L").");
            continue;
        }
        if (page != expected_page) {
            c.errors.push_back(ln + L"الصفحة " + std::to_wstring(page) + L" جاءت بدلًا من الصفحة " +
                               std::to_wstring(expected_page) + L" (صفحة ناقصة أو مكررة؟).");
            expected_page = page;
        }
        ++expected_page;
        if (!rows.empty() && it->second <= rows.back().second) {
            c.errors.push_back(ln + L"الصفحة " + std::to_wstring(page) + L" تبدأ بآية قبل بداية الصفحة السابقة أو عندها.");
        }
        rows.push_back({page, it->second});
    }

    if (rows.empty()) {
        if (c.errors.empty()) c.errors.push_back(L"لا توجد أي صفحة في الملف.");
        return c;
    }
    if (rows.front().second != 1) c.errors.push_back(L"الصفحة الأولى يجب أن تبدأ بأول الفاتحة (الفاتحة، الآية ١).");
    for (const auto& r : rows) c.page_starts.push_back(r.second);
    c.info.page_count = (int)c.page_starts.size();
    if (!c.errors.empty()) return c;

    // فحص الحدود بين الصفحات: صفحات المصحف متقاربة في كمية النص، والبداية المكتوبة خطأً تنقل
    // آيات من صفحة إلى جارتها، فتطول إحداهما وتقصر الأخرى معاً. نقارن كل صفحتين متجاورتين
    // بالصفحات القريبة منهما، ونحسب لعنوان السورة والبسملة مساحة سطرين تقريباً.
    if (ayah_text_len_.empty()) {
        ayah_text_len_.assign(max_id_ + 2, 0);
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db_, "SELECT id, length(text_simple) FROM ayahs", -1, &st, nullptr) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                int id = sqlite3_column_int(st, 0);
                if (id >= 0 && id < (int)ayah_text_len_.size()) ayah_text_len_[id] = sqlite3_column_int(st, 1);
            }
            sqlite3_finalize(st);
        }
    }
    const int n = (int)c.page_starts.size();
    std::set<int> surah_first_ids;
    for (const auto& kv : surah_id_bounds_) surah_first_ids.insert(kv.second.first);
    std::vector<long> raw(n, 0);
    std::vector<int> headers(n, 0);
    for (int i = 0; i < n; ++i) {
        int a = c.page_starts[i], b = (i + 1 < n) ? c.page_starts[i + 1] - 1 : max_id_;
        for (int id = a; id <= b && id < (int)ayah_text_len_.size(); ++id) {
            raw[i] += ayah_text_len_[id];
            if (surah_first_ids.count(id)) ++headers[i];
        }
    }
    std::vector<long> plain(raw.begin() + std::min(2, n), raw.end());
    if (n > 8 && !plain.empty()) {
        std::sort(plain.begin(), plain.end());
        const long header_w = plain[plain.size() / 2] / 8;   // عنوان السورة والبسملة ≈ سطران من ١٥
        std::vector<double> sz(n);
        for (int i = 0; i < n; ++i) sz[i] = (double)(raw[i] + headers[i] * header_w);
        for (int j = 3; j < n - 1; ++j) {          // الحد بين الصفحة j والصفحة j+1 (بترقيم يبدأ من ١)
            int i = j - 1;                          // فهرس الصفحة السابقة
            std::vector<double> nb;
            for (int x = std::max(2, i - 6); x <= std::min(n - 2, j + 6); ++x) {
                if (x != i && x != j) nb.push_back(sz[x]);
            }
            if (nb.size() < 3) continue;
            std::sort(nb.begin(), nb.end());
            double med = nb[nb.size() / 2];
            if (med <= 0) continue;
            double r1 = sz[i] / med, r2 = sz[j] / med;
            bool long_short = r1 > 1.15 && r2 < 0.85;
            bool short_long = r1 < 0.85 && r2 > 1.15;
            if (!long_short && !short_long) continue;
            std::wstring where;
            auto k = id_to_key_.find(c.page_starts[j]);
            if (k != id_to_key_.end()) {
                auto sm = surah_map_.find(k->second.first);
                where = L" (مكتوب أنها تبدأ بسورة " + (sm != surah_map_.end() ? sm->second.name_arabic : L"") +
                        L"، الآية " + std::to_wstring(k->second.second) + L")";
            }
            c.warnings.push_back(L"بداية الصفحة " + std::to_wstring(j + 1) + where + L": " +
                (long_short ? L"الصفحة التي قبلها أطول من المعتاد وهذه أقصر، فربما بدايتها متأخرة بآية أو أكثر."
                            : L"الصفحة التي قبلها أقصر من المعتاد وهذه أطول، فربما بدايتها متقدمة بآية أو أكثر."));
        }
    }
    // المصاحف المطبوعة بين ٥٠٠ و ٩٠٠ صفحة تقريباً؛ الملف الأقصر من ذلك لم يكتمل بعد
    // (تبقى الملاحظات أعلاه مفيدة لمراجعة ما أُدخل حتى الآن)
    if (n < 300) {
        c.errors.push_back(L"الملف فيه " + std::to_wstring(n) +
                           L" صفحة فقط، ويبدو أنه لم يكتمل بعد. أكمل بدايات بقية الصفحات حتى آخر المصحف.");
    }
    return c;
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
