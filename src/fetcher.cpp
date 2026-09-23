#include "fetcher.h"

#include "references.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
constexpr int TIMEOUT_MS = 15000;
const char kEsvHost[] = "https://api.esv.org";
const char kEsvPath[] = "/v3/passage/text/";
const char kWebHost[] = "https://bible-api.com";
const char kEsvCommonQuery[] =
    "include-headings=false"
    "&include-footnotes=false"
    "&include-verse-numbers=true"
    "&include-short-copyright=false"
    "&include-passage-references=false";
} // namespace

Fetcher::Fetcher(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    connect(m_net, &QNetworkAccessManager::finished, this, &Fetcher::onFinished);
}

void Fetcher::fetchEsv(int tag, const QString &reference, const QString &apiKey)
{
    const QString url = QStringLiteral("%1%2?q=%3&%4")
        .arg(QLatin1String(kEsvHost), QLatin1String(kEsvPath),
             References::encodeReference(reference), QLatin1String(kEsvCommonQuery));
    QNetworkRequest request{QUrl(url)};
    request.setTransferTimeout(TIMEOUT_MS);
    request.setRawHeader("Authorization", QByteArray("Token ") + apiKey.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "scripture-android/1.0");
    QNetworkReply *reply = m_net->get(request);
    reply->setProperty("_tag", tag);
    reply->setProperty("_kind", QLatin1String("esv"));
}

void Fetcher::fetchWeb(int tag, const QString &reference, const QString &translation)
{
    QString tr = translation.trimmed().toLower();
    if (tr != QLatin1String("web") && tr != QLatin1String("kjv"))
        tr = QStringLiteral("web");
    QString slug = reference;
    slug.replace(QLatin1Char(' '), QLatin1Char('+'));
    const QString url = QStringLiteral("%1/%2?translation=%3")
        .arg(QLatin1String(kWebHost), slug, tr);
    QNetworkRequest request{QUrl(url)};
    request.setTransferTimeout(TIMEOUT_MS);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "scripture-android/1.0");
    QNetworkReply *reply = m_net->get(request);
    reply->setProperty("_tag", tag);
    reply->setProperty("_kind", QLatin1String("web"));
}

void Fetcher::onFinished(QNetworkReply *reply)
{
    const QString kind = reply->property("_kind").toString();
    const int tag = reply->property("_tag").toInt();
    const QVariant statusAttribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int status = statusAttribute.isValid() ? statusAttribute.toInt() : 0;

    QByteArray data;
    QString error;
    if (reply->error() == QNetworkReply::NoError) {
        if (status >= 400) {
            error = QStringLiteral("server returned HTTP %1").arg(status);
        } else {
            data = reply->readAll();
            if (data.size() > References::MAX_RESPONSE_BYTES) {
                error = QStringLiteral("response exceeds the %1-byte limit")
                            .arg(References::MAX_RESPONSE_BYTES);
                data.clear();
            }
        }
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        error = QStringLiteral("timed out");
    } else {
        error = QStringLiteral("network error: %1").arg(reply->errorString());
    }
    reply->deleteLater();

    const QString body = data.isEmpty() ? QString() : QString::fromUtf8(data);
    if (kind == QLatin1String("esv"))
        emit esvResult(tag, body, error);
    else
        emit webResult(tag, body, error);
}