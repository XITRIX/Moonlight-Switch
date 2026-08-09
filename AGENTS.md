# Repository Guidelines

## Project Structure & Module Organization

Application code lives in `app/src/`, with public headers in `app/include/`. Streaming, renderer, audio, crypto, gesture, and view code is grouped into matching subdirectories. Platform integration belongs under `app/platforms/<platform>/`; keep platform-specific changes out of shared code where practical. Runtime XML, translations, fonts, images, materials, and shaders are in `resources/`. Third-party sources and CMake helpers live in `extern/`; initialize them with `git submodule update --init --recursive` rather than editing vendored code. Prebuilt Switch libraries are under `lib/switch/`, while packaging and developer helpers are in `scripts/` and `docs/`.

## Build, Test, and Development Commands

- `cmake -B build/pc -DPLATFORM_DESKTOP=ON -DCMAKE_BUILD_TYPE=Release` configures a desktop build.
- `cmake --build build/pc --parallel` compiles the configured desktop target. Run from the repository root so external `resources/` remain discoverable.
- `cmake -B build/switch -DPLATFORM_SWITCH=ON` followed by `cmake --build build/switch --target Moonlight.nro --parallel` builds the Switch package with devkitPro installed.
- `cmake --preset linux-release && cmake --build --preset linux-release` uses the supported Ninja preset for portable Linux releases.
- `scripts/psv-dev.sh build` produces `build/psvita/Moonlight.vpk` when `VITASDK` is configured.

Build directories are disposable and must remain untracked.

## Coding Style & Naming Conventions

Use C++17 and four-space indentation. Match the surrounding brace and line-wrapping style. Classes and structs use `PascalCase`; source files and most free functions use `snake_case`; constants and macros follow the convention of their subsystem. Keep headers paired with their implementation and prefer existing Borealis and project helpers over new wrappers. No repository-wide formatter is configured, so keep diffs focused and avoid formatting unrelated lines.

## Testing Guidelines

There is currently no standalone unit-test suite. Treat a clean build for every affected platform as the minimum check. For UI, input, streaming, or decoder changes, manually exercise the affected workflow and report the hardware, OS, renderer, and stream settings in the pull request. Validate resource and localization changes in-app.

## Commit & Pull Request Guidelines

Recent commits use short, imperative summaries such as `Fix Korean on-screen keyboard mapping` and optionally reference issues (`Fix #313: ...`). Keep each commit scoped to one logical change. Pull requests should explain behavior and platform impact, link relevant issues, list build/manual verification, and include screenshots or recordings for visible UI changes. Ensure relevant GitHub Actions platform builds pass before merge.
