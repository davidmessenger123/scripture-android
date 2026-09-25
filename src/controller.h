#ifndef SCRIPTURE_CONTROLLER_H
#define SCRIPTURE_CONTROLLER_H

#include "favorites.h"
#include "fetcher.h"
#include "passage_cache.h"
#include "references.h"
#include "secrets.h"
#include "verse_card.h"

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

// The Scripture app controller. One QObject exposed to the QML UI as the `App`
// singleton (ScriptureRT module). It mirrors the desktop app's AppController:
// verse text/context, translation, history, favorites, reveal progress,
// notices, provider selection with keyless fallback, single plain-anchor retry
// per fetch, no-repeat deck, 25 s fetch timeout, 8-chip favorites,
// at-most-once-per-day auto-open.
class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool overlayOpen READ overlayOpen WRITE setOverlayOpen NOTIFY overlayChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool hasContent READ hasContent NOTIFY verseChanged)
    Q_PROPERTY(bool settingsOpen READ settingsOpen WRITE setSettingsOpen NOTIFY settingsChanged)
    Q_PROPERTY(QString contextBefore READ contextBefore NOTIFY verseChanged)
    Q_PROPERTY(QString verseText READ verseText NOTIFY verseChanged)
    Q_PROPERTY(QString contextAfter READ contextAfter NOTIFY verseChanged)
    Q_PROPERTY(QString verseReference READ verseReference NOTIFY verseChanged)
    Q_PROPERTY(QString translationId READ translationId NOTIFY verseChanged)
    Q_PROPERTY(QString translationLabel READ translationLabel NOTIFY verseChanged)
    Q_PROPERTY(QString translationName READ translationName NOTIFY verseChanged)
    Q_PROPERTY(QString displayText READ displayText NOTIFY displayTextChanged)
    Q_PROPERTY(QString fetchNotice READ fetchNotice NOTIFY verseChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY verseChanged)
    Q_PROPERTY(QString anchor READ anchor NOTIFY verseChanged)
    Q_PROPERTY(bool isFavorite READ isFavorite NOTIFY verseChanged)
    Q_PROPERTY(QString starSymbol READ starSymbol NOTIFY verseChanged)
    Q_PROPERTY(QString fixedReference READ fixedReference NOTIFY settingsChanged)
    Q_PROPERTY(bool histCanBack READ histCanBack NOTIFY verseChanged)
    Q_PROPERTY(bool histCanForward READ histCanForward NOTIFY verseChanged)
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)
    Q_PROPERTY(QVariantList favoritesChips READ favoritesChips NOTIFY favoritesChanged)
    Q_PROPERTY(int favoritesOverflow READ favoritesOverflow NOTIFY favoritesChanged)
    Q_PROPERTY(QString settingsApiKey READ settingsApiKey NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsTranslation READ settingsTranslation NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsFixedReference READ settingsFixedReference NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsAutoOpenAt READ settingsAutoOpenAt NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsDailyNotificationAt READ settingsDailyNotificationAt NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsNotice READ settingsNotice NOTIFY settingsChanged)
    Q_PROPERTY(bool settingsNoticeError READ settingsNoticeError NOTIFY settingsChanged)
    Q_PROPERTY(QString actionNotice READ actionNotice NOTIFY actionNoticeChanged)
    Q_PROPERTY(bool actionNoticeError READ actionNoticeError NOTIFY actionNoticeChanged)
    Q_PROPERTY(QString lastCardPath READ lastCardPath NOTIFY actionNoticeChanged)
    Q_PROPERTY(QStringList bookOptions READ bookOptions CONSTANT)
    Q_PROPERTY(QStringList topicOptions READ topicOptions CONSTANT)
    Q_PROPERTY(QString selectedBook READ selectedBook WRITE setSelectedBook NOTIFY filtersChanged)
    Q_PROPERTY(QString selectedTopic READ selectedTopic WRITE setSelectedTopic NOTIFY filtersChanged)
    Q_PROPERTY(bool filtersActive READ filtersActive NOTIFY filtersChanged)
    Q_PROPERTY(int verseFontSize READ verseFontSize WRITE setVerseFontSize NOTIFY appearanceChanged)
    Q_PROPERTY(int scrimOpacity READ scrimOpacity WRITE setScrimOpacity NOTIFY appearanceChanged)
    Q_PROPERTY(int revealSpeed READ revealSpeed WRITE setRevealSpeed NOTIFY appearanceChanged)
    Q_PROPERTY(bool notificationAvailable READ notificationAvailable NOTIFY notificationChanged)
    Q_PROPERTY(bool notificationPermissionGranted READ notificationPermissionGranted NOTIFY notificationChanged)
    Q_PROPERTY(QString notificationPermissionState READ notificationPermissionState NOTIFY notificationChanged)
    Q_PROPERTY(bool notificationPermissionRequested READ notificationPermissionRequested NOTIFY notificationChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    // -- properties ------------------------------------------------------
    bool overlayOpen() const { return m_overlayOpen; }
    void setOverlayOpen(bool value);
    bool loading() const { return m_loading; }
    bool hasContent() const { return !m_contextBefore.isEmpty() || !m_verseText.isEmpty() || !m_contextAfter.isEmpty(); }
    bool settingsOpen() const { return m_settingsOpen; }
    void setSettingsOpen(bool value);
    QString contextBefore() const { return m_contextBefore; }
    QString verseText() const { return m_verseText; }
    QString contextAfter() const { return m_contextAfter; }
    QString verseReference() const { return m_verseReference; }
    QString translationId() const { return m_translationId; }
    QString translationLabel() const { return m_translationId.isEmpty() ? QString() : m_translationName.toUpper(); }
    QString translationName() const;
    QString displayText() const;
    QString fetchNotice() const { return m_fetchNotice; }
    QString errorText() const { return m_errorText; }
    QString anchor() const { return m_verseAnchor.isEmpty() ? m_pendingAnchor : m_verseAnchor; }
    bool isFavorite() const;
    QString starSymbol() const;
    QString fixedReference() const { return setting(QStringLiteral("fixedReference")); }
    bool histCanBack() const { return m_histPos > 0 && !m_loading; }
    bool histCanForward() const { return m_histPos >= 0 && m_histPos < m_history.size() - 1 && !m_loading; }
    QVariantList favorites() const { return m_favoritesList; }
    QVariantList favoritesChips() const { return m_favoritesList.mid(0, ChipCap); }
    int favoritesOverflow() const { return qMax(0, m_favoritesList.size() - ChipCap); }
    QString settingsApiKey() const { return m_apiKey; }
    QString settingsTranslation() const { return (setting(QStringLiteral("translation")).isEmpty() ? QStringLiteral("ESV") : setting(QStringLiteral("translation"))).toUpper(); }
    QString settingsFixedReference() const { return setting(QStringLiteral("fixedReference")); }
    QString settingsAutoOpenAt() const { return setting(QStringLiteral("autoOpenAt")); }
    QString settingsDailyNotificationAt() const { return setting(QStringLiteral("dailyNotificationAt")); }
    QString settingsNotice() const { return m_settingsNotice; }
    bool settingsNoticeError() const { return m_settingsNoticeError; }
    QString actionNotice() const { return m_actionNotice; }
    bool actionNoticeError() const { return m_actionNoticeError; }
    QString lastCardPath() const { return m_lastCardPath; }
    QStringList bookOptions() const;
    QStringList topicOptions() const;
    QString selectedBook() const { return m_selectedBook; }
    void setSelectedBook(const QString &book);
    QString selectedTopic() const { return m_selectedTopic; }
    void setSelectedTopic(const QString &topic);
    bool filtersActive() const { return !m_selectedBook.isEmpty() || !m_selectedTopic.isEmpty(); }
    int verseFontSize() const { return m_verseFontSize; }
    void setVerseFontSize(int value);
    int scrimOpacity() const { return m_scrimOpacity; }
    void setScrimOpacity(int value);
    int revealSpeed() const { return m_revealSpeed; }
    void setRevealSpeed(int value);
    bool notificationAvailable() const;
    bool notificationPermissionGranted() const;
    QString notificationPermissionState() const;
    bool notificationPermissionRequested() const;

    Q_INVOKABLE void close_overlay() { setOverlayOpen(false); }
    Q_INVOKABLE void toggle_overlay() { setOverlayOpen(!m_overlayOpen); }
    Q_INVOKABLE void toggle_settings() { setSettingsOpen(!m_settingsOpen); }
    Q_INVOKABLE void load_reference(const QString &reference);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void open_in_browser(const QString &reference);
    Q_INVOKABLE void toggle_favorite();
    Q_INVOKABLE void remove_favorite(const QString &reference);
    Q_INVOKABLE void check_auto_open();
    Q_INVOKABLE void save_settings(const QString &apiKey, const QString &translation,
                                   const QString &fixedReference, const QString &autoOpenAt);
    Q_INVOKABLE void save_all_settings(const QString &apiKey, const QString &translation,
                                       const QString &fixedReference, const QString &autoOpenAt,
                                       const QString &dailyNotificationAt, int verseFontSize,
                                       int scrimOpacity, int revealSpeed);
    Q_INVOKABLE bool copy_verse();
    Q_INVOKABLE bool share_verse();
    Q_INVOKABLE bool save_verse_card();
    Q_INVOKABLE bool share_verse_card();
    Q_INVOKABLE void request_notification_permission();
    Q_INVOKABLE bool open_notification_settings();
    Q_INVOKABLE void refresh_notification_status();

signals:
    void verseChanged();
    void displayTextChanged();
    void loadingChanged();
    void favoritesChanged();
    void overlayChanged();
    void settingsChanged();
    void actionNoticeChanged();
    void filtersChanged();
    void appearanceChanged();
    void notificationChanged();

private slots:
    void onEsvResult(int tag, const QString &body, const QString &error, int status);
    void onWebResult(int tag, const QString &body, const QString &error, int status);
    void onFetchTimeout();
    void onRevealTick();
    void onAutoOpenTimer();

private:
    // -- settings helpers ------------------------------------------------
    QString setting(const QString &key, const QString &defaultValue = QString()) const;
    void repairPersistedSettings();
    void syncAutoOpenTimer();
    bool syncNotificationSchedule(QString *reason = nullptr);
    void updateNotificationSnapshot();
    void cacheCurrentPassage(const QString &provider, const QString &before, const QString &focal,
                              const QString &after, const QString &reference,
                              const QString &translationId, const QString &translationName,
                              const QString &attribution);
    bool applyCachedPassage();
    void setActionNotice(const QString &notice, bool error = false);

    // -- fetch pipeline --------------------------------------------------
    QString providerChoice() const;
    QString apiKey() const;
    QString keyFingerprint() const;
    void fetch(const QString &anchor, bool recordHistory);
    void applyVerse(const QString &before, const QString &focal, const QString &after,
                    const QString &reference, const QString &translationId,
                    const QString &translationName);

    // -- history / reveal ------------------------------------------------
    void recordHistory(const QString &anchor);
    void startReveal();

    static constexpr int FetchTimeoutMs = 25000;
    static constexpr int RevealIntervalMs = 16;
    static constexpr int HistoryCap = 200;
    static constexpr int ChipCap = 8;
    static constexpr int MinFontSize = 16;
    static constexpr int MaxFontSize = 56;
    static constexpr int DefaultFontSize = 28;
    static constexpr int DefaultScrimOpacity = 78;
    static constexpr int DefaultRevealSpeed = 50;

    QSettings m_settings;
    QSettings m_legacySettings;
    QString m_dataDir;
    SecureStore m_secrets;
    QString m_apiKey;
    FavoritesStore m_store;
    PassageCache m_cache;
    QVariantList m_favoritesList;
    Fetcher m_fetcher;

    // verse state
    bool m_overlayOpen = false;
    bool m_skipNextOpenRefresh = false;
    bool m_loading = false;
    bool m_settingsOpen = false;
    QString m_settingsNotice;
    bool m_settingsNoticeError = false;
    QString m_actionNotice;
    bool m_actionNoticeError = false;
    QString m_notificationPermissionState = QStringLiteral("unavailable");
    bool m_notificationPermissionRequested = false;
    QString m_lastCardPath;
    QString m_selectedBook;
    QString m_selectedTopic;
    int m_verseFontSize = DefaultFontSize;
    int m_scrimOpacity = DefaultScrimOpacity;
    int m_revealSpeed = DefaultRevealSpeed;
    QString m_contextBefore;
    QString m_verseText;
    QString m_contextAfter;
    QString m_verseReference;
    QString m_translationId;
    QString m_translationName;
    QString m_verseAnchor;
    QString m_pendingAnchor;
    QString m_pendingReference;
    QString m_pendingProvider;
    int m_pendingFocal = 0;
    bool m_webRetried = false;
    bool m_esvRetried = false;
    QString m_currentWebTranslation = QStringLiteral("web");
    QString m_fetchNotice;
    QString m_errorText;
    int m_fetchSeq = 0;

    // reveal
    int m_revealedChars = -1;
    int m_revealTotal = 0;
    int m_revealStep = 1;
    QTimer m_revealTimer;

    // history
    QStringList m_history;
    int m_histPos = -1;

    // auto-open
    QTimer m_openRefreshTimer;
    QTimer m_autoOpenTimer;
    QTimer m_notificationPermissionTimer;
    QString m_lastAutoOpenDay;

    // fetch timeout
    QTimer m_fetchTimeout;

    // no-repeat deck
    References::Deck m_deck;
};

#endif // SCRIPTURE_CONTROLLER_H