#ifndef SCRIPTURE_FETCHER_H
#define SCRIPTURE_FETCHER_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

// Network fetch layer, faithful to the desktop app's fetcher.py: talks directly
// to api.esv.org (ESV, key required) and bible-api.com (WEB/KJV, keyless) over
// QNetworkAccessManager. Responses are bounded and requests carry a transfer
// timeout, mirroring the plugin's guards.
class Fetcher : public QObject
{
    Q_OBJECT
public:
    explicit Fetcher(QObject *parent = nullptr);

    // The `tag` argument round-trips a caller-generated token so the controller
    // can drop stale replies (e.g. after the user spams "Another Verse").
    void fetchEsv(int tag, const QString &reference, const QString &apiKey);
    void fetchWeb(int tag, const QString &reference, const QString &translation);

signals:
    void esvResult(int tag, const QString &body, const QString &error);
    void webResult(int tag, const QString &body, const QString &error);

private slots:
    void onFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_net;
};

#endif // SCRIPTURE_FETCHER_H