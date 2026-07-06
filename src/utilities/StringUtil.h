// =============================================================================
//  StringUtil.h - Conversions between the plugin's UTF-8 std::string, its
//  std::wstring, and the BRX ACHAR (TCHAR) API strings.
// =============================================================================
#pragma once

#include "Platform.h"
#include <string>

namespace electrical {

// UTF-8 std::string  <->  std::wstring
std::wstring utf8ToWide(const std::string& s);
std::string  wideToUtf8(const std::wstring& s);

// std::string / std::wstring  ->  ACHAR* buffer (owning AcString wrapper).
// ACHAR is wchar_t on Windows and (with the emulation layer) on Linux too.
AcString toAcString(const std::string& utf8);
AcString toAcString(const std::wstring& w);

// ACHAR*  ->  UTF-8 std::string
std::string fromAcString(const ACHAR* s);

} // namespace electrical
