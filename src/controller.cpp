#include "controller.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <cmath>

namespace {
QRegularExpression autoOpenRx()
{
    static const QRegularExpression rx(QStringLiteral("^([01]\\d|2[0-3]):[0-5]\\d$"));
    return rx;
}

const char kOrg[] = "davidjm";
const char kApp[] = "scripture";

QJsonDocument parseJson(const QString &text)
{
    if (text.isEmpty())
        return {};
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    return error.error == QJsonParseError::NoError ? doc : QJsonDocument();
}
} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(QLatin1String(kOrg), QLatin1String(kApp))
    , m_store(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , m_fetcher(this)
{
    m_revealTimer.setInterval(RevealIntervalMs);
    connect(&m_revealTimer, &QTimer::timeout, this, &AppController::onRevealTick);

    m_autoOpenTimer.setInterval(30000);
    connect(&m_autoOpenTimer, &QTimer::timeout, this, &AppController::onAutoOpenTimer);
    m_autoOpenTimer.start();

    m_fetchTimeout.setSingleShot(true);
    connect(&m_fetchTimeout, &QTimer::timeout, this, &AppController::onFetchTimeout);

    connect(&m_fetcher, &Fetcher::esvResult, this, &AppController::onEsvResult);
    connect(&m_fetcher, &Fetcher::webResult, this, &AppController::onWebResult);

    try {
        m_store.ensureDir();
        m_favoritesList = m_store.list();
    } catch (const FavoritesError &) {
        m_favoritesList.clear();
    }

    check_auto_open();
    emit favoritesChanged();
}

QString AppController::setting(const QString &key, const QString &defaultValue) const
{
    return m_settings.value(key, defaultValue).toString().trimmed();
}

void AppController::setOverlayOpen(bool value)
{
    if (value == m_overlayOpen)
        return;
    m_overlayOpen = value;
    emit overlayChanged();
    if (value) {
        // Every open draws a fresh verse (fixed reference if configured).
        QTimer::singleShot(1, this, [this]() { refresh(); });
    }
}

void AppController::setSettingsOpen(bool value)
{
    if (value == m_settingsOpen)
        return;
    m_settingsOpen = value;
    emit settingsChanged();
}

void AppController::save_settings(const QString &apiKey, const QString &translation,
                                  const QString &fixedReference, const QString &autoOpenAt)
{
    const QString autoTime = autoOpenAt.trimmed();
    if (!autoTime.isEmpty() && !autoOpenRx().match(autoTime).hasMatch()) {
        m_settingsNotice = QStringLiteral("Auto-open must be a 24-hour time like 07:30.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }
    m_settings.setValue(QStringLiteral("apiKey"), apiKey.trimmed());
    QString tr = translation.trimmed().toUpper();
    if (tr.isEmpty())
        tr = QStringLiteral("ESV");
    m_settings.setValue(QStringLiteral("translation"), tr);
    m_settings.setValue(QStringLiteral("fixedReference"), fixedReference.trimmed());
    m_settings.setValue(QStringLiteral("autoOpenAt"), autoTime);
    m_settings.sync();
    m_settingsNotice = QStringLiteral("Saved.");
    m_settingsNoticeError = false;
    emit settingsChanged();
    check_auto_open();
}

QString AppController::translationName() const
{
    if (!m_translationName.isEmpty())
        return m_translationName;
    return m_translationId == QLatin1String("esv")
        ? QStringLiteral("English Standard Version")
        : QStringLiteral("World English Bible");
}

QString AppController::displayText() const
{
    if (m_loading && !hasContent())
        return QStringLiteral("\u2026");
    if (!hasContent())
        return QString();
    return References::composeRichText(m_contextBefore, m_verseText, m_contextAfter, m_revealedChars);
}

bool AppController::isFavorite() const
{
    const QString current = anchor();
    return !current.isEmpty() && m_favoritesList.contains(current);
}

QString AppController::starSymbol() const
{
    return isFavorite() ? QStringLiteral("\u2605") : QStringLiteral("\u2606");
}

void AppController::load_reference(const QString &reference)
{
    const QString ref = reference.trimmed();
    if (ref.isEmpty())
        return;
    setOverlayOpen(true);
    m_verseAnchor = ref;
    fetch(ref, true);
}

void AppController::refresh()
{
    const QString fixed = setting(QStringLiteral("fixedReference"));
    if (!fixed.isEmpty()) {
        load_reference(fixed);
        return;
    }
    const QString ref = m_deck.draw(anchor());
    m_verseAnchor = ref;
    fetch(ref, true);
}

void AppController::back()
{
    if (m_histPos <= 0)
        return;
    m_histPos--;
    fetch(m_history.at(m_histPos), false);
}

void AppController::forward()
{
    if (m_histPos < 0 || m_histPos >= m_history.size() - 1)
        return;
    m_histPos++;
    fetch(m_history.at(m_histPos), false);
}

void AppController::open_in_browser(const QString &reference)
{
    QString ref = reference.trimmed();
    if (ref.isEmpty())
        ref = m_verseReference;
    if (ref.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl(References::browserUrl(ref, m_translationId)));
}

void AppController::toggle_favorite()
{
    const QString current = anchor();
    if (current.isEmpty())
        return;
    try {
        if (m_favoritesList.contains(current))
            m_favoritesList = m_store.remove(current);
        else
            m_favoritesList = m_store.add(current);
        emit favoritesChanged();
        emit verseChanged();
    } catch (const FavoritesError &exc) {
        m_errorText = QStringLiteral("Favorites: %1").arg(exc.what());
        emit verseChanged();
    }
}

void AppController::remove_favorite(const QString &reference)
{
    const QString ref = reference.trimmed();
    if (ref.isEmpty())
        return;
    try {
        m_favoritesList = m_store.remove(ref);
        emit favoritesChanged();
        emit verseChanged();
    } catch (const FavoritesError &exc) {
        m_errorText = QStringLiteral("Favorites: %1").arg(exc.what());
        emit verseChanged();
    }
}

void AppController::check_auto_open()
{
    const QString target = setting(QStringLiteral("autoOpenAt"));
    if (target.isEmpty())
        return;
    const QDateTime now = QDateTime::currentDateTime();
    const QString hhmm = now.toString(QStringLiteral("HH:mm"));
    const QString day = now.toString(QStringLiteral("yyyy-M-d"));
    if (hhmm == target && day != m_lastAutoOpenDay) {
        m_lastAutoOpenDay = day;
        setOverlayOpen(true);
    }
}

void AppController::onAutoOpenTimer()
{
    check_auto_open();
}

// == fetch pipeline ======================================================

QString AppController::providerChoice() const
{
    QString choice = setting(QStringLiteral("translation"), QStringLiteral("ESV")).toLower();
    if (choice != QLatin1String("esv") && choice != QLatin1String("web") && choice != QLatin1String("kjv"))
        choice = QStringLiteral("esv");
    return choice;
}

QString AppController::apiKey() const
{
    return setting(QStringLiteral("apiKey"));
}

void AppController::fetch(const QString &anchor, bool recordHistoryEntry)
{
    m_loading = true;
    emit loadingChanged();
    m_errorText.clear();
    m_fetchNotice.clear();
    m_pendingAnchor = anchor;
    m_pendingReference = References::rangeQuery(anchor);
    m_pendingFocal = References::focalVerse(anchor);
    m_webRetried = false;
    m_esvRetried = false;
    m_verseReference.clear();
    if (recordHistoryEntry)
        recordHistory(anchor);
    m_fetchTimeout.start(FetchTimeoutMs);
    emit verseChanged();

    const QString choice = providerChoice();
    const QString key = apiKey();
    m_fetchSeq++;
    const int tag = m_fetchSeq;

    if (choice == QLatin1String("esv") && !key.isEmpty()) {
        m_translationId.clear();
        m_translationName.clear();
        m_fetcher.fetchEsv(tag, m_pendingReference, key);
        emit verseChanged();
    } else if (choice == QLatin1String("esv")) {
        m_fetchNotice = QStringLiteral(
            "Set an ESV API key in Settings to read the ESV \u2014 showing the World English Bible.");
        m_currentWebTranslation = QStringLiteral("web");
        m_fetcher.fetchWeb(tag, m_pendingReference, QStringLiteral("web"));
        emit verseChanged();
    } else {
        const QString tr = choice == QLatin1String("kjv") ? QStringLiteral("kjv") : QStringLiteral("web");
        m_currentWebTranslation = tr;
        m_fetcher.fetchWeb(tag, m_pendingReference, tr);
        emit verseChanged();
    }
}

void AppController::onFetchTimeout()
{
    if (!m_loading)
        return;
    m_loading = false;
    emit loadingChanged();
    m_errorText = QStringLiteral("The verse fetch timed out. Try again.");
    emit verseChanged();
}

// -- ESV ----------------------------------------------------------------

void AppController::onEsvResult(int tag, const QString &body, const QString &error)
{
    if (tag != m_fetchSeq || !m_loading)
        return;
    m_fetchTimeout.stop();

    const QJsonDocument doc = parseJson(body);
    const QJsonObject payload = doc.object();
    const QJsonArray passages = payload.value(QStringLiteral("passages")).toArray();
    const bool ok = error.isEmpty() && !payload.isEmpty() && !passages.isEmpty();

    if (!ok && m_pendingAnchor != m_pendingReference && !m_esvRetried) {
        m_esvRetried = true;
        m_fetchSeq++;
        m_fetcher.fetchEsv(m_fetchSeq, m_pendingAnchor, apiKey());
        return;
    }

    if (!ok) {
        m_loading = false;
        emit loadingChanged();
        m_errorText = QStringLiteral("Could not load from the ESV API. Check your key and connection.");
        emit verseChanged();
        return;
    }

    const QString reference = payload.value(QStringLiteral("canonical")).toString(m_pendingReference);
    const References::Passage passage =
        References::parseNumberedPassage(passages.first().toString(), m_pendingFocal);
    applyVerse(passage.before, passage.focal, passage.after, reference, QStringLiteral("esv"),
               QStringLiteral("English Standard Version"));
}

// -- WEB / KJV ----------------------------------------------------------

void AppController::onWebResult(int tag, const QString &body, const QString &error)
{
    if (tag != m_fetchSeq || !m_loading)
        return;
    m_fetchTimeout.stop();

    const QJsonDocument doc = parseJson(body);
    const QJsonObject payload = doc.object();
    const QJsonArray verses = payload.value(QStringLiteral("verses")).toArray();
    const bool ok = error.isEmpty() && !payload.isEmpty() && !payload.value(QStringLiteral("error")).toBool()
        && !verses.isEmpty();

    if (!ok && m_pendingAnchor != m_pendingReference && !m_webRetried) {
        m_webRetried = true;
        m_fetchSeq++;
        m_fetcher.fetchWeb(m_fetchSeq, m_pendingAnchor, m_currentWebTranslation);
        return;
    }

    if (!ok) {
        m_loading = false;
        emit loadingChanged();
        m_errorText = QStringLiteral("Could not load that passage. Try again.");
        emit verseChanged();
        return;
    }

    const QString versionId = m_currentWebTranslation;
    QString versionName = References::translationText(payload);
    if (versionName.isEmpty()) {
        versionName = versionId == QLatin1String("kjv")
            ? QStringLiteral("King James Version")
            : QStringLiteral("World English Bible");
    }
    const References::Passage passage = References::parseWebPassage(payload, m_pendingFocal);
    QString reference = References::referenceText(payload);
    if (reference.isEmpty())
        reference = m_pendingReference;
    applyVerse(passage.before, passage.focal, passage.after, reference, versionId, versionName);
}

void AppController::applyVerse(const QString &before, const QString &focal, const QString &after,
                               const QString &reference, const QString &translationId,
                               const QString &translationName)
{
    m_contextBefore = before;
    m_verseText = focal;
    m_contextAfter = after;
    m_verseReference = reference;
    m_translationId = translationId;
    m_translationName = translationName;
    m_esvRetried = false;
    m_loading = false;
    emit loadingChanged();
    emit verseChanged();
    startReveal();
}

// == history / reveal ====================================================

void AppController::recordHistory(const QString &anchor)
{
    if (!m_history.isEmpty() && m_history.last() == anchor)
        return;
    m_history.append(anchor);
    if (m_history.size() > HistoryCap)
        m_history = m_history.mid(m_history.size() - HistoryCap);
    m_histPos = m_history.size() - 1;
}

void AppController::startReveal()
{
    const int total = m_contextBefore.size() + m_verseText.size() + m_contextAfter.size();
    m_revealTotal = total > 0 ? total : 0;
    m_revealStep = qMax(1, static_cast<int>(std::ceil(total * RevealIntervalMs / static_cast<double>(RevealDurationMs))));
    m_revealedChars = 0;
    if (total <= 0)
        return;
    emit verseChanged();
    m_revealTimer.start();
}

void AppController::onRevealTick()
{
    m_revealedChars = qMin(m_revealTotal, m_revealedChars + m_revealStep);
    if (m_revealedChars >= m_revealTotal) {
        m_revealedChars = m_revealTotal;
        m_revealTimer.stop();
    }
    emit verseChanged();
}