#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QPixmap>
#include <QStatusBar>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

static QPixmap loadLogoTransparent(const QString &path, int threshold = 40)
{
    QImage img(path);
    if (img.isNull())
        return {};
    img = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qRed(line[x]) < threshold &&
                qGreen(line[x]) < threshold &&
                qBlue(line[x]) < threshold)
                line[x] = qRgba(0, 0, 0, 0);
        }
    }
    return QPixmap::fromImage(img);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1600, 950);
    setWindowTitle("MotionBridge Studio");

    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->addWidget(buildTopBar());
    layout->addWidget(buildWorkspace());
    layout->addWidget(buildDiagnostics());
    setCentralWidget(central);

    applyTheme();
    statusBar()->showMessage("Ready  |  Pipeline: Ready  |  Diagnostics: 0 errors, 0 warnings");
}

QWidget* MainWindow::buildTopBar()
{
    auto* bar    = new QFrame(this);
    auto* layout = new QHBoxLayout(bar);

    auto* logo = new QLabel(bar);
    QPixmap logoPix = loadLogoTransparent(QApplication::applicationDirPath() + "/icnos/logo.png");
    if (!logoPix.isNull())
        logo->setPixmap(logoPix.scaledToHeight(48, Qt::SmoothTransformation));

    auto* openGcodeBtn = new QPushButton("Open .g-code", bar);
    auto* openYamlBtn  = new QPushButton("Open YAML",    bar);
    auto* openDatBtn   = new QPushButton("Save .dat",    bar);
    auto* openSrcBtn   = new QPushButton("Save .src",    bar);
    auto* translateBtn = new QPushButton("Translate",    bar);

    connect(openGcodeBtn, &QPushButton::clicked, this, &MainWindow::onOpenGcode);
    connect(openYamlBtn,  &QPushButton::clicked, this, &MainWindow::onOpenYaml);
    connect(openDatBtn,   &QPushButton::clicked, this, &MainWindow::onSaveDat);
    connect(openSrcBtn,   &QPushButton::clicked, this, &MainWindow::onSaveSrc);
    connect(translateBtn, &QPushButton::clicked, this, &MainWindow::onTranslate);

    translateBtn->setObjectName("translateBtn");

    layout->addWidget(logo);
    layout->addSpacing(16);
    layout->addWidget(openGcodeBtn);
    layout->addWidget(openYamlBtn);
    layout->addStretch();
    layout->addWidget(translateBtn);
    layout->addStretch();
    layout->addWidget(openDatBtn);
    layout->addWidget(openSrcBtn);

    return bar;
}

QWidget* MainWindow::buildSideBar()
{
    auto* sidebar = new QFrame(this);
    sidebar->setFixedWidth(220);

    auto* layout = new QVBoxLayout(sidebar);
    layout->addWidget(new QLabel("INPUT FILES", sidebar));
    layout->addWidget(new QLabel("model.gcode\nC:/project/input/model.gcode", sidebar));
    layout->addWidget(new QLabel("config.yaml\nC:/project/config.yaml", sidebar));
    layout->addWidget(new QLabel("tool.dat\nC:/project/tool.dat", sidebar));
    layout->addWidget(new QLabel("program.src\nC:/project/program.src", sidebar));
    layout->addSpacing(20);
    layout->addWidget(new QLabel("PROJECT INFO", sidebar));
    layout->addWidget(new QLabel("Backend\nKUKA (KRL)\n\nRobot\nKR 120 R2700\n\nProfile\ndefault\n\nStatus\nReady", sidebar));
    layout->addStretch();

    return sidebar;
}

static QFrame* makePanel(const QString &title, QWidget *content, QWidget *parent)
{
    auto* panel  = new QFrame(parent);
    panel->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(panel);
    layout->addWidget(new QLabel(title, panel));
    layout->addWidget(content);

    return panel;
}

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

    auto* gcodePanel = makePanel("G-CODE  (model.gcode)",     m_gcodeEditor, this);
    auto* yamlPanel  = makePanel("CONFIG  (config.yaml)",     m_yamlEditor,  this);
    auto* srcPanel   = makePanel("OUTPUT SRC  (program.src)", m_srcEditor,   this);
    auto* datPanel   = makePanel("OUTPUT DAT  (tool.dat)",    m_datEditor,   this);

    auto* inputSplitter = new QSplitter(Qt::Horizontal, this);
    inputSplitter->addWidget(gcodePanel);
    inputSplitter->addWidget(yamlPanel);

    auto* outputSplitter = new QSplitter(Qt::Vertical, this);
    outputSplitter->addWidget(srcPanel);
    outputSplitter->addWidget(datPanel);

    auto* workSplitter = new QSplitter(Qt::Horizontal, this);
    workSplitter->addWidget(buildSideBar());
    workSplitter->addWidget(inputSplitter);
    workSplitter->addWidget(outputSplitter);
    workSplitter->setStretchFactor(0, 0);
    workSplitter->setStretchFactor(1, 2);
    workSplitter->setStretchFactor(2, 2);

    return workSplitter;
}

QWidget* MainWindow::buildDiagnostics()
{
    auto* editor = new QPlainTextEdit(this);
    editor->setPlaceholderText("Diagnostics / Warnings / Errors...");
    editor->setMaximumHeight(170);
    editor->setReadOnly(true);

    return makePanel("DIAGNOSTICS", editor, this);
}

void MainWindow::applyTheme()
{
    QFile f(QApplication::applicationDirPath() + "/dark.qss");
    if (f.open(QFile::ReadOnly | QFile::Text))
        setStyleSheet(QString::fromUtf8(f.readAll()));
}

void MainWindow::loadFile(QPlainTextEdit *target, const QString &filter)
{
    const QString path = QFileDialog::getOpenFileName(this, "Open file", {}, filter);
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        statusBar()->showMessage("Error: could not open " + path, 4000);
        return;
    }

    target->setPlainText(QString::fromUtf8(f.readAll()));
    statusBar()->showMessage("Loaded: " + path, 4000);
}

void MainWindow::onOpenGcode() { loadFile(m_gcodeEditor, "G-code (*.gcode *.nc *.g);;All files (*)"); }
void MainWindow::onOpenYaml()  { loadFile(m_yamlEditor,  "YAML (*.yaml *.yml);;All files (*)"); }
void MainWindow::saveFile(QPlainTextEdit *source, const QString &defaultName, const QString &filter)
{
    const QString path = QFileDialog::getSaveFileName(this, "Save file", defaultName, filter);
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        statusBar()->showMessage("Error: could not save " + path, 4000);
        return;
    }

    f.write(source->toPlainText().toUtf8());
    statusBar()->showMessage("Saved: " + path, 4000);
}

void MainWindow::onSaveDat() { saveFile(m_datEditor, "output.dat", "DAT files (*.dat);;All files (*)"); }
void MainWindow::onSaveSrc() { saveFile(m_srcEditor, "output.src", "SRC files (*.src);;All files (*)"); }

void MainWindow::onTranslate()
{
    // TODO: integrate TranslationEngine
    statusBar()->showMessage("Translate: not yet implemented", 3000);
}
