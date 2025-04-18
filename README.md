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
    auto synth = hlat::QtLocatorSynthesizer<std::vector<hlat::LexicalUnit>(*)(std::string_view)>{ hlat::parsePath }; //< Or custom parser

    const std::string xpath =
        "/window[@title='DemoApp']/container[@name='main']/button[2]";

    for (const auto& rec : synth(xpath)) {
        std::cout << rec.uid << " = " << rec.meta.dump(4) << "\n\n";
    }
}
```

Output (example):

```text
window_ModuleQT = {
    "archetype": "ModuleQT",
    "windowTitle": "DemoApp",
    "visible": 1
}
window_main_ScrollViewQT = {
    "archetype": "ScrollViewQT",
    "container": "window_ModuleQT",
    "name": "main",
    "visible": 1
}
window_main_button_2_PushButtonQT = {
    "archetype": "PushButtonQT",
    "container": "window_main_ScrollViewQT",
    "occurrence": 2,
    "visible": 1
}
```

---

## License

HLAT is released under the MIT License – see the [LICENSE](LICENSE) file.

---

## Maintainer

**Alexander Toepfer** – alexander.toepfer@sartorius.com

Commercial support and integration services available through Sartorius.

