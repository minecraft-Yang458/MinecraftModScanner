#pragma once

#include "AppConfig.h"
#include "HttpClient.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QTimer>

struct McModWikiResult
{
    QString tag;
    QString name;
    QString url;
    QString title;
    QString error;
    bool matched = false;
    bool fallbackSearch = false;
};

class McModWikiClient : public QObject
{
    Q_OBJECT
public:
    explicit McModWikiClient(QObject *parent = nullptr);

    void configure(const AppConfig &config);
    void lookup(const QString &tag, const QString &name);
    void cancelAll();
    bool busy() const;

signals:
    void lookupFinished(const McModWikiResult &result);

private:
    struct Task
    {
        QString tag;
        QString name;
        int attempts = 0;
        qint64 readyAtMs = 0;
    };

    void pump();
    void startTask(const Task &task);
    void finishTask(const Task &task, const McModWikiResult &result);
    void schedulePump(int delayMs);
    McModWikiResult parseResponse(const QByteArray &body, const Task &task) const;
    static QString searchPageUrl(const QString &name);
    static QString apiUrl(const QString &name);
    static QString normalized(const QString &value);

    AppConfig m_config;
    HttpClient *m_http = nullptr;
    QQueue<Task> m_queue;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_clock;
    QElapsedTimer m_lastStart;
    HttpRequest *m_currentRequest = nullptr;
    Task m_currentTask;
    QHash<QString, McModWikiResult> m_cache;
    quint64 m_generation = 0;
    bool m_cancelled = false;
};
