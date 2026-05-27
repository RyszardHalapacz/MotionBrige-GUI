# GUI MotionBridge — zmiany

## Co podmieniasz

| Plik | Status |
|---|---|
| `CMakeLists.txt` | podmień |
| `main.cpp` | podmień |
| `mainwindow.h` | podmień |
| `mainwindow.cpp` | podmień |
| `motionbridge_api.h` | **NOWY** |
| `dummy_engine.cpp` | **NOWY** |

Pliki `dark.qss`, `icnos/`, `LICENSE`, `.gitignore`, `git/` — bez zmian.

## Co zostało naprawione

1. **Data race na fladze `m_isTranslating`** — usunięty. Nie ma flagi
   współdzielonej między wątkami ani pętli pollującej. Sygnalizacja końca
   idzie przez `emit engineStatus(...)` z `Qt::QueuedConnection` — kolejka
   zdarzeń Qt jest synchronizacją.

2. **Zamrożone okno podczas translacji** — naprawione. W oryginale
   `m_translateFn(...)` wołane było synchronicznie na wątku UI; teraz
   `mb_translate` leci na `QThread`, UI pozostaje responsywny.

3. **Dangling pointer** — `toUtf8().constData()` na tymczasowym `QByteArray`
   zwracało wskaźnik na pamięć ginącą na końcu linii. Teraz wejście jest
   kopiowane do `m_gcodeBuf` / `m_yamlBuf` i żyje przez cały czas wywołania
   silnika.

4. **Granica do silnika** — stary `TranslateFn(yaml_path)` brał tylko
   ścieżkę YAML i nie miał którędy oddać SRC/DAT/diagnostyki. Nowy kontrakt
   `motionbridge_api.h` to czyste C ABI: bufory wejścia + callback zwracający
   wynik, opaque handle, jasna własność pamięci. Granica gotowa pod przyszły
   DLL bez zmian w GUI.

5. **Cancel** — przez Spację w `eventFilter` (kolidowało z polami tekstowymi
   i przyciskami). Teraz jawny: przycisk **Cancel** + **Esc** →
   `mb_request_cancel` → `std::atomic<bool>` wewnątrz silnika.

6. **Cykl życia okna** — `MainWindow` tworzony w lambdzie bez właściciela;
   teraz trzymany przez wskaźnik w `main()`, sprzątanie deterministyczne.

7. **Wstrzyknięcie silnika** — okno nie tworzy silnika; dostaje gotowy
   `mb_engine*` przez `setEngine()` z `main.cpp`. Łatwiej będzie zamienić
   dummy na prawdziwy DLL.

8. **Status "Ready" przy starcie** — zmieniony na "Idle" (przed pierwszą
   translacją system nie jest "Ready do działania", nie ma wejścia).

## Jak zachowuje się dummy engine

- Krokowy progres co ~200 ms, 10 kroków = 100%.
- Reaguje na Cancel (Esc / przycisk) → `MB_ABORTED`.
- Puste wejście G-code → `MB_ERROR` + diagnostyka.
- Pełny przebieg → `MB_DONE_WITH_DIAG` + dummy SRC/DAT/warning.

## Gdy będziesz wydzielał silnik do DLL-a (PÓŹNIEJ — nie teraz)

1. `dummy_engine.cpp` → osobny projekt CMake jako `add_library(... SHARED ...)`.
2. W `motionbridge_api.h` dorzucasz makra eksportu (komentarz w pliku
   tłumaczy dokładnie jak).
3. W projekcie GUI usuwasz `dummy_engine.cpp` ze źródeł i linkujesz przez
   `target_link_libraries(GUI PRIVATE motionbridge)`.
4. `main.cpp` i `mainwindow.cpp` — **bez zmian**.

## Co warto zauważyć (niezależne od kodu)

W panelu TARGET wpisany jest `Robot KR 120 R2700`, a cała roadmapa backendu
i URDF w silniku celują w **KR 640 R2800-2**. To zahardkodowane w
`mainwindow.cpp` w `makeTargetInfo()` i `updateTargetInfo()`. Jeśli to
placeholder — OK; jeśli pokazujesz to klientowi, sugeruje gotowość pod
model, którego nie ma w kodzie silnika.
