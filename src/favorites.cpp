#include "favorites.h"

#include "references.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>

namespace {
constexpr int MAX_ANCHOR_BYTES = 120;
constexpr int MAX_ENTRIES = 200;
constexpr int MAX_JSON_BYTES = 65536;
}

FavoritesStore::FavoritesStore(const QString &dataDir)
    : m_dir(dataDir)
    , m_path(QDir(dataDir).filePath(QStringLiteral("favorites.json")))
{
}

QString FavoritesStore::path() const
{
    return m_path;
}

void FavoritesStore::ensureDir()
{
    if (!QDir().mkpath(m_dir))
        throw FavoritesError(QStringLiteral("could not create favorites directory: %1").arg(m_dir));
}

QString FavoritesStore::clean(const QString &reference) const
{
    const QString anchor = References::normalizeReference(reference);
    if (anchor.isEmpty())
        throw FavoritesError(QStringLiteral("reference must look like John 3:16"));
    if (anchor.toUtf8().size() > MAX_ANCHOR_BYTES)
        throw FavoritesError(QStringLiteral("reference exceeds the %1-byte limit").arg(MAX_ANCHOR_BYTES));
    return anchor;
}

QVariantList FavoritesStore::list()
{
    QFile file(m_path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        throw FavoritesError(QStringLiteral("favorites file refused open: %1").arg(file.errorString()));
    const QByteArray data = file.read(MAX_JSON_BYTES + 1);
    if (data.size() > MAX_JSON_BYTES)
        throw FavoritesError(QStringLiteral("favorites list exceeds the %1-byte limit").arg(MAX_JSON_BYTES));
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        throw FavoritesError(QStringLiteral("favorites file is not valid JSON"));
    if (!document.isArray())
        throw FavoritesError(QStringLiteral("favorites file is not a JSON list"));
    const QJsonArray array = document.array();
    if (array.size() > MAX_ENTRIES)
        throw FavoritesError(QStringLiteral("favorites list exceeds the %1-entry limit").arg(MAX_ENTRIES));
    QVariantList result;
    QSet<QString> seen;
    for (const QJsonValue &value : array) {
        if (!value.isString())
            throw FavoritesError(QStringLiteral("favorites file contains a non-string entry"));
        const QString anchor = clean(value.toString());
        if (seen.contains(anchor))
            throw FavoritesError(QStringLiteral("favorites file contains duplicate entries"));
        seen.insert(anchor);
        result.append(anchor);
    }
    return result;
}

QVariantList FavoritesStore::add(const QString &reference)
{
    const QString ref = clean(reference);
    QVariantList anchors;
    anchors.append(ref);
    for (const QVariant &value : list()) {
        const QString anchor = value.toString();
        if (anchor != ref)
            anchors.append(anchor);
    }
    if (anchors.size() > MAX_ENTRIES)
        anchors = anchors.mid(0, MAX_ENTRIES);
    write(anchors);
    return anchors;
}

QVariantList FavoritesStore::remove(const QString &reference)
{
    const QString ref = clean(reference);
    QVariantList anchors;
    for (const QVariant &value : list()) {
        const QString anchor = value.toString();
        if (anchor != ref)
            anchors.append(anchor);
    }
    write(anchors);
    return anchors;
}

QVariantList FavoritesStore::clear()
{
    write({});
    return {};
}

void FavoritesStore::write(const QVariantList &anchors)
{
    if (!QDir().mkpath(m_dir))
        throw FavoritesError(QStringLiteral("could not create favorites directory: %1").arg(m_dir));
    QJsonArray array;
    for (const QVariant &value : anchors) {
        if (!value.isValid() || value.typeId() != QMetaType::QString)
            throw FavoritesError(QStringLiteral("favorites state contains a non-string entry"));
        array.append(clean(value.toString()));
    }
    QByteArray payload = QJsonDocument(array).toJson(QJsonDocument::Compact);
    payload.append('\n');
    if (payload.size() > MAX_JSON_BYTES)
        throw FavoritesError(QStringLiteral("favorites list exceeds the %1-byte limit").arg(MAX_JSON_BYTES));
    QSaveFile save(m_path);
    if (!save.open(QIODevice::WriteOnly))
        throw FavoritesError(QStringLiteral("favorites file refused write: %1").arg(save.errorString()));
    if (save.write(payload) != payload.size() || !save.commit())
        throw FavoritesError(QStringLiteral("favorites file write failed"));
}
