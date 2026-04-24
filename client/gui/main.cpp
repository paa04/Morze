#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QTimer>

#include "ClientBridge.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dwmapi.h>
#  ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#    define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#  endif
#  ifndef DWMWA_BORDER_COLOR
#    define DWMWA_BORDER_COLOR 34
#  endif
#  ifndef DWMWA_CAPTION_COLOR
#    define DWMWA_CAPTION_COLOR 35
#  endif
#  ifndef DWMWA_CAPTION_TEXT_COLOR
#    define DWMWA_CAPTION_TEXT_COLOR 36
#  endif

static void morzeApplyWindowsTitleBarColors(QWindow *window)
{
    if (!window)
        return;
    if (!window->handle())
        window->create();
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;

    // Тёмная тема заголовка (кнопки в стиле тёмной панели, где поддерживается ОС)
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    // Как в Main.qml: color "#0e1621", корень "#0b1219", текст "#e6eef8"
    const COLORREF caption = RGB(0x0e, 0x16, 0x21);
    const COLORREF border = RGB(0x0b, 0x12, 0x19);
    const COLORREF captionText = RGB(0xe6, 0xee, 0xf8);

    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    // Windows 11: цвет подписи в заголовке; на Win10 вызов может вернуть ошибку — игнорируем
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_TEXT_COLOR, &captionText, sizeof(captionText));
}
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    // Иконка приложения (тот же ресурс, что и в QML-модуле Morze_ui)
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Morze_ui/img/morze.svg")));

    QQmlApplicationEngine engine;
    ClientBridge clientBridge;
    engine.rootContext()->setContextProperty("clientBridge", &clientBridge);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Morze_ui", "Main");
    clientBridge.refreshAll();

#ifdef Q_OS_WIN
    if (auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
        const auto apply = [win]() { morzeApplyWindowsTitleBarColors(win); };
        QTimer::singleShot(0, win, apply);
        QTimer::singleShot(100, win, apply);
        QObject::connect(win, &QWindow::visibilityChanged, win, [apply](QWindow::Visibility visibility) {
            if (visibility != QWindow::Hidden)
                apply();
        });
    }
#endif

    return QCoreApplication::exec();
}
