# Contributing to OptiScaler

To maintain build efficiency and project structure, please follow these requirements for all code contributions.

### Precompiled Header (PCH) Usage

We use Precompiled Headers to significantly reduce compile times. To prevent build degradation, please follow these rules:

* **Source Files (`.cpp`):** Every source file must include `"pch.h"` as the **very first** non-comment line.
* **Header Files (`.h`):** Never include `"pch.h"` inside a header file.
* **Utility Code:** Do not add general-purpose utilities, macros, or global declarations to `pch.h`. Use `SysUtils.h` or create a new functional header instead.
* **Adding Dependencies:** Only add large, stable, third-party or system headers (e.g., Windows/SDK headers) to `pch.h`.

### Why this matters

Correct PCH usage keeps our build times fast (more than 2x faster than a standard build). Including `pch.h` in other headers or using it as a global utility bucket breaks the compiler's ability to cache the precompiled state and forces unnecessary full rebuilds.
