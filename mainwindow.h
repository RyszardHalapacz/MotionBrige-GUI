#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QPlainTextEdit;

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
    QPlainTextEdit *m_gcodeEditor = nullptr;
    QPlainTextEdit *m_yamlEditor  = nullptr;
    QPlainTextEdit *m_srcEditor   = nullptr;
    QPlainTextEdit *m_datEditor   = nullptr;

    QWidget* buildTopBar();
    QWidget* buildSideBar();
    QWidget* buildWorkspace();
    QWidget* buildDiagnostics();
    void     applyTheme();
    void     loadFile(QPlainTextEdit *target, const QString &filter);
    void     saveFile(QPlainTextEdit *source, const QString &defaultName, const QString &filter);
};

#endif // MAINWINDOW_H
