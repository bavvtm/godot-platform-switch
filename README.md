# Godot 4 for Nintendo Switch

This repository contains a *Godot 4* platform targeting the Nintendo Switch architecture via [devkitPro](https://devkitpro.org/) and [mesa-switch](https://github.com/danfromtico/mesa-switch.git). See the original [Godot Engine docs](https://docs.godotengine.org/en/stable/) for info about Godot.

> [!NOTE]
> **EXPERIMENTAL BUILD**
> Built and tested on real hardware (**Nintendo Switch Lite**) with real game.
> See [What's Working](#whats-working) below for supported features and things that still need testing.

## What's Working

Core features implemented and verified on hardware:

- ✅ **Vulkan Renderer (`Mobile` & `Forward+`)** — Powered by Mesa NVK (`mesa-switch`) with working persistent `shader_cache` on microSD.
- ✅ **OpenGL ES 3 Renderer (`Compatibility`)** — Works out of the box.
- ✅ **Docked / Handheld Resolution & DPI** — Automatic switching between `720p` (Handheld) and `1080p` (Docked) with model-aware DPI scaling.
- ✅ **Audio (`audren`)** — Background music and sound effects play properly.
- ✅ **Joypad / Controller (`hid`)** — Joy-Con and Pro Controller inputs work natively.
- ✅ **HD Rumble / Vibration (`hid`)** — Joy-Con vibration works natively with timed duration support.
- ✅ **Motion Sensors (`SixAxis`)** — Built-in gyroscope and accelerometer (`Input.get_gyroscope()` / `Input.get_accelerometer()`) work natively.
- ✅ **Touchscreen** — Handheld touch input is supported.
- ✅ **Virtual Keyboard (`SwkbdInline` + `SwitchAPI`)** — Supports both non-blocking inline keyboard (`SwkbdInline`) for standard Godot `LineEdit`/`TextEdit` nodes and blocking modal dialogs via `SwitchAPI`.
- ✅ **Networking & TLS (`mbedtls` + `csrng`)** — Hardware entropy (`csrng`), `Crypto`, HTTPS requests (`HTTPRequest`), and DTLS over UDP are tested and working.
- 🧪 **Other Built-in Third-Party Modules** — Included in the build, though not all modules have been individually tested yet.

## Hardware Testing Notes

- **Primary Test Unit:** Developed and tested mainly on a **Nintendo Switch Lite** (`HARDWARE_HOAG`).
- **Docked Mode, OLED DPI & HD Rumble:** Dynamic `720p` ↔ `1080p` resolution switching, OLED DPI scaling (`210 DPI` handheld / `96 DPI` docked), and HD Rumble vibration were tested and verified using a borrowed **Nintendo Switch OLED** (since I only own a Switch Lite, future testing for docked/OLED-specific regressions depends on whether I can borrow it again).

## Known Quirks & Limitations

- **Slow First Boot (~20s black screen):** The first launch takes around 20 seconds on a black screen while shaders compile and assets load from the microSD card.
- **No GDExtension Support:** Dynamic libraries (`dlopen`) are stubbed out on Switch, so only pure GDScript projects are supported for now.
- **No C# / .NET:** Built without Mono/C# support—GDScript only.
- **Full RAM Mode Required:** Launching from the Album menu (Applet Mode) is blocked due to low memory.

## Quick Usage

- **Godot Version:** Requires **Godot 4.7.2-stable** (standard build)—macos is not supported.
- **Install:** Extract `switch_exporter-*.zip` in your project root and reload the project.
- **Export:** Go to **Project > Export > Add... > Nintendo Switch** and choose your packaging mode:
  - **Separate Build:** Generates `.nro` + `.pck` (copy both to `sdmc:/switch/your_game/`).
  - **Fused Build:** Bundles `game.pck` directly inside `romfs:/`, producing a single self-contained `.nro` file.
- **Custom Launch Flags (Optional):** Place an `args.txt` file in the game folder on your SD card (or inside `romfs:/`) to pass extra CLI arguments.

## Credits & Acknowledgements

This project wouldn't be possible without the amazing work from the Nintendo Switch homebrew and open-source graphics communities:

- **[Godot Engine](https://godotengine.org)** — For the awesome open-source game engine.
- **[devkitPro](https://devkitpro.org)** — For `devkitA64`, `libnx`, and the Switch homebrew toolchain.
- **[Stary2001](https://github.com/Stary2001/godot.git)** — For the foundational `libnx` platform implementation for Godot 3.5 port.
- **[danfromtico](https://github.com/danfromtico/mesa-switch.git)** — For porting Mesa NVK (mesa-switch), this making all Godot rendering type possible.
- **[DemoxDev](https://github.com/DemoxDev/godot-switch-template.git)** — For helpful documentation and references regarding POSIX shims and adapting Switch platform code.
