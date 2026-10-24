# Initial D: Another Stage — GBA Decompilation & Native PC Port

![Platform](https://img.shields.io/badge/Platform-GBA%20%7C%20PC-blue)
![Language](https://img.shields.io/badge/Language-C-green)
![License](https://img.shields.io/badge/License-MIT-brightgreen)
![Status](https://img.shields.io/badge/Status-Work%20in%20Progress-orange)

A reverse-engineering and decompilation project for **Initial D: Another Stage** (Game Boy Advance). The ultimate goal of this project is to reverse-engineer the original GBA ROM into clean C code, create a native, high-performance PC port, and introduce a full English localization.

---

## 🎯 Project Goals

- **100% Matching Decompilation:** Reconstruct the source code in clean C to achieve byte-for-byte fidelity with the original GBA binary.
- **Native PC Port:** Build a standalone executable to run the game natively on modern operating systems (Linux, Windows, macOS) without relying on emulators.
- **English Localization:** Translate all menus, dialogues, story arcs, and UI elements from Japanese into English.
- **Open Source:** Keep all research, source code, and tools completely free and accessible to the retro gaming and modding community.

---

## 🛠️ Tech Stack & Tooling

This project relies on a modern reverse-engineering and automation workflow:

- **Reverse Engineering:** [Ghidra](https://ghidra-sre.org/) + **REA**.
- **Automation & AI Assistance:** Local LLM workflows powered by [OpenCode](https://github.com/anc95/OpenCode) for code analysis, pattern matching, and refactoring assistance.
- **Emulation & Testing:** [mGBA](https://mgba.io/) for dynamic execution inspection and memory mapping.
- **Core Language:** C.

---

## 🚀 Current Progress

- [x] Initial GBA ROM memory mapping and symbol discovery.
- [x] Integration with REA and automated disassembly pipelines.
- [ ] Complete C decompilation of game loops and rendering systems.
- [ ] Multiplatform PC build layer (SDL2/OpenGL).
- [ ] English text extraction and re-injection.

---

## 💖 Support & Contributions

This is a **100% open-source passion project** developed independently by a recent Computer Engineering graduate. Because retro game decompilation is a time-intensive process, community support helps keep this effort moving forward!

If you find this project interesting and would like to support its development, hardware costs, or AI tooling pipelines, consider donating:

👉 **[Support the project on Ko-fi](https://ko-fi.com/turbosolutions)**

### Other Ways to Help
- **Star this repository** to increase project visibility.
- **Open issues or PRs** if you have experience with ARM7TDMI assembly, GBA hardware specs, or C refactoring.

---

## ⚠️ Disclaimer & Legal Notice

This repository **does not contain any copyrighted game assets, proprietary code, or ROM files**. 
To build or run the project, users must provide their own legally obtained copy of *Initial D: Another Stage* for GBA. All trademarks, character names, and game art belong to their respective owners (Sega / Kodansha / Shuichi Shigeno).

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.
