#ifndef SCRIPTURE_REFERENCES_H
#define SCRIPTURE_REFERENCES_H

#include <QJsonObject>
#include <QList>
#include <QString>

namespace References {

// Hard cap mirrors the plugin: a single short passage is a few KB, so this
// bounds a misbehaving endpoint without ever trimming a real response.
constexpr int MAX_RESPONSE_BYTES = 262144;
constexpr int MAX_REFERENCE_BYTES = 120;

// No-repeat rotation through the curated deck. A verse is not repeated until
// every reference has been drawn; a draw that follows a reshuffle can never
// equal the immediately previous reference.
class Deck
{
public:
    Deck();
    QString draw(const QString &avoid = QString());

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

// A curated anchor ('John 3:16') becomes a short context window
// ('John 3:14-18') so a tiny verse is never shown without context.
QString rangeQuery(const QString &reference);

// Which verse number inside a fetched range is the curated anchor? 0 = not derivable.
int focalVerse(const QString &reference);

// Split an ESV passage (with '[n]' verse markers) into (before, focal, after).
Passage parseNumberedPassage(const QString &text, int focal);

// Split a bible-api.com range response into (before, focal, after).
Passage parseWebPassage(const QJsonObject &payload, int focal);

// One rich-text string: leading/trailing context dimmed, the anchor bright.
QString composeRichText(const QString &before, const QString &focal,
                        const QString &after, int maxChars);

// The public reading page for a reference in a given translation.
QString browserUrl(const QString &reference, const QString &translationId);

// Percent-encode like JavaScript's encodeURIComponent for URL segments.
QString encodeReference(const QString &reference);

// Best-effort anchor reference for a bible-api.com response.
QString referenceText(const QJsonObject &payload);

// bible-api.com translation name, or a bare id.
QString translationText(const QJsonObject &payload);

} // namespace References

#endif // SCRIPTURE_REFERENCES_H