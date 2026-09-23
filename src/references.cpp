#include "references.h"

#include <QJsonArray>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace References {

// Well-known, always-valid references drawn across the whole Bible. Random
// picks are drawn from this deck so a request can never throw a nonexistent
// chapter:verse. Kept in sync with references.py / the Omarchy Scripture.js.
namespace {
QList<QString> scripture()
{
    static const QList<QString> deck = {
        QStringLiteral("Genesis 1:1"), QStringLiteral("Genesis 1:27"), QStringLiteral("Genesis 2:18"),
        QStringLiteral("Genesis 12:2"), QStringLiteral("Genesis 28:15"),
        QStringLiteral("Exodus 14:14"), QStringLiteral("Exodus 15:2"), QStringLiteral("Exodus 20:12"),
        QStringLiteral("Exodus 33:14"),
        QStringLiteral("Leviticus 19:18"), QStringLiteral("Leviticus 26:12"),
        QStringLiteral("Numbers 6:24"), QStringLiteral("Numbers 23:19"),
        QStringLiteral("Deuteronomy 6:5"), QStringLiteral("Deuteronomy 31:6"),
        QStringLiteral("Deuteronomy 33:27"),
        QStringLiteral("Joshua 1:9"), QStringLiteral("Joshua 24:15"),
        QStringLiteral("Judges 6:24"),
        QStringLiteral("Ruth 1:16"),
        QStringLiteral("1 Samuel 16:7"), QStringLiteral("1 Samuel 12:24"),
        QStringLiteral("2 Samuel 22:31"),
        QStringLiteral("1 Kings 8:61"),
        QStringLiteral("2 Kings 19:19"),
        QStringLiteral("1 Chronicles 16:11"),
        QStringLiteral("2 Chronicles 7:14"),
        QStringLiteral("Ezra 7:10"),
        QStringLiteral("Nehemiah 8:10"),
        QStringLiteral("Job 1:21"), QStringLiteral("Job 19:25"), QStringLiteral("Job 42:2"),
        QStringLiteral("Psalm 1:1"), QStringLiteral("Psalm 16:11"), QStringLiteral("Psalm 19:1"),
        QStringLiteral("Psalm 23:1"), QStringLiteral("Psalm 23:4"),
        QStringLiteral("Psalm 27:1"), QStringLiteral("Psalm 30:5"), QStringLiteral("Psalm 32:8"),
        QStringLiteral("Psalm 34:8"), QStringLiteral("Psalm 37:4"),
        QStringLiteral("Psalm 37:5"), QStringLiteral("Psalm 46:1"), QStringLiteral("Psalm 46:10"),
        QStringLiteral("Psalm 55:22"), QStringLiteral("Psalm 62:8"),
        QStringLiteral("Psalm 91:1"), QStringLiteral("Psalm 103:12"), QStringLiteral("Psalm 118:24"),
        QStringLiteral("Psalm 119:105"), QStringLiteral("Psalm 127:1"),
        QStringLiteral("Psalm 133:1"), QStringLiteral("Psalm 139:14"), QStringLiteral("Psalm 145:18"),
        QStringLiteral("Psalm 150:6"),
        QStringLiteral("Proverbs 3:5"), QStringLiteral("Proverbs 3:6"), QStringLiteral("Proverbs 16:3"),
        QStringLiteral("Proverbs 17:17"),
        QStringLiteral("Proverbs 18:10"), QStringLiteral("Proverbs 27:17"),
        QStringLiteral("Ecclesiastes 3:1"), QStringLiteral("Ecclesiastes 12:13"),
        QStringLiteral("Song of Solomon 4:7"),
        QStringLiteral("Isaiah 40:8"), QStringLiteral("Isaiah 40:31"), QStringLiteral("Isaiah 41:10"),
        QStringLiteral("Isaiah 41:13"), QStringLiteral("Isaiah 43:2"),
        QStringLiteral("Isaiah 53:5"), QStringLiteral("Isaiah 55:8"), QStringLiteral("Isaiah 58:11"),
        QStringLiteral("Jeremiah 29:11"), QStringLiteral("Jeremiah 33:3"),
        QStringLiteral("Lamentations 3:22"), QStringLiteral("Ezekiel 34:15"),
        QStringLiteral("Daniel 2:20"), QStringLiteral("Hosea 6:6"), QStringLiteral("Joel 2:13"),
        QStringLiteral("Amos 5:24"), QStringLiteral("Jonah 2:2"),
        QStringLiteral("Micah 6:8"), QStringLiteral("Nahum 1:7"), QStringLiteral("Habakkuk 3:19"),
        QStringLiteral("Zephaniah 3:17"), QStringLiteral("Haggai 2:4"),
        QStringLiteral("Zechariah 4:6"), QStringLiteral("Malachi 3:10"),
        QStringLiteral("Matthew 5:14"), QStringLiteral("Matthew 6:33"), QStringLiteral("Matthew 6:34"),
        QStringLiteral("Matthew 7:7"), QStringLiteral("Matthew 11:28"),
        QStringLiteral("Matthew 28:20"),
        QStringLiteral("Mark 9:23"), QStringLiteral("Mark 11:24"),
        QStringLiteral("Luke 1:37"), QStringLiteral("Luke 6:38"), QStringLiteral("Luke 10:27"),
        QStringLiteral("Luke 12:32"), QStringLiteral("Luke 15:10"),
        QStringLiteral("John 1:29"), QStringLiteral("John 3:16"), QStringLiteral("John 6:35"),
        QStringLiteral("John 8:32"), QStringLiteral("John 10:10"), QStringLiteral("John 10:27"),
        QStringLiteral("John 11:25"), QStringLiteral("John 13:34"), QStringLiteral("John 14:6"),
        QStringLiteral("John 14:27"), QStringLiteral("John 15:5"), QStringLiteral("John 16:33"),
        QStringLiteral("Acts 1:8"), QStringLiteral("Acts 4:12"), QStringLiteral("Acts 16:31"),
        QStringLiteral("Romans 3:23"), QStringLiteral("Romans 5:8"), QStringLiteral("Romans 8:28"),
        QStringLiteral("Romans 8:38"), QStringLiteral("Romans 10:9"),
        QStringLiteral("Romans 12:2"), QStringLiteral("Romans 12:12"), QStringLiteral("Romans 15:13"),
        QStringLiteral("1 Corinthians 10:13"), QStringLiteral("1 Corinthians 13:4"),
        QStringLiteral("1 Corinthians 15:58"),
        QStringLiteral("1 Corinthians 16:14"),
        QStringLiteral("2 Corinthians 5:17"), QStringLiteral("2 Corinthians 5:18"),
        QStringLiteral("2 Corinthians 12:9"),
        QStringLiteral("Galatians 5:22"), QStringLiteral("Galatians 6:9"),
        QStringLiteral("Ephesians 2:8"), QStringLiteral("Ephesians 2:10"),
        QStringLiteral("Ephesians 3:20"), QStringLiteral("Ephesians 4:32"),
        QStringLiteral("Ephesians 6:10"),
        QStringLiteral("Philippians 4:4"), QStringLiteral("Philippians 4:6"),
        QStringLiteral("Philippians 4:8"), QStringLiteral("Philippians 4:13"),
        QStringLiteral("Philippians 4:19"),
        QStringLiteral("Colossians 3:2"), QStringLiteral("Colossians 3:23"),
        QStringLiteral("1 Thessalonians 5:16"), QStringLiteral("1 Thessalonians 5:18"),
        QStringLiteral("2 Thessalonians 3:3"),
        QStringLiteral("1 Timothy 2:5"), QStringLiteral("1 Timothy 4:12"),
        QStringLiteral("1 Timothy 6:12"),
        QStringLiteral("2 Timothy 1:7"), QStringLiteral("2 Timothy 2:15"),
        QStringLiteral("2 Timothy 3:16"), QStringLiteral("2 Timothy 4:7"),
        QStringLiteral("Titus 2:11"), QStringLiteral("Philemon 1:6"),
        QStringLiteral("Hebrews 10:35"), QStringLiteral("Hebrews 11:1"),
        QStringLiteral("Hebrews 11:6"), QStringLiteral("Hebrews 12:1"),
        QStringLiteral("Hebrews 12:2"),
        QStringLiteral("Hebrews 13:5"), QStringLiteral("Hebrews 13:8"),
        QStringLiteral("James 1:5"), QStringLiteral("James 1:17"), QStringLiteral("James 2:17"),
        QStringLiteral("James 4:8"), QStringLiteral("James 4:10"), QStringLiteral("James 5:16"),
        QStringLiteral("1 Peter 2:9"), QStringLiteral("1 Peter 5:7"),
        QStringLiteral("2 Peter 1:4"), QStringLiteral("2 Peter 3:9"),
        QStringLiteral("1 John 1:9"), QStringLiteral("1 John 4:7"), QStringLiteral("1 John 4:19"),
        QStringLiteral("1 John 5:14"),
        QStringLiteral("2 John 1:6"),
        QStringLiteral("3 John 1:2"),
        QStringLiteral("Jude 1:21"),
        QStringLiteral("Revelation 3:20"), QStringLiteral("Revelation 21:4"),
        QStringLiteral("Revelation 22:20"),
    };
    return deck;
}

QRegularExpression rangeRx()
{
    static const QRegularExpression rx(QStringLiteral("^(.*?)\\s+(\\d+):(\\d+)$"));
    return rx;
}

QRegularExpression focalRx()
{
    static const QRegularExpression rx(QStringLiteral("^.*?\\s+\\d+:(\\d+)$"));
    return rx;
}

QRegularExpression verseMarkerRx()
{
    static const QRegularExpression rx(QStringLiteral("\\[(\\d+)\\]([\\s\\S]*?)(?=\\[\\d+\\]|$)"));
    return rx;
}

QRegularExpression spaceRx()
{
    static const QRegularExpression rx(QStringLiteral("[ \\t]+"));
    return rx;
}

QRegularExpression wsRx()
{
    static const QRegularExpression rx(QStringLiteral("\\s+"));
    return rx;
}

QRegularExpression blankSwapRx()
{
    static const QRegularExpression rx(QStringLiteral("\\n{2,}"));
    return rx;
}
} // namespace

Deck::Deck()
    : m_deck(scripture())
{
}

void Deck::shuffle()
{
    QList<QString> pool = scripture();
    std::shuffle(pool.begin(), pool.end(), *QRandomGenerator::global());
    m_deck = pool;
    m_pos = 0;
}

QString Deck::draw(const QString &avoid)
{
    if (m_pos >= m_deck.size())
        shuffle();
    QString reference = m_deck.at(m_pos);
    m_pos++;
    if (reference == avoid && m_deck.size() > 1) {
        reference = m_deck.at(m_pos % m_deck.size());
        m_pos++;
    }
    return reference;
}

QString rangeQuery(const QString &reference)
{
    const QString ref = reference.trimmed();
    const QRegularExpressionMatch match = rangeRx().match(ref);
    if (!match.hasMatch())
        return ref;
    const QString book = match.captured(1);
    const int chapter = match.captured(2).toInt();
    const int verse = match.captured(3).toInt();
    const int start = qMax(1, verse - 2);
    const int end = verse + 2;
    if (start == end)
        return book + QLatin1Char(' ') + QString::number(chapter) + QLatin1Char(':') + QString::number(verse);
    return book + QLatin1Char(' ') + QString::number(chapter) + QLatin1Char(':') +
           QString::number(start) + QLatin1Char('-') + QString::number(end);
}

int focalVerse(const QString &reference)
{
    const QRegularExpressionMatch match = focalRx().match(reference.trimmed());
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

static QString collapseSpaces(QString text)
{
    text.replace(wsRx(), QStringLiteral(" "));
    return text.trimmed();
}

Passage parseNumberedPassage(const QString &text, int focal)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized = normalized.replace(spaceRx(), QStringLiteral(" ")).trimmed();

    struct Token
    {
        int n;
        QString t;
    };
    QList<Token> tokens;
    QRegularExpressionMatchIterator it = verseMarkerRx().globalMatch(normalized);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        tokens.append({m.captured(1).toInt(), m.captured(2)});
    }

    if (tokens.isEmpty()) {
        QString clean = normalized.replace(blankSwapRx(), QStringLiteral("\n")).trimmed();
        return {QString(), clean, QString()};
    }

    QStringList beforeParts;
    QString focalText;
    QStringList afterParts;
    int found = false;
    for (const Token &token : tokens) {
        const QString verseText = collapseSpaces(token.t);
        if (verseText.isEmpty())
            continue;
        const QString labelled = QStringLiteral("[%1] %2").arg(token.n).arg(verseText);
        if (token.n == focal && !found) {
            focalText = labelled;
            found = true;
        } else if (!found) {
            beforeParts.append(labelled);
        } else {
            afterParts.append(labelled);
        }
    }

    if (focalText.isEmpty()) {
        QStringList whole;
        for (const Token &token : tokens) {
            const QString verseText = collapseSpaces(token.t);
            if (verseText.isEmpty())
                continue;
            whole.append(QStringLiteral("[%1] %2").arg(token.n).arg(verseText));
        }
        return {QString(), whole.join(QLatin1Char(' ')), QString()};
    }

    return {beforeParts.join(QLatin1Char(' ')), focalText, afterParts.join(QLatin1Char(' '))};
}

Passage parseWebPassage(const QJsonObject &payload, int focal)
{
    QStringList beforeParts;
    QString focalText;
    QStringList afterParts;
    int found = false;

    const QJsonArray verses = payload.value(QStringLiteral("verses")).toArray();
    for (const QJsonValue &value : verses) {
        const QJsonObject verse = value.toObject();
        int number = verse.value(QStringLiteral("verse")).toInt(
            verse.value(QStringLiteral("number")).toInt(0));
        QString text = collapseSpaces(verse.value(QStringLiteral("text")).toString());
        if (text.isEmpty() && number == 0)
            continue;

        const QString labelled = QStringLiteral("[%1] %2").arg(number).arg(text);
        if (number == focal && !found) {
            focalText = labelled;
            found = true;
        } else if (!found) {
            beforeParts.append(labelled);
        } else {
            afterParts.append(labelled);
        }
    }

    if (focalText.isEmpty()) {
        QStringList whole;
        for (const QJsonValue &value : verses) {
            const QString text = collapseSpaces(value.toObject().value(QStringLiteral("text")).toString());
            if (!text.isEmpty())
                whole.append(text);
        }
        return {QString(), whole.join(QLatin1Char(' ')), QString()};
    }

    return {beforeParts.join(QLatin1Char(' ')), focalText, afterParts.join(QLatin1Char(' '))};
}

static QString xmlEscape(const QString &content)
{
    QString escaped = content;
    escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return escaped;
}

QString composeRichText(const QString &before, const QString &focal,
                        const QString &after, int maxChars)
{
    QString b = before;
    QString f = focal;
    QString a = after;

    if (maxChars >= 0) {
        const int bl = b.size();
        const int fl = f.size();
        if (maxChars <= bl) {
            b = b.left(maxChars);
            f.clear();
            a.clear();
        } else if (maxChars <= bl + fl) {
            f = f.left(maxChars - bl);
            a.clear();
        } else {
            a = a.left(maxChars - bl - fl);
        }
    }

    auto span = [](const QString &color, const QString &content) -> QString {
        if (content.isEmpty())
            return QString();
        return QStringLiteral("<span style=\"color:%1;\">%2</span>")
            .arg(color, xmlEscape(content));
    };

    return span(QStringLiteral("rgba(255,255,255,0.55)"), b)
        + span(QStringLiteral("#ffffff"), f)
        + span(QStringLiteral("rgba(255,255,255,0.55)"), a);
}

QString browserUrl(const QString &reference, const QString &translationId)
{
    QString slug = reference;
    slug.replace(QLatin1Char(' '), QLatin1Char('+'));
    if (translationId.compare(QStringLiteral("esv"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("https://www.esv.org/%1/").arg(slug);
    const QString version = translationId.compare(QStringLiteral("kjv"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("KJV")
        : QStringLiteral("WEB");
    return QStringLiteral("https://www.biblegateway.com/passage/?search=%1&version=%2").arg(slug, version);
}

QString encodeReference(const QString &reference)
{
    const QByteArray in = reference.toUtf8();
    auto safe = [](unsigned char c) -> bool {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            return true;
        switch (c) {
        case '-': case '_': case '.': case '!': case '~': case '*': case '\'': case '(': case ')':
            return true;
        default:
            return false;
        }
    };
    QByteArray out;
    out.reserve(in.size());
    for (const char raw : in) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (safe(c)) {
            out += raw;
        } else {
            static constexpr char kHex[] = "0123456789ABCDEF";
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0x0F];
        }
    }
    return QString::fromLatin1(out);
}

QString referenceText(const QJsonObject &payload)
{
    const QJsonObject rv = payload.value(QStringLiteral("random_verse")).toObject();
    const QString book = rv.value(QStringLiteral("book")).toString();
    const int chapter = rv.value(QStringLiteral("chapter")).toInt(0);
    const int verse = rv.value(QStringLiteral("verse")).toInt(0);
    if (!book.isEmpty() && chapter > 0 && verse > 0)
        return QStringLiteral("%1 %2:%3").arg(book).arg(chapter).arg(verse);
    return payload.value(QStringLiteral("reference")).toString().trimmed();
}

QString translationText(const QJsonObject &payload)
{
    const QJsonObject tr = payload.value(QStringLiteral("translation")).toObject();
    const QString name = tr.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty())
        return name;
    return payload.value(QStringLiteral("translation_name")).toString().trimmed();
}

} // namespace References