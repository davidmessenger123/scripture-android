#ifndef SCRIPTURE_REFERENCES_H
#define SCRIPTURE_REFERENCES_H

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace References {

constexpr int MAX_RESPONSE_BYTES = 262144;
constexpr int MAX_REFERENCE_BYTES = 120;

class Deck
{
public:
    Deck();
    QString draw(const QString &avoid = QString());
    void setFilters(const QString &book, const QString &topic);
    QStringList references() const;

private:
    void shuffle();
    QList<QString> m_deck;
    int m_pos = 0;
};

struct Passage
{
    QString before;
    QString focal;
    QString after;
};

QString normalizeReference(const QString &reference);
bool isValidReference(const QString &reference);
QStringList allReferences();
QStringList bookOptions();
QStringList topicOptions();
QString bookName(const QString &reference);
QStringList referencesForBook(const QString &book);
QStringList referencesForTopic(const QString &topic);
QString rangeQuery(const QString &reference);
int focalVerse(const QString &reference);
Passage parseNumberedPassage(const QString &text, int focal);
Passage parseWebPassage(const QJsonObject &payload, int focal);
QString composeRichText(const QString &before, const QString &focal,
                        const QString &after, int maxChars);
QString browserUrl(const QString &reference, const QString &translationId);
QString encodeReference(const QString &reference);
QString referenceText(const QJsonObject &payload);
QString translationText(const QJsonObject &payload);

}

#endif
