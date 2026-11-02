# 🧱 Finecraft — Windows Edition

Finecraft is a lightweight voxel game prototype built in C++.

This is the **native Windows desktop version** of Finecraft, using the shared voxel engine with a Windows-specific GLFW/OpenGL platform layer.

> 🚧 Finecraft is still an early prototype. Expect unfinished systems, placeholder content, and bugs.

## 🖥️ Windows Build

Finecraft runs as a native Windows executable:

```text
Finecraft.exe
```

It is **not a website, browser game, or WebView application**.

The current build is statically linked and does not require a separate installer or DLLs beside the executable.

### Requirements

* Windows 10 or newer
* OpenGL 3.3-compatible GPU
* Keyboard and mouse
* Recommended: dedicated or modern integrated GPU

Almost any reasonably modern PC should be capable of running the prototype.

## 🎮 Controls

```text
W A S D       Move
Mouse         Look around
Space         Jump
Ctrl / Shift  Crouch
F             Toggle fly/debug mode

Left Click    Break block
Right Click   Place block

1–9           Select hotbar slot
Mouse Wheel   Change hotbar slot
E             Open/close inventory

Esc           Pause / release cursor
F11           Toggle fullscreen
```

Controls may change as development continues.

## 🌍 Current Features

### Voxel World

* Procedural terrain generation
* Chunk-based world streaming
* Chunk unloading
* Greedy meshing
* Distance culling
* Frustum culling
* Trees and terrain variation
* Water
* Block breaking
* Block placement
* Dynamic chunk mesh updates
* Cross-chunk boundary handling

### 🧍 Player

* First-person camera
* WASD movement
* Gravity
* Jumping
* Crouching
* 2-block player height
* Headroom detection
* Voxel collision
* Debug fly mode

### 🎨 Rendering

* OpenGL 3.3
* Texture atlas
* Per-face block textures
* Ambient occlusion
* Directional lighting
* Dynamic day/night cycle
* Sky gradient
* Clouds
* Distance fog
* Water rendering

### 🎒 Inventory & Gameplay

* 36-slot inventory
* 9-slot hotbar
* Item stacking
* Block pickup
* Block placement using selected items
* Basic crafting
* Save/load systems under development

### 🎵 Audio

Finecraft also includes an AI-assisted music system/API as part of the broader project.

## 🧭 Menus

The Windows version includes:

### Main Menu

* Play
* Settings
* Quit

### Pause Menu

Press `Esc` during gameplay to access:

* Resume
* Settings
* Quit

### Settings

Current Windows settings include:

* Mouse sensitivity
* Fullscreen
* Additional settings as development continues

## ⚙️ Technical Overview

The Windows version shares most of its core engine with the Android version.

```text
                Finecraft Core
                      │
          ┌───────────┴───────────┐
          │                       │
       Android                  Windows
          │                       │
    JNI + EGL + Touch       GLFW + OpenGL + KB/Mouse
```

### Technologies

| Component           | Technology                |
| ------------------- | ------------------------- |
| Language            | C++                       |
| Windowing           | GLFW                      |
| Rendering           | OpenGL 3.3                |
| Mathematics         | GLM                       |
| Build System        | CMake                     |
| Windows Toolchain   | MinGW-w64 / MSVC          |
| Android Counterpart | OpenGL ES 3.0 + JNI + EGL |

The majority of the engine is platform-independent.

## 📁 Project Structure

```text
app/src/main/cpp/
├── core/
├── world/
├── rendering/
├── player/
├── gameplay/
├── input/
├── platform/
│   └── android/
└── shaders/
```

Windows-specific functionality lives in the desktop platform/rendering layer while world, physics, meshing, and gameplay logic remain shared where possible.

## ⚡ Performance

The prototype is designed to stay lightweight.

Current testing has shown:

* Smooth gameplay on Windows
* Approximately 10% CPU usage on the tested system
* Roughly 10–30 MB RAM during the observed session
* Smooth rendering and chunk streaming

Actual performance will vary depending on hardware and world state.

The engine currently uses:

* Asynchronous chunk generation
* Asynchronous mesh generation
* Greedy meshing
* Distance culling
* Frustum culling
* Chunk unloading
* Limited GPU mesh uploads

## 🚧 Known Limitations

Finecraft is not intended to be a finished game yet.

Current limitations may include:

* Early procedural terrain
* Limited block selection
* Limited crafting content
* Simple mob/gameplay systems
* Early water rendering
* Limited world-generation variety
* Save/load still evolving
* Windows and Android feature parity may temporarily differ

## 🚀 Running the Game

Extract the Windows build and launch:

```text
Finecraft.exe
```

No browser is required.

No installer is required for the current build.

## 🛠️ Building From Source

The Windows build uses:

* CMake
* GLFW
* GLM
* C++ compiler with C++ support
* OpenGL 3.3 development support

A typical build process is:

```text
Configure CMake
        ↓
Build project
        ↓
Finecraft.exe
```

Refer to the repository's build configuration for the exact commands and dependencies.

## 🤖 Development

Finecraft is an experimental project developed with substantial assistance from AI coding tools.

AI has been used for:

* Engine development
* Debugging
* Rendering
* Gameplay systems
* Platform ports
* Refactoring
* Build troubleshooting
* Testing and experimentation

Real hardware testing remains important, especially for graphics, input, performance, and platform-specific behavior.

## 📜 License

Finecraft is currently a prototype/personal development project.

See the repository for the current licensing information.

## ⭐ About

Finecraft began as an Android voxel experiment and has evolved into a native cross-platform voxel project.

The goal is to build a lightweight voxel game with a shared C++ core that can run smoothly on both **Android and Windows**.

**Built from cubes. Powered by C++. 🧱**
