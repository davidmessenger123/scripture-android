#include "android_notifications.h"

#ifdef Q_OS_ANDROID
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#endif

namespace AndroidNotifications {
bool supported()
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/DailyVerseScheduler", "isSupported", "()Z");
#else
    return false;
#endif
}

bool permissionGranted()
{
    return permissionState() == QStringLiteral("granted");
}

bool permissionRequested()
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/DailyVerseScheduler", "permissionRequested", "()Z");
#else
    return false;
#endif
}

QString permissionState()
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticObjectMethod(
        "org/davidjm/scripture/DailyVerseScheduler", "permissionState",
        "()Ljava/lang/String;").toString();
#else
    return QStringLiteral("unavailable");
#endif
}

QString consumePermissionResult()
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticObjectMethod(
        "org/davidjm/scripture/DailyVerseScheduler", "consumePermissionResult",
        "()Ljava/lang/String;").toString();
#else
    return QStringLiteral("unavailable");
#endif
}

QString requestPermission()
{
#ifdef Q_OS_ANDROID
    const QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid())
        return QStringLiteral("unavailable");
    return QJniObject::callStaticObjectMethod(
        "org/davidjm/scripture/DailyVerseScheduler", "requestPermission",
        "(Landroid/app/Activity;)Ljava/lang/String;", activity.object()).toString();
#else
    return QStringLiteral("unavailable");
#endif
}

bool schedule(const QString &time)
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/DailyVerseScheduler", "schedule", "(Ljava/lang/String;)Z", time);
#else
    Q_UNUSED(time)
    return false;
#endif
}

bool openNotificationSettings(const QString &state)
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/DailyVerseScheduler", "openNotificationSettings",
        "(Ljava/lang/String;)Z", state);
#else
    Q_UNUSED(state)
    return false;
#endif
}

bool cancel()
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/DailyVerseScheduler", "cancel", "()Z");
#else
    return false;
#endif
}

void updateSnapshot(const QString &reference, const QString &text)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/DailyVerseScheduler", "updateSnapshot",
        "(Ljava/lang/String;Ljava/lang/String;)Z", reference, text.left(32768));
#endif
}
}
