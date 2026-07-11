#include "ui/Localization.h"
#include "utilities/StringUtil.h"

#include <fstream>
#include <sstream>

namespace electrical {

Localization& Localization::instance() {
    static Localization loc;
    static bool seeded = false;
    if (!seeded) { loc.packs_["en"] = builtinEnglish(); seeded = true; }
    return loc;
}

std::string Localization::tr(const std::string& key) const {
    auto ap = packs_.find(active_);
    if (ap != packs_.end()) {
        auto it = ap->second.find(key);
        if (it != ap->second.end()) return it->second;
    }
    auto en = packs_.find("en");
    if (en != packs_.end()) {
        auto it = en->second.find(key);
        if (it != en->second.end()) return it->second;
    }
    return key;
}

std::wstring Localization::trW(const std::string& key) const {
    return utf8ToWide(tr(key));
}

bool Localization::setLanguage(const std::string& code) {
    if (packs_.find(code) == packs_.end()) return false;
    active_ = code;
    return true;
}

void Localization::loadPacks(const std::string& resourceDir) {
    const char* codes[] = { "en", "pt-BR", "es" };
    for (const char* code : codes) {
        const std::string path = resourceDir + "/lang/" + code + ".json";
        std::ifstream f(path, std::ios::binary);
        if (!f) continue;
        std::ostringstream ss; ss << f.rdbuf();
        auto map = parseFlatJson(ss.str());
        if (!map.empty()) {
            // English pack merges over the built-in so files can extend it.
            for (auto& [k, v] : map) packs_[code][k] = v;
        }
    }
}

// Minimal flat-JSON parser: extracts "key":"value" string pairs. Handles \" and
// \\ escapes; ignores structure/whitespace. Sufficient for flat string maps.
std::map<std::string, std::string> Localization::parseFlatJson(const std::string& text) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    auto readString = [&](std::string& dst) -> bool {
        while (i < text.size() && text[i] != '"') ++i;
        if (i >= text.size()) return false;
        ++i;  // opening quote
        std::string s;
        while (i < text.size()) {
            char c = text[i++];
            if (c == '\\' && i < text.size()) {
                char e = text[i++];
                switch (e) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case '"': s += '"';  break;
                    case '\\': s += '\\'; break;
                    default: s += e; break;
                }
            } else if (c == '"') {
                dst = s; return true;
            } else {
                s += c;
            }
        }
        return false;
    };

    while (i < text.size()) {
        std::string key, val;
        if (!readString(key)) break;
        // expect a ':'
        while (i < text.size() && text[i] != ':' && text[i] != '"') ++i;
        if (i < text.size() && text[i] == ':') ++i;
        if (!readString(val)) break;
        out[key] = val;
    }
    return out;
}

// Built-in English strings. Every UI-facing key the plugin uses is defined here
// so the plugin is fully functional even if the JSON packs are missing.
std::map<std::string, std::string> Localization::builtinEnglish() {
    return {
        { "app.title",            "Electrical" },
        { "palette.title",        "Electrical Design" },
        { "menu.config",          "Project Settings" },
        { "menu.rooms",           "Rooms" },
        { "menu.elements",        "Elements" },
        { "menu.circuits",        "Circuits" },
        { "menu.reports",         "Reports" },

        { "config.title",         "Project Settings" },
        { "config.scale",         "Scale" },
        { "config.unit",          "Unit" },
        { "config.network",       "Electrical network" },
        { "config.voltageLL",     "Voltage (line-to-line)" },
        { "config.voltageLN",     "Voltage (line-to-neutral)" },
        { "config.frequency",     "Frequency (Hz)" },
        { "config.phases",        "Phases" },
        { "config.grounding",     "Grounding system" },
        { "config.installation",  "Installation" },
        { "config.installType",   "Installation type" },
        { "config.conduitMat",    "Conduit material" },
        { "config.wireGauge",     "Default wire gauge (mm2)" },
        { "config.utility",       "Power utility provider" },
        { "config.addProvider",   "Add provider..." },
        { "config.language",      "Interface language" },
        { "config.surface",       "Surface" },
        { "config.concealed",     "Concealed" },

        { "btn.ok",               "OK" },
        { "btn.cancel",           "Cancel" },
        { "btn.apply",            "Apply" },
        { "btn.add",              "Add" },
        { "btn.remove",           "Remove" },

        { "unit.m",               "Meters" },
        { "unit.cm",              "Centimeters" },
        { "unit.dm",              "Decimeters" },
        { "unit.mm",              "Millimeters" },

        { "phases.1",             "Single-phase" },
        { "phases.2",             "Two-phase" },
        { "phases.3",             "Three-phase" },

        { "msg.settingsSaved",    "Project settings saved." },
        { "msg.noProject",        "No electrical project in this drawing yet." },
        { "msg.selectPolyline",   "Select a closed polyline for the room boundary:" },
        { "msg.pickPoint",        "Pick insertion point:" },
        { "msg.done",             "Done." },
        { "msg.undone",           "Last electrical action undone." },
        { "report.legend",        "Legend" },
        { "report.loadSchedule",  "Load Schedule" },
        { "report.bom",           "Bill of Materials" },
        { "report.sld",           "Single-line Diagram" },

        { "msg.roomFirstCorner",  "Pick first room corner:" },
        { "msg.roomNextCorner",   "Pick next corner [Undo] or press Enter to close:" },
        { "msg.roomNeedCorners",  "A room needs at least 3 corners." },
        { "msg.roomUndone",       "Last corner removed." },

        { "height.label",         "Height" },
        { "height.low",           "Low" },
        { "height.medium",        "Medium" },
        { "height.high",          "High" },

        { "switch.sections",      "Sections" },
        { "switch.command",       "Command" },
        { "light.power",          "Power (e.g. 2x32W)" },
        { "light.circuit",        "Circuit" },
        { "light.lampCount",      "Lamp count" },
        { "light.lampWatts",      "Power per lamp (W)" },

        // Room dialog (EL-ROOM) - per-room luminotechnical parameters.
        { "room.name",            "Name" },
        { "room.type",            "Type" },
        { "room.usageDetail",     "Specific use (optional)" },
        { "room.function",        "Function" },
        { "room.ceilingHeight",   "Ceiling height (m)" },
        { "room.workPlane",       "Work plane height (m)" },
        { "room.ceilingRefl",     "Ceiling reflectance (%)" },
        { "room.wallRefl",        "Wall reflectance (%)" },
        { "room.floorRefl",       "Floor reflectance (%)" },
        { "room.contrast",        "Task contrast" },
        { "room.mf",              "Maintenance factor (MF)" },
        { "room.cu",              "Utilization factor (CU, 0 = auto)" },
        { "room.area",            "Area (auto)" },

        { "contrast.low",         "Low" },
        { "contrast.medium",      "Medium" },
        { "contrast.high",        "High" },

        // Lighting run dialog (EL-CALC-LIGHT).
        { "light.run.title",           "Automatic lighting" },
        { "light.run.room",            "Room" },
        { "light.run.allRooms",        "All rooms" },
        { "light.run.luminaire",       "Luminaire model" },
        { "light.run.customLuminaire", "Custom (type the values)" },
        { "light.run.lumens",          "Luminous flux (lumens)" },
        { "light.run.watts",           "Luminaire power (W)" },
    };
}

} // namespace electrical
