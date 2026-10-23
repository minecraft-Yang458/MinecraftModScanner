#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QUrl>

class QNetworkAccessManager;
class QTimer;
struct AppConfig;

struct HttpResult
{
    QUrl url;
    int statusCode = 0;
    QByteArray body;
    QString errorString;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    bool timedOut = false;
    bool aborted = false;
    qint64 elapsedMs = 0;
    QDateTime timestamp;
    QHash<QByteArray, QByteArray> headers;
    int retryAfterMs = 0;
};

class HttpClient;

class HttpRequest : public QObject
{
    Q_OBJECT
public:
    HttpRequest(HttpClient *client, const QUrl &url,
                const QHash<QByteArray, QByteArray> &extraHeaders, int timeoutMs,
                QObject *parent = nullptr);
    ~HttpRequest() override;

    void abort();
    bool isFinished() const { return m_finished; }
    QUrl url() const { return m_url; }

signals:
    void finished(const HttpResult &result);

private slots:
    void onReplyFinished();
    void onTimeout();

private:
    void finishOnce();

    HttpClient *m_client = nullptr;
    QUrl m_url;
    QPointer<QNetworkReply> m_reply;
    QTimer *m_timer = nullptr;
    QElapsedTimer *m_elapsed = nullptr;
    int m_timeoutMs = 0;
    bool m_timedOut = false;
    bool m_aborted = false;
    bool m_finished = false;
};

class HttpClient : public QObject
{
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);
    ~HttpClient() override;

    void configure(const AppConfig &config);
    HttpRequest *get(const QUrl &url,
                     const QHash<QByteArray, QByteArray> &extraHeaders = {},
                     int timeoutMs = 15000);
    void abortAll();

private:
    friend class HttpRequest;
    QNetworkAccessManager *m_manager = nullptr;
    QSet<HttpRequest *> m_active;
    QString m_userAgent;
};
