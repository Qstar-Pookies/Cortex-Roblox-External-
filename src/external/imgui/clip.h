#pragma once

#ifndef CLIP_H_INCLUDED
#define CLIP_H_INCLUDED
#pragma once

#include <cassert>
#include <string>

namespace clip {

    typedef size_t format;

    class image;
    struct image_spec;

    class lock {
    public:

        lock(void* native_window_handle = nullptr);
        ~lock();

        bool locked() const;

        bool clear();

        bool is_convertible(format f) const;
        bool set_data(format f, const char* buf, size_t len);
        bool get_data(format f, char* buf, size_t len) const;
        size_t get_data_length(format f) const;

    private:
        class impl;
        impl* p;
    };

    format register_format(const std::string& name);

    format empty_format();

    format text_format();

    format image_format();

    bool has(format f);

    bool clear();

    enum class ErrorCode {
        CannotLock,
        ImageNotSupported,
    };

    typedef void (*error_handler)(ErrorCode code);

    void set_error_handler(error_handler f);
    error_handler get_error_handler();

    bool set_text(const std::string& value);
    bool get_text(std::string& value);

    struct image_spec {
        unsigned long width;
        unsigned long height;
        unsigned long bits_per_pixel;
        unsigned long bytes_per_row;
        unsigned long red_mask;
        unsigned long green_mask;
        unsigned long blue_mask;
        unsigned long alpha_mask;
        unsigned long red_shift;
        unsigned long green_shift;
        unsigned long blue_shift;
        unsigned long alpha_shift;
    };

    void set_x11_wait_timeout(int msecs);
    int get_x11_wait_timeout();

}

#endif

#ifndef CLIP_LOCK_IMPL_H_INCLUDED
#define CLIP_LOCK_IMPL_H_INCLUDED

namespace clip {

    class lock::impl {
    public:
        impl(void* native_window_handle);
        ~impl();

        bool locked() const { return m_locked; }
        bool clear();
        bool is_convertible(format f) const;
        bool set_data(format f, const char* buf, size_t len);
        bool get_data(format f, char* buf, size_t len) const;
        size_t get_data_length(format f) const;
        bool set_image(const image& image);
        bool get_image(image& image) const;
        bool get_image_spec(image_spec& spec) const;

    private:
        bool m_locked;
    };

}

#endif
