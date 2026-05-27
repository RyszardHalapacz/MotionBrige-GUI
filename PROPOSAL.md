# MotionBridge Studio — Proposal poprawek kodu

## 1. Usuń martwy plik `mainwindow.ui`

**Problem:** `mainwindow.ui` jest kompilowany przez `AUTOUIC`, ale nigdy nieużywany.
`mainwindow.h` nie deklaruje `Ui::MainWindow *ui`, więc wygenerowany `ui_mainwindow.h`
nie jest nigdzie includowany. Plik wprowadza zamieszanie i zbędny krok kompilacji.

**Rozwiązanie:** Usunąć `mainwindow.ui` i wyrzucić go z `CMakeLists.txt`.

```cmake
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    # mainwindow.ui  ← usunąć
)
```

---

## 2. Podłącz sygnały i sloty do przycisków

**Problem:** Wszystkie przyciski (Open .g-code, Open YAML, Open .dat, Open .src, Translate)
są tworzone, ale nie mają połączonych slotów — UI jest czysto statyczne.

**Rozwiązanie:** Rozbudować `mainwindow.h` o pola składowe i sloty prywatne.
Edytory muszą być polami klasy (nie lokalnymi zmiennymi), żeby sloty miały do nich dostęp.

```cpp
// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

class QPlainTextEdit;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onOpenGcode();
    void onOpenYaml();
    void onOpenDat();
    void onOpenSrc();
    void onTranslate();

private:
    QPlainTextEdit *m_gcodeEditor  = nullptr;
    QPlainTextEdit *m_yamlEditor   = nullptr;
    QPlainTextEdit *m_srcEditor    = nullptr;
    QPlainTextEdit *m_datEditor    = nullptr;

    void loadFile(QPlainTextEdit *target, const QString &filter);
};

#endif
```

Każdy `Open *` otwiera `QFileDialog::getOpenFileName()` i ładuje zawartość do odpowiedniego edytora:

```cpp
void MainWindow::loadFile(QPlainTextEdit *target, const QString &filter)
{
    const QString path = QFileDialog::getOpenFileName(this, "Open file", {}, filter);
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text)) return;

    target->setPlainText(QString::fromUtf8(f.readAll()));
    statusBar()->showMessage("Loaded: " + path);
}

void MainWindow::onOpenGcode() { loadFile(m_gcodeEditor, "G-code (*.gcode *.nc *.g);;All files (*)"); }
void MainWindow::onOpenYaml()  { loadFile(m_yamlEditor,  "YAML (*.yaml *.yml);;All files (*)"); }
void MainWindow::onOpenDat()   { loadFile(m_datEditor,   "DAT files (*.dat);;All files (*)"); }
void MainWindow::onOpenSrc()   { loadFile(m_srcEditor,   "SRC files (*.src);;All files (*)"); }
```

---

## 3. Zamień `QLabel` na wbudowany `QStatusBar`

**Problem:** Status bar to `QLabel` dodany ręcznie do layoutu z twardym tekstem.
`QMainWindow` ma wbudowany `statusBar()` — nieużywanie go to obejście własnego frameworka.

**Rozwiązanie:**

```cpp
// Zamiast:
auto* statusBar = new QLabel("Ready ...", this);
mainLayout->addWidget(statusBar);

// Użyj:
statusBar()->showMessage("Ready  |  Pipeline: Ready  |  Diagnostics: 0 errors, 0 warnings");
```

**Zyski:**
- poprawne zachowanie przy resize
- wsparcie dla tymczasowych komunikatów: `statusBar()->showMessage(msg, 3000)`
- brak potrzeby ręcznego stylowania i zarządzania widgetem

---

## 4. Rozbij konstruktor na prywatne metody pomocnicze

**Problem:** Konstruktor `MainWindow` ma ~150 linii kodu UI — trudno go czytać i utrzymywać.

**Rozwiązanie:** Wydzielić logikę budowania sekcji do prywatnych metod:

```cpp
// Dodać do mainwindow.h (sekcja private):
QWidget* buildTopBar();
QWidget* buildSideBar();
QWidget* buildWorkArea();
QWidget* buildDiagnosticsPanel();
void     applyStyleSheet();
```

Konstruktor staje się wtedy czytelny:

```cpp
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    resize(1600, 950);
    setWindowTitle("MotionBridge Studio");

    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->addWidget(buildTopBar());
    layout->addWidget(buildWorkArea());
    layout->addWidget(buildDiagnosticsPanel());
    setCentralWidget(central);

    applyStyleSheet();
    statusBar()->showMessage("Ready");
}
```

---

## 5. Wydziel stylesheet do pliku `.qss`

**Problem:** Blok `setStyleSheet(R"(...)")` ma ~60 linii i zaśmieca logikę UI w `mainwindow.cpp`.
Każda zmiana wyglądu wymaga rekompilacji.

**Rozwiązanie:** Przenieść style do `resources/dark.qss` i załadować przez Qt Resource System.

```cmake
# CMakeLists.txt
qt_add_resources(GUI "styles"
    PREFIX "/styles"
    FILES resources/dark.qss
)
```

```cpp
// mainwindow.cpp
void MainWindow::applyStyleSheet()
{
    QFile f(":/styles/dark.qss");
    if (f.open(QFile::ReadOnly))
        setStyleSheet(QString::fromUtf8(f.readAll()));
}
```

**Zyski:** zmiana motywu bez rekompilacji (wystarczy przeładowanie zasobu w trybie dev),
czystszy `mainwindow.cpp`, możliwość łatwego dodania drugiego motywu.

---

## 6. Oddziel logikę translacji od warstwy UI

**Problem:** Docelowo `onTranslate()` będzie operował bezpośrednio na edytorach — miesza
logikę biznesową z prezentacją i utrudnia testowanie.

**Rozwiązanie:** Wydzielić klasę `Translator` z czystym interfejsem:

```cpp
// translator.h
struct TranslationResult {
    QString src;
    QString dat;
    QStringList diagnostics;
    bool success = false;
};

class Translator {
public:
    TranslationResult translate(const QString &gcode, const QString &yamlConfig) const;
};
```

`MainWindow::onTranslate()` dostarcza tekst z edytorów → woła `Translator::translate()` →
wyświetla wyniki. Warstwa UI nic nie wie o logice translacji.

---

## Priorytetyzacja

| # | Zmiana                              | Trudność | Wartość |
|---|-------------------------------------|----------|---------|
| 1 | Usuń martwy `.ui`                   | niska    | średnia |
| 2 | Sloty + pola składowe               | średnia  | wysoka  |
| 3 | Prawdziwy `statusBar()`             | niska    | średnia |
| 4 | Podział konstruktora na metody      | średnia  | wysoka  |
| 5 | Stylesheet do `.qss`                | niska    | średnia |
| 6 | Oddzielenie logiki translacji       | wysoka   | wysoka  |
