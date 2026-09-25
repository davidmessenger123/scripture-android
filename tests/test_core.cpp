#include "favorites.h"
#include "fetcher.h"
#include "passage_cache.h"
#include "references.h"
#include "secrets.h"
#include "verse_card.h"

#include <QGuiApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>

namespace {
void require(bool condition)
{
    if (!condition)
        std::abort();
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    require(References::normalizeReference(QStringLiteral("  1   John   3:16  ")) == QStringLiteral("1 John 3:16"));
    require(References::rangeQuery(QStringLiteral("John 3:16")) == QStringLiteral("John 3:14-18"));
    require(References::focalVerse(QStringLiteral("John 3:16")) == 16);
    require(References::encodeReference(QStringLiteral("John 3:16")) == QStringLiteral("John%203%3A16"));
    require(References::browserUrl(QStringLiteral("1 John 1:7"), QStringLiteral("web")).contains(QStringLiteral("1%20John%201%3A7")));
    require(References::browserUrl(QStringLiteral("John 3:16&x=y"), QStringLiteral("web")).isEmpty());
    require(References::normalizeReference(QStringLiteral("John 3:9-8")).isEmpty());
    require(References::normalizeReference(QStringLiteral("John\n3:16")).isEmpty());
    const QStringList allReferences = References::allReferences();
    require(allReferences.size() == 185);
    require(References::bookOptions().contains(QStringLiteral("John")));
    require(References::topicOptions().contains(QStringLiteral("Faith")));
    for (const QString &topic : References::topicOptions()) {
        const QStringList topicReferences = References::referencesForTopic(topic);
        require(!topicReferences.isEmpty());
        for (const QString &reference : topicReferences)
            require(allReferences.contains(reference));
    }
    References::Deck filteredDeck;
    filteredDeck.setFilters(QStringLiteral("John"), QStringLiteral("Faith"));
    require(!filteredDeck.references().isEmpty());
    for (const QString &reference : filteredDeck.references()) {
        require(References::bookName(reference) == QStringLiteral("John"));
        require(allReferences.contains(reference));
    }
    filteredDeck.setFilters(QStringLiteral("Genesis"), QStringLiteral("Faith"));
    require(filteredDeck.references().isEmpty());
    require(filteredDeck.draw().isEmpty());

    const QByteArray unsupported = R"({"error":"Passage ranges are not supported"})";
    require(shouldRetryRange(QStringLiteral("web"), QStringLiteral("John 3:14-18"),
                             QStringLiteral("John 3:16"), unsupported, 400));
    require(!shouldRetryRange(QStringLiteral("web"), QStringLiteral("John 3:14-18"),
                              QStringLiteral("John 3:16"), R"({"error":"Unauthorized"})", 401));
    require(!shouldRetryRange(QStringLiteral("web"), QStringLiteral("John 3:14-18"),
                              QStringLiteral("John 3:16"), unsupported, 0));
    require(!shouldRetryRange(QStringLiteral("web"), QStringLiteral("John 3:14-18"),
                              QStringLiteral("John 3:16"), "not-json", 400));
    require(!shouldRetryRange(QStringLiteral("web"), QStringLiteral("John 3:14-18"),
                              QStringLiteral("John 3:16"), R"({"verses":[]})", 400));
    require(!shouldRetryRange(QStringLiteral("web"), QStringLiteral("John 3:16"),
                              QStringLiteral("John 3:16"), unsupported, 400));
    require(isTransientFetchFailure(QStringLiteral("network error: offline"), 0));
    require(isTransientFetchFailure(QStringLiteral("server returned HTTP 503"), 503));
    require(!isTransientFetchFailure(QStringLiteral("server returned HTTP 401"), 401));
    require(!isTransientFetchFailure(QString(), 200));
    const QByteArray esvUnsupported = R"({"code":4001,"message":"Passage ranges are not supported"})";
    require(shouldRetryRange(QStringLiteral("esv"), QStringLiteral("John 3:14-18"),
                             QStringLiteral("John 3:16"), esvUnsupported, 400));
    require(!shouldRetryRange(QStringLiteral("esv"), QStringLiteral("John 3:14-18"),
                              QStringLiteral("John 3:16"),
                              R"({"message":"Passage ranges are not supported","detail":"Invalid token"})", 400));
    require(!shouldRetryRange(QStringLiteral("esv"), QStringLiteral("John 3:14-18"),
                              QStringLiteral("John 3:16"), esvUnsupported, 500));
    require(!shouldRetryRange(QStringLiteral("esv"), QStringLiteral("John 3:14-18"),
                              QStringLiteral("John 3:16"), esvUnsupported, 400, true));
    const QJsonObject web = QJsonDocument::fromJson(R"({"verses":[{"verse":16,"text":"For God so loved"}]})").object();
    const References::Passage passage = References::parseWebPassage(web, 16);
    require(passage.focal.contains(QStringLiteral("For God so loved")));

    QTemporaryDir directory;
    require(directory.isValid());

    PassageCache cache(directory.path());
    cache.ensureDir();
    PassageCacheEntry cached;
    cached.provider = QStringLiteral("web");
    cached.requestedReference = QStringLiteral("John 3:16");
    cached.rangeReference = QStringLiteral("John 3:14-18");
    cached.focalVerse = 16;
    cached.before = QStringLiteral("[14] Before");
    cached.focal = QStringLiteral("[16] For God so loved");
    cached.after = QStringLiteral("[18] After");
    cached.reference = QStringLiteral("John 3:14-18");
    cached.translationId = QStringLiteral("web");
    cached.translationName = QStringLiteral("World English Bible");
    const QString legalAttribution = QStringLiteral("Scripture quotations are from the ESV® Bible (The Holy Bible, English Standard Version®), © 2001 by Crossway. Used by permission. All rights reserved. esv.org");
    cached.attribution = legalAttribution;
    cache.put(cached);
    const std::optional<PassageCacheEntry> loaded = cache.get(
        cached.provider, cached.requestedReference, cached.rangeReference, cached.focalVerse);
    require(loaded.has_value());
    require(loaded->focal == cached.focal);
    require(loaded->attribution == legalAttribution);
    PassageCacheEntry esvEntry = cached;
    esvEntry.provider = QStringLiteral("esv");
    esvEntry.translationId = QStringLiteral("esv");
    esvEntry.translationName = QStringLiteral("English Standard Version");
    esvEntry.keyFingerprint = QStringLiteral("0123456789abcdef01234567");
    cache.put(esvEntry);
    require(cache.get(esvEntry.provider, esvEntry.requestedReference,
                      esvEntry.rangeReference, esvEntry.focalVerse,
                      esvEntry.keyFingerprint).has_value());
    require(!cache.get(esvEntry.provider, esvEntry.requestedReference,
                       esvEntry.rangeReference, esvEntry.focalVerse).has_value());
    require(!cache.get(esvEntry.provider, esvEntry.requestedReference,
                       esvEntry.rangeReference, esvEntry.focalVerse,
                       QStringLiteral("fedcba9876543210fedcba98")).has_value());
    QFile cacheFile(cache.path());
    require(cacheFile.open(QIODevice::ReadOnly));
    const QByteArray cacheBytes = cacheFile.readAll();
    cacheFile.close();
    require(!cacheBytes.contains("apiKey"));
    require(!cacheBytes.contains("authorization"));
    QJsonObject cacheRoot = QJsonDocument::fromJson(cacheBytes).object();
    QJsonObject cacheEntry = cacheRoot.value(QStringLiteral("entries")).toArray().first().toObject();
    cacheEntry.insert(QStringLiteral("apiKey"), QStringLiteral("secret"));
    QJsonArray invalidEntries;
    invalidEntries.append(cacheEntry);
    QFile::remove(cache.path());
    QFile invalidCacheFile(cache.path());
    require(invalidCacheFile.open(QIODevice::WriteOnly));
    const QByteArray invalidPayload = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), PassageCache::SchemaVersion}, {QStringLiteral("entries"), invalidEntries}}).toJson(QJsonDocument::Compact);
    require(invalidCacheFile.write(invalidPayload) == invalidPayload.size());
    invalidCacheFile.close();
    bool cacheRejected = false;
    try {
        cache.get(cached.provider, cached.requestedReference, cached.rangeReference, cached.focalVerse);
    } catch (const PassageCacheError &) {
        cacheRejected = true;
    }
    require(cacheRejected);
    QJsonObject missingEntry = cacheEntry;
    missingEntry.remove(QStringLiteral("apiKey"));
    missingEntry.remove(QStringLiteral("translationName"));
    QJsonArray missingEntries;
    missingEntries.append(missingEntry);
    QFile missingCacheFile(cache.path());
    require(missingCacheFile.open(QIODevice::WriteOnly));
    const QByteArray missingPayload = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), PassageCache::SchemaVersion}, {QStringLiteral("entries"), missingEntries}}).toJson(QJsonDocument::Compact);
    require(missingCacheFile.write(missingPayload) == missingPayload.size());
    missingCacheFile.close();
    bool missingRejected = false;
    try {
        cache.get(cached.provider, cached.requestedReference, cached.rangeReference, cached.focalVerse);
    } catch (const PassageCacheError &) {
        missingRejected = true;
    }
    require(missingRejected);
    QFile::remove(cache.path());
    cache.put(cached);
    for (int index = 0; index < PassageCache::MaxEntries + 1; ++index) {
        PassageCacheEntry bounded = cached;
        bounded.requestedReference = QStringLiteral("Cache 1:%1").arg(index + 1);
        bounded.focalVerse = index + 1;
        bounded.rangeReference = References::rangeQuery(bounded.requestedReference);
        bounded.reference = bounded.rangeReference;
        bounded.focal = QStringLiteral("[%1] Cached").arg(index + 1);
        cache.put(bounded);
    }
    require(cache.count() == PassageCache::MaxEntries);
    PassageCacheEntry invalidReference = cached;
    invalidReference.rangeReference = QStringLiteral("John 3:15-17");
    invalidReference.storedAt = QDateTime::currentSecsSinceEpoch();
    require(!PassageCache::isValid(invalidReference));
    PassageCacheEntry invalidProvider = cached;
    invalidProvider.provider = QStringLiteral("other");
    invalidProvider.storedAt = QDateTime::currentSecsSinceEpoch();
    require(!PassageCache::isValid(invalidProvider));
    PassageCacheEntry stale = cached;
    stale.requestedReference = QStringLiteral("John 3:17");
    stale.rangeReference = QStringLiteral("John 3:15-19");
    stale.focalVerse = 17;
    stale.focal = QStringLiteral("[17] Stale");
    stale.reference = QStringLiteral("John 3:14-18");
    stale.storedAt = QDateTime::currentSecsSinceEpoch() - PassageCache::MaxAgeSeconds - 1;
    cache.put(stale);
    require(cache.count() == PassageCache::MaxEntries);

    const VerseCardContent cardContent{cached.before, cached.focal, cached.after,
                                        cached.reference, cached.translationId,
                                        cached.translationName};
    const QString userFacingText = VerseCardRenderer::plainText(cardContent);
    require(!userFacingText.contains(legalAttribution));
    require(!userFacingText.contains(QStringLiteral("<span")));
    require(userFacingText.contains(cached.focal));
    require(userFacingText.contains(cached.reference));
    require(userFacingText.contains(cached.translationName));
    QString cardPath;
    QString cardError;
    require(VerseCardRenderer::save(cardContent, directory.path(), &cardPath, &cardError));
    QImage firstCard(cardPath);
    require(!firstCard.isNull());
    require(firstCard.width() == 1080 && firstCard.height() == 1350);
    QFile cardFile(cardPath);
    require(cardFile.open(QIODevice::ReadOnly));
    const QByteArray savedCardBytes = cardFile.readAll();
    require(VerseCardRenderer::save(cardContent, directory.path(), &cardPath, &cardError));
    cardFile.close();
    require(cardFile.open(QIODevice::ReadOnly));
    require(cardFile.readAll() == savedCardBytes);

    FavoritesStore favorites(directory.path());
    favorites.ensureDir();
    require(favorites.add(QStringLiteral("John 3:16")).size() == 1);
    QFile invalid(directory.filePath(QStringLiteral("favorites.json")));
    require(invalid.open(QIODevice::WriteOnly));
    invalid.write("{\"not\":\"a-list\"}");
    invalid.close();
    bool rejected = false;
    try {
        favorites.list();
    } catch (const FavoritesError &) {
        rejected = true;
    }
    require(rejected);

    SecureStore secrets(directory.path());
    QString error;
    require(secrets.save(QStringLiteral("key-value"), &error));
    require(secrets.load(&error) == QStringLiteral("key-value"));
    require(!secrets.save(QString(513, QLatin1Char('x')), &error));
    require(!secrets.save(QStringLiteral("bad\nkey"), &error));
    require(secrets.load(&error) == QStringLiteral("key-value"));
    require(secrets.clear(&error));
    return 0;
}
