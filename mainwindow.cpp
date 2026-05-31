#include "mainwindow.h"
#include "pipeline_options.h"

#include <QApplication>
#include <QButtonGroup>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QComboBox>
#include <QMetaObject>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

// ============================================================================
//  Globalny wskaźnik do silnika — ustawiany przez setPTR przed runGUI
// ============================================================================

static TranslationCallback g_engineFn   = nullptr;
static QString             g_configPath;

extern "C" void        setPTR(TranslationCallback cb)  { g_engineFn = cb; }
extern "C" void        setConfigPath(const char* path) { g_configPath = QString::fromUtf8(path); }
extern "C" const char* getVersion()                    { return "0.1.0"; }

// ============================================================================
//  Splash screen
// ============================================================================

static QWidget* createSplashScreen()
{
    auto* splash = new QWidget(nullptr);
    splash->setWindowTitle("MotionBridge Studio");
    splash->setObjectName("splashScreen");
    splash->setAttribute(Qt::WA_DeleteOnClose);
    splash->setStyleSheet(R"(
        QWidget#splashScreen {
            background-color: #0f141b;
        }
        QLabel#splashLogo {
            background-color: #0f141b;
            border: none;
        }
    )");

    const QPixmap pixmap(QApplication::applicationDirPath() + "/icnos/content.png");

    auto* layout = new QVBoxLayout(splash);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* logo = new QLabel(splash);
    logo->setObjectName("splashLogo");
    logo->setAlignment(Qt::AlignCenter);

    if (!pixmap.isNull()) {
        const QSize sz = pixmap.size().scaled(900, 560, Qt::KeepAspectRatio);
        splash->resize(sz);
        logo->setPixmap(pixmap.scaled(sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        splash->resize(900, 560);
        logo->setText("MotionBridge Studio");
        logo->setStyleSheet("color: #d7dde7; font-size: 28px; font-weight: 600;");
    }

    layout->addWidget(logo);

    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect geometry = screen->availableGeometry();
        splash->move(geometry.center() - splash->rect().center());
    }
    return splash;
}

// ============================================================================
//  runGUI — entry point dla Qt event loop; woła się z main()
// ============================================================================

static MainWindow* g_mainWindow = nullptr;

extern "C" int runGUI(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto* splash = createSplashScreen();
    splash->show();

    QTimer::singleShot(5000, [splash]() {
        g_mainWindow = new MainWindow;
        g_mainWindow->setEngineCallback(g_engineFn);
        splash->close();
        g_mainWindow->show();
    });

    const int rc = QApplication::exec();
    delete g_mainWindow;
    g_mainWindow = nullptr;
    return rc;
}

// ============================================================================
//  Pomocnicze factory widgetów
// ============================================================================

static QPixmap loadLogoTransparent(const QString &path, int threshold = 40)
{
    QImage img(path);
    if (img.isNull()) return {};
    img = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qRed(line[x]) < threshold &&
                qGreen(line[x]) < threshold &&
                qBlue(line[x]) < threshold)
                line[x] = qRgba(0, 0, 0, 0);
        }
    }
    return QPixmap::fromImage(img);
}

static QFrame* makePanel(const QString &title, QWidget *content, QWidget *parent)
{
    auto* panel = new QFrame(parent);
    panel->setObjectName("editorPanel");
    panel->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title, panel);
    titleLabel->setObjectName("panelTitle");

    layout->addWidget(titleLabel);
    layout->addWidget(content);
    return panel;
}

static QPushButton* makeFileCard(const QString &badge, const QString &name,
                                  const QString &description, const QString &path,
                                  QWidget *parent)
{
    auto* card = new QPushButton(parent);
    card->setObjectName("fileCard");
    card->setCheckable(true);
    card->setCursor(Qt::PointingHandCursor);
    card->setText(QStringLiteral("%1    %2\n     %3\n     %4")
                      .arg(badge, name, description, path));
    card->setMinimumHeight(70);
    return card;
}

static QLabel* makeSectionTitle(const QString &text, QWidget *parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("sectionTitle");
    return label;
}


// ============================================================================
//  MainWindow
// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_progressTimer(new QTimer(this))
{
    resize(1600, 950);
    setWindowTitle("MotionBridge Studio");

    m_progressTimer->setInterval(200);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::onProgressTick);

    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 0);
    layout->setSpacing(8);

    // buildDiagnostics() tworzy m_diagEditor — musi powstać PRZED buildWorkspace(),
    // bo buildSideBar() może wywołać appendDiagWarning() (np. gdy load() YAML padnie).
    QWidget* diagnostics = buildDiagnostics();
    layout->addWidget(buildTopBar());
    layout->addWidget(buildWorkspace());
    layout->addWidget(diagnostics);

    setCentralWidget(central);

    applyTheme();
    activatePanel(FocusPanel::Gcode);
    statusBar()->showMessage("Ready  |  Pipeline: Ready  |  Diagnostics: 0 errors, 0 warnings");
}

MainWindow::~MainWindow()
{
    m_progressTimer->stop();
}

void MainWindow::setEngineCallback(TranslationCallback cb)
{
    m_engineFn = cb;
}

// ============================================================================
//  buildTopBar
// ============================================================================

QWidget* MainWindow::buildTopBar()
{
    auto* bar = new QFrame(this);
    bar->setObjectName("topBar");

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(10);

    auto* logo = new QLabel(bar);
    QPixmap logoPix = loadLogoTransparent(QApplication::applicationDirPath() + "/icnos/logo.png");
    if (!logoPix.isNull())
        logo->setPixmap(logoPix.scaledToHeight(48, Qt::SmoothTransformation));
    else
        logo->setText("MotionBridge");

    m_openGcodeBtn = new QPushButton("Open .g-code", bar);
    m_openYamlBtn  = new QPushButton("Open YAML",    bar);
    m_saveDatBtn   = new QPushButton("Save .dat",    bar);
    m_saveSrcBtn   = new QPushButton("Save .src",    bar);
    m_translateBtn = new QPushButton("Translate",    bar);
    m_cancelBtn    = new QPushButton("Cancel",       bar);

    m_translateBtn->setObjectName("translateBtn");
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setVisible(false);

    m_progressBar = new QProgressBar(bar);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedWidth(200);
    m_progressBar->setFixedHeight(18);
    m_progressBar->setVisible(false);
    m_progressBar->setObjectName("translationProgress");

    connect(m_openGcodeBtn, &QPushButton::clicked, this, &MainWindow::onOpenGcode);
    connect(m_openYamlBtn,  &QPushButton::clicked, this, &MainWindow::onOpenYaml);
    connect(m_saveDatBtn,   &QPushButton::clicked, this, &MainWindow::onSaveDat);
    connect(m_saveSrcBtn,   &QPushButton::clicked, this, &MainWindow::onSaveSrc);
    connect(m_translateBtn, &QPushButton::clicked, this, &MainWindow::onTranslate);
    connect(m_cancelBtn,    &QPushButton::clicked, this, &MainWindow::onCancel);

    layout->addWidget(logo);
    layout->addSpacing(16);
    layout->addWidget(m_openGcodeBtn);
    layout->addWidget(m_openYamlBtn);
    layout->addStretch();
    layout->addWidget(m_progressBar);
    layout->addWidget(m_translateBtn);
    layout->addWidget(m_cancelBtn);
    layout->addStretch();
    layout->addWidget(m_saveDatBtn);
    layout->addWidget(m_saveSrcBtn);

    return bar;
}

// ============================================================================
//  buildSideBar
// ============================================================================

QWidget* MainWindow::buildSideBar()
{
    auto* sidebar = new QFrame(this);
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(220);
    sidebar->setMaximumWidth(280);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* group = new QButtonGroup(sidebar);
    group->setExclusive(true);

    m_gcodeCard = makeFileCard("G", "model.gcode",  "G-code source",          "C:/project/input/model.gcode", sidebar);
    m_yamlCard  = makeFileCard("Y", "config.yaml",  "Backend configuration",  "C:/project/config.yaml",       sidebar);
    m_srcCard   = makeFileCard("S", "program.src",  "KRL motion program",     "C:/project/program.src",       sidebar);
    m_datCard   = makeFileCard("D", "tool.dat",     "KRL data file",          "C:/project/tool.dat",          sidebar);

    group->addButton(m_gcodeCard);
    group->addButton(m_yamlCard);
    group->addButton(m_srcCard);
    group->addButton(m_datCard);

    connect(m_gcodeCard, &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Gcode); });
    connect(m_yamlCard,  &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Yaml);  });
    connect(m_srcCard,   &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Src);   });
    connect(m_datCard,   &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Dat);   });

    auto* inputBox = new QFrame(sidebar);
    inputBox->setObjectName("sidebarBox");
    auto* inputLayout = new QVBoxLayout(inputBox);
    inputLayout->setContentsMargins(10, 10, 10, 10);
    inputLayout->setSpacing(8);
    inputLayout->addWidget(makeSectionTitle("INPUT PROJECT", inputBox));
    inputLayout->addWidget(m_gcodeCard);
    inputLayout->addWidget(m_yamlCard);

    auto* outputBox = new QFrame(sidebar);
    outputBox->setObjectName("sidebarBox");
    auto* outputLayout = new QVBoxLayout(outputBox);
    outputLayout->setContentsMargins(10, 10, 10, 10);
    outputLayout->setSpacing(8);
    outputLayout->addWidget(makeSectionTitle("OUTPUT PROJECT", outputBox));
    outputLayout->addWidget(m_srcCard);
    outputLayout->addWidget(m_datCard);

    /* Załaduj opcje pipeline'u — ścieżka od silnika lub fallback na zasób Qt */
    const QString yamlPath = g_configPath.isEmpty() ? ":/config/pipeline_options.yaml" : g_configPath;
    const auto loaded = PipelineOptionsLoader::load(yamlPath);
    if (!loaded)
        appendDiagWarning("pipeline_options.yaml not found or invalid — using built-in defaults");
    m_pipelineOpts = loaded.value_or(PipelineOptionsLoader::fallback());

    auto* watermark = new QLabel("M", sidebar);
    watermark->setObjectName("watermark");
    watermark->setAlignment(Qt::AlignCenter);

    layout->addWidget(inputBox);
    layout->addWidget(outputBox);
    layout->addWidget(buildConfigSection(m_pipelineOpts));
    layout->addStretch();
    layout->addWidget(watermark);

    return sidebar;
}

QWidget* MainWindow::buildConfigSection(const PipelineOptions& opts)
{
    auto* box = new QFrame(this);
    box->setObjectName("sidebarBox");

    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(4);

    layout->addWidget(makeSectionTitle("CONFIGURATION", box));

    auto addLabel = [&](const QString& text) {
        auto* lbl = new QLabel(text, box);
        lbl->setObjectName("configLabel");
        layout->addSpacing(6);
        layout->addWidget(lbl);
    };

    /* Frontend */
    addLabel("Frontend");
    m_frontendCombo = new QComboBox(box);
    m_frontendCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (const auto& opt : opts.frontends)
        m_frontendCombo->addItem(opt.label, opt.id);
    layout->addWidget(m_frontendCombo);

    /* Backend */
    addLabel("Backend");
    m_backendCombo = new QComboBox(box);
    m_backendCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (const auto& backend : opts.backends)
        m_backendCombo->addItem(backend.label, backend.id);
    layout->addWidget(m_backendCombo);

    /* Robot — dynamiczny, zależny od wybranego backendu */
    addLabel("Robot");
    m_robotCombo = new QComboBox(box);
    m_robotCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (!opts.backends.isEmpty())
        for (const auto& robot : opts.backends[0].robots)
            m_robotCombo->addItem(robot.label, robot.id);
    layout->addWidget(m_robotCombo);

    connect(m_backendCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onBackendChanged);

    return box;
}

void MainWindow::onBackendChanged(int index)
{
    if (!m_robotCombo) return;
    m_robotCombo->clear();

    if (index < 0 || index >= m_pipelineOpts.backends.size()) return;

    const auto& robots = m_pipelineOpts.backends[index].robots;
    for (const auto& robot : robots)
        m_robotCombo->addItem(robot.label, robot.id);

    m_robotCombo->setEnabled(!robots.isEmpty());
}

// ============================================================================
//  buildWorkspace / buildDiagnostics
// ============================================================================

QWidget* MainWindow::buildWorkspace()
{
    m_gcodeEditor = new QPlainTextEdit(this);
    m_yamlEditor  = new QPlainTextEdit(this);
    m_srcEditor   = new QPlainTextEdit(this);
    m_datEditor   = new QPlainTextEdit(this);

    m_gcodeEditor->setPlaceholderText("G-code source...");
    m_yamlEditor->setPlaceholderText("YAML configuration...");
    m_srcEditor->setPlaceholderText("Generated .SRC...");
    m_datEditor->setPlaceholderText("Generated .DAT...");

    m_gcodeEditor->setReadOnly(true);
    m_srcEditor->setReadOnly(true);
    m_datEditor->setReadOnly(true);

    m_gcodePanel = makePanel("G-CODE  (model.gcode)",     m_gcodeEditor, this);
    m_yamlPanel  = makePanel("CONFIG  (config.yaml)",     m_yamlEditor,  this);
    m_srcPanel   = makePanel("OUTPUT SRC  (program.src)", m_srcEditor,   this);
    m_datPanel   = makePanel("OUTPUT DAT  (tool.dat)",    m_datEditor,   this);

    m_inputSplitter = new QSplitter(Qt::Horizontal, this);
    m_inputSplitter->addWidget(m_gcodePanel);
    m_inputSplitter->addWidget(m_yamlPanel);

    m_outputSplitter = new QSplitter(Qt::Vertical, this);
    m_outputSplitter->addWidget(m_srcPanel);
    m_outputSplitter->addWidget(m_datPanel);

    m_workSplitter = new QSplitter(Qt::Horizontal, this);
    m_workSplitter->addWidget(buildSideBar());
    m_workSplitter->addWidget(m_inputSplitter);
    m_workSplitter->addWidget(m_outputSplitter);
    m_workSplitter->setStretchFactor(0, 0);
    m_workSplitter->setStretchFactor(1, 3);
    m_workSplitter->setStretchFactor(2, 3);

    return m_workSplitter;
}

QWidget* MainWindow::buildDiagnostics()
{
    m_diagEditor = new QPlainTextEdit(this);
    m_diagEditor->setPlaceholderText("Diagnostics / Warnings / Errors...");
    m_diagEditor->setMaximumHeight(140);
    m_diagEditor->setReadOnly(true);

    return makePanel("DIAGNOSTICS", m_diagEditor, this);
}

// ============================================================================
//  Translacja — zapis YAML + wątek roboczy
// ============================================================================

void MainWindow::onTranslate()
{
    if (m_busy) return;
    if (!m_engineFn) {
        statusBar()->showMessage("Error: no engine registered", 4000);
        return;
    }
    if (m_gcodeEditor->toPlainText().trimmed().isEmpty()) {
        statusBar()->showMessage("Error: load a G-code file first", 4000);
        return;
    }

    /* Katalog roboczy w temp */
    const QString workDir = QApplication::applicationDirPath() + "/motionbridge_studio";
    QDir().mkpath(workDir);

    /* Ścieżki plików */
    const QString gcodePath    = workDir + "/input.gcode";
    const QString runYamlPath  = workDir + "/motionbridge_run.yaml";
    m_progressFilePath         = workDir + "/motionbridge_progress.txt";
    m_resultFilePath           = workDir + "/motionbridge_result.yaml";
    m_cancelFlagPath           = workDir + "/cancel.flag";

    /* Zapisz G-code do pliku */
    {
        QFile f(gcodePath);
        if (f.open(QFile::WriteOnly | QFile::Text))
            f.write(m_gcodeEditor->toPlainText().toUtf8());
    }

    /* Usuń stare pliki progresu i cancel */
    QFile::remove(m_progressFilePath);
    QFile::remove(m_cancelFlagPath);
    m_progressFilePos = 0;

    /* Zapisz motionbridge_run.yaml */
    {
        QFile f(runYamlPath);
        if (f.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream s(&f);
            s << "input:\n"
              << "  gcode:             \"" << gcodePath << "\"\n";
            if (!m_yamlEditor->toPlainText().trimmed().isEmpty()) {
                const QString ctxPath = workDir + "/context.yaml";
                QFile ctx(ctxPath);
                if (ctx.open(QFile::WriteOnly | QFile::Text))
                    ctx.write(m_yamlEditor->toPlainText().toUtf8());
                s << "  reference_context: \"" << ctxPath << "\"\n";
            }
            s << "translation:\n"
              << "  frontend: \"" << m_frontendCombo->currentData().toString() << "\"\n";

            // Gdy robot combo aktywny i ma wybór → użyj pełnego ID robota z robot combo
            // (np. "kuka_krl_kr640"). Gdy robot combo pusty/nieaktywny → użyj ID backendu
            // (np. "urscript", "debug").
            const QString backendId = (m_robotCombo->isEnabled() && m_robotCombo->count() > 0)
                ? m_robotCombo->currentData().toString()
                : m_backendCombo->currentData().toString();
            s << "  backend:  \"" << backendId << "\"\n"
              << "output:\n"
              << "  dir:           \"" << workDir << "\"\n"
              << "  result_file:   \"motionbridge_result.yaml\"\n"
              << "  progress_file: \"" << m_progressFilePath << "\"\n";
        }
    }

    setBusyUi(true);
    m_progressBar->setValue(0);
    m_progressTimer->start();

    /* Wątek roboczy — woła callback silnika */
    auto* thread = QThread::create([this, runYamlPath]() {
        QByteArray path = runYamlPath.toUtf8();
        int rc = m_engineFn(path.constData());
        QMetaObject::invokeMethod(this, "onTranslateDone",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, rc));
    });
    thread->setParent(this);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

// ============================================================================
//  Cancel — tworzy plik cancel.flag (silnik sprawdza między krokami)
// ============================================================================

void MainWindow::onCancel()
{
    if (!m_busy) return;
    QFile f(m_cancelFlagPath);
    (void)f.open(QFile::WriteOnly);
    statusBar()->showMessage("Cancelling...");
}

// ============================================================================
//  Polling pliku progresu co 200 ms
// ============================================================================

void MainWindow::onProgressTick()
{
    QFile f(m_progressFilePath);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        return;

    f.seek(m_progressFilePos);

    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        m_progressFilePos = f.pos();

        if (line == "START") {
            statusBar()->showMessage("Translating...");

        } else if (line.startsWith("PROGRESS ")) {
            /* "PROGRESS 45 lowering poses" */
            const QString rest  = line.mid(9);
            const int     space = rest.indexOf(' ');
            const int     pct   = rest.left(space < 0 ? rest.size() : space).toInt();
            const QString stage = space < 0 ? QString{} : rest.mid(space + 1);
            m_progressBar->setValue(pct);
            statusBar()->showMessage(
                QString("Translating: %1%  —  %2").arg(pct).arg(stage));
        }
        /* DONE obsługuje onTranslateDone */
    }
}

// ============================================================================
//  Koniec translacji
// ============================================================================

void MainWindow::onTranslateDone(int rc)
{
    m_progressTimer->stop();
    /* Ostatni odczyt progresu zanim posprzątamy */
    onProgressTick();
    setBusyUi(false);

    if (rc != 0 && rc != 3) {
        m_progressBar->setValue(0);
        statusBar()->showMessage(QString("Translation FAILED (code %1)").arg(rc), 6000);
        return;
    }
    if (rc == 3) {
        m_progressBar->setValue(0);
        statusBar()->showMessage("Translation cancelled", 4000);
        return;
    }

    m_progressBar->setValue(100);
    parseAndLoadResults();
    activatePanel(FocusPanel::Src);
    statusBar()->showMessage("Translation completed", 5000);
}

// ============================================================================
//  Diagnostics helper
// ============================================================================

void MainWindow::appendDiagWarning(const QString& msg)
{
    if (!m_diagEditor) return;
    m_diagEditor->appendPlainText("[WARN] " + msg);
    statusBar()->showMessage(msg, 5000);
}

// ============================================================================
//  Parsowanie motionbridge_result.yaml i ładowanie plików do edytorów
// ============================================================================

QString MainWindow::loadTextFile(const QString &path)
{
    if (path.isEmpty()) return {};
    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

void MainWindow::parseAndLoadResults()
{
    const QString yaml = loadTextFile(m_resultFilePath);
    if (yaml.isEmpty()) return;

    auto extract = [&](const QString& key) -> QString {
        QRegularExpression re(key + ":\\s*\"([^\"]+)\"");
        const auto m = re.match(yaml);
        return m.hasMatch() ? m.captured(1) : QString{};
    };

    const QString srcContent  = loadTextFile(extract("src"));
    const QString datContent  = loadTextFile(extract("dat"));

    QString diagContent;
    for (const QString& key : {"errors", "warnings", "infos"}) {
        const QString text = loadTextFile(extract(key));
        if (!text.trimmed().isEmpty())
            diagContent += text;
    }

    if (!srcContent.isEmpty())  m_srcEditor->setPlainText(srcContent);
    if (!datContent.isEmpty())  m_datEditor->setPlainText(datContent);
    if (!diagContent.isEmpty()) m_diagEditor->appendPlainText(diagContent);
}

// ============================================================================
//  setBusyUi
// ============================================================================

void MainWindow::setBusyUi(bool busy)
{
    m_busy = busy;
    m_translateBtn->setEnabled(!busy);
    m_translateBtn->setVisible(!busy);
    m_cancelBtn->setVisible(busy);
    m_progressBar->setVisible(busy);
    m_openGcodeBtn->setEnabled(!busy);
    m_openYamlBtn->setEnabled(!busy);
}

// ============================================================================
//  Panel focus / theme / keyboard
// ============================================================================

void MainWindow::activatePanel(FocusPanel panel)
{
    if (!m_workSplitter || !m_inputSplitter || !m_outputSplitter) return;

    switch (panel) {
    case FocusPanel::Gcode:
        if (m_gcodeCard) m_gcodeCard->setChecked(true);
        m_workSplitter->setSizes({240, 1050, 420});
        m_inputSplitter->setSizes({820, 280});
        m_outputSplitter->setSizes({1, 1});
        setActivePanelFrame(m_gcodePanel);
        statusBar()->showMessage("Focus: model.gcode", 2500);
        break;
    case FocusPanel::Yaml:
        if (m_yamlCard) m_yamlCard->setChecked(true);
        m_workSplitter->setSizes({240, 1050, 420});
        m_inputSplitter->setSizes({280, 820});
        m_outputSplitter->setSizes({1, 1});
        setActivePanelFrame(m_yamlPanel);
        statusBar()->showMessage("Focus: config.yaml", 2500);
        break;
    case FocusPanel::Src:
        if (m_srcCard) m_srcCard->setChecked(true);
        m_workSplitter->setSizes({240, 520, 1050});
        m_inputSplitter->setSizes({1, 1});
        m_outputSplitter->setSizes({760, 260});
        setActivePanelFrame(m_srcPanel);
        statusBar()->showMessage("Focus: program.src", 2500);
        break;
    case FocusPanel::Dat:
        if (m_datCard) m_datCard->setChecked(true);
        m_workSplitter->setSizes({240, 520, 1050});
        m_inputSplitter->setSizes({1, 1});
        m_outputSplitter->setSizes({260, 760});
        setActivePanelFrame(m_datPanel);
        statusBar()->showMessage("Focus: tool.dat", 2500);
        break;
    }
}

void MainWindow::setActivePanelFrame(QFrame *activePanel)
{
    for (auto* panel : {m_gcodePanel, m_yamlPanel, m_srcPanel, m_datPanel}) {
        if (!panel) continue;
        panel->setProperty("active", panel == activePanel);
        refreshWidgetStyle(panel);
    }
}

void MainWindow::refreshWidgetStyle(QWidget *widget)
{
    if (!widget) return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void MainWindow::applyTheme()
{
    QFile f(QApplication::applicationDirPath() + "/dark.qss");
    if (f.open(QFile::ReadOnly | QFile::Text))
        setStyleSheet(QString::fromUtf8(f.readAll()));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_1: activatePanel(FocusPanel::Gcode); break;
    case Qt::Key_2: activatePanel(FocusPanel::Yaml);  break;
    case Qt::Key_3: activatePanel(FocusPanel::Src);   break;
    case Qt::Key_4: activatePanel(FocusPanel::Dat);   break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (event->modifiers() & Qt::ControlModifier) onTranslate();
        break;
    case Qt::Key_Escape:
        if (m_busy) onCancel();
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

// ============================================================================
//  File I/O
// ============================================================================

void MainWindow::loadFile(QPlainTextEdit *target, const QString &filter)
{
    const QString path = QFileDialog::getOpenFileName(this, "Open file", {}, filter);
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        appendDiagWarning("Cannot open file: " + path);
        return;
    }
    target->setPlainText(QString::fromUtf8(f.readAll()));
    statusBar()->showMessage("Loaded: " + path, 4000);
}

void MainWindow::saveFile(QPlainTextEdit *source, const QString &defaultName, const QString &filter)
{
    const QString path = QFileDialog::getSaveFileName(this, "Save file", defaultName, filter);
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        statusBar()->showMessage("Error: could not save " + path, 4000);
        return;
    }
    f.write(source->toPlainText().toUtf8());
    statusBar()->showMessage("Saved: " + path, 4000);
}

void MainWindow::onOpenGcode()
{
    activatePanel(FocusPanel::Gcode);
    loadFile(m_gcodeEditor, "G-code (*.gcode *.nc *.g);;All files (*)");
}

void MainWindow::onOpenYaml()
{
    activatePanel(FocusPanel::Yaml);
    loadFile(m_yamlEditor, "YAML (*.yaml *.yml);;All files (*)");
}

void MainWindow::onSaveDat()
{
    activatePanel(FocusPanel::Dat);
    saveFile(m_datEditor, "output.dat", "DAT files (*.dat);;All files (*)");
}

void MainWindow::onSaveSrc()
{
    activatePanel(FocusPanel::Src);
    saveFile(m_srcEditor, "output.src", "SRC files (*.src);;All files (*)");
}
