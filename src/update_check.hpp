#pragma once
// البحث عن إصدار أحدث من البرنامج في صفحة الإصدارات على GitHub.
// يقرأ آخر إصدار منشور فقط ولا ينزّل شيئًا: إن وُجد إصدار أحدث عرض البرنامج رسالة
// وفتح صفحة التنزيل في المتصفح بموافقة المستخدم.
// مكتبة WinHTTP تُحمَّل وقت التشغيل (موجودة في ويندوز منذ إصدار ٧)، فلا يتغيّر شيء في البناء.

#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <string>
#include <vector>
#include "utf8_util.hpp"

// رقم الإصدار الحالي (يطابق src/app.rc و installer.iss)
constexpr const wchar_t* kAppVersion = L"1.0.0";

// مصدر الإصدارات وصفحتها
constexpr const wchar_t* kUpdateApiHost = L"api.github.com";
constexpr const wchar_t* kUpdateApiPath = L"/repos/omnya-tech/AlMushafAlSharif/releases/latest";
constexpr const wchar_t* kReleasesPageUrl = L"https://github.com/omnya-tech/AlMushafAlSharif/releases/latest";

struct UpdateCheckResult {
    enum class Status { Newer, UpToDate, NoRelease, Failed };
    Status status = Status::Failed;
    std::wstring latest_version;   // بلا البادئة v، كـ«1.1.0»
    std::wstring page_url;         // صفحة الإصدار على GitHub
    bool manual = false;           // طلبه المستخدم من القائمة (لا البحث التلقائي)
};

// «v1.2.3» → {1, 2, 3}؛ يُتجاهل ما بعد الأرقام (كـ«-beta»)
inline std::vector<int> parse_version_numbers(const std::wstring& v) {
    std::vector<int> out;
    size_t i = 0;
    while (i < v.size() && !(v[i] >= L'0' && v[i] <= L'9')) ++i;
    while (i < v.size()) {
        int n = 0;
        bool any = false;
        while (i < v.size() && v[i] >= L'0' && v[i] <= L'9') { n = n * 10 + (v[i] - L'0'); ++i; any = true; }
        if (!any) break;
        out.push_back(n);
        if (i < v.size() && v[i] == L'.') ++i; else break;
    }
    return out;
}

// سالب إن كان a أقدم، وصفر إن تساويا، وموجب إن كان a أحدث
inline int compare_versions(const std::wstring& a, const std::wstring& b) {
    std::vector<int> x = parse_version_numbers(a), y = parse_version_numbers(b);
    const size_t n = std::max(x.size(), y.size());
    for (size_t i = 0; i < n; ++i) {
        int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

inline std::wstring strip_version_prefix(const std::wstring& tag) {
    size_t i = 0;
    while (i < tag.size() && !(tag[i] >= L'0' && tag[i] <= L'9')) ++i;
    return tag.substr(i);
}

// قيمة نصية لمفتاح في JSON بسيط ("key": "value")، مع فك المحارف المهرَّبة
inline bool json_string_value(const std::string& json, const char* key, std::wstring& out) {
    const std::string needle = std::string("\"") + key + "\"";
    size_t p = json.find(needle);
    while (p != std::string::npos) {
        size_t q = p + needle.size();
        while (q < json.size() && (json[q] == ' ' || json[q] == '\t' || json[q] == '\r' || json[q] == '\n')) ++q;
        if (q < json.size() && json[q] == ':') {
            ++q;
            while (q < json.size() && (json[q] == ' ' || json[q] == '\t' || json[q] == '\r' || json[q] == '\n')) ++q;
            if (q >= json.size() || json[q] != '"') return false;
            ++q;
            std::string utf8;
            while (q < json.size() && json[q] != '"') {
                char c = json[q++];
                if (c != '\\') { utf8.push_back(c); continue; }
                if (q >= json.size()) break;
                char e = json[q++];
                switch (e) {
                    case 'n': utf8.push_back('\n'); break;
                    case 't': utf8.push_back('\t'); break;
                    case 'r': utf8.push_back('\r'); break;
                    case 'b': case 'f': break;
                    case 'u': {
                        if (q + 4 > json.size()) return false;
                        unsigned cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = json[q + k];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                            else return false;
                        }
                        q += 4;
                        std::wstring w(1, (wchar_t)cp);
                        utf8 += mushaf::to_utf8(w);
                        break;
                    }
                    default: utf8.push_back(e); break;
                }
            }
            out = mushaf::to_wide(utf8);
            return true;
        }
        p = json.find(needle, p + 1);
    }
    return false;
}

// يقرأ آخر إصدار منشور. لا يُستدعى من خيط الواجهة لأنه ينتظر الشبكة.
inline UpdateCheckResult fetch_latest_release() {
    UpdateCheckResult r;
    HMODULE lib = LoadLibraryW(L"winhttp.dll");
    if (!lib) return r;

    using fn_open_t = HINTERNET (WINAPI*)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
    using fn_setopt_t = BOOL (WINAPI*)(HINTERNET, DWORD, LPVOID, DWORD);
    using fn_timeouts_t = BOOL (WINAPI*)(HINTERNET, int, int, int, int);
    using fn_connect_t = HINTERNET (WINAPI*)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
    using fn_openreq_t = HINTERNET (WINAPI*)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
    using fn_send_t = BOOL (WINAPI*)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
    using fn_recv_t = BOOL (WINAPI*)(HINTERNET, LPVOID);
    using fn_query_t = BOOL (WINAPI*)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);
    using fn_read_t = BOOL (WINAPI*)(HINTERNET, LPVOID, DWORD, LPDWORD);
    using fn_close_t = BOOL (WINAPI*)(HINTERNET);

    auto p_open = reinterpret_cast<fn_open_t>(GetProcAddress(lib, "WinHttpOpen"));
    auto p_setopt = reinterpret_cast<fn_setopt_t>(GetProcAddress(lib, "WinHttpSetOption"));
    auto p_timeouts = reinterpret_cast<fn_timeouts_t>(GetProcAddress(lib, "WinHttpSetTimeouts"));
    auto p_connect = reinterpret_cast<fn_connect_t>(GetProcAddress(lib, "WinHttpConnect"));
    auto p_openreq = reinterpret_cast<fn_openreq_t>(GetProcAddress(lib, "WinHttpOpenRequest"));
    auto p_send = reinterpret_cast<fn_send_t>(GetProcAddress(lib, "WinHttpSendRequest"));
    auto p_recv = reinterpret_cast<fn_recv_t>(GetProcAddress(lib, "WinHttpReceiveResponse"));
    auto p_query = reinterpret_cast<fn_query_t>(GetProcAddress(lib, "WinHttpQueryHeaders"));
    auto p_read = reinterpret_cast<fn_read_t>(GetProcAddress(lib, "WinHttpReadData"));
    auto p_close = reinterpret_cast<fn_close_t>(GetProcAddress(lib, "WinHttpCloseHandle"));
    if (!p_open || !p_setopt || !p_timeouts || !p_connect || !p_openreq || !p_send ||
        !p_recv || !p_query || !p_read || !p_close) {
        FreeLibrary(lib);
        return r;
    }

    const std::wstring agent = std::wstring(L"AlMushafAlSharif/") + kAppVersion;
    // الوكيل التلقائي (ويندوز ٨٫١ فأحدث)، وإلا وكيل النظام
    HINTERNET session = p_open(agent.c_str(), 4 /* WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY */, nullptr, nullptr, 0);
    if (!session) session = p_open(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        p_timeouts(session, 8000, 8000, 10000, 10000);
        // GitHub يطلب TLS 1.2؛ في ويندوز ٧ يلزم تفعيله صراحةً (ويعمل بعد تحديثات النظام)
        DWORD protocols = 0x00000080 | 0x00000200 | 0x00000800;   // TLS 1.0 | 1.1 | 1.2
        p_setopt(session, 84 /* WINHTTP_OPTION_SECURE_PROTOCOLS */, &protocols, sizeof(protocols));

        HINTERNET conn = p_connect(session, kUpdateApiHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (conn) {
            HINTERNET req = p_openreq(conn, L"GET", kUpdateApiPath, nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (req) {
                const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
                if (p_send(req, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && p_recv(req, nullptr)) {
                    DWORD code = 0, size = sizeof(code);
                    p_query(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX);
                    std::string body;
                    char buf[4096];
                    DWORD got = 0;
                    while (body.size() < 512 * 1024 && p_read(req, buf, sizeof(buf), &got) && got > 0) body.append(buf, got);

                    if (code == 404) {
                        r.status = UpdateCheckResult::Status::NoRelease;
                    } else if (code == 200) {
                        std::wstring tag, url;
                        if (json_string_value(body, "tag_name", tag) && !parse_version_numbers(tag).empty()) {
                            json_string_value(body, "html_url", url);
                            r.latest_version = strip_version_prefix(tag);
                            // لا تُفتح إلا صفحة من مستودع البرنامج نفسه
                            const std::wstring base = L"https://github.com/omnya-tech/AlMushafAlSharif/";
                            r.page_url = (url.compare(0, base.size(), base) == 0) ? url : kReleasesPageUrl;
                            r.status = compare_versions(tag, kAppVersion) > 0 ? UpdateCheckResult::Status::Newer
                                                                              : UpdateCheckResult::Status::UpToDate;
                        }
                    }
                }
                p_close(req);
            }
            p_close(conn);
        }
        p_close(session);
    }
    FreeLibrary(lib);
    return r;
}
