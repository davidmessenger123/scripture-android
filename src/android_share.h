#ifndef SCRIPTURE_ANDROID_SHARE_H
#define SCRIPTURE_ANDROID_SHARE_H

#include <QString>

namespace AndroidShare {
bool shareText(const QString &text);
bool shareImage(const QString &path, const QString &text);
}

#endif
