#include "controller.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include <cmath>
#include <limits>

namespace {
QRegularExpression autoOpenRx()
{
    static const QRegularExpression rx(QStringLiteral("^([01]\\d|2[0-3]):[0-5]\\d$"));
    return rx;
}

const char kOrg[] = "davidjm";
const char kApp[] = "Scripture";
const char kLegacyApp[] = "scripture";
const char kEsvAttribution[] = "Scripture quotations are from the ESV® Bible (The Holy Bible, English Standard Version®), © 2001 by Crossway. Used by permission. All rights reserved. esv.org";

bool validEsvPayload(const QJsonObject &payload)
{
    const QJsonValue passagesValue = payload.value(QStringLiteral("passages"));
    if (!passagesValue.isArray())
        return false;
    const QJsonArray passages = passagesValue.toArray();
    if (passages.isEmpty() || passages.size() > 32)
        return false;
    for (const QJsonValue &value : passages) {
        if (!value.isString() || value.toString().trimmed().isEmpty())
            return false;
    }
    for (const QString &key : {QStringLiteral("canonical"), QStringLiteral("copyright"), QStringLiteral("attribution")}) {
        if (payload.contains(key) && !payload.value(key).isString())
            return false;
    }
    return true;
}

bool validWebPayload(const QJsonObject &payload)
{
    if (payload.contains(QStringLiteral("error")))
        return false;
    const QJsonValue versesValue = payload.value(QStringLiteral("verses"));
    if (!versesValue.isArray())
        return false;
    const QJsonArray verses = versesValue.toArray();
    if (verses.isEmpty() || verses.size() > 32)
        return false;
    for (const QJsonValue &value : verses) {
        if (!value.isObject())
            return false;
        const QJsonObject verse = value.toObject();
        if (!verse.value(QStringLiteral("text")).isString() || verse.value(QStringLiteral("text")).toString().trimmed().isEmpty())
            return false;
        if (!verse.contains(QStringLiteral("verse")) && !verse.contains(QStringLiteral("number")))
            return false;
        for (const QString &key : {QStringLiteral("verse"), QStringLiteral("number")}) {
            if (!verse.contains(key))
                continue;
            const QJsonValue number = verse.value(key);
            if (number.isString()) {
                const QString text = number.toString();
                if (text.isEmpty() || text.size() > 3)
                    return false;
                bool numeric = false;
                for (const QChar character : text) {
                    if (character < QLatin1Char('0') || character > QLatin1Char('9')) {
                        numeric = false;
                        break;
                    }
                    numeric = true;
                }
                if (!numeric || text.toInt() <= 0)
                    return false;
            } else if (!number.isDouble() || !std::isfinite(number.toDouble())
                       || number.toDouble() <= 0 || number.toDouble() > 999
                       || std::floor(number.toDouble()) != number.toDouble()) {
                return false;
            }
        }
    }
    for (const QString &key : {QStringLiteral("reference"), QStringLiteral("attribution"), QStringLiteral("copyright"), QStringLiteral("translation_name")}) {
        if (payload.contains(key) && !payload.value(key).isString())
            return false;
    }
    if (payload.contains(QStringLiteral("translation"))) {
        if (!payload.value(QStringLiteral("translation")).isObject())
            return false;
        const QJsonObject translation = payload.value(QStringLiteral("translation")).toObject();
        if (translation.contains(QStringLiteral("name")) && !translation.value(QStringLiteral("name")).isString())
            return false;
    }
    return true;
}

bool validApiKey(const QString &value)
{
    if (value.isEmpty() || value.toUtf8().size() > 512)
        return false;
    for (const QChar character : value) {
        if (character.unicode() < 32 || character.unicode() == 127)
            return false;
    }
    return true;
}

QString normalizeFixedReference(const QString &value)
{
    const QString text = value.trimmed();
    if (text.isEmpty())
        return QString();
    return References::normalizeReference(text);
}
}

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(QLatin1String(kOrg), QLatin1String(kApp))
    , m_legacySettings(QLatin1String(kOrg), QLatin1String(kLegacyApp))
    , m_secrets(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , m_store(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , m_fetcher(this)
{
    for (const QString &key : {QStringLiteral("translation"), QStringLiteral("fixedReference"), QStringLiteral("autoOpenAt"), QStringLiteral("lastAutoOpenDay")}) {
        if (!m_settings.contains(key) && m_legacySettings.contains(key))
            m_settings.setValue(key, m_legacySettings.value(key));
    }
    m_settings.sync();
    QString secretError;
    m_apiKey = m_secrets.load(&secretError);
    if (!secretError.isEmpty())
        m_errorText = QStringLiteral("Settings: %1").arg(secretError);
    QString legacyKey = m_settings.value(QStringLiteral("apiKey")).toString().trimmed();
    if (legacyKey.isEmpty())
        legacyKey = m_legacySettings.value(QStringLiteral("apiKey")).toString().trimmed();
    if (!legacyKey.isEmpty()) {
        bool migrationOk = !m_apiKey.isEmpty();
        if (!validApiKey(legacyKey)) {
            if (m_errorText.isEmpty())
                m_errorText = QStringLiteral("Settings: stored API key is invalid");
            migrationOk = false;
        } else if (m_apiKey.isEmpty() && secretError.isEmpty()) {
            QString migrationError;
            if (m_secrets.save(legacyKey, &migrationError)) {
                m_apiKey = legacyKey;
            } else {
                if (m_errorText.isEmpty())
                    m_errorText = QStringLiteral("Settings: %1").arg(migrationError);
                migrationOk = false;
            }
        } else if (m_apiKey.isEmpty()) {
            migrationOk = false;
        }
        if (m_apiKey.isEmpty() && validApiKey(legacyKey))
            m_apiKey = legacyKey;
        if (migrationOk) {
            m_settings.remove(QStringLiteral("apiKey"));
            m_legacySettings.remove(QStringLiteral("apiKey"));
            m_settings.sync();
            m_legacySettings.sync();
            if ((m_settings.status() != QSettings::NoError || m_legacySettings.status() != QSettings::NoError)
                && m_errorText.isEmpty()) {
                m_errorText = QStringLiteral("Settings: could not remove the legacy API key");
            }
        }
    }

    m_lastAutoOpenDay = m_settings.value(QStringLiteral("lastAutoOpenDay")).toString().trimmed();
    m_revealTimer.setInterval(RevealIntervalMs);
    connect(&m_revealTimer, &QTimer::timeout, this, &AppController::onRevealTick);

    m_openRefreshTimer.setSingleShot(true);
    connect(&m_openRefreshTimer, &QTimer::timeout, this, &AppController::refresh);

    m_autoOpenTimer.setInterval(30000);
    connect(&m_autoOpenTimer, &QTimer::timeout, this, &AppController::onAutoOpenTimer);

    m_fetchTimeout.setSingleShot(true);
    connect(&m_fetchTimeout, &QTimer::timeout, this, &AppController::onFetchTimeout);

    connect(&m_fetcher, &Fetcher::esvResult, this, &AppController::onEsvResult);
    connect(&m_fetcher, &Fetcher::webResult, this, &AppController::onWebResult);

    try {
        m_store.ensureDir();
        m_favoritesList = m_store.list();
    } catch (const FavoritesError &error) {
        m_favoritesList.clear();
        if (m_errorText.isEmpty())
            m_errorText = QStringLiteral("Favorites: %1").arg(error.what());
    }

    syncAutoOpenTimer();
    check_auto_open();
    emit favoritesChanged();
}

QString AppController::setting(const QString &key, const QString &defaultValue) const
{
    return m_settings.value(key, defaultValue).toString().trimmed();
}

void AppController::syncAutoOpenTimer()
{
    if (setting(QStringLiteral("autoOpenAt")).isEmpty())
        m_autoOpenTimer.stop();
    else if (!m_autoOpenTimer.isActive())
        m_autoOpenTimer.start();
}

void AppController::setOverlayOpen(bool value)
{
    if (value == m_overlayOpen)
        return;
    m_overlayOpen = value;
    emit overlayChanged();
    if (value) {
        if (m_skipNextOpenRefresh)
            m_skipNextOpenRefresh = false;
        else
            m_openRefreshTimer.start(1);
    } else {
        m_openRefreshTimer.stop();
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
    QString selectedTranslation = translation.trimmed().toUpper();
    if (selectedTranslation.isEmpty())
        selectedTranslation = QStringLiteral("ESV");
    if (selectedTranslation != QLatin1String("ESV") && selectedTranslation != QLatin1String("WEB") && selectedTranslation != QLatin1String("KJV")) {
        m_settingsNotice = QStringLiteral("Translation must be ESV, WEB, or KJV.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }
    const QString fixedInput = fixedReference.trimmed();
    const QString fixed = normalizeFixedReference(fixedInput);
    if (!fixedInput.isEmpty() && fixed.isEmpty()) {
        m_settingsNotice = QStringLiteral("Fixed reference is invalid.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }
    QString secretError;
    const QString oldKey = m_apiKey;
    const QString newKey = apiKey.trimmed();
    const bool secretSaved = newKey.isEmpty() ? m_secrets.clear(&secretError) : m_secrets.save(newKey, &secretError);
    if (!secretSaved) {
        m_settingsNotice = QStringLiteral("Settings: %1").arg(secretError);
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }

    const QVariant oldTranslation = m_settings.value(QStringLiteral("translation"));
    const QVariant oldFixed = m_settings.value(QStringLiteral("fixedReference"));
    const QVariant oldAuto = m_settings.value(QStringLiteral("autoOpenAt"));
    m_settings.setValue(QStringLiteral("translation"), selectedTranslation);
    m_settings.setValue(QStringLiteral("fixedReference"), fixed);
    m_settings.setValue(QStringLiteral("autoOpenAt"), autoTime);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        if (oldTranslation.isValid())
            m_settings.setValue(QStringLiteral("translation"), oldTranslation);
        else
            m_settings.remove(QStringLiteral("translation"));
        if (oldFixed.isValid())
            m_settings.setValue(QStringLiteral("fixedReference"), oldFixed);
        else
            m_settings.remove(QStringLiteral("fixedReference"));
        if (oldAuto.isValid())
            m_settings.setValue(QStringLiteral("autoOpenAt"), oldAuto);
        else
            m_settings.remove(QStringLiteral("autoOpenAt"));
        m_settings.sync();
        QString rollbackError;
        const bool keyRestored = oldKey.isEmpty() ? m_secrets.clear(&rollbackError) : m_secrets.save(oldKey, &rollbackError);
        if (!keyRestored) {
            m_settingsNotice = QStringLiteral("Settings and API key could not be saved.");
            m_settingsNoticeError = true;
            emit settingsChanged();
            return;
        }
        m_settingsNotice = QStringLiteral("Settings could not be saved.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }
    m_apiKey = newKey;
    m_settingsNotice = QStringLiteral("Saved.");
    m_settingsNoticeError = false;
    emit settingsChanged();
    syncAutoOpenTimer();
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
    const QString ref = References::normalizeReference(reference);
    if (ref.isEmpty()) {
        if (!reference.trimmed().isEmpty()) {
            m_errorText = QStringLiteral("Reference is invalid.");
            emit verseChanged();
        }
        return;
    }
    m_openRefreshTimer.stop();
    if (!m_overlayOpen)
        m_skipNextOpenRefresh = true;
    setOverlayOpen(true);
    m_verseAnchor = ref;
    fetch(ref, true);
}

void AppController::refresh()
{
    m_openRefreshTimer.stop();
    const QString fixedInput = setting(QStringLiteral("fixedReference"));
    const QString fixed = normalizeFixedReference(fixedInput);
    if (!fixedInput.isEmpty() && fixed.isEmpty()) {
        m_errorText = QStringLiteral("Fixed reference is invalid.");
        emit verseChanged();
        return;
    }
    if (!fixed.isEmpty()) {
        m_verseAnchor = fixed;
        fetch(fixed, true);
        return;
    }
    const QString ref = m_deck.draw(anchor());
    m_verseAnchor = ref;
    fetch(ref, true);
}

void AppController::back()
{
    m_openRefreshTimer.stop();
    if (m_histPos <= 0)
        return;
    m_histPos--;
    fetch(m_history.at(m_histPos), false);
}

void AppController::forward()
{
    m_openRefreshTimer.stop();
    if (m_histPos < 0 || m_histPos >= m_history.size() - 1)
        return;
    m_histPos++;
    fetch(m_history.at(m_histPos), false);
}

void AppController::open_in_browser(const QString &reference)
{
    QString ref = References::normalizeReference(reference);
    if (ref.isEmpty())
        ref = References::normalizeReference(m_verseReference);
    if (ref.isEmpty())
        return;
    const QString url = References::browserUrl(ref, m_translationId);
    const QUrl target(url);
    if (!url.isEmpty() && target.isValid() && target.scheme() == QStringLiteral("https") &&
        (target.host() == QStringLiteral("www.esv.org") || target.host() == QStringLiteral("www.biblegateway.com")))
        QDesktopServices::openUrl(target);
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
        m_errorText.clear();
        emit favoritesChanged();
        emit verseChanged();
    } catch (const FavoritesError &error) {
        m_errorText = QStringLiteral("Favorites: %1").arg(error.what());
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
        m_errorText.clear();
        emit favoritesChanged();
        emit verseChanged();
    } catch (const FavoritesError &error) {
        m_errorText = QStringLiteral("Favorites: %1").arg(error.what());
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
        m_settings.setValue(QStringLiteral("lastAutoOpenDay"), day);
        m_settings.sync();
        if (m_overlayOpen)
            m_openRefreshTimer.start(1);
        else
            setOverlayOpen(true);
    }
}

void AppController::onAutoOpenTimer()
{
    check_auto_open();
}

QString AppController::providerChoice() const
{
    QString choice = setting(QStringLiteral("translation"), QStringLiteral("ESV")).toLower();
    if (choice != QLatin1String("esv") && choice != QLatin1String("web") && choice != QLatin1String("kjv"))
        choice = QStringLiteral("esv");
    return choice;
}

QString AppController::apiKey() const
{
    return m_apiKey;
}

void AppController::fetch(const QString &anchor, bool recordHistoryEntry)
{
    const QString normalizedAnchor = References::normalizeReference(anchor);
    if (normalizedAnchor.isEmpty()) {
        m_errorText = QStringLiteral("Reference is invalid.");
        emit verseChanged();
        return;
    }
    m_fetchSeq++;
    const int tag = m_fetchSeq;
    m_fetcher.abort();
    m_loading = true;
    emit loadingChanged();
    m_errorText.clear();
    m_fetchNotice.clear();
    m_translationAttribution.clear();
    m_pendingAnchor = normalizedAnchor;
    m_pendingReference = References::rangeQuery(normalizedAnchor);
    m_pendingFocal = References::focalVerse(normalizedAnchor);
    m_webRetried = false;
    m_esvRetried = false;
    m_verseReference.clear();
    if (recordHistoryEntry)
        recordHistory(normalizedAnchor);
    m_fetchTimeout.start(FetchTimeoutMs);
    emit verseChanged();
    emit displayTextChanged();

    const QString choice = providerChoice();
    const QString key = apiKey();

    if (choice == QLatin1String("esv") && !key.isEmpty()) {
        m_translationId.clear();
        m_translationName.clear();
        m_fetcher.fetchEsv(tag, m_pendingReference, key);
        emit verseChanged();
    } else if (choice == QLatin1String("esv")) {
        m_fetchNotice = QStringLiteral("Set an ESV API key in Settings to read the ESV — showing the World English Bible.");
        m_currentWebTranslation = QStringLiteral("web");
        m_fetcher.fetchWeb(tag, m_pendingReference, QStringLiteral("web"));
        emit verseChanged();
    } else {
        m_currentWebTranslation = choice == QLatin1String("kjv") ? QStringLiteral("kjv") : QStringLiteral("web");
        m_fetcher.fetchWeb(tag, m_pendingReference, m_currentWebTranslation);
        emit verseChanged();
    }
}

void AppController::onFetchTimeout()
{
    if (!m_loading)
        return;
    m_fetchSeq++;
    m_fetcher.abort();
    m_loading = false;
    emit loadingChanged();
    m_errorText = QStringLiteral("The verse fetch timed out. Try again.");
    emit verseChanged();
    emit displayTextChanged();
}

void AppController::onEsvResult(int tag, const QString &body, const QString &error, int status)
{
    if (tag != m_fetchSeq || !m_loading)
        return;
    m_fetchTimeout.stop();
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8(), &parseError);
    const QJsonObject payload = document.object();
    const bool ok = error.isEmpty() && parseError.error == QJsonParseError::NoError && document.isObject() && validEsvPayload(payload);

    if (!ok && !m_esvRetried && shouldRetryRange(
            QStringLiteral("esv"), m_pendingReference, m_pendingAnchor,
            body.toUtf8(), status) && error == QStringLiteral("server returned HTTP %1").arg(status)) {
        m_esvRetried = true;
        m_fetchSeq++;
        m_fetchTimeout.start(FetchTimeoutMs);
        m_fetcher.fetchEsv(m_fetchSeq, m_pendingAnchor, apiKey());
        return;
    }
    if (!ok) {
        m_loading = false;
        emit loadingChanged();
        m_errorText = QStringLiteral("Could not load from the ESV API. Check your key and connection.");
        emit verseChanged();
        emit displayTextChanged();
        return;
    }

    const QString canonical = References::normalizeReference(payload.value(QStringLiteral("canonical")).toString());
    const QString reference = canonical.isEmpty() ? m_pendingReference : canonical;
    const References::Passage passage = References::parseNumberedPassage(payload.value(QStringLiteral("passages")).toArray().first().toString(), m_pendingFocal);
    QString attribution = payload.value(QStringLiteral("copyright")).toString().trimmed();
    if (attribution.isEmpty())
        attribution = payload.value(QStringLiteral("attribution")).toString().trimmed();
    if (attribution.isEmpty())
        attribution = QLatin1String(kEsvAttribution);
    applyVerse(passage.before, passage.focal, passage.after, reference, QStringLiteral("esv"), QStringLiteral("English Standard Version"), attribution);
}

void AppController::onWebResult(int tag, const QString &body, const QString &error, int status)
{
    if (tag != m_fetchSeq || !m_loading)
        return;
    m_fetchTimeout.stop();
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8(), &parseError);
    const QJsonObject payload = document.object();
    const bool ok = error.isEmpty() && parseError.error == QJsonParseError::NoError && document.isObject() && validWebPayload(payload);

    if (!ok && !m_webRetried && error == QStringLiteral("server returned HTTP %1").arg(status)
        && shouldRetryRange(QStringLiteral("web"), m_pendingReference, m_pendingAnchor,
                            body.toUtf8(), status)) {
        m_webRetried = true;
        m_fetchSeq++;
        m_fetchTimeout.start(FetchTimeoutMs);
        m_fetcher.fetchWeb(m_fetchSeq, m_pendingAnchor, m_currentWebTranslation);
        return;
    }
    if (!ok) {
        m_loading = false;
        emit loadingChanged();
        m_errorText = QStringLiteral("Could not load that passage. Try again.");
        emit verseChanged();
        emit displayTextChanged();
        return;
    }

    const QString versionId = m_currentWebTranslation;
    QString versionName = References::translationText(payload);
    if (versionName.isEmpty())
        versionName = versionId == QLatin1String("kjv") ? QStringLiteral("King James Version") : QStringLiteral("World English Bible");
    const References::Passage passage = References::parseWebPassage(payload, m_pendingFocal);
    QString reference = References::referenceText(payload);
    if (reference.isEmpty())
        reference = m_pendingReference;
    const QString attribution = payload.value(QStringLiteral("attribution")).toString().trimmed().isEmpty()
        ? payload.value(QStringLiteral("copyright")).toString().trimmed()
        : payload.value(QStringLiteral("attribution")).toString().trimmed();
    applyVerse(passage.before, passage.focal, passage.after, reference, versionId, versionName, attribution);
}

void AppController::applyVerse(const QString &before, const QString &focal, const QString &after,
                               const QString &reference, const QString &translationId,
                               const QString &translationName, const QString &attribution)
{
    m_contextBefore = before;
    m_verseText = focal;
    m_contextAfter = after;
    const QString normalizedReference = References::normalizeReference(reference);
    m_verseReference = normalizedReference.isEmpty() ? m_pendingReference : normalizedReference;
    m_translationId = translationId;
    m_translationName = translationName;
    m_translationAttribution = attribution;
    m_esvRetried = false;
    m_loading = false;
    emit loadingChanged();
    emit verseChanged();
    emit displayTextChanged();
    startReveal();
}

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
    const qsizetype total = m_contextBefore.size() + m_verseText.size() + m_contextAfter.size();
    m_revealTotal = total > 0 ? static_cast<int>(qMin<qsizetype>(total, std::numeric_limits<int>::max())) : 0;
    m_revealStep = qMax(1, static_cast<int>(std::ceil(static_cast<double>(m_revealTotal) * RevealIntervalMs / RevealDurationMs)));
    m_revealedChars = 0;
    if (total <= 0)
        return;
    emit displayTextChanged();
    m_revealTimer.start();
}

void AppController::onRevealTick()
{
    m_revealedChars = qMin(m_revealTotal, m_revealedChars + m_revealStep);
    if (m_revealedChars >= m_revealTotal) {
        m_revealedChars = m_revealTotal;
        m_revealTimer.stop();
    }
    emit displayTextChanged();
}
