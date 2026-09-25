#include "android_share.h"

#ifdef Q_OS_ANDROID
#include <QtCore/qjniobject.h>
#endif

namespace AndroidShare {
bool shareText(const QString &text)
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/ShareHelper", "shareText",
        "(Ljava/lang/String;)Z", text.left(65536));
#else
    Q_UNUSED(text)
    return false;
#endif
}

bool shareImage(const QString &path, const QString &text)
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/ShareHelper", "shareImage",
        "(Ljava/lang/String;Ljava/lang/String;)Z", path.left(4096), text.left(65536));
#else
    Q_UNUSED(path)
    Q_UNUSED(text)
    return false;
#endif
}
}
