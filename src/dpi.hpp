#pragma once
// -*- coding: utf-8 -*-
// dpi.hpp — تكيّف البرنامج مع دقة كل شاشة (تكبير النص في ويندوز: ١٠٠٪، ١٢٥٪، ١٥٠٪، ٢٠٠٪...).
// كل المقاسات في الكود مكتوبة لشاشة ٩٦ نقطة في البوصة، وتُضرب هنا في دقة الشاشة الحالية.

#include <windows.h>
#include <algorithm>
#include <vector>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace mushaf {

// دقة الشاشة التي عليها النافذة الرئيسية الآن (تتغير عند نقلها إلى شاشة أخرى)
inline int& ui_dpi() {
    static int d = 96;
    return d;
}

inline int DS(int v) { return MulDiv(v, ui_dpi(), 96); }

// دقة نافذة بعينها؛ ويندوز ٧ و٨ لا يعرفان الدقة لكل شاشة فنأخذ دقة النظام
inline int window_dpi(HWND h) {
    // للاختبار فقط: MUSHAF_TEST_DPI=144 يحاكي تكبير ١٥٠٪ على شاشة عادية
    static int forced = [] {
        wchar_t buf[16] = {0};
        return GetEnvironmentVariableW(L"MUSHAF_TEST_DPI", buf, 16) ? _wtoi(buf) : 0;
    }();
    if (forced >= 72 && forced <= 480) return forced;
    using Fn = UINT(WINAPI*)(HWND);
    static Fn fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn && h) {
        UINT d = fn(h);
        if (d) return (int)d;
    }
    HDC dc = GetDC(nullptr);
    int d = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return d > 0 ? d : 96;
}

// منطقة العمل (بلا شريط المهام) في الشاشة التي فيها النافذة
inline RECT work_area_of(HWND h) {
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
    return mi.rcWork;
}

// النوافذ الفرعية مكتوبة بمقاسات ٩٦ نقطة: نكبّر عناصرها بدقة الشاشة، ونصغّر إن لم تتسع الشاشة،
// ثم نوسّطها فوق النافذة الأم داخل حدود الشاشة. تُستدعى بعد إنشاء كل العناصر.
inline void fit_dialog_to_screen(HWND dlg, HWND owner) {
    if (!dlg) return;
    RECT wr, cr;
    GetWindowRect(dlg, &wr);
    GetClientRect(dlg, &cr);
    const int nc_w = (wr.right - wr.left) - cr.right;
    const int nc_h = (wr.bottom - wr.top) - cr.bottom;
    const RECT work = work_area_of(owner ? owner : dlg);
    const int work_w = work.right - work.left, work_h = work.bottom - work.top;

    // المقياس: دقة الشاشة، مع حد أعلى يُبقي النافذة كلها داخل الشاشة
    double s = window_dpi(dlg) / 96.0;
    if (cr.right > 0 && cr.bottom > 0) {
        double fit_w = (work_w - nc_w - 8) / (double)cr.right;
        double fit_h = (work_h - nc_h - 8) / (double)cr.bottom;
        s = std::min(s, std::min(fit_w, fit_h));
    }
    if (s < 0.5) s = 0.5;

    if (s < 0.99 || s > 1.01) {
        // نجمع مواضع العناصر أولاً ثم نحرّكها، حتى لا يؤثر تحريك عنصر على قياس غيره
        struct Item { HWND h; RECT r; };
        std::vector<Item> items;
        for (HWND c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
            RECT r;
            GetWindowRect(c, &r);
            MapWindowPoints(HWND_DESKTOP, dlg, reinterpret_cast<POINT*>(&r), 2);
            if (r.left > r.right) std::swap(r.left, r.right);   // النافذة المعكوسة تقلب طرفي المستطيل
            wchar_t cls[32] = {0};
            GetClassNameW(c, cls, 32);
            if (lstrcmpiW(cls, L"ComboBox") == 0) {
                // ارتفاع القائمة المنسدلة هو المطلوب لا ارتفاعها مغلقة
                RECT dr{};
                if (SendMessageW(c, CB_GETDROPPEDCONTROLRECT, 0, (LPARAM)&dr) && dr.bottom > dr.top)
                    r.bottom = r.top + (dr.bottom - dr.top);
            }
            items.push_back({c, r});
        }
        auto sc = [s](LONG v) { return (int)(v * s + (v >= 0 ? 0.5 : -0.5)); };
        for (const auto& it : items) {
            SetWindowPos(it.h, nullptr, sc(it.r.left), sc(it.r.top),
                         sc(it.r.right - it.r.left), sc(it.r.bottom - it.r.top),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        // إن صغّرنا النافذة عن دقة الشاشة لتتسع لها، يصغر خطها بالنسبة نفسها حتى لا تُقصّ الكتابة
        const double dpi_s = window_dpi(dlg) / 96.0;
        if (s < dpi_s * 0.98 && !items.empty()) {
            HFONT cur = (HFONT)SendMessageW(items.front().h, WM_GETFONT, 0, 0);
            LOGFONTW lf{};
            if (cur && GetObjectW(cur, sizeof(lf), &lf)) {
                lf.lfHeight = (LONG)(lf.lfHeight * s / dpi_s);
                lf.lfWidth = 0;
                // خط واحد لكل حجم يُعاد استعماله طوال عمل البرنامج
                static std::vector<std::pair<LONG, HFONT>> cache;
                HFONT f = nullptr;
                for (const auto& c : cache) if (c.first == lf.lfHeight) f = c.second;
                if (!f) {
                    f = CreateFontIndirectW(&lf);
                    cache.push_back({lf.lfHeight, f});
                }
                for (const auto& it : items) {
                    if ((HFONT)SendMessageW(it.h, WM_GETFONT, 0, 0) == cur) SendMessageW(it.h, WM_SETFONT, (WPARAM)f, TRUE);
                }
            }
        }
    }

    const int w = (int)(cr.right * s) + nc_w, h = (int)(cr.bottom * s) + nc_h;
    RECT orc;
    if (owner) GetWindowRect(owner, &orc); else orc = work;
    int x = (orc.left + orc.right) / 2 - w / 2;
    int y = (orc.top + orc.bottom) / 2 - h / 2;
    x = std::max((int)work.left, std::min(x, (int)work.right - w));
    y = std::max((int)work.top, std::min(y, (int)work.bottom - h));
    SetWindowPos(dlg, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

} // namespace mushaf
