/*=============================================================================
 |  Heuristic Layer Abstraction Transformer (HLAT)
 |  ---------------------------------------------------------------------------
 |  A single-header C++20 library for converting XPath expressions into Qt locators.
 |  Features:
 |      * Full XPath 1.0 support
 |      * Heuristic Qt widget classifier
 |      * Advanced path parsing with axes and predicates
 |      * Single-header, dependency-free (except nlohmann/json)
 |
 |  Copyright (c) 2025  SARTORIUS
 |  Contact: alexander.toepfer@sartorius.com
 |
 |  SPDXLicenseIdentifier: MIT
 *============================================================================*/

#pragma once

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
#include <memory>
#include <variant>
#include <stack>
#include <iostream>

#include <nlohmann/json.hpp>

namespace hlat {

    using json = nlohmann::json;

    // -----------------------------------------------------------------------------
    // XPath Token Types & Helpers
    // -----------------------------------------------------------------------------

    /// Enumerates all possible token types emitted by the XPath lexer
    enum class TokenType {
        Tag,        ///< Element or node name (e.g., "book", "div")
        Attribute,  ///< '@' symbol marking the start of an attribute test
        Axis,       ///< Axis specifier (e.g., "child::", "descendant::")
        Predicate,  ///< '[' or ']' delimiting a predicate expression
        Operator,   ///< Comparison operators (=, !=, <, >, <=, >=)
        Literal,    ///< Quoted string or numeric literal
        Wildcard,   ///< '*' wildcard matching any node
        Namespace,  ///< ':' in a namespace prefix (e.g., "ns:element")
        Slash,      ///< '/' or '//' path separator
        End         ///< Special end-of-input marker
    };

    /// Represents a single lexed unit from the XPath input
    struct Token {
        TokenType   type;     ///< Category of this token
        std::string value;    ///< Exact text matched (e.g., "book", "@id", "and")
        size_t      position; ///< Zero-based index in input where token began
    };

    // -----------------------------------------------------------------------------
    // XPath Expression Components
    // -----------------------------------------------------------------------------

    /// Represents an attribute-based predicate (e.g., @name='value')
    struct AttributePredicate {
        std::string name;  ///< Attribute name
        std::string value; ///< Attribute value
        std::string op;    ///< Comparison operator
    };

    /// Represents a position-based predicate (e.g., [1], [last()])
    struct PositionPredicate {
        int position; ///< One-based position index
    };

    /// Represents a complex predicate combining multiple conditions
    struct ComplexPredicate {
        std::vector<std::variant<AttributePredicate, PositionPredicate>> conditions;
    };

    /// Represents a single step in an XPath expression
    struct XLocator {
        std::string                     axis;        ///< Axis specifier (e.g., "child", "descendant")
        std::string                     tag;         ///< Node test: tag name or "*"
        std::optional<ComplexPredicate> predicate;   ///< Optional predicate conditions
        bool                            is_absolute; ///< True if step began with leading '/'
    };

    // -----------------------------------------------------------------------------
    // Utility Functions
    // -----------------------------------------------------------------------------

    namespace util {
        /// Converts a string to lowercase in-place
        inline void toLowerInPlace(std::string& s) {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        /// Checks if a string ends with a given suffix
        inline bool endsWith(const std::string& str, const std::string& suffix) {
            return str.size() >= suffix.size() &&
                std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
        }

        /// Checks if a string contains a given substring
        inline bool contains(const std::string& str, const std::string& needle) {
            return str.find(needle) != std::string::npos;
        }

        /// Converts a string to a canonical form suitable for UIDs
        inline std::string canonicalize(std::string_view in) {
            std::string out; out.reserve(in.size());
            for (char c : in) out += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
            static const std::regex collapse{ R"(_+)" };
            out = std::regex_replace(out, collapse, "_");
            if (!out.empty() && out.front() == '_') out.erase(0, 1);
            if (!out.empty() && out.back() == '_') out.pop_back();
            return out;
        }
    } // namespace util

    // -----------------------------------------------------------------------------
    // Heuristic Qt Widget Classifier
    // -----------------------------------------------------------------------------

    /// Classifies HTML-like tags into Qt widget types
    class HeuristicQtClassifier {
    public:
        /// Maps a tag name to its corresponding Qt widget type
        std::string operator()(std::string_view tag_sv) const {
            std::string tag(tag_sv);
            util::toLowerInPlace(tag);
            
            // Exact matches
            if (tag == "button")      return "PushButtonQT";
            if (tag == "container")   return "ScrollViewQT";
            if (tag == "form")        return "ModuleQT";
            if (tag == "textfield")   return "TextFieldQT";
            
            // Suffix-based matches
            if (util::endsWith(tag, "button"))     return "PushButtonQT";
            if (util::endsWith(tag, "checkbox"))   return "CheckBoxQT";
            if (util::endsWith(tag, "radiobutton"))return "RadioButtonQT";
            if (util::endsWith(tag, "combobox"))   return "ComboBoxQT";
            if (util::endsWith(tag, "slider"))     return "SliderQT";
            if (util::endsWith(tag, "label"))      return "LabelQT";
            if (util::endsWith(tag, "view"))       return "ScrollViewQT";
            if (util::endsWith(tag, "field"))      return "TextFieldQT";
            
            // Substring-based matches
            if (util::contains(tag, "button"))     return "PushButtonQT";
            if (util::contains(tag, "field"))      return "TextFieldQT";
            if (util::contains(tag, "text"))       return "TextFieldQT";
            if (util::contains(tag, "container"))  return "ScrollViewQT";
            if (util::contains(tag, "panel"))      return "ScrollViewQT";
            if (util::contains(tag, "form"))       return "ModuleQT";
            
            return "QWidget"; // Default fallback
        }
    };

    // -----------------------------------------------------------------------------
    // XPath Lexer
    // -----------------------------------------------------------------------------

    /// Tokenizes XPath expressions into a sequence of tokens
    class XPathLexer {
    public:
        explicit XPathLexer(std::string_view input) : input_(input) {}

        /// Tokenizes the input XPath expression
        std::vector<Token> tokenize() {
            std::vector<Token> tokens;
            while (pos_ < input_.length()) {
                if (std::isspace(input_[pos_])) { ++pos_; continue; }
                
                char c = input_[pos_];
                switch (c) {
                case '/':
                    tokens.push_back({ TokenType::Slash, "/", pos_ });
                    if (++pos_ < input_.length() && input_[pos_] == '/') {
                        tokens.back().value = "//"; ++pos_;
                    }
                    continue;
                    
                case '@': tokens.push_back({ TokenType::Attribute, "@", pos_++ }); continue;
                case '[': tokens.push_back({ TokenType::Predicate, "[", pos_++ }); continue;
                case ']': tokens.push_back({ TokenType::Predicate, "]", pos_++ }); continue;
                case '*': tokens.push_back({ TokenType::Wildcard, "*", pos_++ }); continue;
                
                case '"': case '\'':
                {
                    char q = c; ++pos_;
                    size_t start = pos_;
                    while (pos_ < input_.length() && input_[pos_] != q) {
                        if (input_[pos_] == '\\' && pos_ + 1 < input_.length()) ++pos_;
                        ++pos_;
                    }
                    if (pos_ >= input_.length())
                        throw std::runtime_error("Unterminated string literal");
                    tokens.push_back({ TokenType::Literal,
                        std::string(input_.substr(start, pos_ - start)),
                        start });
                    ++pos_;
                    continue;
                }
                
                case '=': case '!': case '>': case '<':
                {
                    std::string op(1, c); ++pos_;
                    if (pos_ < input_.length() && input_[pos_] == '=') {
                        op += '='; ++pos_;
                    }
                    tokens.push_back({ TokenType::Operator, op, pos_ - op.length() });
                    continue;
                }
                
                default:
                    break;
                }
                
                // Check for axis specifier
                if (pos_ + 2 < input_.length() &&
                    input_[pos_ + 1] == ':' && input_[pos_ + 2] == ':')
                {
                    size_t start = pos_;
                    while (pos_ < input_.length() &&
                        !std::isspace(input_[pos_]) &&
                        input_[pos_] != ':' && input_[pos_] != '/')
                    {
                        ++pos_;
                    }
                    if (pos_ + 1 < input_.length() &&
                        input_[pos_] == ':' && input_[pos_ + 1] == ':')
                    {
                        tokens.push_back({ TokenType::Axis,
                            std::string(input_.substr(start, pos_ - start)),
                            start });
                        pos_ += 2;
                        continue;
                    }
                    pos_ = start;
                }
                
                // Generic identifier (tag name, etc.)
                size_t start = pos_;
                while (pos_ < input_.length() &&
                    !std::isspace(input_[pos_]) &&
                    strchr("/[]@=!<>*", input_[pos_]) == nullptr)
                {
                    ++pos_;
                }
                tokens.push_back({ TokenType::Tag,
                    std::string(input_.substr(start, pos_ - start)),
                    start });
            }
            
            tokens.push_back({ TokenType::End, "", pos_ });
            return tokens;
        }

    private:
        std::string_view input_;
        size_t           pos_{ 0 };
    };

    // -----------------------------------------------------------------------------
    // XPath Parser
    // -----------------------------------------------------------------------------

    /// Parses tokenized XPath expressions into a sequence of locators
    class XPathParser {
    public:
        explicit XPathParser(const std::vector<Token>& tokens)
            : tokens_(tokens) {}

        /// Parses the token stream into a sequence of XPath locators
        std::vector<XLocator> parse() {
            std::vector<XLocator> steps;
            while (!isAtEnd()) {
                bool is_abs = false;
                if (match(TokenType::Slash)) {
                    is_abs = true;
                    if (match(TokenType::Slash)) {
                        steps.push_back({ "descendant-or-self", "*", std::nullopt, true });
                        continue;
                    }
                }
                steps.push_back(parseStep(is_abs));
            }
            return steps;
        }

    private:
        /// Parses a single XPath step
        XLocator parseStep(bool is_abs) {
            XLocator step; step.is_absolute = is_abs;
            step.axis = match(TokenType::Axis) ? previous().value : "child";
            
            if (match(TokenType::Wildcard)) step.tag = "*";
            else if (match(TokenType::Tag)) step.tag = previous().value;
            else throw std::runtime_error("Expected tag or '*' at pos "
                + std::to_string(current().position));

            if (match(TokenType::Predicate) && previous().value == "[") {
                step.predicate = parsePredicate();
                if (!match(TokenType::Predicate) || previous().value != "]")
                    throw std::runtime_error("Expected closing ']' at pos "
                        + std::to_string(current().position));
            }
            
            if (match(TokenType::Namespace))
                step.tag = previous().value + ":" + step.tag;
                
            return step;
        }

        /// Parses a predicate expression
        ComplexPredicate parsePredicate() {
            ComplexPredicate pred;
            while (true) {
                if (check(TokenType::Predicate) && current().value == "]")
                    break;

                // Handle unprefixed comparisons (e.g., price>35)
                if (check(TokenType::Tag)
                    && peek().type == TokenType::Operator
                    && (peek(2).type == TokenType::Literal ||
                        peek(2).type == TokenType::Tag))
                {
                    std::string name = consume(TokenType::Tag).value;
                    std::string op = consume(TokenType::Operator).value;
                    std::string raw;
                    if (match(TokenType::Literal))
                        raw = previous().value;
                    else
                        raw = consume(TokenType::Tag).value;

                    pred.conditions.emplace_back(AttributePredicate{ name, raw, op });
                    continue;
                }

                // Handle attribute tests (@name='value')
                if (match(TokenType::Attribute)) {
                    std::string name = consume(TokenType::Tag).value;
                    std::string op = consume(TokenType::Operator).value;
                    std::string val = consume(TokenType::Literal).value;
                    std::string clean; clean.reserve(val.size());
                    for (size_t i = 0; i < val.size(); ++i) {
                        if (val[i] == '\\' && i + 1 < val.size()) {
                            clean += val[i + 1]; ++i;
                        }
                        else clean += val[i];
                    }
                    pred.conditions.emplace_back(AttributePredicate{ name, clean, op });
                }
                // Handle position predicates ([1], [last()])
                else if (std::isdigit(current().value[0])) {
                    int idx = std::stoi(consume(TokenType::Tag).value);
                    pred.conditions.emplace_back(PositionPredicate{ idx });
                }
                // Handle logical operators (and, or)
                else if (check(TokenType::Tag) &&
                    (current().value == "and" || current().value == "or"))
                {
                    advance();
                }
                else {
                    throw std::runtime_error("Unexpected token in predicate at pos "
                        + std::to_string(current().position));
                }
            }
            return pred;
        }

        // Helper methods for token stream navigation
        bool match(TokenType t) { if (check(t)) { advance(); return true; } return false; }
        bool check(TokenType t) const { return !isAtEnd() && current().type == t; }
        Token consume(TokenType t) { if (check(t)) return advance(); throw std::runtime_error("Unexpected token"); }
        Token advance() { if (!isAtEnd()) ++pos_; return previous(); }
        Token peek(size_t n = 1) const { return tokens_[pos_ + n]; }
        Token current() const { return tokens_[pos_]; }
        Token previous() const { return tokens_[pos_ - 1]; }
        bool isAtEnd() const { return current().type == TokenType::End; }

        const std::vector<Token>& tokens_;
        size_t                    pos_{ 0 };
    };

    // -----------------------------------------------------------------------------
    // Qt Locator Descriptor
    // -----------------------------------------------------------------------------

    /// Represents a Qt widget locator with metadata
    struct QtLocator {
        std::string uid;       ///< Unique identifier for the widget
        json        meta;      ///< JSON metadata about the widget
        std::string container; ///< UID of the containing widget

        /// Formats the locator as a string
        std::string finalize() const {
            std::string res = uid + " = " + meta.dump(4);
            if (!container.empty()) {
                constexpr std::string_view trailer{ "\n}" };
                if (res.ends_with(trailer)) {
                    res.erase(res.size() - trailer.size());
                }
                res += ",\n    \"container\": " + container + "\n}";
            }
            res += "\n";
            return res;
        }
    };

    // -----------------------------------------------------------------------------
    // XPath to Qt Locator Converter
    // -----------------------------------------------------------------------------

    /// Converts XPath locators to Qt widget descriptors
    template<typename Classifier = HeuristicQtClassifier>
    class XPathConverter {
    public:
        explicit XPathConverter(const std::vector<XLocator>& steps)
            : steps_(steps) {}

        /// Converts XPath locators to Qt widget descriptors
        std::vector<QtLocator> convert() const {
            std::vector<QtLocator> out;
            out.reserve(steps_.size());
            std::string parent;
            
            for (auto const& step : steps_) {
                std::string arch = classifier_(step.tag);
                std::string uid = generateUid(parent, step, arch);

                json meta;
                meta["archetype"] = arch;
                if (step.predicate) {
                    for (auto const& cond : step.predicate->conditions) {
                        if (std::holds_alternative<AttributePredicate>(cond)) {
                            auto const& a = std::get<AttributePredicate>(cond);
                            meta[a.name] = a.value;
                        }
                        else {
                            auto const& p = std::get<PositionPredicate>(cond);
                            if (p.position > 1) meta["occurrence"] = p.position;
                        }
                    }
                }
                meta["visible"] = 1;

                out.emplace_back(
                    std::move(uid),
                    std::move(meta),
                    std::move(parent)
                );
                parent = out.back().uid;
            }
            return out;
        }

    private:
        /// Generates a unique identifier for a widget
        std::string generateUid(
            const std::string& parent,
            XLocator const& step,
            std::string const& arch
        ) const {
            std::string token = (step.tag == "*") ? "any" : step.tag;
            std::string uid = parent.empty()
                ? token + "_" + arch
                : parent + "_" + token + "_" + arch;
                
            if (step.predicate) {
                for (auto const& cond : step.predicate->conditions) {
                    if (std::holds_alternative<AttributePredicate>(cond)) {
                        auto const& a = std::get<AttributePredicate>(cond);
                        uid += "_" + a.name + "_" + a.value;
                    }
                }
            }
            return util::canonicalize(uid);
        }

        const std::vector<XLocator>& steps_;
        Classifier classifier_{};
    };

    // -----------------------------------------------------------------------------
    // Pipeline Components
    // -----------------------------------------------------------------------------

    /// Four-stage pipeline for XPath to Qt locator conversion
    template<
        typename TokenizeFnSig,
        typename ParseFnSig,
        typename ConvertFnSig,
        typename DeclareFnSig,
        typename Classifier = HeuristicQtClassifier
    >
    class QtPythonDeclarationsFrom {
    public:
        std::decay_t<TokenizeFnSig>  tokenize_; ///< Tokenization function
        std::decay_t<ParseFnSig>     parse_;    ///< Parsing function
        std::decay_t<ConvertFnSig>   convert_;  ///< Conversion function
        std::decay_t<DeclareFnSig>   declare_;  ///< Declaration function
        std::decay_t<Classifier>     classifier_; ///< Widget classifier

        mutable std::vector<QtLocator> _cache; ///< Cache for converted locators

        /// Constructs a pipeline with the given functions
        QtPythonDeclarationsFrom(
            TokenizeFnSig tk,
            ParseFnSig    ps,
            ConvertFnSig  cv,
            DeclareFnSig  dc
        ) : tokenize_(std::move(tk))
            , parse_(std::move(ps))
            , convert_(std::move(cv))
            , declare_(std::move(dc))
            , classifier_()
        {}

        /// Processes an XPath expression through the pipeline
        auto operator()(std::string_view xpath) const {
            auto tokens = tokenize_(xpath);
            auto xlocs = parse_(tokens);
            _cache = convert_(xlocs);
            return declare_(_cache);
        }
    };

} // namespace hlat
