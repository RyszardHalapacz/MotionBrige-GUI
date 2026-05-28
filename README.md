# MotionBridge Studio — GUI

Qt6 frontend do silnika MotionBridge. Komunikuje się z silnikiem przez czysty C ABI — bez wspólnych nagłówków C++, bez zależności linkera między projektami.

---

## Podpinanie silnika (DLL / statyczna lib)

### Kontrakt — jedyne co GUI wymaga od silnika

```c
// motionbridge_api.h — jedyny nagłówek który silnik i GUI współdzielą
typedef int (*TranslationCallback)(char const* yaml_path);
//  0 = OK
//  2 = FAILED
//  3 = CANCELLED
```

Silnik musi eksportować jedną funkcję:

```c
// C ABI — extern "C", bez name manglingu
int runTranslationFromYaml(char const* yaml_path);
```

### Wariant A — statyczny (teraz, dummy)

`dummy_engine.cpp` jest wkompilowany bezpośrednio w exe. `main.cpp` rejestruje go ręcznie:

```cpp
extern "C" int runTranslationFromYaml(char const* yaml_path);

int main(int argc, char* argv[]) {
    setPTR(&runTranslationFromYaml);
    return runGUI(argc, argv);
}
```

### Wariant B — DLL (przyszłość)

Gdy silnik wyjdzie do osobnej biblioteki:

```cpp
// main.cpp — ładowanie DLL runtime
#include <windows.h>
#include "motionbridge_api.h"

int main(int argc, char* argv[]) {
    HMODULE lib = LoadLibraryW(L"motionbridge_engine.dll");
    auto fn = (TranslationCallback)GetProcAddress(lib, "runTranslationFromYaml");
    setPTR(fn);
    int rc = runGUI(argc, argv);
    FreeLibrary(lib);
    return rc;
}
```

Tylko `main.cpp` się zmienia. `mainwindow.cpp` i cała reszta GUI — bez dotykania.

---

## Format plików wymiany

GUI zapisuje do `%TEMP%/motionbridge_studio/` przed każdą translacją.

### motionbridge_run.yaml — wejście dla silnika

```yaml
input:
  gcode: "C:/path/to/input.gcode"
  reference_context: "C:/path/to/context.yaml"   # opcjonalne
translation:
  frontend: gcode_printer3d                       # gcode_cnc | gcode_printer3d
  backend: kuka_krl                               # kuka_krl | urscript | debug
  urdf: "C:/path/to/robot.urdf"                   # opcjonalne, wymagane dla kuka_krl
output:
  dir: "C:/path/to/output/"
  result_file: "motionbridge_result.yaml"
  progress_file: "motionbridge_progress.txt"
```

### motionbridge_result.yaml — wynik od silnika

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

### motionbridge_progress.txt — progres w czasie rzeczywistym

Plik tekstowy, UTF-8, LF. Silnik **dopisuje** (append), **flush po każdej linii**.

```
START
PROGRESS 5 parsing
PROGRESS 20 parsing G-code (line 1234)
PROGRESS 50 lowering
PROGRESS 65 lowering pose 312/700
PROGRESS 85 emitting KRL
PROGRESS 100 done
DONE 0
```

GUI czyta ten plik co 200 ms i aktualizuje progress bar.

### cancel.flag — przerwanie

GUI tworzy pusty plik `cancel.flag` w tym samym katalogu co `progress.txt`.  
Silnik sprawdza istnienie między etapami — jeśli plik istnieje, kończy z kodem `3`.

---

## Budowanie

```
Qt 6.x + MinGW 64-bit, C++20
CMake >= 3.16
```

Qt Creator: otwórz `CMakeLists.txt`, wybierz kit `Desktop Qt 6.x MinGW 64-bit`, build.
