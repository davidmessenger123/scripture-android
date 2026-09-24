#include "fetcher.h"

#include "references.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
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
    "&include-copyright=true"
    "&include-short-copyright=false"
    "&include-passage-references=false";

#ifndef SCRIPTURE_VERSION
#define SCRIPTURE_VERSION "dev"
#endif

QString userAgent()
{
    return QStringLiteral("scripture-android/%1").arg(QLatin1String(SCRIPTURE_VERSION));
}

bool explicitRangeUnsupported(const QString &text)
{
    static const QRegularExpression unsupported(
        QStringLiteral("(?:unsupported[ _-]*range|range[ _-]*not[ _-]*supported|(?:passage[ _-]*)?ranges?[ _-]+(?:is|are)[ _-]+(?:unsupported|not[ _-]+supported)|(?:does|do)[ _-]+not[ _-]+support[ _-]+(?:passage[ _-]*)?ranges?)"),
        QRegularExpression::CaseInsensitiveOption);
    return unsupported.match(text).hasMatch();
}

bool validApiKey(const QString &value)
{
    if (value.isEmpty() || value.toUtf8().size() > 512)
        return false;
    for (const QChar character : value) {
        if (character.unicode() < 32 || character.unicode() == 127)
            return false;
    }
    return true;
}

bool safeNetworkUrl(const QUrl &url, const QString &host)
{
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && url.host() == host && url.userInfo().isEmpty() && url.fragment().isEmpty()
        && (url.port(-1) == -1 || url.port(-1) == 443);
}

bool authenticationFailure(const QString &text)
{
    static const QRegularExpression authentication(
        QStringLiteral("(?:unauthori[sz]ed|forbidden|authentication|authorization|invalid[ _-]*(?:api[ _-]*)?key|missing[ _-]*(?:api[ _-]*)?key|credential|token)"),
        QRegularExpression::CaseInsensitiveOption);
    return authentication.match(text).hasMatch();
}
}

bool shouldRetryRange(const QString &provider, const QString &rangeReference,
                      const QString &anchorReference, const QByteArray &body,
                      int httpStatus, bool alreadyRetried)
{
    if (alreadyRetried || (httpStatus != 400 && httpStatus != 404 && httpStatus != 422) ||
        body.isEmpty() || body.size() > References::MAX_RESPONSE_BYTES)
        return false;
    const QString normalizedRange = References::normalizeReference(rangeReference);
    const QString normalizedAnchor = References::normalizeReference(anchorReference);
    if (normalizedRange.isEmpty() || normalizedAnchor.isEmpty() || normalizedRange == normalizedAnchor ||
        !normalizedRange.contains(QRegularExpression(QStringLiteral(":\\d+-\\d+$"))))
        return false;
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return false;
    const QJsonObject payload = document.object();
    QStringList values;
    const QString providerId = provider.trimmed().toLower();
    if (providerId == QLatin1String("web")) {
        if (payload.contains(QStringLiteral("verses")) || !payload.value(QStringLiteral("error")).isString())
            return false;
        values.append(payload.value(QStringLiteral("error")).toString());
    } else if (providerId == QLatin1String("esv")) {
        if (payload.contains(QStringLiteral("passages")))
            return false;
        for (const QString &key : {QStringLiteral("code"), QStringLiteral("message"),
                                   QStringLiteral("detail"), QStringLiteral("error")}) {
            const QJsonValue value = payload.value(key);
            if (value.isString()) {
                values.append(value.toString());
            } else if (value.isObject()) {
                const QJsonObject object = value.toObject();
                for (const QString &nested : {QStringLiteral("code"), QStringLiteral("message"),
                                              QStringLiteral("detail")}) {
                    if (object.value(nested).isString())
                        values.append(object.value(nested).toString());
                }
            }
        }
    } else {
        return false;
    }
    if (values.isEmpty() || authenticationFailure(values.join(QLatin1Char(' '))))
        return false;
    for (const QString &value : values) {
        if (explicitRangeUnsupported(value))
            return true;
    }
    return false;
}

Fetcher::Fetcher(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    connect(m_net, &QNetworkAccessManager::finished, this, &Fetcher::onFinished);
}

void Fetcher::abort()
{
    QNetworkReply *reply = m_activeReply;
    if (!reply)
        return;
    m_activeReply = nullptr;
    m_buffer.clear();
    m_received = 0;
    reply->setProperty("_aborted", true);
    reply->abort();
}

void Fetcher::fetchEsv(int tag, const QString &reference, const QString &apiKey)
{
    abort();
    const QString key = apiKey.trimmed();
    const QString encoded = References::encodeReference(reference);
    const QString urlText = QStringLiteral("%1%2?q=%3&%4")
        .arg(QLatin1String(kEsvHost), QLatin1String(kEsvPath), encoded, QLatin1String(kEsvCommonQuery));
    const QUrl url(urlText);
    if (encoded.isEmpty() || !validApiKey(key) || !safeNetworkUrl(url, QStringLiteral("api.esv.org"))) {
        emit esvResult(tag, QString(), QStringLiteral("invalid verse request"), 0);
        return;
    }
    QNetworkRequest request{url};
    request.setTransferTimeout(TIMEOUT_MS);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setMaximumRedirectsAllowed(0);
    request.setRawHeader("Authorization", QByteArray("Token ") + key.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", userAgent().toUtf8());
    QNetworkReply *reply = m_net->get(request);
    m_buffer.clear();
    m_received = 0;
    m_failure.clear();
    reply->setProperty("_tag", tag);
    reply->setProperty("_kind", QLatin1String("esv"));
    reply->setProperty("_aborted", false);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() { drain(reply); });
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl &) {
        if (reply == m_activeReply) {
            m_failure = QStringLiteral("redirects are not allowed");
            reply->abort();
        }
    });
    m_activeReply = reply;
}

void Fetcher::fetchWeb(int tag, const QString &reference, const QString &translation)
{
    abort();
    QString tr = translation.trimmed().toLower();
    if (tr != QLatin1String("web") && tr != QLatin1String("kjv"))
        tr = QStringLiteral("web");
    const QString encoded = References::encodeReference(reference);
    const QUrl url = QStringLiteral("%1/%2?translation=%3")
        .arg(QLatin1String(kWebHost), encoded, tr);
    if (encoded.isEmpty() || !safeNetworkUrl(url, QStringLiteral("bible-api.com"))) {
        emit webResult(tag, QString(), QStringLiteral("invalid verse request"), 0);
        return;
    }
    QNetworkRequest request{url};
    request.setTransferTimeout(TIMEOUT_MS);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setMaximumRedirectsAllowed(0);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", userAgent().toUtf8());
    QNetworkReply *reply = m_net->get(request);
    m_buffer.clear();
    m_received = 0;
    m_failure.clear();
    reply->setProperty("_tag", tag);
    reply->setProperty("_kind", QLatin1String("web"));
    reply->setProperty("_aborted", false);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() { drain(reply); });
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl &) {
        if (reply == m_activeReply) {
            m_failure = QStringLiteral("redirects are not allowed");
            reply->abort();
        }
    });
    m_activeReply = reply;
}

void Fetcher::drain(QNetworkReply *reply, bool requireActive)
{
    if (requireActive && reply != m_activeReply)
        return;
    while (reply->bytesAvailable() > 0) {
        const qint64 remaining = static_cast<qint64>(References::MAX_RESPONSE_BYTES) + 1 - m_received;
        if (remaining <= 0) {
            m_failure = QStringLiteral("response exceeds the %1-byte limit")
                .arg(References::MAX_RESPONSE_BYTES);
            reply->abort();
            return;
        }
        const QByteArray chunk = reply->read(qMin<qint64>(65536, remaining));
        if (chunk.isEmpty())
            break;
        m_received += chunk.size();
        m_buffer += chunk;
        if (m_received > References::MAX_RESPONSE_BYTES) {
            m_failure = QStringLiteral("response exceeds the %1-byte limit")
                .arg(References::MAX_RESPONSE_BYTES);
            reply->abort();
            return;
        }
    }
}

void Fetcher::onFinished(QNetworkReply *reply)
{
    if (reply != m_activeReply) {
        reply->deleteLater();
        return;
    }
    m_activeReply = nullptr;
    if (reply->property("_aborted").toBool()) {
        reply->deleteLater();
        m_buffer.clear();
        m_received = 0;
        return;
    }
    drain(reply, false);
    const QString kind = reply->property("_kind").toString();
    const int tag = reply->property("_tag").toInt();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString error = m_failure;
    const QByteArray data = m_buffer;
    m_buffer.clear();
    m_received = 0;
    const qint64 declared = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    if (error.isEmpty() && status != 0) {
        if (status != 200) {
            error = QStringLiteral("server returned HTTP %1").arg(status);
            if (declared > 0 && declared != data.size())
                error = QStringLiteral("response size does not match Content-Length");
        } else if (declared > References::MAX_RESPONSE_BYTES) {
            error = QStringLiteral("response exceeds the %1-byte limit").arg(References::MAX_RESPONSE_BYTES);
        } else if (declared > 0 && declared != data.size()) {
            error = QStringLiteral("response size does not match Content-Length");
        }
    } else if (error.isEmpty() && reply->error() == QNetworkReply::OperationCanceledError) {
        error = QStringLiteral("request canceled");
    } else if (error.isEmpty()) {
        error = QStringLiteral("network error: %1").arg(reply->errorString());
    }
    reply->deleteLater();
    const bool retainErrorBody = status == 400 || status == 404 || status == 422;
    const QString body = error.isEmpty() || retainErrorBody ? QString::fromUtf8(data) : QString();
    if (kind == QLatin1String("esv"))
        emit esvResult(tag, body, error, status);
    else
        emit webResult(tag, body, error, status);
}
