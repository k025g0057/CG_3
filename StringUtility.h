#pragma once

#include <string>

namespace StringUtility {
    // UTF-8 の std::string と std::wstring を相互変換する。
    std::wstring ConvertString(const std::string& str);
    std::string ConvertString(const std::wstring& str);
}
