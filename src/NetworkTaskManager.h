#pragma once

#include "AppConfig.h"
#include "SearchTypes.h"

#include <QObject>
#include <QSet>

class CurseForgeClient;
class ModrinthClient;

class NetworkTaskManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkTaskManager(QObject *parent = nullptr);

    void configure(const AppConfig &config);
    void submitSearch(Platform platform, const QString &query, const QString &tag);
    void cancelAll();
    void checkConnections(bool modrinth, bool curseForge);
    void cancelChecks();
    bool busy() const;

signals:
    void searchStarted(const QString &tag, int platform, const QString &query, int attempt,
                       int maxAttempts);
    void searchRetry(const QString &tag, int platform, const QString &query, int nextAttempt,
                     int maxAttempts, int delayMs, const QString &reason);
    void searchFinished(const QString &tag, int platform, const PlatformResult &result);
    void connectionChecked(int platform, bool ok, const QString &message, int httpStatus);
    void connectionCheckFinished();

private slots:
    void onConnectionChecked(int platform, bool ok, const QString &message, int httpStatus);

private:
    ModrinthClient *m_modrinth = nullptr;
    CurseForgeClient *m_curseForge = nullptr;
    QSet<int> m_pendingChecks;
    bool m_checksRunning = false;
};
