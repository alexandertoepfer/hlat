# Heuristic Layer Abstraction Transformer (HLAT)

*A tiny, header‑only C++20 engine that converts XPath‑style selectors into **Qt locator descriptors** for JavaScript/Python-safe objects.*

Originally designed for GUI automation and model‑based testing, it is useful anywhere a stable mapping from DOM‑like
paths to Qt widget metadata is required, infers Qt widget archetypes (e.g. `PushButtonQT`, `TextFieldQT`) directly from tag names.

## 🚀 Quick Start (30 lines)

```cpp
#include "hlat.hpp"

#include <numeric>
#include <iostream>

int main() {
    auto pydecl = hlat::QtPythonDeclarationsFrom<
        hlat::XPathLexerFn,
        hlat::XPathParserFn,
        hlat::QtLocatorBuilderFn,
        hlat::QtLocatorEmitterFn,
        hlat::HeuristicQtClassifier
    >{
        [](auto xpath) { return hlat::XPathLexer(xpath).tokenize(); },
        [](auto const& toks) { return hlat::XPathParser(toks).parse(); },
        [](auto const& xlocs) { return hlat::XPathConverter(xlocs).convert(); },
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
        std::cout << (pydecl | xpath) << "\n";
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
}
div_QWidget_class_header_span_QWidget = {
    "archetype": "QWidget",
    "visible": 1,
    "container": div_QWidget_class_header
}
div_QWidget_class_header_span_QWidget_text_TextFieldQT = {
    "archetype": "TextFieldQT",
    "visible": 1,
    "container": div_QWidget_class_header_span_QWidget
}

any_QWidget_name_content = {
    "archetype": "QWidget",
    "name": "content",
    "visible": 1
}
any_QWidget_name_content_button_PushButtonQT = {
    "archetype": "PushButtonQT",
    "occurrence": 2,
    "visible": 1,
    "container": any_QWidget_name_content
}
```
