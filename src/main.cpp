#include "controller.h"
#include "favorites.h"
#include "updater.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>

#include <QtQml/qqml.h>

// Scripture for Android — the desktop Scripture app ported to Qt Quick with a
// C++ backend, reusing the desktop app's main.qml UI. Both singletons are
// registered into the `ScriptureRT` module so the QML import resolves.
//
// `--smoke` runs a headless load test (QT_QPA_PLATFORM=offscreen) that mirrors
// the Python desktop app's smoke mode: open overlay + settings, then quit.

int main(int argc, char *argv[])
{
    // Pin Qt Quick Controls to the "Basic" style, matching the desktop app, so
    // no platform-specific native style module is ever required.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Scripture"));
    app.setOrganizationName(QStringLiteral("davidjm"));
    app.setQuitOnLastWindowClosed(false);

    const QStringList args = app.arguments();
    const bool smoke = args.contains(QStringLiteral("--smoke"));

    AppController controller;
    UpdateChecker updater;

    qmlRegisterSingletonInstance("ScriptureRT", 1, 0, "App", &controller);
    qmlRegisterSingletonInstance("ScriptureRT", 1, 0, "Updater", &updater);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [](const QUrl &url) {
                         qWarning() << "failed to load" << url.toString();
                         QCoreApplication::exit(1);
                     });

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "no root objects; main.qml did not load";
        return 1;
    }

#ifdef Q_OS_ANDROID
    // The phone app opens straight to a verse (like the Omarchy widget does);
    // the desktop app stays dormant until the tray or auto-open triggers it.
    controller.setOverlayOpen(true);
#endif

    if (smoke) {
        controller.setOverlayOpen(true);
        controller.setSettingsOpen(true);
        QTimer::singleShot(1200, &app, &QCoreApplication::quit);
        qInfo() << "smoke: engine loaded, controller OK";
    }

    return app.exec();
}