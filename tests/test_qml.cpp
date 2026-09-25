#include "controller.h"
#include "updater.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <QtQml/qqml.h>

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ScriptureQmlTest"));
    app.setOrganizationName(QStringLiteral("davidjm"));

    QSettings persisted(QStringLiteral("davidjm"), QStringLiteral("Scripture"));
    persisted.setValue(QStringLiteral("translation"), QStringLiteral("invalid"));
    persisted.setValue(QStringLiteral("fixedReference"), QStringLiteral("John\n3:16"));
    persisted.setValue(QStringLiteral("autoOpenAt"), QStringLiteral("99:99"));
    persisted.setValue(QStringLiteral("dailyNotificationAt"), QStringLiteral("99:99"));
    persisted.setValue(QStringLiteral("bookFilter"), QStringLiteral("Not a book"));
    persisted.setValue(QStringLiteral("topicFilter"), QStringLiteral("Not a topic"));
    persisted.setValue(QStringLiteral("verseFontSize"), 999);
    persisted.setValue(QStringLiteral("scrimOpacity"), 999);
    persisted.setValue(QStringLiteral("revealSpeed"), -1);
    persisted.sync();

    AppController controller;
    UpdateChecker updater;
    if (controller.settingsTranslation() != QStringLiteral("ESV")
        || !controller.settingsFixedReference().isEmpty()
        || !controller.settingsAutoOpenAt().isEmpty()
        || !controller.settingsDailyNotificationAt().isEmpty()
        || !controller.selectedBook().isEmpty()
        || !controller.selectedTopic().isEmpty()
        || controller.verseFontSize() != 56
        || controller.scrimOpacity() != 100
        || controller.revealSpeed() != 0
        || !controller.settingsNotice().contains(QStringLiteral("repaired")))
        return 2;
    if (controller.notificationPermissionState() != QStringLiteral("unavailable")
        || controller.notificationPermissionRequested()
        || controller.notificationPermissionGranted())
        return 3;
    controller.setSelectedBook(QStringLiteral("Genesis"));
    controller.setSelectedTopic(QStringLiteral("Faith"));
    if (controller.selectedBook() != QStringLiteral("Genesis")
        || controller.selectedTopic() != QStringLiteral("Faith")
        || !controller.actionNotice().contains(QStringLiteral("No verses match")))
        return 4;
    controller.save_all_settings(controller.settingsApiKey(), controller.settingsTranslation(),
                                  controller.settingsFixedReference(), controller.settingsAutoOpenAt(),
                                  QStringLiteral("99:99"), controller.verseFontSize(),
                                  controller.scrimOpacity(), controller.revealSpeed());
    if (!controller.settingsDailyNotificationAt().isEmpty()
        || !controller.settingsNotice().contains(QStringLiteral("Daily notification")))
        return 5;
    qmlRegisterSingletonInstance("ScriptureRT", 1, 0, "App", &controller);
    qmlRegisterSingletonInstance("ScriptureRT", 1, 0, "Updater", &updater);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    controller.setVerseFontSize(999);
    controller.setScrimOpacity(-20);
    controller.setRevealSpeed(999);
    if (controller.verseFontSize() != 56 || controller.scrimOpacity() != 0
        || controller.revealSpeed() != 100)
        return 2;
    controller.setOverlayOpen(true);
    QObject *settingsButton = engine.rootObjects().constFirst()->findChild<QObject *>(
        QStringLiteral("settingsButton"));
    if (!settingsButton
        || !QMetaObject::invokeMethod(settingsButton, "click", Qt::DirectConnection)
        || !controller.settingsOpen())
        return 6;
    if (!engine.rootObjects().constFirst()->findChild<QObject *>(
            QStringLiteral("notificationPermissionButton")))
        return 7;
    QTimer::singleShot(250, &app, [&]() {
        app.exit(controller.settingsOpen() && controller.bookOptions().size() > 0 ? 0 : 2);
    });
    return app.exec();
}
