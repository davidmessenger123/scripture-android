#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdlib>

namespace {
void require(bool condition)
{
    if (!condition)
        std::abort();
}

bool ndkVersionCheck(const QString &properties)
{
    QTemporaryDir directory;
    if (!directory.isValid())
        return false;
    QFile source(directory.filePath(QStringLiteral("source.properties")));
    if (!source.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    source.write(properties.toUtf8());
    source.close();
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("ANDROID_NDK_ROOT"), directory.path());
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral("bash"), QStringList{
        QStringLiteral(SCRIPTURE_SOURCE_DIR "/tools/build_android_openssl.sh"),
        QStringLiteral("--check-ndk-version")});
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    require(ndkVersionCheck(QStringLiteral(" Pkg.Revision \t= \t27.2.12479018 \r\n")));
    require(ndkVersionCheck(QStringLiteral("Pkg.Revision=27.2.12479018\n")));
    require(!ndkVersionCheck(QStringLiteral("Pkg.Revision = 27.2.12479017\n")));
    QFile manifest(QStringLiteral(SCRIPTURE_SOURCE_DIR "/android/AndroidManifest.xml"));
    require(manifest.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(manifest.readAll());
    require(text.contains(QStringLiteral("android:enableOnBackInvokedCallback=\"false\"")));
    require(text.contains(QStringLiteral("android.permission.POST_NOTIFICATIONS")));
    require(text.contains(QStringLiteral("android.permission.RECEIVE_BOOT_COMPLETED")));
    require(text.contains(QStringLiteral("org.davidjm.scripture.VerseShareProvider")));
    require(text.contains(QStringLiteral("org.davidjm.scripture.DailyVerseReceiver")));
    require(text.contains(QStringLiteral("org.davidjm.scripture.MainActivity")));
    require(text.contains(QStringLiteral("org.davidjm.scripture.DAILY_VERSE")));
    require(text.contains(QStringLiteral("android.intent.action.TIME_SET")));
    require(text.contains(QStringLiteral("android.intent.action.TIMEZONE_CHANGED")));
    require(text.contains(QStringLiteral("android.intent.action.MY_PACKAGE_REPLACED")));
    QFile workflow(QStringLiteral(SCRIPTURE_SOURCE_DIR "/.github/workflows/ci.yml"));
    require(workflow.open(QIODevice::ReadOnly));
    const QString workflowText = QString::fromUtf8(workflow.readAll());
    require(workflowText.contains(QStringLiteral("libegl-dev")));
    require(workflowText.contains(QStringLiteral("libegl1")));
    require(workflowText.contains(QStringLiteral("QT_VERSION: 6.9.0")));
    require(workflowText.contains(QStringLiteral("ANDROID_NDK_VERSION: 27.2.12479018")));
    require(workflowText.contains(QStringLiteral("ANDROID_API: 36")));
    require(workflowText.contains(QStringLiteral("ANDROID_CMDLINE_TOOLS_VERSION: 11076708")));
    require(workflowText.contains(QStringLiteral("command -v sdkmanager")));
    require(workflowText.contains(QStringLiteral("cmdline-tools")));
    require(workflowText.contains(QStringLiteral("GITHUB_ENV")));
    require(workflowText.contains(QStringLiteral("ANDROID_SDK_ROOT")));
    require(workflowText.contains(QStringLiteral("ANDROID_NDK_ROOT")));
    require(workflowText.contains(QStringLiteral("commandlinetools-linux-${ANDROID_CMDLINE_TOOLS_VERSION}_latest.zip")));
    require(workflowText.contains(QStringLiteral("sdk_root=\"$RUNNER_TEMP/android-sdk\"")));
    require(workflowText.contains(QStringLiteral("mkdir -p \"$sdk_root\"")));
    require(workflowText.contains(QStringLiteral("path_sdkmanager")));
    require(workflowText.contains(QStringLiteral("test -x \"$sdkmanager_path\"")));
    require(workflowText.contains(QStringLiteral("Android sdkmanager is not executable")));
    require(!workflowText.contains(QStringLiteral("sdk_root=\"${ANDROID_SDK_ROOT")));
    require(!workflowText.contains(QStringLiteral("sdk_root=\"$candidate\"")));
    require(!workflowText.contains(QStringLiteral("! -w \"$sdk_root\"")));
    require(!workflowText.contains(QStringLiteral("chmod +x")));
    require(!workflowText.contains(QStringLiteral("export ANDROID_SDK_ROOT=\"$RUNNER_TEMP/android-sdk\"")));
    QFile icon(QStringLiteral(SCRIPTURE_SOURCE_DIR "/android/res/drawable/ic_notification.xml"));
    require(icon.open(QIODevice::ReadOnly));
    require(QString::fromUtf8(icon.readAll()).contains(QStringLiteral("<vector")));
    QFile scheduler(QStringLiteral(SCRIPTURE_SOURCE_DIR "/android/src/org/davidjm/scripture/DailyVerseScheduler.java"));
    require(scheduler.open(QIODevice::ReadOnly));
    const QString schedulerText = QString::fromUtf8(scheduler.readAll());
    require(schedulerText.contains(QStringLiteral("consumePermissionResult")));
    require(schedulerText.contains(QStringLiteral("onPermissionResult")));
    require(schedulerText.contains(QStringLiteral("PERMISSION_REQUESTED")));
    require(schedulerText.contains(QStringLiteral("runtime-denied")));
    require(schedulerText.contains(QStringLiteral("app-blocked")));
    require(schedulerText.contains(QStringLiteral("channel-disabled")));
    require(schedulerText.contains(QStringLiteral("Settings.ACTION_APP_NOTIFICATION_SETTINGS")));
    require(schedulerText.contains(QStringLiteral("Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS")));
    require(schedulerText.contains(QStringLiteral("Settings.EXTRA_CHANNEL_ID")));
    require(schedulerText.contains(QStringLiteral("openNotificationSettings")));
    require(schedulerText.contains(QStringLiteral("STATE_RUNTIME_DENIED.equals(state)")));
    require(schedulerText.contains(QStringLiteral("if (preferences.getBoolean(PERMISSION_REQUESTED, false))")));
    require(schedulerText.contains(QStringLiteral("activity.requestPermissions")));
    require(schedulerText.contains(QStringLiteral("setAndAllowWhileIdle")));
    require(schedulerText.contains(QStringLiteral("if (!scheduleNext(context))")));
    require(schedulerText.contains(QStringLiteral("rollback")));
    QFile main(QStringLiteral(SCRIPTURE_SOURCE_DIR "/src/qml/main.qml"));
    require(main.open(QIODevice::ReadOnly));
    const QString mainText = QString::fromUtf8(main.readAll());
    require(mainText.contains(QStringLiteral("objectName: \"settingsButton\"")));
    require(mainText.contains(QStringLiteral("App.toggle_settings()")));
    QFile settings(QStringLiteral(SCRIPTURE_SOURCE_DIR "/src/qml/ScriptureSettings.qml"));
    require(settings.open(QIODevice::ReadOnly));
    const QString settingsText = QString::fromUtf8(settings.readAll());
    require(settingsText.contains(QStringLiteral("notificationPermissionButton")));
    require(settingsText.contains(QStringLiteral("App.open_notification_settings()")));
    require(settingsText.contains(QStringLiteral("App.notificationPermissionState === \"runtime-denied\"")));
    require(settingsText.contains(QStringLiteral("App.notificationPermissionRequested")));
    QFile bridge(QStringLiteral(SCRIPTURE_SOURCE_DIR "/src/android_notifications.cpp"));
    require(bridge.open(QIODevice::ReadOnly));
    require(QString::fromUtf8(bridge.readAll()).contains(QStringLiteral("openNotificationSettings")));
    QFile controller(QStringLiteral(SCRIPTURE_SOURCE_DIR "/src/controller.cpp"));
    require(controller.open(QIODevice::ReadOnly));
    const QString controllerText = QString::fromUtf8(controller.readAll());
    require(controllerText.contains(QStringLiteral("open_notification_settings")));
    require(controllerText.contains(QStringLiteral("deniedRecovery")));
    require(controllerText.contains(QStringLiteral("m_notificationPermissionRequested")));
    QFile receiver(QStringLiteral(SCRIPTURE_SOURCE_DIR "/android/src/org/davidjm/scripture/DailyVerseReceiver.java"));
    require(receiver.open(QIODevice::ReadOnly));
    const QString receiverText = QString::fromUtf8(receiver.readAll());
    require(receiverText.contains(QStringLiteral("ACTION_DAILY.equals(action)")));
    require(receiverText.contains(QStringLiteral("ACTION_TIMEZONE_CHANGED")));
    require(receiverText.contains(QStringLiteral("ACTION_PACKAGE_REPLACED")));
    QFile activity(QStringLiteral(SCRIPTURE_SOURCE_DIR "/android/src/org/davidjm/scripture/MainActivity.java"));
    require(activity.open(QIODevice::ReadOnly));
    require(QString::fromUtf8(activity.readAll()).contains(QStringLiteral("onRequestPermissionsResult")));
    return 0;
}
