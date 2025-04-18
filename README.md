# Heuristic Layer Abstraction Transformer (HLAT)

HLAT is a single‑header C++20 library that converts XPath‑style strings into **canonical Qt locator descriptors**. It was written for GUI automation and model‑based testing, but it is useful anywhere you need a deterministic mapping from DOM‑like paths to Qt widget metadata.

---

## Features

* Header‑only. Only external requirement is [**nlohmann/json**](https://github.com/nlohmann/json).
* Modern C++20 (`std::ranges`, `std::regex`, CTAD, lambda‑based parser).
* Heuristic classifier derives Qt widget archetypes from tag names—no lookup table.
* Emits stable UID strings plus rich JSON metadata.
* MIT‑licensed, tiny source for easy reuse.

---

## Supported toolchains

| Compiler                  | C++ Standard | Tested on |
|---------------------------|--------------|-----------|
| **MSVC 19.38+** (Visual Studio 2022 17.9) | `/std:c++latest` | Windows 10/11 |
| **GCC 12+**               | `-std=c++20` | Ubuntu 22.04 |
| **Clang 15+**             | `-std=c++20` | macOS 14 / Linux |

> **MSVC note** – set *C++ Language Standard* to **Preview – Features from the Latest C++ Working Draft**.

---

## Dependency management with vcpkg

HLAT requires only *nlohmann‑json*. Using vcpkg with a static triplet keeps everything in one executable.

```powershell
# one‑time integration
vcpkg integrate install

# install json for static MSVC target
vcpkg install nlohmann-json:x64-windows-static
```

Configure with the vcpkg toolchain:

```powershell
cmake -B build -G "Visual Studio 17 2022" ^
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
      -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
      -DCMAKE_CXX_STANDARD=20
cmake --build build --config Release
```

For GCC/Clang on Linux or macOS, just install `nlohmann-json` from your package manager and run the usual `cmake -B build`.

---

## Quick example

```cpp
#include "hlat.hpp"

int main() {
    auto pyDecl = hlat::QtPythonDeclarationsFrom<
    std::vector<hlat::Token>(std::string_view),
    std::vector<hlat::XLocator>(const std::vector<hlat::Token>&),
    std::vector<hlat::QtLocator>(const std::vector<hlat::XLocator>&),
    std::string(std::vector<hlat::QtLocator>&),
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
        std::cout << pyDecl(xpath) << "\n";
    }
}
```

Output (example):

```text
Processing XPath : //div[@class='header']/span[1]/text()
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

Processing XPath : //*[@name='content']//button[2]
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

---

## License

HLAT is released under the MIT License – see the [LICENSE](LICENSE) file.

---

## Maintainer

**Alexander Toepfer** – alexander.toepfer@sartorius.com

Commercial support and integration services available through Sartorius.

