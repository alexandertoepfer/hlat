# 🌐 Heuristic Layer Abstraction Transformer (HLAT)

*A tiny, header‑only C++20 engine that converts XPath‑style selectors into **canonical Qt locator descriptors**.*

Originally designed for GUI automation and model‑based testing, it is useful anywhere a stable mapping from DOM‑like
paths to Qt widget metadata is required.

## 🔑 Key Features

- **Header‑only** – drop in `hlat.hpp`, depend only on [nlohmann/json](https://github.com/nlohmann/json).  
- **Modern C++20** – uses `<ranges>`, `<regex>`, `<string_view>`, and lambda‑injection.  
- **Heuristic classification** – infers Qt widget archetypes (e.g. `PushButtonQT`, `TextFieldQT`) directly from tag names.  
- **Rich metadata** – generates stable UID strings plus a JavaScript/Python-safe JSON blobs (`archetype`, `attributes`, `occurrence`, `visibility`, `container`).  
- **MIT‑licensed** – lightweight, production‑ready, and easy to integrate.

## 🚀 Quick Start (30 lines)

```cpp
#include "hlat.hpp"
#include <numeric>
#include <iostream>

int main() {
    auto pydecl = hlat::QtPythonDeclarationsFrom<
        std::vector<hlat::Token>(*)(std::string_view),
        std::vector<hlat::XLocator>(*)(const std::vector<hlat::Token>&),
        std::vector<hlat::QtLocator>(*)(const std::vector<hlat::XLocator>&),
        std::string(*)(std::vector<hlat::QtLocator>&),
        hlat::HeuristicQtClassifier
        >{
            // tokenize:
            [](auto xpath) { return hlat::XPathLexer(xpath).tokenize(); },
            // parse:
            [](auto const& toks) { return hlat::XPathParser(toks).parse(); },
            // convert:
            [](auto const& xlocs) { return hlat::XPathConverter(xlocs).convert(); },
            // finalize:
            [](auto& qtlocs) -> std::string {
                return std::accumulate(
                    qtlocs.begin(), qtlocs.end(),
                    std::string{},
                    [](std::string acc, auto& qt) {
                        return std::move(acc) + qt.finalize();
                    }
                );
            }
        };

    std::vector<std::string> xpaths = {
        "//div[@class='header']/span[1]/text()",
        "//*[@name='content']//button[2]",
        "//form/child::container[1]/following-sibling::button",
        "//button[@name='submit' and @enabled='true']",
        "//ns:form//*[@type='input']",
        "//bookstore/book[price>35]/title",
        "//ul/li[position()<3]",
        "//section[@id='intro']/descendant::p",
        "//*[local-name()='svg']/*[name()='path']",
        "/root/*[2]//child::leaf",
        "//parent::node()/preceding-sibling::sibling"
    };

    for (auto const& xpath : xpaths) {
        std::cout << "Processing XPath : " << xpath << "\n";
        std::cout << pydecl(xpath) << "\n";
    }
    return 0;
}
```

Sample output:
```text
div_QWidget_class_header = {
    "archetype": "QWidget",
    "class": "header",
    "visible": 1
};
div_QWidget_class_header_span_QWidget = {
    "archetype": "QWidget",
    "visible": 1,
    "container": div_QWidget_class_header
};
div_QWidget_class_header_span_QWidget_text_TextFieldQT = {
    "archetype": "TextFieldQT",
    "visible": 1,
    "container": div_QWidget_class_header_span_QWidget
};

any_QWidget_name_content = {
    "archetype": "QWidget",
    "name": "content",
    "visible": 1
};
any_QWidget_name_content_button_PushButtonQT = {
    "archetype": "PushButtonQT",
    "occurrence": 2,
    "visible": 1,
    "container": any_QWidget_name_content
};
```

## 📚 Features

### XPath Support
- Full XPath 1.0 syntax support
- Axis specifiers (`child::`, `parent::`, `following-sibling::`, etc.)
- Predicates with attribute tests and position
- Namespace support
- Wildcard matching (`*`)

## 🛠️ Building and Integration

### Dependencies
- C++20 compliant compiler
- [nlohmann/json](https://github.com/nlohmann/json) (header-only)

### Integration Steps
1. Copy `hlat.hpp` to your project
2. Include nlohmann/json
3. Use the provided pipeline or customize as needed

## 📄 License

Released under the MIT License – see LICENSE.
