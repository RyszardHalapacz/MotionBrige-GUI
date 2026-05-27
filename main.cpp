#include "mainwindow.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QPixmap>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

static QWidget* createSplashScreen()
{
    auto* splash = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::SplashScreen);
    splash->setObjectName("splashScreen");
    splash->setAttribute(Qt::WA_DeleteOnClose);
    splash->resize(900, 560);
    splash->setStyleSheet(R"(
        QWidget#splashScreen {
            background-color: #0f141b;
            border: 1px solid #293241;
        }

        QLabel#splashLogo {
            background: transparent;
            color: #d7dde7;
            font-family: "Segoe UI";
            font-size: 28px;
            font-weight: 600;
        }
    )");

    auto* layout = new QVBoxLayout(splash);
    layout->setContentsMargins(48, 48, 48, 48);

    auto* logo = new QLabel(splash);
    logo->setObjectName("splashLogo");
    logo->setAlignment(Qt::AlignCenter);

    const QPixmap pixmap(QApplication::applicationDirPath() + "/icnos/content.png");
    if (!pixmap.isNull()) {
        logo->setPixmap(
            pixmap.scaled(
                760,
                430,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            )
        );
    } else {
        logo->setText("MotionBridge Studio");
    }

    layout->addStretch();
    layout->addWidget(logo, 0, Qt::AlignCenter);
    layout->addStretch();

    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect geometry = screen->availableGeometry();
        splash->move(geometry.center() - splash->rect().center());
    }

    return splash;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto* splash = createSplashScreen();
    splash->show();

    QTimer::singleShot(5000, [splash] {
        auto* mainWindow = new MainWindow;
        splash->close();
        mainWindow->show();
    });

    return QApplication::exec();
}
