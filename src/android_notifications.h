#ifndef SCRIPTURE_ANDROID_NOTIFICATIONS_H
#define SCRIPTURE_ANDROID_NOTIFICATIONS_H

#include <QString>

namespace AndroidNotifications {
bool supported();
bool permissionGranted();
bool permissionRequested();
QString permissionState();
QString consumePermissionResult();
QString requestPermission();
bool schedule(const QString &time);
bool openNotificationSettings(const QString &state);
bool cancel();
void updateSnapshot(const QString &reference, const QString &text);
}

#endif
