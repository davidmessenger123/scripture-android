#include "passage_cache.h"

#include "references.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>
#include <QRegularExpression>

namespace {
bool boundedText(const QString &value, int maximum)
{
    if (value.toUtf8().size() > maximum)
        return false;
    for (const QChar character : value) {
        if (character == QChar(127)
            || (character.unicode() < 32 && character != QLatin1Char('\n')
                && character != QLatin1Char('\r') && character != QLatin1Char('\t')))
            return false;
    }
    return true;
}

bool exactFields(const QJsonObject &object, const QStringList &fields)
{
    if (object.size() != fields.size())
        return false;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!fields.contains(it.key()))
            return false;
    }
    return true;
}

QString passageBase(const QString &reference)
{
    const int separator = reference.lastIndexOf(QLatin1Char(':'));
    return separator > 0 ? reference.left(separator) : QString();
}

bool validFingerprint(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{24}$"));
    return expression.match(value).hasMatch();
}

bool sameKey(const PassageCacheEntry &left, const PassageCacheEntry &right)
{
    return left.provider == right.provider
        && left.requestedReference == right.requestedReference
        && left.rangeReference == right.rangeReference
        && left.focalVerse == right.focalVerse
        && left.keyFingerprint == right.keyFingerprint;
}
}

PassageCache::PassageCache(const QString &dataDir)
    : m_dir(dataDir)
    , m_path(QDir(dataDir).filePath(QStringLiteral("passage-cache.json")))
{
}

QString PassageCache::path() const
{
    return m_path;
}

void PassageCache::ensureDir() const
{
    if (m_dir.isEmpty() || !QDir().mkpath(m_dir))
        throw PassageCacheError(QStringLiteral("could not create passage cache directory"));
}

void PassageCache::compact()
{
    if (!QFile::exists(m_path))
        return;
    QList<PassageCacheEntry> entries;
    try {
        entries = read();
    } catch (const PassageCacheError &) {
        return;
    }
    write(entries);
}

bool PassageCache::isValid(const PassageCacheEntry &entry)
{
    if (entry.provider != QLatin1String("esv")
        && entry.provider != QLatin1String("web")
        && entry.provider != QLatin1String("kjv"))
        return false;
    if (entry.translationId != entry.provider)
        return false;
    if (entry.provider == QLatin1String("esv") ? !validFingerprint(entry.keyFingerprint)
                                               : !entry.keyFingerprint.isEmpty())
        return false;
    if (References::normalizeReference(entry.requestedReference) != entry.requestedReference
        || References::normalizeReference(entry.rangeReference) != entry.rangeReference
        || References::normalizeReference(entry.reference) != entry.reference)
        return false;
    if (References::rangeQuery(entry.requestedReference) != entry.rangeReference
        || entry.focalVerse != References::focalVerse(entry.requestedReference))
        return false;
    const QString base = passageBase(entry.requestedReference);
    if (base.isEmpty() || passageBase(entry.rangeReference) != base
        || passageBase(entry.reference) != base)
        return false;
    if (entry.focalVerse > 0
        && !entry.focal.contains(QStringLiteral("[%1]").arg(entry.focalVerse)))
        return false;
    if (entry.storedAt <= 0 || entry.focal.trimmed().isEmpty()
        || entry.translationName.trimmed().isEmpty())
        return false;
    if (!boundedText(entry.before, MaxEntryBytes)
        || !boundedText(entry.focal, MaxEntryBytes)
        || !boundedText(entry.after, MaxEntryBytes)
        || !boundedText(entry.translationName, 1024)
        || !boundedText(entry.attribution, 16384))
        return false;
    const int total = entry.before.toUtf8().size()
        + entry.focal.toUtf8().size()
        + entry.after.toUtf8().size()
        + entry.translationName.toUtf8().size()
        + entry.attribution.toUtf8().size();
    return total <= MaxEntryBytes;
}

QJsonObject PassageCache::toJson(const PassageCacheEntry &entry)
{
    return QJsonObject{
        {QStringLiteral("provider"), entry.provider},
        {QStringLiteral("requestedReference"), entry.requestedReference},
        {QStringLiteral("rangeReference"), entry.rangeReference},
        {QStringLiteral("focalVerse"), entry.focalVerse},
        {QStringLiteral("before"), entry.before},
        {QStringLiteral("focal"), entry.focal},
        {QStringLiteral("after"), entry.after},
        {QStringLiteral("reference"), entry.reference},
        {QStringLiteral("translationId"), entry.translationId},
        {QStringLiteral("translationName"), entry.translationName},
        {QStringLiteral("attribution"), entry.attribution},
        {QStringLiteral("keyFingerprint"), entry.keyFingerprint},
        {QStringLiteral("storedAt"), entry.storedAt}
    };
}

PassageCacheEntry PassageCache::fromJson(const QJsonObject &object)
{
    PassageCacheEntry entry;
    entry.provider = object.value(QStringLiteral("provider")).toString();
    entry.requestedReference = object.value(QStringLiteral("requestedReference")).toString();
    entry.rangeReference = object.value(QStringLiteral("rangeReference")).toString();
    entry.focalVerse = object.value(QStringLiteral("focalVerse")).toInt(-1);
    entry.before = object.value(QStringLiteral("before")).toString();
    entry.focal = object.value(QStringLiteral("focal")).toString();
    entry.after = object.value(QStringLiteral("after")).toString();
    entry.reference = object.value(QStringLiteral("reference")).toString();
    entry.translationId = object.value(QStringLiteral("translationId")).toString();
    entry.translationName = object.value(QStringLiteral("translationName")).toString();
    entry.attribution = object.value(QStringLiteral("attribution")).toString();
    entry.keyFingerprint = object.value(QStringLiteral("keyFingerprint")).toString();
    entry.storedAt = static_cast<qint64>(object.value(QStringLiteral("storedAt")).toDouble(-1));
    return entry;
}

bool PassageCache::isFresh(const PassageCacheEntry &entry, qint64 now)
{
    return entry.storedAt > 0 && entry.storedAt <= now + 300
        && now - entry.storedAt <= MaxAgeSeconds;
}

QList<PassageCacheEntry> PassageCache::read() const
{
    QFile file(m_path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        throw PassageCacheError(QStringLiteral("passage cache refused open"));
    const QByteArray data = file.read(MaxJsonBytes + 1);
    if (data.size() > MaxJsonBytes)
        throw PassageCacheError(QStringLiteral("passage cache exceeds its size limit"));
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        throw PassageCacheError(QStringLiteral("passage cache is not valid JSON"));
    const QJsonObject root = document.object();
    if (!exactFields(root, {QStringLiteral("version"), QStringLiteral("entries")})
        || root.value(QStringLiteral("version")).toInt(-1) != SchemaVersion
        || !root.value(QStringLiteral("entries")).isArray())
        throw PassageCacheError(QStringLiteral("passage cache has an invalid schema"));
    const QJsonArray array = root.value(QStringLiteral("entries")).toArray();
    if (array.size() > MaxEntries)
        throw PassageCacheError(QStringLiteral("passage cache exceeds its entry limit"));
    const QStringList fields = {
        QStringLiteral("provider"), QStringLiteral("requestedReference"),
        QStringLiteral("rangeReference"), QStringLiteral("focalVerse"),
        QStringLiteral("before"), QStringLiteral("focal"), QStringLiteral("after"),
        QStringLiteral("reference"), QStringLiteral("translationId"),
        QStringLiteral("translationName"), QStringLiteral("attribution"),
        QStringLiteral("keyFingerprint"), QStringLiteral("storedAt")
    };
    QList<PassageCacheEntry> entries;
    QSet<QString> keys;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            throw PassageCacheError(QStringLiteral("passage cache contains an invalid entry"));
        const QJsonObject object = value.toObject();
        if (!exactFields(object, fields))
            throw PassageCacheError(QStringLiteral("passage cache contains unexpected data"));
        const PassageCacheEntry entry = fromJson(object);
        if (!isValid(entry))
            throw PassageCacheError(QStringLiteral("passage cache contains an invalid entry"));
        if (!isFresh(entry, now))
            continue;
        const QString key = entry.provider + QLatin1Char('\x1f')
            + entry.requestedReference + QLatin1Char('\x1f')
            + entry.rangeReference + QLatin1Char('\x1f')
            + QString::number(entry.focalVerse) + QLatin1Char('\x1f')
            + entry.keyFingerprint;
        if (keys.contains(key))
            throw PassageCacheError(QStringLiteral("passage cache contains duplicate entries"));
        keys.insert(key);
        entries.append(entry);
    }
    return entries;
}

void PassageCache::write(const QList<PassageCacheEntry> &entries) const
{
    ensureDir();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QList<PassageCacheEntry> bounded;
    for (const PassageCacheEntry &entry : entries) {
        if (isFresh(entry, now))
            bounded.append(entry);
    }
    if (bounded.size() > MaxEntries)
        bounded = bounded.mid(0, MaxEntries);
    QJsonArray array;
    for (const PassageCacheEntry &entry : bounded) {
        if (!isValid(entry))
            throw PassageCacheError(QStringLiteral("passage cache entry is invalid"));
        array.append(toJson(entry));
    }
    QJsonObject root{{QStringLiteral("version"), SchemaVersion}, {QStringLiteral("entries"), array}};
    QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    while (payload.size() > MaxJsonBytes && !array.isEmpty()) {
        array.removeLast();
        root[QStringLiteral("entries")] = array;
        payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    }
    if (payload.size() > MaxJsonBytes)
        throw PassageCacheError(QStringLiteral("passage cache entry exceeds its size limit"));
    payload.append('\n');
    if (payload.size() > MaxJsonBytes)
        throw PassageCacheError(QStringLiteral("passage cache exceeds its size limit"));
    QSaveFile file(m_path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
        throw PassageCacheError(QStringLiteral("passage cache refused write"));
    if (file.write(payload) != payload.size() || !file.commit())
        throw PassageCacheError(QStringLiteral("passage cache write failed"));
    QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

std::optional<PassageCacheEntry> PassageCache::get(const QString &provider,
                                                       const QString &requestedReference,
                                                       const QString &rangeReference,
                                                       int focalVerse,
                                                       const QString &keyFingerprint) const
{
    if ((provider != QLatin1String("esv") && provider != QLatin1String("web")
         && provider != QLatin1String("kjv"))
        || References::normalizeReference(requestedReference) != requestedReference
        || References::normalizeReference(rangeReference) != rangeReference
        || References::rangeQuery(requestedReference) != rangeReference
        || focalVerse != References::focalVerse(requestedReference)
        || (provider == QLatin1String("esv") ? !validFingerprint(keyFingerprint)
                                             : !keyFingerprint.isEmpty()))
        return std::nullopt;
    const QList<PassageCacheEntry> entries = read();
    for (const PassageCacheEntry &entry : entries) {
        if (entry.provider == provider && entry.requestedReference == requestedReference
            && entry.rangeReference == rangeReference && entry.focalVerse == focalVerse
            && entry.keyFingerprint == keyFingerprint)
            return entry;
    }
    return std::nullopt;
}

void PassageCache::put(const PassageCacheEntry &entry)
{
    PassageCacheEntry current = entry;
    if (current.storedAt <= 0)
        current.storedAt = QDateTime::currentSecsSinceEpoch();
    if (!isValid(current))
        throw PassageCacheError(QStringLiteral("passage cache entry is invalid"));
    QList<PassageCacheEntry> entries;
    try {
        entries = read();
    } catch (const PassageCacheError &) {
        entries.clear();
    }
    for (qsizetype index = entries.size() - 1; index >= 0; --index) {
        if (sameKey(current, entries.at(index)))
            entries.removeAt(index);
    }
    entries.prepend(current);
    write(entries);
}

int PassageCache::count() const
{
    return static_cast<int>(read().size());
}
