#ifndef SCRIPTURE_SECRETS_H
#define SCRIPTURE_SECRETS_H

#include <QString>

class SecureStore
{
public:
    explicit SecureStore(const QString &dataDir);

    QString load(QString *error = nullptr) const;
    bool save(const QString &value, QString *error = nullptr) const;
    bool clear(QString *error = nullptr) const;

private:
    QString loadFile(QString *error) const;
    bool saveFile(const QString &value, QString *error) const;
    bool clearFile(QString *error) const;

    QString m_dir;
    QString m_path;
};

#endif
