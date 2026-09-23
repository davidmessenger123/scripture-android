#include "favorites.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {
constexpr int MAX_ANCHOR_BYTES = 120;
constexpr int MAX_ENTRIES = 200;
constexpr int MAX_JSON_BYTES = 65536;
} // namespace

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

static QString stripControlCharacters(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (const QChar c : value) {
        if (c.unicode() >= 32)
            out += c;
    }
    return out;
}

QString FavoritesStore::clean(const QString &reference) const
{
    const QString anchor = reference.trimmed();
    if (anchor.isEmpty())
        throw FavoritesError(QStringLiteral("no reference provided"));
    if (anchor.toUtf8().size() > MAX_ANCHOR_BYTES)
        throw FavoritesError(QStringLiteral("reference exceeds the %1-byte limit").arg(MAX_ANCHOR_BYTES));
    if (stripControlCharacters(anchor) != anchor)
        throw FavoritesError(QStringLiteral("reference contains control characters"));
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
    file.close();
    if (data.size() > MAX_JSON_BYTES)
        throw FavoritesError(QStringLiteral("favorites list exceeds the %1-byte limit").arg(MAX_JSON_BYTES));

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        throw FavoritesError(QStringLiteral("favorites file is not valid JSON"));
    if (!doc.isArray())
        throw FavoritesError(QStringLiteral("favorites file is not a JSON list"));

    QVariantList result;
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (value.isString())
            result.append(value.toString());
    }
    return result;
}

QVariantList FavoritesStore::add(const QString &reference)
{
    const QString ref = clean(reference);
    QVariantList anchors;
    anchors.append(ref);
    for (const QVariant &a : list()) {
        if (a.toString() != ref)
            anchors.append(a);
    }
    anchors = anchors.mid(0, MAX_ENTRIES);
    write(anchors);
    return anchors;
}

QVariantList FavoritesStore::remove(const QString &reference)
{
    const QString ref = clean(reference);
    QVariantList anchors;
    for (const QVariant &a : list()) {
        if (a.toString() != ref)
            anchors.append(a);
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
    for (const QVariant &a : anchors)
        array.append(a.toString());
    QByteArray payload = QJsonDocument(array).toJson(QJsonDocument::Compact);
    payload.append('\n');
    if (payload.size() > MAX_JSON_BYTES)
        throw FavoritesError(QStringLiteral("favorites list exceeds the %1-byte limit").arg(MAX_JSON_BYTES));

    // QSaveFile writes to a temp file and atomically renames it on commit, the
    // C++ equivalent of favorites.py's fsync-before-os.replace discipline.
    QSaveFile save(m_path);
    if (!save.open(QIODevice::WriteOnly)) {
        throw FavoritesError(QStringLiteral("favorites file refused write: %1").arg(save.errorString()));
    }
    save.write(payload);
    if (!save.commit())
        throw FavoritesError(QStringLiteral("favorites file write failed"));
}