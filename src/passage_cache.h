#ifndef SCRIPTURE_PASSAGE_CACHE_H
#define SCRIPTURE_PASSAGE_CACHE_H

#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>
#include <exception>
#include <string>

struct PassageCacheEntry
{
    QString provider;
    QString requestedReference;
    QString rangeReference;
    int focalVerse = 0;
    QString before;
    QString focal;
    QString after;
    QString reference;
    QString translationId;
    QString translationName;
    QString attribution;
    QString keyFingerprint;
    qint64 storedAt = 0;
};

class PassageCacheError : public std::exception
{
public:
    explicit PassageCacheError(const QString &message)
        : m_message(message.toStdString())
    {
    }

    const char *what() const noexcept override
    {
        return m_message.c_str();
    }

private:
    std::string m_message;
};

class PassageCache
{
public:
    static constexpr int SchemaVersion = 2;
    static constexpr int MaxEntries = 256;
    static constexpr int MaxJsonBytes = 1024 * 1024;
    static constexpr int MaxEntryBytes = 131072;
    static constexpr qint64 MaxAgeSeconds = 60 * 60 * 24 * 30;

    explicit PassageCache(const QString &dataDir);

    QString path() const;
    void ensureDir() const;
    void compact();
    std::optional<PassageCacheEntry> get(const QString &provider,
                                          const QString &requestedReference,
                                          const QString &rangeReference,
                                          int focalVerse,
                                          const QString &keyFingerprint = QString()) const;
    void put(const PassageCacheEntry &entry);
    int count() const;
    static bool isValid(const PassageCacheEntry &entry);

private:
    QList<PassageCacheEntry> read() const;
    void write(const QList<PassageCacheEntry> &entries) const;
    static QJsonObject toJson(const PassageCacheEntry &entry);
    static PassageCacheEntry fromJson(const QJsonObject &object);
    static bool isFresh(const PassageCacheEntry &entry, qint64 now);

    QString m_dir;
    QString m_path;
};

#endif
