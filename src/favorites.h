#ifndef SCRIPTURE_FAVORITES_H
#define SCRIPTURE_FAVORITES_H

#include <QString>
#include <QVariantList>

#include <stdexcept>

// User favorites persistence, faithful to the desktop app's favorites.py:
// references travel as plain strings, the list is capped, and writes go through
// an atomic replace so a partially-written store can never be observed. The
// file lives in the platform app-data directory.
class FavoritesError : public std::runtime_error
{
public:
    explicit FavoritesError(const QString &message)
        : std::runtime_error(message.toStdString())
    {
    }
};

class FavoritesStore
{
public:
    explicit FavoritesStore(const QString &dataDir);

    QString path() const;

    // Creates the data directory on demand. Throws FavoritesError on failure.
    void ensureDir();

    // Throws FavoritesError on any read/validation problem.
    QVariantList list();

    QVariantList add(const QString &reference);
    QVariantList remove(const QString &reference);
    QVariantList clear();

private:
    QString clean(const QString &reference) const;
    void write(const QVariantList &anchors);

    QString m_dir;
    QString m_path;
};

#endif // SCRIPTURE_FAVORITES_H