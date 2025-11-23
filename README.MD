---
# Endless Dodge 🎮

A fast, addictive, endlessly escalating 2D dodge game written in **pure C** using **SDL2**.
Survive as long as possible, dodge falling blocks, and beat your high score!

![Gameplay](./screenshot.png) <!-- optional; remove if no screenshot -->
---

## 🚀 Features

- ⚡ **Fast-paced endless gameplay**
- 📈 **Dynamic difficulty** — faster spawns & falling speed over time
- 💾 **Persistent high scores** (`highscore.dat`)
- 🎮 **Smooth controls** (A/D or ←/→)
- 🧱 **Optimized obstacle pool**
- 🧵 **Stable 60 FPS loop**
- 🧱 **No external assets required**
- 🔒 **Production-ready, error-checked SDL initialization**

---

## 📦 Requirements

- C compiler with C99 support
- SDL2 development libraries

### Install SDL2

#### Linux

```bash
sudo apt install libsdl2-dev
```

#### macOS (Homebrew)

```bash
brew install sdl2
```

#### Windows

Install the SDL2 development bundle from:
[https://libsdl.org/download](https://libsdl.org/download)

---

## 🔨 Build Instructions

### Linux / macOS

```bash
gcc -std=c99 -Wall -Wextra -O2 game.c -o endless_dodge `sdl2-config --cflags --libs`
```

### Windows (WSL)

```bash
gcc game.c -lm -o endless_dodge `sdl2-config --cflags --libs`

```

Then run:

```bash
./endless_dodge
```

---

## 🎮 Controls

| Action          | Key                     |
| --------------- | ----------------------- |
| Move Left       | **A** or **←**          |
| Move Right      | **D** or **→**          |
| Start / Restart | **Enter**               |
| Pause / Resume  | **P**                   |
| Quit            | **Esc** or close window |

---

## 🕹 Gameplay Overview

- Survive while random blocks fall from the top.
- Score grows over time and by dodging blocks.
- Difficulty ramps up automatically.
- Crashing into a block ends the run.
- Your **best score** is automatically written to `highscore.dat`.

---

## 📁 Project Structure

```
.
├── game.c            # Full game source code
├── highscore.dat     # Auto-generated after first run
└── README.md         # This file
```

---

## 🧩 How It Works (Technical Summary)

- Uses an **obstacle pool** (fixed-size array) for fast, GC-free gameplay.
- Implements a **state machine**: MENU → PLAYING → PAUSED → GAME OVER
- Dynamic spawn timing using exponential decay.
- Stable update loop with delta timing and optional frame limiting.
- Solid AABB collision and renderer abstraction.
- High score persisted in a binary file.

---

## 🛠 Future Enhancements (Optional Ideas)

- 🔊 Sound effects via SDL_mixer
- ⭐ Power-ups / shields
- 🧩 Multiple game modes
- 🎨 Custom themes
- ⌨️ Key remapping
- 🖥 High-res UI overlay or bitmap font rendering

If you want any of these implemented, I can build them for you.

---

## ❤️ License

MIT — Feel free to use, modify, or build on this game.

---
