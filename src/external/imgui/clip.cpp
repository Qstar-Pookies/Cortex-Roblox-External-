#include "clip.h"

#include <vector>
#include <stdexcept>

namespace clip {

    namespace {

        void default_error_handler(ErrorCode code) {
            static const char* err[] = {
              ("Cannot lock clipboard"),
              ("Image format is not supported")
            };
            throw std::runtime_error(err[static_cast<int>(code)]);
        }

    }

    error_handler g_error_handler = default_error_handler;

    lock::lock(void* native_window_handle)
        : p(new impl(native_window_handle)) {
    }

    lock::~lock() {
        delete p;
    }

    bool lock::locked() const {
        return p->locked();
    }

    bool lock::clear() {
        return p->clear();
    }

    bool lock::is_convertible(format f) const {
        return p->is_convertible(f);
    }

    bool lock::set_data(format f, const char* buf, size_t length) {
        return p->set_data(f, buf, length);
    }

    bool lock::get_data(format f, char* buf, size_t len) const {
        return p->get_data(f, buf, len);
    }

    size_t lock::get_data_length(format f) const {
        return p->get_data_length(f);
    }

    format empty_format() { return 0; }
    format text_format() { return 1; }
    format image_format() { return 2; }

    bool has(format f) {
        lock l;
        if (l.locked())
            return l.is_convertible(f);
        else
            return false;
    }

    bool clear() {
        lock l;
        if (l.locked())
            return l.clear();
        else
            return false;
    }

    bool set_text(const std::string& value) {
        lock l;
        if (l.locked()) {
            l.clear();
            return l.set_data(text_format(), value.c_str(), value.size());
        }
        else
            return false;
    }

    bool get_text(std::string& value) {
        lock l;
        if (!l.locked())
            return false;

        format f = text_format();
        if (!l.is_convertible(f))
            return false;

        size_t len = l.get_data_length(f);
        if (len > 0) {
            std::vector<char> buf(len);
            l.get_data(f, &buf[0], len);
            value = &buf[0];
            return true;
        }
        else {
            value.clear();
            return true;
        }
    }

    void set_error_handler(error_handler handler) {
        g_error_handler = handler;
    }

    error_handler get_error_handler() {
        return g_error_handler;
    }

#ifdef HAVE_XCB_XLIB_H
    static int g_x11_timeout = 1000;
    void set_x11_wait_timeout(int msecs) { g_x11_timeout = msecs; }
    int get_x11_wait_timeout() { return g_x11_timeout; }
#else
    void set_x11_wait_timeout(int) { }
    int get_x11_wait_timeout() { return 1000; }
#endif

}

#include "clip.h"

#include <cassert>
#include <vector>
#include <windows.h>

#ifndef LCS_WINDOWS_COLOR_SPACE
#define LCS_WINDOWS_COLOR_SPACE 'Win '
#endif

#ifndef CF_DIBV5
#define CF_DIBV5            17
#endif

namespace clip {

    namespace {

        unsigned long get_shift_from_mask(unsigned long mask) {
            unsigned long shift = 0;
            for (shift = 0; shift < sizeof(unsigned long) * 8; ++shift)
                if (mask & (1 << shift))
                    return shift;
            return shift;
        }

        class Hglobal {
        public:
            Hglobal() : m_handle(nullptr) {
            }

            explicit Hglobal(HGLOBAL handle) : m_handle(handle) {
            }

            explicit Hglobal(size_t len) : m_handle(GlobalAlloc(GHND, len)) {
            }

            ~Hglobal() {
                if (m_handle)
                    GlobalFree(m_handle);
            }

            void release() {
                m_handle = nullptr;
            }

            operator HGLOBAL() {
                return m_handle;
            }

        private:
            HGLOBAL m_handle;
        };

    }

    lock::impl::impl(void* hwnd) : m_locked(false) {
        for (int i = 0; i < 5; ++i) {
            if (OpenClipboard((HWND)hwnd)) {
                m_locked = true;
                break;
            }
            Sleep(20);
        }

        if (!m_locked) {
            error_handler e = get_error_handler();
            if (e)
                e(ErrorCode::CannotLock);
        }
    }

    lock::impl::~impl() {
        if (m_locked)
            CloseClipboard();
    }

    bool lock::impl::clear() {
        return (EmptyClipboard() ? true : false);
    }

    bool lock::impl::is_convertible(format f) const {
        if (f == text_format()) {
            return
                (IsClipboardFormatAvailable(CF_TEXT) ||
                    IsClipboardFormatAvailable(CF_UNICODETEXT) ||
                    IsClipboardFormatAvailable(CF_OEMTEXT));
        }
        else if (f == image_format()) {
            return (IsClipboardFormatAvailable(CF_DIB) ? true : false);
        }
        else if (IsClipboardFormatAvailable(f))
            return true;
        else
            return false;
    }

    bool lock::impl::set_data(format f, const char* buf, size_t len) {
        bool result = false;

        if (f == text_format()) {
            if (len > 0) {
                int reqsize = MultiByteToWideChar(CP_UTF8, 0, buf, len, NULL, 0);
                if (reqsize > 0) {
                    ++reqsize;

                    Hglobal hglobal(sizeof(WCHAR) * reqsize);
                    LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
                    MultiByteToWideChar(CP_UTF8, 0, buf, len, lpstr, reqsize);
                    GlobalUnlock(hglobal);

                    result = (SetClipboardData(CF_UNICODETEXT, hglobal)) ? true : false;
                    if (result)
                        hglobal.release();
                }
            }
        }
        else {
            Hglobal hglobal(len + sizeof(size_t));
            if (hglobal) {
                size_t* dst = (size_t*)GlobalLock(hglobal);
                if (dst) {
                    *dst = len;
                    memcpy(dst + 1, buf, len);
                    GlobalUnlock(hglobal);
                    result = (SetClipboardData(f, hglobal) ? true : false);
                    if (result)
                        hglobal.release();
                }
            }
        }

        return result;
    }

    bool lock::impl::get_data(format f, char* buf, size_t len) const {
        assert(buf);

        if (!buf || !is_convertible(f))
            return false;

        bool result = false;

        if (f == text_format()) {
            if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
                HGLOBAL hglobal = GetClipboardData(CF_UNICODETEXT);
                if (hglobal) {
                    LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
                    if (lpstr) {
                        size_t reqsize =
                            WideCharToMultiByte(CP_UTF8, 0, lpstr, -1,
                                nullptr, 0, nullptr, nullptr);

                        assert(reqsize <= len);
                        if (reqsize <= len) {
                            WideCharToMultiByte(CP_UTF8, 0, lpstr, -1,
                                buf, reqsize, nullptr, nullptr);
                            result = true;
                        }
                        GlobalUnlock(hglobal);
                    }
                }
            }
            else if (IsClipboardFormatAvailable(CF_TEXT)) {
                HGLOBAL hglobal = GetClipboardData(CF_TEXT);
                if (hglobal) {
                    LPSTR lpstr = static_cast<LPSTR>(GlobalLock(hglobal));
                    if (lpstr) {

                        memcpy(buf, lpstr, len);
                        result = true;
                        GlobalUnlock(hglobal);
                    }
                }
            }
        }
        else {
            if (IsClipboardFormatAvailable(f)) {
                HGLOBAL hglobal = GetClipboardData(f);
                if (hglobal) {
                    const size_t* ptr = (const size_t*)GlobalLock(hglobal);
                    if (ptr) {
                        size_t reqsize = *ptr;
                        assert(reqsize <= len);
                        if (reqsize <= len) {
                            memcpy(buf, ptr + 1, reqsize);
                            result = true;
                        }
                        GlobalUnlock(hglobal);
                    }
                }
            }
        }

        return result;
    }

    size_t lock::impl::get_data_length(format f) const {
        size_t len = 0;

        if (f == text_format()) {
            if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
                HGLOBAL hglobal = GetClipboardData(CF_UNICODETEXT);
                if (hglobal) {
                    LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
                    if (lpstr) {
                        len =
                            WideCharToMultiByte(CP_UTF8, 0, lpstr, -1,
                                nullptr, 0, nullptr, nullptr);
                        GlobalUnlock(hglobal);
                    }
                }
            }
            else if (IsClipboardFormatAvailable(CF_TEXT)) {
                HGLOBAL hglobal = GetClipboardData(CF_TEXT);
                if (hglobal) {
                    LPSTR lpstr = (LPSTR)GlobalLock(hglobal);
                    if (lpstr) {
                        len = strlen(lpstr) + 1;
                        GlobalUnlock(hglobal);
                    }
                }
            }
        }

        else if (f != empty_format()) {
            if (IsClipboardFormatAvailable(f)) {
                HGLOBAL hglobal = GetClipboardData(f);
                if (hglobal) {
                    const size_t* ptr = (const size_t*)GlobalLock(hglobal);
                    if (ptr) {
                        len = *ptr;
                        GlobalUnlock(hglobal);
                    }
                }
            }
        }

        return len;
    }

    format register_format(const std::string& name) {
        int reqsize = 1 + MultiByteToWideChar(CP_UTF8, 0,
            name.c_str(), name.size(), NULL, 0);
        std::vector<WCHAR> buf(reqsize);
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name.size(),
            &buf[0], reqsize);

        return (format)RegisterClipboardFormatW(&buf[0]);
    }

}
