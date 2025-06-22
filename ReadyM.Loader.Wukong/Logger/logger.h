#pragma once
#include <cstdint>
#include <format>
#include <chrono>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <mutex>
#include <sstream>
#include <utility>


template<typename T>
concept is_wformatable = requires {
    typename std::formatter<T, wchar_t>;
};


template<typename T>
concept is_formatable = requires {
    typename std::formatter<T, char>;
};


static inline std::wstring get_wtimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto seconds = time_point_cast<std::chrono::seconds>(now);
    auto fraction = now - seconds;

    std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm;
#if defined(_MSC_VER)
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif

    std::wostringstream oss;
    oss << std::put_time(&local_tm, L"%m-%d %H:%M:%S");
    oss << L'.' << std::setw(3) << std::setfill(L'0')
        << std::chrono::duration_cast<std::chrono::milliseconds>(fraction).count();
    return oss.str();
}


static inline std::string get_timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto seconds = time_point_cast<std::chrono::seconds>(now);
    auto fraction = now - seconds;

    std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm;
#if defined(_MSC_VER)
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%m-%d %H:%M:%S");
    oss << '.' << std::setw(3) << std::setfill('0')
        << std::chrono::duration_cast<std::chrono::milliseconds>(fraction).count();
    return oss.str();
}


static std::mutex g_log_mutex;


template<typename... FormatArgs>
    requires (is_wformatable<FormatArgs> && ...)
void log_impl(const std::wstring& prefix, std::wformat_string<FormatArgs...> fmt, FormatArgs&&... fmt_args)
{
    std::lock_guard lock(g_log_mutex);

    std::wstring message = std::vformat(fmt.get(), std::make_wformat_args(fmt_args...));

    std::wcout << get_wtimestamp()
               << L" [" << prefix << L"] "
               << message
               << L"\n" << std::flush;
}


template<typename... FormatArgs>
    requires (is_formatable<FormatArgs> && ...)
void log_impl(const std::string& prefix, std::format_string<FormatArgs...> fmt, FormatArgs&&... fmt_args)
{
    std::lock_guard lock(g_log_mutex);

    std::string message = std::vformat(fmt.get(), std::make_format_args(fmt_args...));

    std::cout << get_timestamp()
              << " [" << prefix << "] "
              << message
              << "\n" << std::flush;
}


template<typename... FormatArgs>
    requires (is_wformatable<FormatArgs> && ...)
void log_debug(std::wformat_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl(L"D", fmt, std::forward<FormatArgs>(fmt_args)...);
}

template<typename... FormatArgs>
    requires (is_wformatable<FormatArgs> && ...)
void log_info(std::wformat_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl(L"I", fmt, std::forward<FormatArgs>(fmt_args)...);
}

template<typename... FormatArgs>
    requires (is_wformatable<FormatArgs> && ...)
void log_warn(std::wformat_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl(L"W", fmt, std::forward<FormatArgs>(fmt_args)...);
}

template<typename... FormatArgs>
    requires (is_wformatable<FormatArgs> && ...)
void log_error(std::wformat_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl(L"E", fmt, std::forward<FormatArgs>(fmt_args)...);
}


template<typename... FormatArgs>
    requires (is_formatable<FormatArgs> && ...)
void log_debug(std::format_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl("D", fmt, std::forward<FormatArgs>(fmt_args)...);
}

template<typename... FormatArgs>
    requires (is_formatable<FormatArgs> && ...)
void log_info(std::format_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl("I", fmt, std::forward<FormatArgs>(fmt_args)...);
}

template<typename... FormatArgs>
    requires (is_formatable<FormatArgs> && ...)
void log_warn(std::format_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl("W", fmt, std::forward<FormatArgs>(fmt_args)...);
}

template<typename... FormatArgs>
    requires (is_formatable<FormatArgs> && ...)
void log_error(std::format_string<FormatArgs...> fmt, FormatArgs&&... fmt_args) {
    log_impl("E", fmt, std::forward<FormatArgs>(fmt_args)...);
}


void log_debug_ptr(const std::string& name, void* ptr);
void log_debug_ptr(const std::string& name, uint64_t ptr);
void log_error_missing_ptr(const std::string& name);
