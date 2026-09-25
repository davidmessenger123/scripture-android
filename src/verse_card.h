#ifndef SCRIPTURE_VERSE_CARD_H
#define SCRIPTURE_VERSE_CARD_H

#include <QImage>
#include <QString>

struct VerseCardContent
{
    QString before;
    QString focal;
    QString after;
    QString reference;
    QString translationId;
    QString translationName;
};

class VerseCardRenderer
{
public:
    static QString plainText(const VerseCardContent &content);
    static QString fileName(const VerseCardContent &content);
    static QImage render(const VerseCardContent &content);
    static bool save(const VerseCardContent &content, const QString &dataDir,
                     QString *path = nullptr, QString *error = nullptr);
};

#endif
