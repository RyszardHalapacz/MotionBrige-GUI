#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QFrame;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QString;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onOpenGcode();
    void onOpenYaml();
    void onSaveDat();
    void onSaveSrc();
    void onTranslate();

private:
    enum class FocusPanel {
        Gcode,
        Yaml,
        Src,
        Dat
    };

    QPlainTextEdit *m_gcodeEditor = nullptr;
    QPlainTextEdit *m_yamlEditor  = nullptr;
    QPlainTextEdit *m_srcEditor   = nullptr;
    QPlainTextEdit *m_datEditor   = nullptr;

    QSplitter *m_workSplitter   = nullptr;
    QSplitter *m_inputSplitter  = nullptr;
    QSplitter *m_outputSplitter = nullptr;

    QFrame *m_gcodePanel = nullptr;
    QFrame *m_yamlPanel  = nullptr;
    QFrame *m_srcPanel   = nullptr;
    QFrame *m_datPanel   = nullptr;

    QPushButton *m_gcodeCard = nullptr;
    QPushButton *m_yamlCard  = nullptr;
    QPushButton *m_srcCard   = nullptr;
    QPushButton *m_datCard   = nullptr;

    QWidget* buildTopBar();
    QWidget* buildSideBar();
    QWidget* buildWorkspace();
    QWidget* buildDiagnostics();

    void applyTheme();
    void activatePanel(FocusPanel panel);
    void setActivePanelFrame(QFrame *activePanel);
    void refreshWidgetStyle(QWidget *widget);

    void loadFile(QPlainTextEdit *target, const QString &filter);
    void saveFile(QPlainTextEdit *source, const QString &defaultName, const QString &filter);
};

#endif // MAINWINDOW_H
