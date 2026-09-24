#ifndef SCRIPTURE_UPDATER_H
#define SCRIPTURE_UPDATER_H

#include <QObject>
#include <QString>

class UpdateChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateChanged)
    Q_PROPERTY(QString updateTag READ updateTag NOTIFY updateChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(double updateProgress READ updateProgress NOTIFY progressChanged)
    Q_PROPERTY(QString updateError READ updateError NOTIFY errorChanged)
    Q_PROPERTY(bool updateApplied READ updateApplied NOTIFY updateChanged)

public:
    explicit UpdateChecker(QObject *parent = nullptr);

    bool updateAvailable() const { return false; }
    QString updateTag() const { return QString(); }
    bool downloading() const { return false; }
    double updateProgress() const { return 0.0; }
    QString updateError() const { return QString(); }
    bool updateApplied() const { return false; }

public slots:
    void check() {}
    void open() {}
    void download() {}
    void apply() {}

signals:
    void updateChanged();
    void downloadingChanged();
    void progressChanged();
    void errorChanged();
    void quitRequested();
};

#endif
