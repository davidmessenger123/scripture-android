#include "secrets.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QSaveFile>

#ifdef Q_OS_ANDROID
#include <QAndroidJniObject>
#endif

namespace {
QString validatedKey(const QString &value)
{
    const QString key = value.trimmed();
    if (key.toUtf8().size() > 512)
        return QString();
    for (const QChar character : key) {
        if (character.unicode() < 32 || character.unicode() == 127)
            return QString();
    }
    return key;
}
}

SecureStore::SecureStore(const QString &dataDir)
    : m_dir(dataDir)
    , m_path(QDir(dataDir).filePath(QStringLiteral("esv-api-key")))
{
}

QString SecureStore::load(QString *error) const
{
#ifdef Q_OS_ANDROID
    const bool hasStoredValue = QAndroidJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/SecureKeyStore", "hasStoredValue", "()Z");
    if (hasStoredValue) {
        const QAndroidJniObject result = QAndroidJniObject::callStaticObjectMethod(
            "org/davidjm/scripture/SecureKeyStore", "get", "()Ljava/lang/String;");
        const QString value = result.isValid() ? result.toString() : QString();
        if (!value.isEmpty() && !validatedKey(value).isEmpty()) {
            QString removalError;
            if (!clearFile(&removalError) && error)
                *error = removalError;
            return value;
        }
        if (error)
            *error = QStringLiteral("Android secure storage could not read the API key");
        return QString();
    }
    const QString legacy = loadFile(error);
    if (!legacy.isEmpty()) {
        const bool migrated = QAndroidJniObject::callStaticMethod<bool>(
            "org/davidjm/scripture/SecureKeyStore", "set", "(Ljava/lang/String;)Z", legacy);
        if (migrated) {
            QString removalError;
            if (!clearFile(&removalError) && error)
                *error = removalError;
        } else if (error) {
            *error = QStringLiteral("Android secure key migration failed");
        }
    }
    return legacy;
#else
    return loadFile(error);
#endif
}

bool SecureStore::save(const QString &value, QString *error) const
{
    const QString key = validatedKey(value);
    if (key.isEmpty()) {
        if (error)
            *error = QStringLiteral("API key is invalid");
        return false;
    }
#ifdef Q_OS_ANDROID
    const bool saved = QAndroidJniObject::callStaticMethod<bool>(
        "org/davidjm/scripture/SecureKeyStore", "set", "(Ljava/lang/String;)Z", key);
    if (saved)
        return true;
    if (error)
        *error = QStringLiteral("Android secure storage failed");
    return false;
#else
    return saveFile(key, error);
#endif
}

bool SecureStore::clear(QString *error) const
{
    if (!clearFile(error))
        return false;
#ifdef Q_OS_ANDROID
    if (!QAndroidJniObject::callStaticMethod<bool>(
            "org/davidjm/scripture/SecureKeyStore", "delete", "()Z")) {
        if (error)
            *error = QStringLiteral("Android secure storage could not remove the API key");
        return false;
    }
#endif
    return true;
}

QString SecureStore::loadFile(QString *error) const
{
    QFile file(m_path);
    if (!file.exists())
        return QString();
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("could not read the stored API key");
        return QString();
    }
    const QFileDevice::Permissions permissions = file.permissions();
    const QFileDevice::Permissions broad = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    if (permissions & broad) {
        if (error)
            *error = QStringLiteral("stored API key permissions are too broad");
        return QString();
    }
    const QByteArray data = file.read(8193);
    if (data.size() > 8192) {
        if (error)
            *error = QStringLiteral("stored API key exceeds the size limit");
        return QString();
    }
    const QString value = QString::fromUtf8(data).trimmed();
    if (!value.isEmpty() && validatedKey(value).isEmpty()) {
        if (error)
            *error = QStringLiteral("stored API key is invalid");
        return QString();
    }
    return value;
}

bool SecureStore::saveFile(const QString &value, QString *error) const
{
    if (!QDir().mkpath(m_dir)) {
        if (error)
            *error = QStringLiteral("could not create the settings directory");
        return false;
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("could not save the API key");
        return false;
    }
    if (file.write(value.toUtf8()) != value.toUtf8().size() || !file.commit()) {
        if (error)
            *error = QStringLiteral("could not save the API key");
        return false;
    }
    if (!QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        QFile::remove(m_path);
        if (error)
            *error = QStringLiteral("could not protect the API key file");
        return false;
    }
    return true;
}

bool SecureStore::clearFile(QString *error) const
{
    QFile file(m_path);
    if (!file.exists())
        return true;
    if (!file.remove()) {
        if (error)
            *error = QStringLiteral("could not remove the stored API key");
        return false;
    }
    return true;
}
