#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include "motionbridge_api.h"

class QFrame;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSplitter;
class QTimer;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setEngineCallback(TranslationCallback cb);

private slots:
    void onOpenGcode();
    void onOpenYaml();
    void onSaveDat();
    void onSaveSrc();
    void onTranslate();
    void onCancel();
    void onProgressTick();
    void onTranslateDone(int rc);

private:
    enum class FocusPanel { Gcode, Yaml, Src, Dat };

    TranslationCallback m_engineFn      = nullptr;

    QTimer*  m_progressTimer    = nullptr;
    QString  m_progressFilePath;
    QString  m_resultFilePath;
    QString  m_cancelFlagPath;
    qint64   m_progressFilePos  = 0;

    bool m_busy = false;

    // editors
    QPlainTextEdit *m_gcodeEditor = nullptr;
    QPlainTextEdit *m_yamlEditor  = nullptr;
    QPlainTextEdit *m_srcEditor   = nullptr;
    QPlainTextEdit *m_datEditor   = nullptr;
    QPlainTextEdit *m_diagEditor  = nullptr;

    // splitters & panels
    QSplitter *m_workSplitter   = nullptr;
    QSplitter *m_inputSplitter  = nullptr;
    QSplitter *m_outputSplitter = nullptr;

    QFrame *m_gcodePanel = nullptr;
    QFrame *m_yamlPanel  = nullptr;
    QFrame *m_srcPanel   = nullptr;
    QFrame *m_datPanel   = nullptr;

    // sidebar file cards
    QPushButton *m_gcodeCard = nullptr;
    QPushButton *m_yamlCard  = nullptr;
    QPushButton *m_srcCard   = nullptr;
    QPushButton *m_datCard   = nullptr;

    // top-bar buttons + progress
    QPushButton  *m_openGcodeBtn = nullptr;
    QPushButton  *m_openYamlBtn  = nullptr;
    QPushButton  *m_saveDatBtn   = nullptr;
    QPushButton  *m_saveSrcBtn   = nullptr;
    QPushButton  *m_translateBtn = nullptr;
    QPushButton  *m_cancelBtn    = nullptr;
    QProgressBar *m_progressBar  = nullptr;

    QLabel *m_targetInfoLabel = nullptr;

    QWidget* buildTopBar();
    QWidget* buildSideBar();
    QWidget* buildWorkspace();
    QWidget* buildDiagnostics();

    void applyTheme();
    void activatePanel(FocusPanel panel);
    void setActivePanelFrame(QFrame *activePanel);
    void refreshWidgetStyle(QWidget *widget);
    void setBusyUi(bool busy);

    void keyPressEvent(QKeyEvent *event) override;

    void loadFile(QPlainTextEdit *target, const QString &filter);
    void saveFile(QPlainTextEdit *source, const QString &defaultName, const QString &filter);
    QString loadTextFile(const QString &path);
    void parseAndLoadResults();
};

#endif // MAINWINDOW_H
