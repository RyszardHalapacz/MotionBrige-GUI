# MotionBridge Studio — GUI

Qt6 GUI for the MotionBridge engine. Communicates with the engine through a pure C ABI — no shared C++ headers, no linker dependencies between projects.

---

## Integrating the GUI into your engine project

The GUI is distributed as a shared library (`motionbridge_gui.dll`). The engine loads it and calls three functions to start the UI.

### Option A — FetchContent (recommended)

Add to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
    motionbridge_gui
    GIT_REPOSITORY https://github.com/RyszardHalapacz/GUI.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(motionbridge_gui)

target_link_libraries(your_engine PRIVATE motionbridge_gui)
```

Then include the API header:

```cpp
#include <motionbridge_api.h>
```

### Option B — manual (pre-built DLL)

Copy `motionbridge_gui.dll` and `motionbridge_gui.lib` next to your executable, then link against the import lib:

```cmake
target_link_libraries(your_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/motionbridge_gui.lib
)
```

---

## API reference

All functions use a pure C ABI (`extern "C"`). Call them in this order before entering your own event loop.

### `getVersion`

```c
const char* getVersion();
```

Returns a null-terminated version string (e.g. `"0.1.0"`). Call this first to verify you loaded the expected version of the DLL. The returned pointer is valid for the lifetime of the process.

---

### `setPTR`

```c
void setPTR(TranslationCallback cb);
```

Registers your translation function with the GUI. The GUI calls it on a background thread each time the user clicks **Translate**.

```c
typedef int (*TranslationCallback)(const char* yaml_path);
```

`yaml_path` is an absolute path to `motionbridge_run.yaml` that the GUI has already written. Your function must:

- read `motionbridge_run.yaml` to get input paths and translation parameters
- write progress lines to the file at `output.progress_file`
- write `motionbridge_result.yaml` to `output.dir` when done
- return `0` (OK), `2` (FAILED), or `3` (CANCELLED)

**Must be called before `runGUI`.** Not thread-safe — call from the main thread.

---

### `setConfigPath`

```c
void setConfigPath(const char* path);
```

Tells the GUI where to find `pipeline_options.yaml` on the filesystem. The GUI reads this file once at startup to populate the Frontend / Backend / Robot dropdowns.

`path` must be an absolute path to a valid `pipeline_options.yaml`. If not called, or if the file cannot be opened, the GUI falls back to a built-in default configuration.

**Must be called before `runGUI`.**

---

### `runGUI`

```c
int runGUI(int argc, char* argv[]);
```

Initialises Qt, creates the main window, and enters the event loop. **Blocks until the user closes the window.** Returns the Qt application exit code (typically `0`).

`argc`/`argv` are passed straight to `QApplication` — pass your process's own `argc`/`argv`.

---

## GUI workflow

This is what happens inside the GUI after `runGUI` is called:

```
runGUI()
  │
  ├─ reads pipeline_options.yaml (path from setConfigPath)
  │    └─ populates Frontend / Backend / Robot dropdowns
  │
  ├─ user loads a .gcode file
  │
  └─ user clicks Translate
       │
       ├─ GUI writes working files to <applicationDir>/motionbridge_studio/
       │    ├─ input.gcode           (copy of the loaded file)
       │    ├─ context.yaml          (optional — only if YAML editor is not empty)
       │    └─ motionbridge_run.yaml (translation request — see format below)
       │
       ├─ GUI deletes motionbridge_progress.txt and cancel.flag if they exist
       │
       ├─ GUI calls your TranslationCallback(yaml_path) on a background thread
       │
       ├─ GUI polls motionbridge_progress.txt every 200 ms
       │    └─ updates progress bar and status bar from PROGRESS lines
       │
       ├─ [optional] user clicks Cancel
       │    └─ GUI creates cancel.flag — your callback must check and return 3
       │
       └─ your callback returns
            ├─ 0 → GUI reads motionbridge_result.yaml, loads output files into editors
            ├─ 2 → GUI shows "Translation FAILED"
            └─ 3 → GUI shows "Translation cancelled"
```

---

## pipeline_options.yaml

The file passed to `setConfigPath` defines the frontends, backends and robots available in the UI dropdowns.

```yaml
frontends:
  - id: gcode_printer3d
    label: GCode 3D Printer
  - id: gcode_cnc
    label: GCode CNC

backends:
  - id: kuka_krl
    label: KUKA KRL
    robots:
      - id: kr640_r2800_2
        label: KUKA KR 640 R2800-2
      - id: kr4_r600
        label: KUKA KR 4 R600

  - id: urscript
    label: URScript
    robots:
      - id: ur5e
        label: UR5e
      - id: ur10e
        label: UR10e

  - id: pseudo_robot_3d
    label: Pseudo Robot 3D
    robots: []

  - id: debug
    label: Debug
    robots: []
```

The robot list updates automatically when the user changes the backend dropdown.

---

## Exchange files

The GUI writes to `<applicationDir>/motionbridge_studio/` before each translation.

### motionbridge_run.yaml — input for the engine

```yaml
input:
  gcode:             "C:/path/to/input.gcode"
  reference_context: "C:/path/to/context.yaml"   # optional

translation:
  frontend: "gcode_printer3d"
  backend:  "kuka_krl"
  robot:    "kr640_r2800_2"

output:
  dir:           "C:/path/to/motionbridge_studio"
  result_file:   "motionbridge_result.yaml"
  progress_file: "C:/path/to/motionbridge_studio/motionbridge_progress.txt"
```

### motionbridge_result.yaml — engine output

```yaml
status:
  success: true
  code: 0
  message: "Translation completed successfully."
files:
  src:      "C:/output/program.src"
  dat:      "C:/output/program.dat"
  errors:   "C:/output/errors.log"
  warnings: "C:/output/warnings.log"
  infos:    "C:/output/infos.log"
```

### motionbridge_progress.txt — real-time progress

UTF-8, LF. The engine **appends** lines and **flushes after each one**.

```
START
PROGRESS 20 parsing G-code
PROGRESS 50 lowering
PROGRESS 95 writing output files
PROGRESS 100 done
DONE 0
```

`PROGRESS <0–100> <stage description>`  
`DONE <code>` — `0` OK, `2` FAILED, `3` CANCELLED

GUI polls this file every 200 ms and updates the progress bar.

### cancel.flag — cancellation

GUI creates an empty `cancel.flag` in the working directory. The engine checks for it between stages — if present, stop and return `3`.

---

## Building from source

```
Qt 6.x + MinGW 64-bit, C++20
CMake >= 3.16
```

```
cmake -B build
cmake --build build
```

Output: `motionbridge_gui.dll` + `motionbridge_gui.lib`
