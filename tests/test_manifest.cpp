#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSysInfo>
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

QString ndkHostTag()
{
    const QString kernel = QSysInfo::kernelType().toLower();
    const QString architecture = QSysInfo::currentCpuArchitecture().toLower();
    if (kernel == QStringLiteral("linux") && architecture == QStringLiteral("x86_64"))
        return QStringLiteral("linux-x86_64");
    if (kernel == QStringLiteral("linux")
        && (architecture == QStringLiteral("aarch64") || architecture == QStringLiteral("arm64")))
        return QStringLiteral("linux-aarch64");
    if (kernel == QStringLiteral("darwin")
        && (architecture == QStringLiteral("x86_64") || architecture == QStringLiteral("arm64")))
        return QStringLiteral("darwin-x86_64");
    return QString();
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
    const QString hostTag = ndkHostTag();
    if (hostTag.isEmpty())
        return false;
    const QString binPath = directory.filePath(
        QStringLiteral("toolchains/llvm/prebuilt/%1/bin").arg(hostTag));
    if (!QDir().mkpath(binPath))
        return false;
    const QString targetClangPath = binPath + QStringLiteral("/aarch64-linux-android28-clang");
    const QString targetClangxxPath = binPath + QStringLiteral("/aarch64-linux-android28-clang++");
    const QStringList tools{QStringLiteral("clang"), QStringLiteral("llvm-ar"),
                            QStringLiteral("llvm-ranlib"), QStringLiteral("aarch64-linux-android28-clang"),
                            QStringLiteral("aarch64-linux-android28-clang++")};
    for (const QString &tool : tools) {
        QFile toolFile(binPath + QLatin1Char('/') + tool);
        if (!toolFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        toolFile.close();
        if (!toolFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ExeOwner))
            return false;
    }
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("ANDROID_NDK_ROOT"), directory.path());
    environment.insert(QStringLiteral("PATH"), QStringLiteral(":/usr/bin:/bin:"));
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral("bash"), QStringList{
        QStringLiteral(SCRIPTURE_SOURCE_DIR "/tools/build_android_openssl.sh"),
        QStringLiteral("--check-ndk-version")});
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return false;
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    return output.contains(QStringLiteral("ndk-wrappers/aarch64-linux-android-gcc"))
        && output.contains(QStringLiteral("ndk-wrappers/aarch64-linux-android-g++"))
        && output.contains(targetClangPath) && output.contains(targetClangxxPath);
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
    QFile opensslScript(QStringLiteral(SCRIPTURE_SOURCE_DIR "/tools/build_android_openssl.sh"));
    require(opensslScript.open(QIODevice::ReadOnly));
    const QString opensslText = QString::fromUtf8(opensslScript.readAll());
    require(opensslText.contains(QStringLiteral("uname -s")));
    require(opensslText.contains(QStringLiteral("uname -m")));
    require(opensslText.contains(QStringLiteral("NDK_BIN")));
    require(opensslText.contains(QStringLiteral("NDK_WRAPPER_DIR")));
    require(opensslText.contains(QStringLiteral("WRAPPER_PREFIX=aarch64-linux-android")));
    require(opensslText.contains(QStringLiteral("${WRAPPER_PREFIX}-gcc")));
    require(opensslText.contains(QStringLiteral("${TARGET_CLANG_PREFIX}${OPENSSL_API}-clang")));
    require(opensslText.contains(QStringLiteral("ln -s")));
    require(opensslText.contains(QStringLiteral("export PATH=\"$NDK_WRAPPER_DIR:$NDK_BIN")));
    require(opensslText.contains(QStringLiteral("command -v \"${WRAPPER_PREFIX}-gcc\"")));
    require(opensslText.contains(QStringLiteral("command -v \"${WRAPPER_PREFIX}-g++\"")));
    require(opensslText.contains(QStringLiteral("llvm-ar")));
    require(opensslText.contains(QStringLiteral("llvm-ranlib")));
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
