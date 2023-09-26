// Copyright © 2023 Yegor Bychin <j@siga.icu>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See the COPYING file for more details.

#include <format>
#include <iostream>
#include <ranges>
#include <span>

#include <shlwapi.h>
#include <windows.h>

std::error_code make_sys_ec(int ec) {
    return std::error_code{ec, std::system_category()};
}

std::error_code make_sys_ec(DWORD ec = ::GetLastError()) {
    return make_sys_ec(static_cast<int>(ec));
}

class errno_exception_t : public std::system_error
{
public:
    errno_exception_t() : errno_exception_t{::GetLastError()} {}
    errno_exception_t(DWORD ec) : std::system_error{make_sys_ec(ec)} {}
};

class local_free_t
{
public:
    void operator()(void *ptr) const { ::LocalFree(ptr); }
};

class argv_t
{
public:
    [[nodiscard]] argv_t(std::wstring_view cmd)
        : argv_{::CommandLineToArgvW(cmd.data(), &argc_)} //
    {
        if(argv_ == nullptr || argc_ < 0) {
            throw errno_exception_t{};
        }
    }

public:
    auto get_span() const {
        return std::span<const wchar_t *const>{
            argv_.get(),
            static_cast<std::size_t>(argc_)
        };
    }

private:
    int argc_;
    std::unique_ptr<wchar_t *, local_free_t> argv_;
};

std::wstring find_in_path(std::wstring_view exe) {
    wchar_t result[MAX_PATH];

    DWORD success = ::SearchPathW(
        nullptr,                   // lpPath
        exe.data(),                // lpFileName
        nullptr,                   // lpExtension
        std::ranges::size(result), // nBufferLength
        result,                    // lpBuffer
        nullptr                    // lpFilePart
    );

    if(success == 0) {
        throw errno_exception_t{};
    }

    return result;
}

std::wstring quote(std::wstring_view str) {
    wchar_t result[MAX_PATH];

    // `>=` because `size()` returns the size without the trailing zero
    if(std::ranges::size(str) >= std::ranges::size(result)) {
        throw std::runtime_error{"The argument is too big"};
    }

    auto [_, last] = std::ranges::copy(str, std::ranges::begin(result));
    *last          = '\0';
    (void)::PathQuoteSpacesW(result);
    return result;
}

template<std::ranges::range Range>
std::wstring join(const Range &rng) {
    std::wstring ret;

    bool first = true;
    for(const auto &string : rng) {
        if(!std::exchange(first, false)) {
            ret += L' ';
        }

        ret += string;
    }

    return ret;
}

void set_sane_winapi_defaults() {
    constexpr auto search_path_mode_flags =
        BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT;

    if(!::SetSearchPathMode(search_path_mode_flags)) {
        throw errno_exception_t{};
    }
}

int main() try {
    set_sane_winapi_defaults();

    constexpr std::wstring_view app_name = L"nvim-qt.exe";
    const std::wstring app_path          = find_in_path(app_name);

    const argv_t argv{::GetCommandLineW()};
    // clang-format off
    const auto arguments = argv.get_span() | std::views::drop(1)
                                           | std::views::transform(quote);
    // clang-format on

    // `::CreateProcessW` may modify the contents of this variable
    std::wstring nvim_cmd =
        std::ranges::empty(arguments)
            ? quote(app_path)
            : std::format(L"{} -- {}", quote(app_path), join(arguments));

    STARTUPINFOW startup_info{};
    startup_info.cb          = sizeof(startup_info);
    startup_info.wShowWindow = true;
    startup_info.dwFlags     = STARTF_USESHOWWINDOW;

    PROCESS_INFORMATION process_info;

    bool res = ::CreateProcessW(
        app_path.data(), // lpApplicationName
        nvim_cmd.data(), // lpCommandLine
        nullptr,         // lpProcessAttributes
        nullptr,         // lpThreadAttributes
        false,           // bInheritHandles
        0,               // dwCreationFlags
        nullptr,         // lpEnvironment
        nullptr,         // lpCurrentDirectory
        &startup_info,   // lpStartupInfo
        &process_info    // lpProcessInformation
    );

    if(!res) {
        throw errno_exception_t{};
    }

    return 0;
} catch(const std::exception &ex) {
    std::cerr << "nvim-qt wrapper error: " << ex.what() << std::endl;
    return 1;
}
