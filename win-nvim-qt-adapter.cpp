// Copyright © 2023 Yegor Bychin <j@siga.icu>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See the COPYING file for more details.

#include <codecvt>
#include <format>
#include <iomanip>
#include <iostream>
#include <locale>
#include <ranges>
#include <span>

#include <windows.h>
#include <shlwapi.h>

template<typename T, bool round_brackets = true>
class construct_t
{
public:
    template<typename... Args>
    T operator()(Args&&... args) const
    {
        if constexpr(round_brackets) {
            return T(std::forward<Args>(args)...);
        } else {
            return T{std::forward<Args>(args)...};
        }
    }
};

class local_free_t
{
public:
    void operator()(void* ptr) const
    {
        ::LocalFree(ptr);
    }
};

std::error_code make_ec(int ec = static_cast<int>(::GetLastError()))
{
    return std::error_code{static_cast<int>(ec), std::system_category()};
}

class errno_exception_t : public std::system_error
{
public:
    errno_exception_t() : errno_exception_t{::GetLastError()} {}
    errno_exception_t(DWORD ec) : std::system_error{make_ec(static_cast<int>(ec))} {}
};

class argv_t
{
public:
    [[nodiscard]] argv_t(std::wstring_view cmd)
        : argv_{::CommandLineToArgvW(cmd.data(), &argc_)}
    {
        if(argv_ == nullptr || argc_ < 0) {
            throw errno_exception_t{};
        }
    }

public:
    auto raw_span() const
    {
        return std::span<const wchar_t* const>{argv_.get(), static_cast<std::size_t>(argc_)};
    }

    auto sv_span() const
    {
        return raw_span() | std::views::transform(construct_t<std::wstring_view>{});
    }

private:
    int argc_;
    std::unique_ptr<wchar_t*, local_free_t> argv_;
};

std::wstring find_in_path(std::wstring_view exe)
{
    wchar_t result[MAX_PATH];

    DWORD success = ::SearchPathW(
        nullptr, // lpPath
        exe.data(), // lpFileName
        nullptr, // lpExtension
        std::ranges::size(result), // nBufferLength
        result, // lpBuffer
        nullptr // lpFilePart
    );

    if(success == 0) {
        throw errno_exception_t{};
    }

    return result;
}

std::wstring quote(std::wstring_view str)
{
    wchar_t result[MAX_PATH];

    // `>=` because `size()` returns the size without the trailing zero
    if(str.size() >= std::ranges::size(result)) {
        throw std::runtime_error{"The argument is too big"};
    }

    auto [_, last] = std::ranges::copy(str, std::ranges::begin(result));
    *last = '\0';
    (void)::PathQuoteSpacesW(result);
    return result;
}

template<std::ranges::range Range>
std::wstring join(const Range& rng)
{
    std::wstring ret;

    bool first = true;
    for(const auto& string : rng) {
        if(!std::exchange(first, false)) {
            ret += L' ';
        }

        ret += string;
    }

    return ret;
}

int main()
try {
    if(!::SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT)) {
        throw errno_exception_t{};
    }

    constexpr std::wstring_view app_name = L"nvim-qt.exe";

    const std::wstring app_path = find_in_path(app_name);

    const argv_t argv{::GetCommandLineW()};
    const auto my_cmd = argv.sv_span();
    const auto arguments = my_cmd | std::views::drop(1)
                                  | std::views::transform(&quote);

    // `::CreateProcessW` may modify the contents of this variable
    std::wstring nvim_cmd = arguments.empty() ? quote(app_path)
                                              : std::format(L"{} -- {}", quote(app_path), join(arguments))
    ;

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.wShowWindow = true;
    startup_info.dwFlags = STARTF_USESHOWWINDOW;

    PROCESS_INFORMATION process_info;

    bool res = ::CreateProcessW(
        app_path.data(), // lpApplicationName
        nvim_cmd.data(), // lpCommandLine
        nullptr, // lpProcessAttributes
        nullptr, // lpThreadAttributes
        false, // bInheritHandles
        0, // dwCreationFlags
        nullptr, // lpEnvironment
        nullptr, // lpCurrentDirectory
        &startup_info, // lpStartupInfo
        &process_info // lpProcessInformation
    );

    if(!res) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::string error_msg = make_ec().message();
        std::wstring wide_error_msg = converter.from_bytes(error_msg);
        std::wcerr << "Cannot start " << std::quoted(nvim_cmd) << ": " << wide_error_msg << std::endl;
    }

    return 0;
} catch(const std::exception& ex) {
    std::cerr << "nvim-qt wrapper error: " << ex.what() << std::endl;
    return 1;
}
