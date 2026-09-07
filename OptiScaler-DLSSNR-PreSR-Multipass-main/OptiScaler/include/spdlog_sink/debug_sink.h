#pragma once

#if defined(_WIN32)

#include <spdlog/details/null_mutex.h>
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
#include <spdlog/details/os.h>
#endif
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

// Avoid including windows.h (https://stackoverflow.com/a/30741042)
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
typedef void (*PFN_OutputDebugStringW)(const wchar_t* lpOutputString);
PFN_OutputDebugStringW o_OutputDebugStringW = nullptr;
#else
typedef void (*PFN_OutputDebugStringA)(const char* lpOutputString);
PFN_OutputDebugStringA o_OutputDebugStringA = nullptr;
#endif

namespace spdlog
{
namespace sinks
{
/*
 * Debug sink (logging using kernel32 OutputDebugStringA)
 */
template <typename Mutex> class debug_sink : public base_sink<Mutex>
{
  public:
    debug_sink()
    {
        auto kernelModule = GetModuleHandle(L"kernel32.dll");

#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
        o_OutputDebugStringW = (PFN_OutputDebugStringW) GetProcAddress(kernelModule, "OutputDebugStringW");
#else
        o_OutputDebugStringA = (PFN_OutputDebugStringA) GetProcAddress(kernelModule, "OutputDebugStringA");
#endif
    }

  protected:
    void sink_it_(const details::log_msg& msg) override
    {
        memory_buf_t formatted;
        base_sink<Mutex>::formatter_->format(msg, formatted);
        formatted.push_back('\0'); // add a null terminator for OutputDebugString
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
        wmemory_buf_t wformatted;
        details::os::utf8_to_wstrbuf(string_view_t(formatted.data(), formatted.size()), wformatted);
        o_OutputDebugStringW(wformatted.data());
#else
        o_OutputDebugStringA(formatted.data());
#endif
    }

    void flush_() override {}

    bool check_debugger_present_ = true;
};

using debug_sink_mt = debug_sink<std::mutex>;
using debug_sink_st = debug_sink<details::null_mutex>;

} // namespace sinks
} // namespace spdlog

#endif
