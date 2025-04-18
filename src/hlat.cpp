/*=============================================================================
 |  Heuristic Layer Abstraction Transformer (HLAT)
 |  ---------------------------------------------------------------------------
 |  Converts XPathlike strings into canonical Qt locator descriptors for
 |  GUI automation or modelbased testing. Key features:
 |      * Fast regex-based path parser
 |      * Heuristic Qt widget classifier
 |
 |  Copyright (c) 2025  SARTORIUS
 |  Contact: alexander.toepfer@sartorius.com
 |
 |  SPDXLicenseIdentifier: MIT
 *============================================================================*/

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <regex>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <optional>
#include <charconv>
#include <ranges>
#include <type_traits>

#include <nlohmann/json.hpp>

namespace hlat {

    // Alias for convenience
    using json = nlohmann::json;

    // -----------------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------------
    namespace util {
        inline void toLowerInPlace(std::string& s) {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }
        inline bool endsWith(const std::string& str, const std::string& suffix) {
            return str.size() >= suffix.size() &&
                std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
        }
        inline bool contains(const std::string& str, const std::string& needle) {
            return str.find(needle) != std::string::npos;
        }
    } // namespace util

    // -----------------------------------------------------------------------------
    // Data structures
    // -----------------------------------------------------------------------------
    struct LexicalUnit {
        std::string                lexeme;
        std::optional<std::string> attribute_key;
        std::optional<std::string> attribute_val;
        int                        ordinal = 1;
    };

    struct LocatorDescriptor {
        std::string uid;
        json        meta;
    };

    // -----------------------------------------------------------------------------
    // Utility functions
    // -----------------------------------------------------------------------------
    inline std::string canonicalize(std::string_view in) {
        std::string out; out.reserve(in.size());
        for (char c : in) out += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
        static const std::regex collapse{ R"(_+)" };
        out = std::regex_replace(out, collapse, "_");
        if (!out.empty() && out.front() == '_') out.erase(0, 1);
        if (!out.empty() && out.back() == '_') out.pop_back();
        return out;
    }

    inline int parseOrdinal(std::string_view sv, int fallback = 1) {
        int value = 0;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
        return ec == std::errc() ? value : fallback;
    }

    // -----------------------------------------------------------------------------
    // Heuristic classifier for Qt widget archetypes
    // -----------------------------------------------------------------------------
    class HeuristicQtClassifier {
    public:
        std::string operator()(std::string_view tag_sv) const {
            std::string tag(tag_sv);
            util::toLowerInPlace(tag);
            if (tag == "button")         return "PushButtonQT";
            if (tag == "container")      return "ScrollViewQT";
            if (tag == "form")           return "ModuleQT";
            if (tag == "textfield")      return "TextFieldQT";
            if (util::endsWith(tag, "button"))      return "PushButtonQT";
            if (util::endsWith(tag, "checkbox"))    return "CheckBoxQT";
            if (util::endsWith(tag, "radiobutton")) return "RadioButtonQT";
            if (util::endsWith(tag, "combobox"))    return "ComboBoxQT";
            if (util::endsWith(tag, "slider"))      return "SliderQT";
            if (util::endsWith(tag, "label"))       return "LabelQT";
            if (util::endsWith(tag, "view"))        return "ScrollViewQT";
            if (util::endsWith(tag, "field"))       return "TextFieldQT";
            if (util::contains(tag, "button"))      return "PushButtonQT";
            if (util::contains(tag, "field"))       return "TextFieldQT";
            if (util::contains(tag, "text"))        return "TextFieldQT";
            if (util::contains(tag, "container"))   return "ScrollViewQT";
            if (util::contains(tag, "panel"))       return "ScrollViewQT";
            if (util::contains(tag, "form"))        return "ModuleQT";
            return "QWidget";
        }
    };

    // -----------------------------------------------------------------------------
    // Synthesizer that builds Qt locators
    // -----------------------------------------------------------------------------

    template<typename ParserT, typename ClassifierT = HeuristicQtClassifier>
    class QtLocatorSynthesizer {
        std::decay_t<ParserT> parser_;
        ClassifierT           classifier_;
    public:
        QtLocatorSynthesizer(ParserT p, ClassifierT c = {})
            : parser_(std::move(p)), classifier_(std::move(c)) {}

        std::vector<LocatorDescriptor> operator()(std::string_view path) const {
            std::vector<LocatorDescriptor> out;
            std::string parent_uid;

            for (const auto& unit : parser_(path)) {
                std::string archetype = classifier_(unit.lexeme);
                std::string token = canonicalize(unit.attribute_val.value_or(unit.lexeme));
                std::string uid = parent_uid.empty()
                    ? token + "_" + archetype
                    : parent_uid + "_" + token + "_" + archetype;

                json meta;
                meta["archetype"] = archetype;
                if (unit.attribute_key) {
                    if (*unit.attribute_key == "title" && unit.attribute_val) {
                        std::string esc = *unit.attribute_val;
                        for (size_t pos = 0; (pos = esc.find('\\', pos)) != std::string::npos; pos += 2)
                            esc.insert(pos, "\\");
                        meta["windowTitle"] = esc;
                    }
                    else if (*unit.attribute_key == "name" && unit.attribute_val) {
                        meta["name"] = *unit.attribute_val;
                    }
                }
                if (!parent_uid.empty()) meta["container"] = parent_uid;
                if (unit.ordinal > 1)   meta["occurrence"] = unit.ordinal;
                meta["visible"] = 1;

                out.push_back({ uid, meta });
                parent_uid = out.back().uid;
            }
            return out;
        }
    };

} // namespace hlat

// -----------------------------------------------------------------------------
// In-line path parser
// -----------------------------------------------------------------------------
inline constexpr auto parsePath = [](std::string_view path_sv) -> std::vector<hlat::LexicalUnit> {
    /*
        (\w+)
            Capture the tag name.

        (?:\[@(\w+)='([^']+)'\])?
            Optional noncapturing block for an attribute:
              \[@              literal "[@"
              (\w+)            capture attribute key
              ='               literal "='"
              ([^']+)          capture attribute value (no single quotes allowed)
              '\]              literal "']"

        (?:\[(\d+)\])?
            Optional noncapturing block for a numeric index:
              \[               literal "["
              (\d+)            capture one or more digits
              \]               literal "]"
    */
    static const std::regex rx{ R"((\w+)(?:\[@(\w+)='([^']+)'\])?(?:\[(\d+)\])?)" };
    std::vector<hlat::LexicalUnit> units;

    if (!path_sv.empty() && path_sv.front() == '/') path_sv.remove_prefix(1);

    for (auto&& seg : path_sv | std::views::split('/')) {
        std::string token(seg.begin(), seg.end());
        std::match_results<std::string::const_iterator> m;
        if (!std::regex_match(token.cbegin(), token.cend(), m, rx))
            continue;

        units.push_back({
            m[1].str(),
            m[2].matched ? std::optional<std::string>(m[2].str()) : std::nullopt,
            m[3].matched ? std::optional<std::string>(m[3].str()) : std::nullopt,
            m[4].matched ? hlat::parseOrdinal(std::string_view(m[4].str())) : 1
        });
    }
    return units;
};

int main() {

    auto synthesizer = hlat::QtLocatorSynthesizer<std::vector<hlat::LexicalUnit>(*)(std::string_view)>{ parsePath };

    const std::string xpath = "";

    for (const auto& rec : synthesizer(xpath)) {
        std::cout << rec.uid << " = " << rec.meta.dump(4) << "\n\n";
    }

    return 0;
}
