#ifndef SCRIPTURE_FETCHER_H
#define SCRIPTURE_FETCHER_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;

bool shouldRetryRange(const QString &provider, const QString &rangeReference,
                      const QString &anchorReference, const QByteArray &body,
                      int httpStatus, bool alreadyRetried = false);

class Fetcher : public QObject
{
    Q_OBJECT
public:
    explicit Fetcher(QObject *parent = nullptr);

    void fetchEsv(int tag, const QString &reference, const QString &apiKey);
    void fetchWeb(int tag, const QString &reference, const QString &translation);
    void abort();

signals:
    void esvResult(int tag, const QString &body, const QString &error, int status);
    void webResult(int tag, const QString &body, const QString &error, int status);

private:
    void drain(QNetworkReply *reply, bool requireActive = true);

private slots:
    void onFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_net;
    QNetworkReply *m_activeReply = nullptr;
    QByteArray m_buffer;
    qint64 m_received = 0;
    QString m_failure;
};

#endif
