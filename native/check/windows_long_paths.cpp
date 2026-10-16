// SPDX-License-Identifier: MIT
// Verify the app's manifest with real, unprefixed Win32 paths beyond MAX_PATH.
// Usage: cos_windows_long_paths --root <fresh private directory>; fixtures stay
// there for inspection. No game saves or Windows policy settings are changed.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;

[[noreturn]] void fail(const char* message, DWORD error = ERROR_SUCCESS) {
    std::fprintf(stderr, "Windows long paths: FAIL %s (Win32=%lu)\n", message,
                 static_cast<unsigned long>(error));
    std::exit(1);
}
void need(bool condition, const char* message) {
    if (!condition) fail(message);
}
void win32(bool condition, const char* message) {
    if (!condition) fail(message, GetLastError());
}

void checkManifest() {
    const HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(24));
    win32(resource != nullptr, "embedded application manifest is missing");
    const DWORD size = SizeofResource(nullptr, resource);
    need(size != 0 && size <= 65536, "invalid embedded manifest size");
    const HGLOBAL loaded = LoadResource(nullptr, resource);
    win32(loaded != nullptr, "cannot load embedded manifest");
    const auto* bytes = static_cast<const char*>(LockResource(loaded));
    need(bytes != nullptr, "cannot inspect embedded manifest");
    const std::string xml(bytes, size);
    const auto name = xml.find("longPathAware");
    const auto begin = name == std::string::npos ? name : xml.find('>', name);
    const auto end = begin == std::string::npos ? begin : xml.find('<', begin + 1);
    need(begin != std::string::npos && end != std::string::npos &&
         xml.find("http://schemas.microsoft.com/SMI/2016/WindowsSettings") != std::string::npos,
         "longPathAware manifest setting is missing");
    const auto value = xml.substr(begin + 1, end - begin - 1);
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    need(first != std::string::npos && value.substr(first, last - first + 1) == "true",
         "longPathAware is not enabled");
}

bool policyEnabled() {
    DWORD value = 0, size = sizeof value;
    const LSTATUS result = RegGetValueW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\FileSystem", L"LongPathsEnabled",
        RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (result == ERROR_FILE_NOT_FOUND) return false;
    if (result != ERROR_SUCCESS) fail("cannot inspect long-path policy", result);
    return value == 1;
}

void unprefixed(const fs::path& path) {
    const auto& text = path.native();
    need(path.is_absolute() && text.size() < 4096 && text.rfind(L"\\\\?\\", 0) != 0 &&
         text.rfind(L"\\\\.\\", 0) != 0 && text.find(L'/') == std::wstring::npos,
         "fixture path must be an ordinary absolute Win32 path");
}
void checkAncestors(fs::path path) {
    while (!path.empty()) {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        win32(attributes != INVALID_FILE_ATTRIBUTES, "fixture parent does not exist");
        need((attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
             "fixture ancestors must be ordinary directories");
        const auto parent = path.parent_path();
        if (parent == path) break;
        path = parent;
    }
}
void createDirectory(const fs::path& path) {
    unprefixed(path);
    win32(CreateDirectoryW(path.c_str(), nullptr) != 0, "CreateDirectoryW failed");
}

void checkUtf8RoundTrip(const fs::path& path) {
    const auto& wide = path.native();
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), -1,
                                         nullptr, 0, nullptr, nullptr);
    win32(bytes > 0, "UTF-16 to UTF-8 conversion failed");
    std::string utf8(static_cast<size_t>(bytes), '\0');
    win32(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), -1,
                             utf8.data(), bytes, nullptr, nullptr) == bytes,
          "UTF-8 path conversion failed");
    const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), -1, nullptr, 0);
    win32(chars > 0, "UTF-8 to UTF-16 conversion failed");
    std::wstring restored(static_cast<size_t>(chars), L'\0');
    win32(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), -1,
                             restored.data(), chars) == chars, "UTF-16 path conversion failed");
    need(restored.size() == wide.size() + 1 && !std::wcscmp(restored.c_str(), wide.c_str()),
         "Unicode path changed during UTF-8 round trip");
}

void createFile(const fs::path& path, const char* payload) {
    unprefixed(path);
    need(path.native().size() > MAX_PATH, "actual file path must exceed MAX_PATH");
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    win32(file != INVALID_HANDLE_VALUE, "unprefixed CreateFileW failed");
    const auto size = static_cast<DWORD>(std::strlen(payload));
    DWORD written = 0;
    win32(WriteFile(file, payload, size, &written, nullptr) != 0, "WriteFile failed");
    need(written == size, "short file write");
    win32(FlushFileBuffers(file) != 0, "FlushFileBuffers failed");
    win32(CloseHandle(file) != 0, "CloseHandle after write failed");
}
void readFile(const fs::path& path, const char* expected) {
    unprefixed(path);
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    win32(file != INVALID_HANDLE_VALUE, "unprefixed reopen failed");
    char bytes[128]{};
    DWORD count = 0;
    win32(ReadFile(file, bytes, sizeof bytes, &count, nullptr) != 0, "ReadFile failed");
    need(count == std::strlen(expected) && !std::memcmp(bytes, expected, count),
         "readback did not match the published file");
    DWORD extra = 1;
    win32(ReadFile(file, bytes, 1, &extra, nullptr) != 0, "EOF read failed");
    need(extra == 0, "unexpected trailing file bytes");
    win32(CloseHandle(file) != 0, "CloseHandle after read failed");
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        need(argc == 3 && !std::wcscmp(argv[1], L"--root"), "use --root <fresh private directory>");
        checkManifest();
        if (!policyEnabled()) {
            std::puts("Windows long paths: SKIP LongPathsEnabled is off; no deep I/O was tested");
            return 0;
        }
        fs::path root = fs::absolute(argv[2]).lexically_normal();
        root.make_preferred();
        unprefixed(root);
        need(root.root_name().native().size() == 2 && root.root_name().native()[1] == L':',
             "fixture must use a local drive");
        checkAncestors(root.parent_path());
        SetLastError(ERROR_SUCCESS);
        const DWORD existing = GetFileAttributesW(root.c_str());
        need(existing == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND,
             "fixture root must be fresh; existing files are never overwritten");
        createDirectory(root);

        fs::path directory = root;
        unsigned levels = 0;
        while (directory.native().size() <= 300) {
            need(++levels <= 16, "fixture directory bound exceeded");
            directory /= L"unicode-\u03a9-\u65e5\u672c-0123456789012345678901234567890123456789";
            createDirectory(directory);
        }
        const fs::path target = directory / L"01-GZLE-gczelda.gci";
        const fs::path temporary = directory / L"01-GZLE-gczelda.gci.tmp-18446744073709551615";
        checkUtf8RoundTrip(temporary);
        createFile(target, "before-atomic-v1\n");
        readFile(target, "before-atomic-v1\n");
        createFile(temporary, "after-atomic-v1\n");
        win32(MoveFileExW(temporary.c_str(), target.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
              "unprefixed atomic replacement failed");
        readFile(target, "after-atomic-v1\n");
        SetLastError(ERROR_SUCCESS);
        need(GetFileAttributesW(temporary.c_str()) == INVALID_FILE_ATTRIBUTES &&
             GetLastError() == ERROR_FILE_NOT_FOUND, "atomic temporary file was not consumed");
        std::printf("Windows long paths: PASS target-chars=%zu temporary-chars=%zu "
                    "unprefixed=1 Unicode-roundtrip=1 create-flush-replace-reopen=1; no gameplay proof\n",
                    target.native().size(), temporary.native().size());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Windows long paths: FAIL %s\n", error.what());
        return 1;
    }
}
