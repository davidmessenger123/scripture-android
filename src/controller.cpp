#include "controller.h"

#include "android_notifications.h"
#include "android_share.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
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

QString notificationGuidance(const QString &state)
{
    if (state == QStringLiteral("runtime-denied"))
        return QStringLiteral("Notifications are denied. Enable them for Scripture in Android Settings.");
    if (state == QStringLiteral("app-blocked"))
        return QStringLiteral("Notifications are blocked for Scripture. Enable them in Android Settings.");
    if (state == QStringLiteral("channel-disabled"))
        return QStringLiteral("The Daily verse notification channel is disabled. Enable it in Android Settings.");
    if (state == QStringLiteral("pending"))
        return QStringLiteral("Notification permission is pending. Complete the Android permission prompt.");
    return QStringLiteral("Daily notifications are unavailable.");
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
    , m_dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , m_secrets(m_dataDir)
    , m_store(m_dataDir)
    , m_cache(m_dataDir)
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

    repairPersistedSettings();
    m_lastAutoOpenDay = m_settings.value(QStringLiteral("lastAutoOpenDay")).toString().trimmed();
    const QString savedBook = setting(QStringLiteral("bookFilter"));
    m_selectedBook = bookOptions().contains(savedBook) ? savedBook : QString();
    const QString savedTopic = setting(QStringLiteral("topicFilter"));
    m_selectedTopic = topicOptions().contains(savedTopic) ? savedTopic : QString();
    m_verseFontSize = qBound(MinFontSize, setting(QStringLiteral("verseFontSize"), QString::number(DefaultFontSize)).toInt(), MaxFontSize);
    m_scrimOpacity = qBound(0, setting(QStringLiteral("scrimOpacity"), QString::number(DefaultScrimOpacity)).toInt(), 100);
    m_revealSpeed = qBound(0, setting(QStringLiteral("revealSpeed"), QString::number(DefaultRevealSpeed)).toInt(), 100);
    m_deck.setFilters(m_selectedBook, m_selectedTopic);
    if (m_deck.references().isEmpty() && filtersActive())
        setActionNotice(QStringLiteral("No verses match the selected filters."), true);
    m_revealTimer.setInterval(RevealIntervalMs);
    connect(&m_revealTimer, &QTimer::timeout, this, &AppController::onRevealTick);

    m_openRefreshTimer.setSingleShot(true);
    connect(&m_openRefreshTimer, &QTimer::timeout, this, &AppController::refresh);

    m_autoOpenTimer.setInterval(30000);
    connect(&m_autoOpenTimer, &QTimer::timeout, this, &AppController::onAutoOpenTimer);

    m_notificationPermissionTimer.setInterval(250);
    connect(&m_notificationPermissionTimer, &QTimer::timeout, this, &AppController::refresh_notification_status);

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
    try {
        m_cache.ensureDir();
        m_cache.compact();
    } catch (const PassageCacheError &error) {
        if (m_errorText.isEmpty())
            m_errorText = QStringLiteral("Cache: %1").arg(error.what());
    }

    syncAutoOpenTimer();
    QString scheduleReason;
    if (!syncNotificationSchedule(&scheduleReason) && AndroidNotifications::supported()) {
        m_settings.remove(QStringLiteral("dailyNotificationAt"));
        m_settings.sync();
        m_settingsNotice = QStringLiteral("Daily notification setting removed: %1.").arg(scheduleReason);
        m_settingsNoticeError = true;
    } else if (!scheduleReason.isEmpty() && m_settingsNotice.isEmpty()) {
        m_settingsNotice = scheduleReason;
        m_settingsNoticeError = m_notificationPermissionState != QStringLiteral("pending");
    }
    check_auto_open();
    emit favoritesChanged();
}

QString AppController::setting(const QString &key, const QString &defaultValue) const
{
    return m_settings.value(key, defaultValue).toString().trimmed();
}

void AppController::repairPersistedSettings()
{
    QStringList repairs;
    const QString translation = setting(QStringLiteral("translation"), QStringLiteral("ESV")).toUpper();
    if (translation != QLatin1String("ESV") && translation != QLatin1String("WEB")
        && translation != QLatin1String("KJV")) {
        m_settings.setValue(QStringLiteral("translation"), QStringLiteral("ESV"));
        repairs.append(QStringLiteral("translation"));
    } else if (translation != setting(QStringLiteral("translation"))) {
        m_settings.setValue(QStringLiteral("translation"), translation);
        repairs.append(QStringLiteral("translation"));
    }
    const QString fixed = setting(QStringLiteral("fixedReference"));
    const QString normalizedFixed = References::normalizeReference(fixed);
    if (!fixed.isEmpty() && normalizedFixed.isEmpty()) {
        m_settings.remove(QStringLiteral("fixedReference"));
        repairs.append(QStringLiteral("fixed verse"));
    } else if (!fixed.isEmpty() && normalizedFixed != fixed) {
        m_settings.setValue(QStringLiteral("fixedReference"), normalizedFixed);
        repairs.append(QStringLiteral("fixed verse"));
    }
    for (const QString &key : {QStringLiteral("autoOpenAt"), QStringLiteral("dailyNotificationAt")}) {
        const QString value = setting(key);
        if (!value.isEmpty() && !autoOpenRx().match(value).hasMatch()) {
            m_settings.remove(key);
            repairs.append(key == QStringLiteral("autoOpenAt")
                               ? QStringLiteral("auto-open time")
                               : QStringLiteral("daily time"));
        }
    }
    const QString day = setting(QStringLiteral("lastAutoOpenDay"));
    if (!day.isEmpty() && !QRegularExpression(QStringLiteral("^\\d{4}-\\d{1,2}-\\d{1,2}$")).match(day).hasMatch()) {
        m_settings.remove(QStringLiteral("lastAutoOpenDay"));
        repairs.append(QStringLiteral("auto-open day"));
    }
    struct NumericSetting
    {
        QString key;
        int low;
        int high;
        int fallback;
    };
    for (const NumericSetting &numeric : {
             NumericSetting{QStringLiteral("verseFontSize"), MinFontSize, MaxFontSize, DefaultFontSize},
             NumericSetting{QStringLiteral("scrimOpacity"), 0, 100, DefaultScrimOpacity},
             NumericSetting{QStringLiteral("revealSpeed"), 0, 100, DefaultRevealSpeed}}) {
        bool ok = false;
        const QString raw = setting(numeric.key, QString::number(numeric.fallback));
        const int value = raw.toInt(&ok);
        const int repaired = ok ? qBound(numeric.low, value, numeric.high) : numeric.fallback;
        if (!ok || repaired != value) {
            m_settings.setValue(numeric.key, repaired);
            repairs.append(numeric.key);
        }
    }
    const QString book = setting(QStringLiteral("bookFilter"));
    if (!book.isEmpty() && !References::bookOptions().contains(book)) {
        m_settings.remove(QStringLiteral("bookFilter"));
        repairs.append(QStringLiteral("book filter"));
    }
    const QString topic = setting(QStringLiteral("topicFilter"));
    if (!topic.isEmpty() && !References::topicOptions().contains(topic)) {
        m_settings.remove(QStringLiteral("topicFilter"));
        repairs.append(QStringLiteral("topic filter"));
    }
    if (!repairs.isEmpty()) {
        m_settings.sync();
        m_settingsNotice = QStringLiteral("Settings repaired: %1.").arg(repairs.join(QStringLiteral(", ")));
        m_settingsNoticeError = m_settings.status() != QSettings::NoError;
    }
}

void AppController::syncAutoOpenTimer()
{
    if (setting(QStringLiteral("autoOpenAt")).isEmpty())
        m_autoOpenTimer.stop();
    else if (!m_autoOpenTimer.isActive())
        m_autoOpenTimer.start();
}

QStringList AppController::bookOptions() const
{
    return References::bookOptions();
}

QStringList AppController::topicOptions() const
{
    return References::topicOptions();
}

void AppController::setSelectedBook(const QString &book)
{
    const QString selected = book.trimmed();
    const QString normalized = References::bookOptions().contains(selected) ? selected : QString();
    if (m_selectedBook == normalized)
        return;
    m_selectedBook = normalized;
    m_settings.setValue(QStringLiteral("bookFilter"), m_selectedBook);
    m_settings.sync();
    m_deck.setFilters(m_selectedBook, m_selectedTopic);
    if (m_deck.references().isEmpty())
        setActionNotice(QStringLiteral("No verses match the selected filters."), true);
    emit filtersChanged();
}

void AppController::setSelectedTopic(const QString &topic)
{
    const QString selected = topic.trimmed();
    const QString normalized = References::topicOptions().contains(selected) ? selected : QString();
    if (m_selectedTopic == normalized)
        return;
    m_selectedTopic = normalized;
    m_settings.setValue(QStringLiteral("topicFilter"), m_selectedTopic);
    m_settings.sync();
    m_deck.setFilters(m_selectedBook, m_selectedTopic);
    if (m_deck.references().isEmpty())
        setActionNotice(QStringLiteral("No verses match the selected filters."), true);
    emit filtersChanged();
}

void AppController::setVerseFontSize(int value)
{
    const int clamped = qBound(MinFontSize, value, MaxFontSize);
    if (m_verseFontSize == clamped)
        return;
    m_verseFontSize = clamped;
    m_settings.setValue(QStringLiteral("verseFontSize"), m_verseFontSize);
    m_settings.sync();
    startReveal();
    emit appearanceChanged();
}

void AppController::setScrimOpacity(int value)
{
    const int clamped = qBound(0, value, 100);
    if (m_scrimOpacity == clamped)
        return;
    m_scrimOpacity = clamped;
    m_settings.setValue(QStringLiteral("scrimOpacity"), m_scrimOpacity);
    m_settings.sync();
    emit appearanceChanged();
}

void AppController::setRevealSpeed(int value)
{
    const int clamped = qBound(0, value, 100);
    if (m_revealSpeed == clamped)
        return;
    m_revealSpeed = clamped;
    m_settings.setValue(QStringLiteral("revealSpeed"), m_revealSpeed);
    m_settings.sync();
    startReveal();
    emit appearanceChanged();
}

bool AppController::notificationAvailable() const
{
    return AndroidNotifications::supported();
}

bool AppController::notificationPermissionGranted() const
{
    return m_notificationPermissionState == QStringLiteral("granted");
}

QString AppController::notificationPermissionState() const
{
    return m_notificationPermissionState;
}

bool AppController::notificationPermissionRequested() const
{
    return m_notificationPermissionRequested;
}

void AppController::setActionNotice(const QString &notice, bool error)
{
    m_actionNotice = notice;
    m_actionNoticeError = error;
    emit actionNoticeChanged();
}

void AppController::updateNotificationSnapshot()
{
    if (!hasContent())
        return;
    const VerseCardContent content{m_contextBefore, m_verseText, m_contextAfter,
                                     m_verseReference, m_translationId, m_translationName};
    AndroidNotifications::updateSnapshot(m_verseReference, VerseCardRenderer::plainText(content));
}

bool AppController::syncNotificationSchedule(QString *reason)
{
    if (reason)
        reason->clear();
    if (AndroidNotifications::supported()) {
        m_notificationPermissionState = AndroidNotifications::permissionState();
        m_notificationPermissionRequested = AndroidNotifications::permissionRequested();
    } else {
        m_notificationPermissionState = QStringLiteral("unavailable");
        m_notificationPermissionRequested = false;
    }
    const QString time = setting(QStringLiteral("dailyNotificationAt"));
    if (time.isEmpty()) {
        if (AndroidNotifications::supported() && !AndroidNotifications::cancel()) {
            if (reason)
                *reason = QStringLiteral("could not cancel the daily notification schedule");
            return false;
        }
        return true;
    }
    if (!AndroidNotifications::supported()) {
        if (reason)
            *reason = QStringLiteral("daily notifications are unavailable on this platform");
        return true;
    }
    if (m_notificationPermissionState == QStringLiteral("pending")) {
        if (reason)
            *reason = notificationGuidance(m_notificationPermissionState);
        return true;
    }
    if (m_notificationPermissionState != QStringLiteral("granted")) {
        AndroidNotifications::cancel();
        if (reason)
            *reason = notificationGuidance(m_notificationPermissionState);
        return true;
    }
    if (!AndroidNotifications::schedule(time)) {
        AndroidNotifications::cancel();
        if (reason)
            *reason = QStringLiteral("the daily notification could not be scheduled");
        return false;
    }
    return true;
}

void AppController::request_notification_permission()
{
    if (!AndroidNotifications::supported()) {
        m_notificationPermissionState = QStringLiteral("unavailable");
        setActionNotice(QStringLiteral("Daily notifications are unavailable on this platform."), true);
        m_settingsNotice = QStringLiteral("Daily notifications are unavailable on this platform.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        emit notificationChanged();
        return;
    }
    const QString requestedState = AndroidNotifications::requestPermission();
    m_notificationPermissionState = requestedState.isEmpty()
        ? AndroidNotifications::permissionState() : requestedState;
    m_notificationPermissionRequested = AndroidNotifications::permissionRequested();
    if (m_notificationPermissionState == QStringLiteral("pending")) {
        m_notificationPermissionTimer.start();
        setActionNotice(notificationGuidance(m_notificationPermissionState), false);
        m_settingsNotice = QStringLiteral("Complete the Android notification permission prompt.");
        m_settingsNoticeError = false;
        emit settingsChanged();
        emit notificationChanged();
        return;
    }
    if (m_notificationPermissionState == QStringLiteral("granted")) {
        QString reason;
        if (!syncNotificationSchedule(&reason)) {
            m_settings.remove(QStringLiteral("dailyNotificationAt"));
            m_settings.sync();
            setActionNotice(reason, true);
            m_settingsNotice = QStringLiteral("Daily notification scheduling failed; the time was removed.");
            m_settingsNoticeError = true;
        } else {
            setActionNotice(QStringLiteral("Notification permission granted."));
            m_settingsNotice = QStringLiteral("Notification permission granted.");
            m_settingsNoticeError = false;
        }
    } else {
        setActionNotice(notificationGuidance(m_notificationPermissionState), true);
        m_settingsNotice = notificationGuidance(m_notificationPermissionState);
        m_settingsNoticeError = true;
    }
    emit settingsChanged();
    emit notificationChanged();
}

bool AppController::open_notification_settings()
{
    const QString state = m_notificationPermissionState;
    const bool deniedRecovery = state == QStringLiteral("runtime-denied")
        && m_notificationPermissionRequested;
    if (state != QStringLiteral("app-blocked") && state != QStringLiteral("channel-disabled")
        && !deniedRecovery) {
        if (state == QStringLiteral("runtime-denied"))
            setActionNotice(QStringLiteral("Request notification permission before opening Android Settings."), true);
        else
            setActionNotice(QStringLiteral("Android notification settings recovery is unavailable for this state."), true);
        return false;
    }
    if (!AndroidNotifications::openNotificationSettings(state)) {
        setActionNotice(notificationGuidance(state), true);
        m_settingsNotice = QStringLiteral("Android notification settings could not be opened. Enable notifications manually, then return to Scripture.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        return false;
    }
    setActionNotice(QStringLiteral("Android notification settings opened."), false);
    m_settingsNotice = QStringLiteral("Android notification settings opened. Return to Scripture to refresh the notification state.");
    m_settingsNoticeError = false;
    emit settingsChanged();
    return true;
}

void AppController::refresh_notification_status()
{
    if (!AndroidNotifications::supported()) {
        m_notificationPermissionState = QStringLiteral("unavailable");
        emit notificationChanged();
        return;
    }
    QString state = AndroidNotifications::consumePermissionResult();
    if (state.isEmpty())
        state = AndroidNotifications::permissionState();
    const QString previous = m_notificationPermissionState;
    m_notificationPermissionState = state;
    m_notificationPermissionRequested = AndroidNotifications::permissionRequested();
    if (state != QStringLiteral("pending"))
        m_notificationPermissionTimer.stop();
    if (state == QStringLiteral("pending")) {
        emit notificationChanged();
        return;
    }
    if (state == QStringLiteral("granted")) {
        QString reason;
        if (!syncNotificationSchedule(&reason)) {
            m_settings.remove(QStringLiteral("dailyNotificationAt"));
            m_settings.sync();
            setActionNotice(reason, true);
            m_settingsNotice = QStringLiteral("Daily notification scheduling failed; the time was removed.");
            m_settingsNoticeError = true;
            emit settingsChanged();
        } else if (previous != state) {
            setActionNotice(QStringLiteral("Notification permission granted."));
            m_settingsNotice = QStringLiteral("Notification permission granted.");
            m_settingsNoticeError = false;
            emit settingsChanged();
        }
    } else {
        AndroidNotifications::cancel();
        if (previous != state) {
            setActionNotice(notificationGuidance(state), true);
            m_settingsNotice = notificationGuidance(state);
            m_settingsNoticeError = true;
            emit settingsChanged();
        }
    }
    emit notificationChanged();
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
    save_all_settings(apiKey, translation, fixedReference, autoOpenAt,
                      setting(QStringLiteral("dailyNotificationAt")), m_verseFontSize,
                      m_scrimOpacity, m_revealSpeed);
}

void AppController::save_all_settings(const QString &apiKey, const QString &translation,
                                      const QString &fixedReference, const QString &autoOpenAt,
                                      const QString &dailyNotificationAt, int verseFontSize,
                                      int scrimOpacity, int revealSpeed)
{
    const QString autoTime = autoOpenAt.trimmed();
    if (!autoTime.isEmpty() && !autoOpenRx().match(autoTime).hasMatch()) {
        m_settingsNotice = QStringLiteral("Auto-open must be a 24-hour time like 07:30.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }
    const QString dailyTime = dailyNotificationAt.trimmed();
    if (!dailyTime.isEmpty() && !autoOpenRx().match(dailyTime).hasMatch()) {
        m_settingsNotice = QStringLiteral("Daily notification must be a 24-hour time like 08:00.");
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }
    const int selectedFontSize = qBound(MinFontSize, verseFontSize, MaxFontSize);
    const int selectedScrimOpacity = qBound(0, scrimOpacity, 100);
    const int selectedRevealSpeed = qBound(0, revealSpeed, 100);
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
    const QVariant oldDaily = m_settings.value(QStringLiteral("dailyNotificationAt"));
    const QVariant oldFont = m_settings.value(QStringLiteral("verseFontSize"));
    const QVariant oldScrim = m_settings.value(QStringLiteral("scrimOpacity"));
    const QVariant oldSpeed = m_settings.value(QStringLiteral("revealSpeed"));
    const auto rollbackSettings = [this, oldTranslation, oldFixed, oldAuto, oldDaily,
                                   oldFont, oldScrim, oldSpeed, oldKey]() {
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
        if (oldDaily.isValid())
            m_settings.setValue(QStringLiteral("dailyNotificationAt"), oldDaily);
        else
            m_settings.remove(QStringLiteral("dailyNotificationAt"));
        if (oldFont.isValid())
            m_settings.setValue(QStringLiteral("verseFontSize"), oldFont);
        else
            m_settings.remove(QStringLiteral("verseFontSize"));
        if (oldScrim.isValid())
            m_settings.setValue(QStringLiteral("scrimOpacity"), oldScrim);
        else
            m_settings.remove(QStringLiteral("scrimOpacity"));
        if (oldSpeed.isValid())
            m_settings.setValue(QStringLiteral("revealSpeed"), oldSpeed);
        else
            m_settings.remove(QStringLiteral("revealSpeed"));
        m_settings.sync();
        QString rollbackError;
        return oldKey.isEmpty() ? m_secrets.clear(&rollbackError)
                                : m_secrets.save(oldKey, &rollbackError);
    };
    m_settings.setValue(QStringLiteral("translation"), selectedTranslation);
    m_settings.setValue(QStringLiteral("fixedReference"), fixed);
    m_settings.setValue(QStringLiteral("autoOpenAt"), autoTime);
    m_settings.setValue(QStringLiteral("dailyNotificationAt"), dailyTime);
    m_settings.setValue(QStringLiteral("verseFontSize"), selectedFontSize);
    m_settings.setValue(QStringLiteral("scrimOpacity"), selectedScrimOpacity);
    m_settings.setValue(QStringLiteral("revealSpeed"), selectedRevealSpeed);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError) {
        if (!rollbackSettings()) {
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
    QString scheduleReason;
    if (!syncNotificationSchedule(&scheduleReason) && AndroidNotifications::supported()) {
        if (!rollbackSettings()) {
            m_settingsNotice = QStringLiteral("Settings and API key could not be rolled back.");
            m_settingsNoticeError = true;
            emit settingsChanged();
            return;
        }
        syncNotificationSchedule();
        m_settingsNotice = QStringLiteral("Settings were not saved: %1.").arg(scheduleReason);
        m_settingsNoticeError = true;
        emit settingsChanged();
        return;
    }
    m_apiKey = newKey;
    m_verseFontSize = selectedFontSize;
    m_scrimOpacity = selectedScrimOpacity;
    m_revealSpeed = selectedRevealSpeed;
    startReveal();
    m_settingsNotice = QStringLiteral("Saved.");
    m_settingsNoticeError = false;
    emit settingsChanged();
    emit appearanceChanged();
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
    if (ref.isEmpty()) {
        m_errorText = QStringLiteral("No verses match the selected filters.");
        emit verseChanged();
        return;
    }
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

bool AppController::copy_verse()
{
    if (!hasContent()) {
        setActionNotice(QStringLiteral("There is no verse to copy."), true);
        return false;
    }
    const VerseCardContent content{m_contextBefore, m_verseText, m_contextAfter,
                                     m_verseReference, m_translationId, m_translationName};
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        setActionNotice(QStringLiteral("Clipboard access is unavailable."), true);
        return false;
    }
    clipboard->setText(VerseCardRenderer::plainText(content));
    setActionNotice(QStringLiteral("Verse copied."));
    return true;
}

bool AppController::share_verse()
{
    if (!hasContent()) {
        setActionNotice(QStringLiteral("There is no verse to share."), true);
        return false;
    }
    const VerseCardContent content{m_contextBefore, m_verseText, m_contextAfter,
                                     m_verseReference, m_translationId, m_translationName};
    if (!AndroidShare::shareText(VerseCardRenderer::plainText(content))) {
        setActionNotice(QStringLiteral("Sharing is unavailable on this platform."), true);
        return false;
    }
    setActionNotice(QStringLiteral("Verse shared."));
    return true;
}

bool AppController::save_verse_card()
{
    if (!hasContent()) {
        setActionNotice(QStringLiteral("There is no verse to save."), true);
        return false;
    }
    const VerseCardContent content{m_contextBefore, m_verseText, m_contextAfter,
                                     m_verseReference, m_translationId, m_translationName};
    QString path;
    QString error;
    if (!VerseCardRenderer::save(content, m_dataDir, &path, &error)) {
        setActionNotice(error.isEmpty() ? QStringLiteral("Could not save the verse card.") : error, true);
        return false;
    }
    m_lastCardPath = path;
    setActionNotice(QStringLiteral("Verse card saved."));
    return true;
}

bool AppController::share_verse_card()
{
    if (!hasContent()) {
        setActionNotice(QStringLiteral("There is no verse card to share."), true);
        return false;
    }
    const VerseCardContent content{m_contextBefore, m_verseText, m_contextAfter,
                                     m_verseReference, m_translationId, m_translationName};
    QString path = m_lastCardPath;
    if (path.isEmpty() || !QFile::exists(path)
        || QFileInfo(path).fileName() != VerseCardRenderer::fileName(content)) {
        QString error;
        if (!VerseCardRenderer::save(content, m_dataDir, &path, &error)) {
            setActionNotice(error.isEmpty() ? QStringLiteral("Could not prepare the verse card.") : error, true);
            return false;
        }
    }
    m_lastCardPath = path;
    if (!AndroidShare::shareImage(path, VerseCardRenderer::plainText(content))) {
        setActionNotice(QStringLiteral("Card sharing is unavailable on this platform."), true);
        return false;
    }
    setActionNotice(QStringLiteral("Verse card shared."));
    return true;
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

void AppController::cacheCurrentPassage(const QString &provider, const QString &before,
                                         const QString &focal, const QString &after,
                                         const QString &reference, const QString &translationId,
                                         const QString &translationName, const QString &attribution)
{
    PassageCacheEntry entry;
    entry.provider = provider;
    entry.requestedReference = m_pendingAnchor;
    entry.rangeReference = m_pendingReference;
    entry.focalVerse = m_pendingFocal;
    entry.before = before;
    entry.focal = focal;
    entry.after = after;
    entry.reference = reference;
    entry.translationId = translationId;
    entry.translationName = translationName;
    entry.attribution = attribution;
    entry.keyFingerprint = provider == QLatin1String("esv") ? keyFingerprint() : QString();
    entry.storedAt = QDateTime::currentSecsSinceEpoch();
    try {
        m_cache.put(entry);
    } catch (const PassageCacheError &) {
        if (m_fetchNotice.isEmpty())
            m_fetchNotice = QStringLiteral("Verse loaded; offline cache is unavailable.");
    }
}

bool AppController::applyCachedPassage()
{
    if (m_pendingProvider.isEmpty() || m_pendingAnchor.isEmpty() || m_pendingReference.isEmpty())
        return false;
    try {
        const std::optional<PassageCacheEntry> entry = m_cache.get(
            m_pendingProvider, m_pendingAnchor, m_pendingReference, m_pendingFocal,
            m_pendingProvider == QLatin1String("esv") ? keyFingerprint() : QString());
        if (!entry.has_value())
            return false;
        m_fetchTimeout.stop();
        m_fetcher.abort();
        m_loading = false;
        emit loadingChanged();
        m_fetchNotice = QStringLiteral("Offline: showing the last saved passage.");
        m_errorText.clear();
        applyVerse(entry->before, entry->focal, entry->after, entry->reference,
                   entry->translationId, entry->translationName);
        return true;
    } catch (const PassageCacheError &) {
        return false;
    }
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

QString AppController::keyFingerprint() const
{
    if (m_apiKey.isEmpty())
        return QString();
    return QString::fromLatin1(
        QCryptographicHash::hash(m_apiKey.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

void AppController::fetch(const QString &anchor, bool recordHistoryEntry)
{
    const QString normalizedAnchor = References::normalizeReference(anchor);
    if (normalizedAnchor.isEmpty()) {
        m_errorText = QStringLiteral("Reference is invalid.");
        emit verseChanged();
        return;
    }
    m_verseAnchor = normalizedAnchor;
    m_fetchSeq++;
    const int tag = m_fetchSeq;
    m_fetcher.abort();
    m_loading = true;
    emit loadingChanged();
    m_errorText.clear();
    m_fetchNotice.clear();
    if (!m_actionNotice.isEmpty())
        setActionNotice(QString());
    m_pendingAnchor = normalizedAnchor;
    m_pendingReference = References::rangeQuery(normalizedAnchor);
    m_pendingProvider = providerChoice();
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
        m_pendingProvider = QStringLiteral("esv");
        m_translationId.clear();
        m_translationName.clear();
        m_fetcher.fetchEsv(tag, m_pendingReference, key);
        emit verseChanged();
    } else if (choice == QLatin1String("esv")) {
        m_pendingProvider = QStringLiteral("web");
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
    if (applyCachedPassage())
        return;
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
        if (isTransientFetchFailure(error, status) && applyCachedPassage())
            return;
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
    cacheCurrentPassage(QStringLiteral("esv"), passage.before, passage.focal, passage.after,
                        reference, QStringLiteral("esv"), QStringLiteral("English Standard Version"), attribution);
    applyVerse(passage.before, passage.focal, passage.after, reference, QStringLiteral("esv"), QStringLiteral("English Standard Version"));
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
        if (isTransientFetchFailure(error, status) && applyCachedPassage())
            return;
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
    cacheCurrentPassage(m_pendingProvider, passage.before, passage.focal, passage.after,
                        reference, versionId, versionName, attribution);
    applyVerse(passage.before, passage.focal, passage.after, reference, versionId, versionName);
}

void AppController::applyVerse(const QString &before, const QString &focal, const QString &after,
                               const QString &reference, const QString &translationId,
                               const QString &translationName)
{
    m_contextBefore = before;
    m_verseText = focal;
    m_contextAfter = after;
    const QString normalizedReference = References::normalizeReference(reference);
    m_verseReference = normalizedReference.isEmpty() ? m_pendingReference : normalizedReference;
    m_translationId = translationId;
    m_translationName = translationName;
    updateNotificationSnapshot();
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
    m_revealTimer.stop();
    const qsizetype total = m_contextBefore.size() + m_verseText.size() + m_contextAfter.size();
    m_revealTotal = total > 0 ? static_cast<int>(qMin<qsizetype>(total, std::numeric_limits<int>::max())) : 0;
    m_revealedChars = m_revealSpeed == 0 ? m_revealTotal : 0;
    if (total <= 0)
        return;
    if (m_revealSpeed == 0) {
        emit displayTextChanged();
        return;
    }
    const int duration = qBound(250, 4400 - m_revealSpeed * 44, 4400);
    m_revealStep = qMax(1, static_cast<int>(std::ceil(static_cast<double>(m_revealTotal) * RevealIntervalMs / duration)));
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
