#include "utilities/StringUtil.h"
#include <codecvt>
#include <locale>

namespace electrical {

// NOTE: std::codecvt_utf8 is deprecated in C++17 but still the most portable
// header-only option that works identically under MSVC and libstdc++. If the
// target toolchain drops it, swap for ICU or a small hand-rolled converter.
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
    return conv.from_bytes(s);
}

std::string wideToUtf8(const std::wstring& s) {
    if (s.empty()) return std::string();
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
    return conv.to_bytes(s);
}

AcString toAcString(const std::string& utf8) {
    return AcString(utf8ToWide(utf8).c_str());
}

AcString toAcString(const std::wstring& w) {
    return AcString(w.c_str());
}

std::string fromAcString(const ACHAR* s) {
    if (!s) return std::string();
    return wideToUtf8(std::wstring(s));
}

} // namespace electrical
