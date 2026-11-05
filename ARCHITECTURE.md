# Finecraft Architecture

This project is a Minecraft-like voxel engine running natively on Android using C++, OpenGL ES 3.0, and EGL. The architecture relies on JNI to bridge the Android Kotlin lifecycle events to the native C++ engine.

## Directory Structure

`app/src/main/cpp/`
- **core/**
  - `Logger.h`: Defines Android logging macros.
  - `Globals.h`/`Globals.cpp`: Centralized state (atomic variables, thread sync primities, global variables for player/world).
- **world/**
  - `World.h`/`World.cpp`: Voxel world generation, greedy meshing, chunk manipulation logic.
  - `ChunkManager.h`/`ChunkManager.cpp`: The native chunk worker thread that processes world generation and chunk meshing queues.
- **rendering/**
  - `Renderer.h`/`Renderer.cpp`: Contains the EGL setup and the primary OpenGL ES 3.0 render loop. Responsible for submitting geometry to the GPU.
- **player/**
  - `Physics.h`/`Physics.cpp`: Voxel AABB collision detection and player movement physics ticking.
- **gameplay/**
  - `Raycast.h`/`Raycast.cpp`: Line-of-sight raycasting logic for breaking or placing blocks.
- **platform/android/**
  - `JNI.cpp`: Android Java Native Interface hooks capturing touch inputs, lifecycle events, and screen dimensions.
- **shaders/**
  - `Shaders.h`: Inline GLSL definitions for voxel terrain rendering and atmospheric sky generation.

## Key Design Principles
- **Global State**: For rapid prototyping, most state resides in `Globals.h` avoiding complex dependency injection patterns between the decoupled subsystems.
- **Threading**: The system uses two main native threads:
  1. `renderLoop`: The main EGL render thread driving the GPU.
  2. `chunkWorkerLoop`: A background worker handling chunk generation and greedy meshing to avoid stuttering on the main thread.
- **Vertex Layout**: A fixed 10-float vertex format is strictly used for terrain rendering to maintain alignment across attributes (Position, UV, Texture Index, Normal, Ambient Occlusion).

## Build System
The native library is built as `libfinecraft.so` using CMake, and depends on external math library `glm`.
