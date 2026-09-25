#include "verse_card.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>

namespace {
QString cleanText(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QString());
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return value.trimmed();
}

QString passageText(const VerseCardContent &content)
{
    QStringList parts;
    for (const QString &value : {content.before, content.focal, content.after}) {
        const QString clean = cleanText(value);
        if (!clean.isEmpty())
            parts.append(clean);
    }
    return parts.join(QLatin1Char(' '));
}

void drawCentered(QPainter &painter, const QRectF &rect, const QString &text,
                  int flags, const QFont &font, const QColor &color)
{
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, flags, text);
}

void pruneCards(const QString &directory, const QString &keepPath)
{
    QDir dir(directory);
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Name);
    QFileInfoList ordered = files;
    std::sort(ordered.begin(), ordered.end(), [](const QFileInfo &left, const QFileInfo &right) {
        if (left.lastModified() == right.lastModified())
            return left.fileName() < right.fileName();
        return left.lastModified() < right.lastModified();
    });
    QFileInfoList removable;
    for (const QFileInfo &file : ordered) {
        if (file.filePath() != keepPath)
            removable.append(file);
    }
    const int excess = removable.size() - 31;
    for (int index = 0; index < excess; ++index)
        QFile::remove(removable.at(index).filePath());
}
}

QString VerseCardRenderer::plainText(const VerseCardContent &content)
{
    QStringList lines;
    const QString passage = passageText(content);
    if (!passage.isEmpty())
        lines.append(passage);
    const QString reference = cleanText(content.reference);
    if (!reference.isEmpty())
        lines.append(reference);
    const QString translation = cleanText(content.translationName);
    if (!translation.isEmpty())
        lines.append(translation);
    return lines.join(QLatin1Char('\n'));
}

QString VerseCardRenderer::fileName(const VerseCardContent &content)
{
    QByteArray identity;
    identity += plainText(content).toUtf8();
    identity += '\n';
    identity += content.translationId.toUtf8();
    const QString digest = QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(24));
    return QStringLiteral("verse-card-%1.png").arg(digest);
}

QImage VerseCardRenderer::render(const VerseCardContent &content)
{
    QImage image(1080, 1350, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#0d1b2a")));
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    painter.setPen(QPen(QColor(QStringLiteral("#f5c542")), 16));
    painter.drawLine(540, 110, 540, 300);
    painter.drawLine(460, 170, 620, 170);
    painter.setPen(QPen(QColor(245, 197, 66, 55), 2));
    painter.drawRoundedRect(QRectF(54, 54, 972, 1242), 30, 30);

    const QString passage = passageText(content);
    QFont verseFont;
    verseFont.setStyleHint(QFont::SansSerif);
    verseFont.setPixelSize(48);
    verseFont.setWeight(QFont::Light);
    QRectF verseRect(120, 350, 840, 610);
    int verseSize = 48;
    while (verseSize >= 28) {
        verseFont.setPixelSize(verseSize);
        const QFontMetricsF metrics(verseFont);
        const QRectF bounds = metrics.boundingRect(QRectF(0, 0, verseRect.width(), 10000),
                                                     Qt::TextWordWrap, passage);
        if (bounds.height() <= verseRect.height())
            break;
        verseSize -= 2;
    }
    verseFont.setPixelSize(verseSize);
    drawCentered(painter, verseRect, passage, Qt::AlignCenter | Qt::TextWordWrap,
                 verseFont, QColor(QStringLiteral("#ffffff")));

    QFont referenceFont;
    referenceFont.setStyleHint(QFont::SansSerif);
    referenceFont.setPixelSize(30);
    referenceFont.setWeight(QFont::Bold);
    drawCentered(painter, QRectF(120, 1010, 840, 60), cleanText(content.reference),
                 Qt::AlignCenter, referenceFont, QColor(QStringLiteral("#faa968")));

    QFont detailFont;
    detailFont.setStyleHint(QFont::SansSerif);
    detailFont.setPixelSize(20);
    QStringList details;
    if (!cleanText(content.translationName).isEmpty())
        details.append(cleanText(content.translationName));
    drawCentered(painter, QRectF(120, 1090, 840, 150), details.join(QLatin1Char('\n')),
                 Qt::AlignCenter | Qt::TextWordWrap, detailFont, QColor(230, 235, 240, 190));
    painter.end();
    return image;
}

bool VerseCardRenderer::save(const VerseCardContent &content, const QString &dataDir,
                             QString *path, QString *error)
{
    const QString directory = QDir(dataDir).filePath(QStringLiteral("cards"));
    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QStringLiteral("could not create the verse card directory");
        return false;
    }
    const QString target = QDir(directory).filePath(fileName(content));
    QSaveFile file(target);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("could not save the verse card");
        return false;
    }
    const QImage image = render(content);
    if (!image.save(&file, "PNG")) {
        if (error)
            *error = QStringLiteral("could not encode the verse card");
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("could not save the verse card");
        return false;
    }
    pruneCards(directory, target);
    if (path)
        *path = target;
    if (error)
        error->clear();
    return true;
}
