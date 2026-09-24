#include "favorites.h"
#include "fetcher.h"
#include "references.h"
#include "secrets.h"

#include <QCoreApplication>
#include <QFile>
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
    QCoreApplication app(argc, argv);
    require(References::normalizeReference(QStringLiteral("  1   John   3:16  ")) == QStringLiteral("1 John 3:16"));
    require(References::rangeQuery(QStringLiteral("John 3:16")) == QStringLiteral("John 3:14-18"));
    require(References::focalVerse(QStringLiteral("John 3:16")) == 16);
    require(References::encodeReference(QStringLiteral("John 3:16")) == QStringLiteral("John%203%3A16"));
    require(References::browserUrl(QStringLiteral("1 John 1:7"), QStringLiteral("web")).contains(QStringLiteral("1%20John%201%3A7")));
    require(References::browserUrl(QStringLiteral("John 3:16&x=y"), QStringLiteral("web")).isEmpty());
    require(References::normalizeReference(QStringLiteral("John 3:9-8")).isEmpty());
    require(References::normalizeReference(QStringLiteral("John\n3:16")).isEmpty());

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
