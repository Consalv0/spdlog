// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/wincolor_sink.h>
#endif

#include <spdlog/common.h>
#include <spdlog/details/pattern_formatter.h>

namespace spdlog {
namespace sinks {

template<typename ConsoleMutex>
SPDLOG_INLINE wincolor_sink<ConsoleMutex>::wincolor_sink(HANDLE out_handle, color_mode mode)
    : out_handle_(out_handle)
    , mutex_(ConsoleMutex::mutex())
    , formatter_(details::make_unique<spdlog::pattern_formatter>())
{
    // check if out_handle is points to the actual console.
    // ::GetConsoleMode() should return 0 if it is redirected or not valid console handle.
    DWORD console_mode;
    in_console_ = ::GetConsoleMode(out_handle, &console_mode) != 0;

    set_color_mode(mode);
    colors_[level::trace] = WHITE;
    colors_[level::debug] = GREEN;
    colors_[level::info] = BOLD;
    colors_[level::warn] = YELLOW | BOLD;
    colors_[level::err] = RED | BOLD;                         // red bold
    colors_[level::critical] = BACKGROUND_RED | WHITE | BOLD; // white bold on red background
    colors_[level::off] = 0;
}

template<typename ConsoleMutex>
SPDLOG_INLINE wincolor_sink<ConsoleMutex>::~wincolor_sink()
{
    this->flush();
}

// change the color for the given level
template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::set_color(level::level_enum level, WORD color)
{
    std::lock_guard<mutex_t> lock(mutex_);
    colors_[level] = color;
}

template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::log(const details::log_msg &msg)
{
    std::lock_guard<mutex_t> lock(mutex_);
    memory_buf_t formatted;
    formatter_->format(msg, formatted);
    if (!in_console_)
    {
        write_to_file_(formatted);
        return;
    }

    if (should_do_colors_ && msg.color_range_end > msg.color_range_start)
    {
        // before color range
        print_range_(formatted, 0, msg.color_range_start);

        // in color range
        auto orig_attribs = set_foreground_color_(colors_[msg.level]);
        print_range_(formatted, msg.color_range_start, msg.color_range_end);
        // reset to orig colors
        ::SetConsoleTextAttribute(out_handle_, orig_attribs);
        print_range_(formatted, msg.color_range_end, formatted.size());
    }
    else // print without colors if color range is invalid (or color is disabled)
    {
        print_range_(formatted, 0, formatted.size());
    }
}

template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::flush()
{
    // windows console always flushed?
}

template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::set_pattern(const std::string &pattern)
{
    std::lock_guard<mutex_t> lock(mutex_);
    formatter_ = std::unique_ptr<spdlog::formatter>(new pattern_formatter(pattern));
}

template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter)
{
    std::lock_guard<mutex_t> lock(mutex_);
    formatter_ = std::move(sink_formatter);
}

template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::set_color_mode(color_mode mode)
{
    switch (mode)
    {
    case color_mode::always:
    case color_mode::automatic:
        should_do_colors_ = true;
        break;
    case color_mode::never:
        should_do_colors_ = false;
        break;
    default:
        should_do_colors_ = true;
    }
}

// set foreground color and return the orig console attributes (for resetting later)
template<typename ConsoleMutex>
WORD SPDLOG_INLINE wincolor_sink<ConsoleMutex>::set_foreground_color_(WORD attribs)
{
    CONSOLE_SCREEN_BUFFER_INFO orig_buffer_info;
    ::GetConsoleScreenBufferInfo(out_handle_, &orig_buffer_info);
    WORD back_color = orig_buffer_info.wAttributes;
    // retrieve the current background color
    back_color &= static_cast<WORD>(~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY));
    // keep the background color unchanged
    ::SetConsoleTextAttribute(out_handle_, attribs | back_color);
    return orig_buffer_info.wAttributes; // return orig attribs
}

// print a range of formatted message to console
template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::print_range_(const memory_buf_t &formatted, size_t start, size_t end)
{
    auto size = static_cast<DWORD>(end - start);
    ::WriteConsoleA(out_handle_, formatted.data() + start, size, nullptr, nullptr);
}

template<typename ConsoleMutex>
void SPDLOG_INLINE wincolor_sink<ConsoleMutex>::write_to_file_(const memory_buf_t &formatted)
{
    if (out_handle_ == nullptr) // no console and no file redirect
    {
        return;
    }
    auto size = static_cast<DWORD>(formatted.size());
    if (size == 0)
    {
        return;
    }

    DWORD total_written = 0;
    do
    {
        DWORD bytes_written = 0;
        bool ok = ::WriteFile(out_handle_, formatted.data() + total_written, size - total_written, &bytes_written, nullptr) != 0;
        if (!ok || bytes_written == 0)
        {
            SPDLOG_THROW(spdlog_ex("wincolor_sink: write_to_file_ failed. GetLastError(): " + std::to_string(::GetLastError())));
        }
        total_written += bytes_written;
    } while (total_written < size);
}

// wincolor_stdout_sink
template<typename ConsoleMutex>
SPDLOG_INLINE wincolor_stdout_sink<ConsoleMutex>::wincolor_stdout_sink(color_mode mode)
    : wincolor_sink<ConsoleMutex>(::GetStdHandle(STD_OUTPUT_HANDLE), mode)
{}

// wincolor_stderr_sink
template<typename ConsoleMutex>
SPDLOG_INLINE wincolor_stderr_sink<ConsoleMutex>::wincolor_stderr_sink(color_mode mode)
    : wincolor_sink<ConsoleMutex>(::GetStdHandle(STD_ERROR_HANDLE), mode)
{}

} // namespace sinks
} // namespace spdlog
