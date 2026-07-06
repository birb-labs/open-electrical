// =============================================================================
//  Localization.h - Runtime UI language switching.
//
//  Commands and internal identifiers are ALWAYS English. Only user-facing UI
//  strings are translated. Language packs are flat JSON string maps under
//  resources/lang/<code>.json ("en", "pt-BR", "es"). English is the built-in
//  fallback: any key missing from the active pack falls back to English, and
//  any key missing everywhere returns itself.
// =============================================================================
#pragma once

#include <map>
#include <string>

namespace electrical {

class Localization {
public:
    static Localization& instance();

    // Loads all language packs found in `resourceDir`/lang. Safe to call again.
    void loadPacks(const std::string& resourceDir);

    // Switches the active language ("en", "pt-BR", "es"). Unknown codes keep
    // the current language and return false.
    bool setLanguage(const std::string& code);
    const std::string& language() const { return active_; }

    // Translates a key to the active language (UTF-8). Falls back to English,
    // then to the key itself.
    std::string tr(const std::string& key) const;

    // Convenience: translated string as std::wstring for the wx/BRX layer.
    std::wstring trW(const std::string& key) const;

private:
    Localization() = default;
    std::map<std::string, std::map<std::string, std::string>> packs_;  // code -> (key -> text)
    std::string active_ = "en";

    static std::map<std::string, std::string> parseFlatJson(const std::string& text);
    static std::map<std::string, std::string> builtinEnglish();
};

// Shorthand used throughout the UI code.
inline std::string  EL_TR(const std::string& k)  { return Localization::instance().tr(k); }
inline std::wstring EL_TRW(const std::string& k) { return Localization::instance().trW(k); }

} // namespace electrical
