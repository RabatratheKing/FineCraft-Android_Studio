# 🧱 Finecraft

**Finecraft** is a lightweight cross-platform voxel game prototype built with C++ and designed to run on **Android and Windows**.

It started as an Android project in Google AI Studio and has evolved into a small custom voxel engine with procedural terrain, asynchronous chunk streaming, greedy meshing, first-person physics, block interaction, mobile controls, and a native Windows build.

> 🚧 Finecraft is still a prototype. Expect bugs, placeholder systems, and ongoing experimentation.

## 🎮 Current Features

### 🌍 Voxel World

* Procedural terrain generation
* Infinite-style chunk streaming
* Chunk unloading
* Greedy meshing
* Distance and frustum culling
* Trees, villages, water, and terrain variation
* Block breaking and placement
* Cross-chunk mesh updates

### 🧍 Player

* First-person movement
* Gravity
* Jumping
* Crouching
* Voxel collision
* 2-block player height
* Headroom detection
* Debug fly mode

### 🎨 Rendering

* OpenGL ES 3.0 on Android
* OpenGL 3.3 on Windows
* Texture atlas rendering
* Per-face block materials
* Ambient occlusion
* Directional lighting
* Dynamic day/night cycle
* Sky gradient
* Clouds
* Distance fog
* Water rendering

### 📱 Android

* Jetpack Compose UI
* Virtual joystick
* Touch camera controls
* Mobile action controls
* Main menu
* Settings menu
* Persistent settings

### 🖥️ Windows

* Native `.exe`
* GLFW windowing and input
* Keyboard + mouse controls
* Main menu
* Pause menu
* Settings
* Fullscreen support
* No web wrapper

## 🛠️ Technology

| Component                | Technology      |
| ------------------------ | --------------- |
| Core Engine              | C++             |
| Android Renderer         | OpenGL ES 3.0   |
| Windows Renderer         | OpenGL 3.3      |
| Android UI               | Jetpack Compose |
| Windows Windowing        | GLFW            |
| Math                     | GLM             |
| Android Bridge           | JNI             |
| Android Graphics Context | EGL             |
| Build System             | CMake + Gradle  |
| Windows Toolchain        | MinGW-w64       |

## 🏗️ Architecture

The native engine is split into logical subsystems:

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

The goal is to keep the majority of the game logic platform-independent.

```text
                    Finecraft Core
                         │
             ┌───────────┴───────────┐
             │                       │
         Android                   Windows
             │                       │
       Compose + JNI          GLFW + Keyboard/Mouse
```

## ⚙️ Performance

The current prototype has been tested on a physical Android device at approximately **60 FPS** and runs smoothly on Windows hardware.

The engine uses:

* Asynchronous chunk generation
* Asynchronous mesh generation
* Chunk streaming priorities
* Greedy meshing
* Frustum culling
* Distance culling
* GPU upload budgeting
* Chunk unloading

Performance will continue to be improved as the project grows.

## 🎮 Controls

### Android

Touch controls include:

* Virtual movement joystick
* Touch camera
* Jump
* Crouch
* Break
* Place
* Inventory/hotbar controls

### Windows

```text
W A S D       Move
Mouse         Look
Space         Jump
Ctrl / Shift  Crouch
F             Toggle fly/debug mode
Left Click    Break block
Right Click   Place block
1–9           Hotbar selection
E             Inventory
Esc           Pause / menu
F11           Fullscreen
```

Controls may change as development continues.

## 📂 Project Status

### Working

* ✅ Procedural terrain
* ✅ Chunk streaming
* ✅ Chunk unloading
* ✅ Greedy meshing
* ✅ Frustum culling
* ✅ First-person movement
* ✅ Collision
* ✅ Crouching
* ✅ Jumping
* ✅ Block breaking
* ✅ Block placement
* ✅ Texture atlas
* ✅ Day/night cycle
* ✅ Clouds
* ✅ Fog
* ✅ Water
* ✅ Android build
* ✅ Windows build
* ✅ Main menu
* ✅ Settings

### In Development

* 🔄 Inventory and hotbar expansion
* 🔄 Crafting
* 🔄 World save/load
* 🔄 Better world generation
* 🔄 More blocks and materials
* 🔄 Mobs and AI
* 🔄 Improved lighting
* 🔄 Additional PC/mobile parity

## 🚧 Known Limitations

Finecraft is still an early prototype.

Some systems are intentionally simple, and visual quality, world generation, AI, survival mechanics, and content are still being developed.

The project prioritizes experimentation, performance, and engine development over finished-game polish.

## 🚀 Building

### Android

The Android version uses the Gradle/Android Studio toolchain with the NDK and CMake.

Open the project in Android Studio and build the application normally.

### Windows

The Windows version uses:

* CMake
* GLFW
* GLM
* OpenGL 3.3
* MinGW-w64 or MSVC

The project produces a native:

```text
Finecraft.exe
```

## 🤖 Development

Finecraft is an experimental project developed with significant assistance from AI coding tools.

AI has been used for:

* Engine development
* Debugging
* Platform ports
* Refactoring
* Rendering work
* Gameplay systems
* Build troubleshooting

Human testing and validation are still essential, especially for real-device performance and gameplay behavior.

## 📜 License

This repository is currently a **prototype / personal development project**.

See the repository for the current licensing terms.

## ⭐ About

Finecraft started as a simple Android voxel experiment and is growing into a lightweight cross-platform voxel engine.

The long-term goal is a small, efficient voxel game that runs smoothly across **Android and Windows** while keeping the core engine shared between platforms.

**Built from cubes. Powered by C++. 🧱**
