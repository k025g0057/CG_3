#include "StringUtility.h"

#include <Windows.h>

namespace StringUtility {
    std::wstring ConvertString(const std::string& str) {
        if (str.empty()) {
            return {};
        }

        const int length = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0);
        if (length == 0) {
            return {};
        }

        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), result.data(), length);
        return result;
    }

    std::string ConvertString(const std::wstring& str) {
        if (str.empty()) {
            return {};
        }

        const int length = WideCharToMultiByte(
            CP_ACP, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
        if (length == 0) {
            return {};
        }

        std::string result(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(
            CP_ACP, 0, str.data(), static_cast<int>(str.size()), result.data(), length, nullptr, nullptr);
        return result;
    }
}
