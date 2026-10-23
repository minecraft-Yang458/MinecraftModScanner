#pragma once

#include "AppConfig.h"
#include "HttpClient.h"
#include "SearchTypes.h"

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QUrl>

class BasePlatformClient : public QObject
{
    Q_OBJECT
public:
    explicit BasePlatformClient(Platform platform, QObject *parent = nullptr);
    ~BasePlatformClient() override;

    Platform platform() const { return m_platform; }
    void configure(const AppConfig &config);

    QString submitSearch(const QString &query, const QString &tag = QString());
    void cancelAll();
    void abortProbe();

    bool isBusy() const;
    int pendingCount() const;

    virtual void checkConnection() = 0;

signals:
    void searchStarted(const QString &tag, int platform, const QString &query, int attempt,
                       int maxAttempts);
    void searchRetry(const QString &tag, int platform, const QString &query, int nextAttempt,
                     int maxAttempts, int delayMs, const QString &reason);
    void searchFinished(const QString &tag, int platform, const PlatformResult &result);
    void connectionChecked(int platform, bool ok, const QString &message, int httpStatus);

protected:
    struct Task
    {
        QString tag;
        QString query;
        int attempts = 0;
        qint64 readyAtMs = 0;
        qint64 totalElapsedMs = 0;
    };

    const AppConfig &currentConfig() const { return m_config; }
    HttpClient *httpClient() const { return m_http; }
    int platformTimeoutMs() const;
    virtual QUrl buildSearchUrl(const QString &query) const = 0;
    virtual QHash<QByteArray, QByteArray> requestHeaders() const = 0;
    virtual bool validateCredentials(QString *error) const = 0;
    virtual bool parseSearchResponse(const QByteArray &body, const QUrl &url,
                                     const QString &query, PlatformResult *result,
                                     QString *error) const = 0;
    virtual QString httpErrorText(int statusCode, const QByteArray &body) const = 0;
    virtual QString displayName() const = 0;

    QString transportErrorText(const HttpResult &http) const;
    void reportConnection(bool ok, const QString &message, int httpStatus = 0);

    HttpRequest *m_probe = nullptr;
    static QString normalizedForMatch(const QString &value);
    static SearchCandidate primaryCandidate(const QVector<SearchCandidate> &candidates,
                                            const QString &query);

private:
    void pump();
    void schedulePump(int delayMs = 0);
    void startTask(const Task &task);
    void handleAttempt(const Task &task, const HttpResult &http);
    void enqueueRetry(Task task, int delayMs, const QString &reason);
    void finishTask(const Task &task, PlatformResult result);
    int firstReadyIndex(qint64 nowMs) const;
    void logSummary(const QString &query, const HttpResult &http, int attempt) const;

    Platform m_platform;
    AppConfig m_config;
    HttpClient *m_http = nullptr;
    QQueue<Task> m_queue;
    QHash<HttpRequest *, Task> m_active;
    QTimer *m_pumpTimer = nullptr;
    QElapsedTimer m_clock;
    QElapsedTimer m_lastStart;
    quint64 m_tagCounter = 0;
    bool m_pumping = false;
    bool m_pumpAgain = false;
    bool m_cancelled = false;
};
