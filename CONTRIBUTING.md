# Contributing to Catchim

Thank you for your interest in contributing to Catchim! Whether you are helping improve the C++ native engine or enhancing the Web editor interface, all contributions are welcome.

## Development Setup

Catchim consists of two distinct components:

### 1. App (`app/` - Native C++ Desktop & WebAssembly)
- **Toolchain:** CMake 3.20+, C++20 compiler (MSVC on Windows, GCC, or Clang), Qt 6.6+, and FFmpeg.
- **WebAssembly Build:** Emscripten SDK (`emsdk`). Run `./Build_Web.bat` or `app/tools/build-browser.ps1`.

### 2. Web (`web/` - Browser Editor)
- **Toolchain:** Node.js 18+ or Bun.
- **Installation:**
  ```bash
  cd web
  npm install
  npm run dev
  ```

## Submitting Pull Requests

1. Fork the repository and create your branch from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
2. Ensure your changes compile cleanly without warnings.
3. Follow the existing coding conventions:
   - C++: Modern C++20 idioms, clean ownership, RAII.
   - TypeScript/React: Strict typing, modern hooks, Tailwind CSS styling.
4. Commit your changes with descriptive commit messages following the Conventional Commits specification (e.g. `feat: ...`, `fix: ...`, `docs: ...`).
5. Open a Pull Request on GitHub with a description of the changes and testing performed.

## Community Standards

All participants are expected to adhere to the project's [Code of Conduct](CODE_OF_CONDUCT.md).
