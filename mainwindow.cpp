#include "mainwindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

static QPixmap loadLogoTransparent(const QString &path, int threshold = 40)
{
    QImage img(path);
    if (img.isNull())
        return {};

    img = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qRed(line[x]) < threshold &&
                qGreen(line[x]) < threshold &&
                qBlue(line[x]) < threshold) {
                line[x] = qRgba(0, 0, 0, 0);
            }
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

static QPushButton* makeFileCard(
    const QString &badge,
    const QString &name,
    const QString &description,
    const QString &path,
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

static QLabel* makeTargetInfo(QWidget *parent)
{
    auto* info = new QLabel(parent);
    info->setObjectName("targetInfo");
    info->setText(
        "Backend          KUKA (KRL)\n"
        "Robot            KR 120 R2700\n"
        "Profile          default\n"
        "Mode             TRANSFORM\n"
        "Status           ● Ready"
    );
    return info;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1600, 950);
    setWindowTitle("MotionBridge Studio");

    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 0);
    layout->setSpacing(8);

    layout->addWidget(buildTopBar());
    layout->addWidget(buildWorkspace());
    layout->addWidget(buildDiagnostics());

    setCentralWidget(central);

    applyTheme();
    activatePanel(FocusPanel::Gcode);
    statusBar()->showMessage("Ready  |  Pipeline: Ready  |  Diagnostics: 0 errors, 0 warnings");
}

QWidget* MainWindow::buildTopBar()
{
    auto* bar = new QFrame(this);
    bar->setObjectName("topBar");

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(10);

    auto* logo = new QLabel(bar);
    QPixmap logoPix = loadLogoTransparent(QApplication::applicationDirPath() + "/icnos/logo.png");
    if (!logoPix.isNull()) {
        logo->setPixmap(logoPix.scaledToHeight(48, Qt::SmoothTransformation));
    } else {
        logo->setText("MotionBridge");
        logo->setObjectName("logoFallback");
    }

    auto* openGcodeBtn = new QPushButton("Open .g-code", bar);
    auto* openYamlBtn  = new QPushButton("Open YAML",    bar);
    auto* saveDatBtn   = new QPushButton("Save .dat",    bar);
    auto* saveSrcBtn   = new QPushButton("Save .src",    bar);
    auto* translateBtn = new QPushButton("Translate",    bar);

    connect(openGcodeBtn, &QPushButton::clicked, this, &MainWindow::onOpenGcode);
    connect(openYamlBtn,  &QPushButton::clicked, this, &MainWindow::onOpenYaml);
    connect(saveDatBtn,   &QPushButton::clicked, this, &MainWindow::onSaveDat);
    connect(saveSrcBtn,   &QPushButton::clicked, this, &MainWindow::onSaveSrc);
    connect(translateBtn, &QPushButton::clicked, this, &MainWindow::onTranslate);

    translateBtn->setObjectName("translateBtn");

    layout->addWidget(logo);
    layout->addSpacing(16);
    layout->addWidget(openGcodeBtn);
    layout->addWidget(openYamlBtn);
    layout->addStretch();
    layout->addWidget(translateBtn);
    layout->addStretch();
    layout->addWidget(saveDatBtn);
    layout->addWidget(saveSrcBtn);

    return bar;
}

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

    m_gcodeCard = makeFileCard("G", "model.gcode", "G-code source", "C:/project/input/model.gcode", sidebar);
    m_yamlCard  = makeFileCard("Y", "config.yaml", "Backend configuration", "C:/project/config.yaml", sidebar);
    m_srcCard   = makeFileCard("S", "program.src", "KRL motion program", "C:/project/program.src", sidebar);
    m_datCard   = makeFileCard("D", "tool.dat", "KRL data file", "C:/project/tool.dat", sidebar);

    group->addButton(m_gcodeCard);
    group->addButton(m_yamlCard);
    group->addButton(m_srcCard);
    group->addButton(m_datCard);

    connect(m_gcodeCard, &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Gcode); });
    connect(m_yamlCard,  &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Yaml); });
    connect(m_srcCard,   &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Src); });
    connect(m_datCard,   &QPushButton::clicked, this, [this] { activatePanel(FocusPanel::Dat); });

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

    auto* targetBox = new QFrame(sidebar);
    targetBox->setObjectName("sidebarBox");
    auto* targetLayout = new QVBoxLayout(targetBox);
    targetLayout->setContentsMargins(10, 10, 10, 10);
    targetLayout->setSpacing(8);
    targetLayout->addWidget(makeSectionTitle("TARGET", targetBox));
    targetLayout->addWidget(makeTargetInfo(targetBox));

    auto* watermark = new QLabel("M", sidebar);
    watermark->setObjectName("watermark");
    watermark->setAlignment(Qt::AlignCenter);

    layout->addWidget(inputBox);
    layout->addWidget(outputBox);
    layout->addWidget(targetBox);
    layout->addStretch();
    layout->addWidget(watermark);

    return sidebar;
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
    auto* editor = new QPlainTextEdit(this);
    editor->setPlaceholderText("Diagnostics / Warnings / Errors...");
    editor->setMaximumHeight(140);
    editor->setReadOnly(true);

    return makePanel("DIAGNOSTICS", editor, this);
}

void MainWindow::activatePanel(FocusPanel panel)
{
    if (!m_workSplitter || !m_inputSplitter || !m_outputSplitter)
        return;

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
    const auto panels = {m_gcodePanel, m_yamlPanel, m_srcPanel, m_datPanel};
    for (auto* panel : panels) {
        if (!panel)
            continue;

        panel->setProperty("active", panel == activePanel);
        refreshWidgetStyle(panel);
    }
}

void MainWindow::refreshWidgetStyle(QWidget *widget)
{
    if (!widget)
        return;

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

void MainWindow::onTranslate()
{
    // TODO: integrate TranslationEngine
    statusBar()->showMessage("Translate: not yet implemented", 3000);
}
