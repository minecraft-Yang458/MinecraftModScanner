#include "HttpClient.h"

#include "AppConfig.h"

#include <QElapsedTimer>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkRequest>
#include <QTimer>
#include <QTimeZone>

namespace {
int parseRetryAfterMs(const QByteArray &raw)
{
    if (raw.trimmed().isEmpty())
        return 0;

    bool ok = false;
    const int seconds = QString::fromLatin1(raw).trimmed().toInt(&ok);
    if (ok)
        return qMax(0, seconds) * 1000;

    QDateTime target = QDateTime::fromString(QString::fromLatin1(raw), Qt::RFC2822Date);
    if (!target.isValid()) {
        target = QLocale::c().toDateTime(QString::fromLatin1(raw),
                                         QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
    }
    if (!target.isValid())
        return 0;
    target.setTimeZone(QTimeZone::UTC);
    const qint64 ms = QDateTime::currentDateTimeUtc().msecsTo(target.toUTC());
    return static_cast<int>(qBound<qint64>(0LL, ms, 10LL * 60 * 1000));
}
} // namespace

HttpRequest::HttpRequest(HttpClient *client, const QUrl &url,
                         const QHash<QByteArray, QByteArray> &extraHeaders, int timeoutMs,
                         QObject *parent)
    : QObject(parent ? parent : client)
    , m_client(client)
    , m_url(url)
    , m_timeoutMs(qMax(1, timeoutMs))
{
    m_elapsed = new QElapsedTimer;
    m_elapsed->start();

    if (!m_client || !m_client->m_manager) {
        QTimer::singleShot(0, this, [this]() {
            if (!m_finished) {
                HttpResult result;
                result.url = m_url;
                result.errorString = QStringLiteral("网络管理器未初始化");
                result.timestamp = QDateTime::currentDateTime();
                m_finished = true;
                emit finished(result);
                deleteLater();
            }
        });
        return;
    }

    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_client->m_userAgent);
    request.setRawHeader("Accept", "application/json, text/html;q=0.9, */*;q=0.8");
    request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    request.setRawHeader("Accept-Encoding", "identity");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);

    for (auto it = extraHeaders.constBegin(); it != extraHeaders.constEnd(); ++it)
        request.setRawHeader(it.key(), it.value());
    // 统一使用当前配置的 UA，extraHeaders 不得覆盖。
    request.setHeader(QNetworkRequest::UserAgentHeader, m_client->m_userAgent);

    m_reply = m_client->m_manager->get(request);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &HttpRequest::onTimeout);
    m_timer->start(m_timeoutMs);

    connect(m_reply, &QNetworkReply::finished, this, &HttpRequest::onReplyFinished);
}

HttpRequest::~HttpRequest()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        if (!m_reply->isFinished())
            m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    delete m_elapsed;
    m_elapsed = nullptr;
}

void HttpRequest::abort()
{
    if (m_finished)
        return;
    m_aborted = true;
    if (m_reply && !m_reply->isFinished())
        m_reply->abort();
    else
        finishOnce();
}

void HttpRequest::onTimeout()
{
    if (m_finished)
        return;
    m_timedOut = true;
    if (m_reply && !m_reply->isFinished())
        m_reply->abort();
    else
        finishOnce();
}

void HttpRequest::onReplyFinished()
{
    finishOnce();
}

void HttpRequest::finishOnce()
{
    if (m_finished)
        return;
    m_finished = true;
    if (m_timer)
        m_timer->stop();

    HttpResult result;
    result.url = m_url;
    result.timestamp = QDateTime::currentDateTime();
    result.elapsedMs = m_elapsed ? m_elapsed->elapsed() : 0;

    if (m_reply) {
        result.statusCode =
            m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = m_reply->readAll();
        result.networkError = m_reply->error();
        result.errorString = m_reply->errorString();

        const auto pairs = m_reply->rawHeaderPairs();
        for (const auto &pair : pairs)
            result.headers.insert(pair.first.toLower(), pair.second);
        result.retryAfterMs =
            parseRetryAfterMs(result.headers.value(QByteArrayLiteral("retry-after")));

        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_timedOut) {
        result.networkError = QNetworkReply::TimeoutError;
        result.errorString = QStringLiteral("请求超时（%1 秒）").arg(m_timeoutMs / 1000);
    }
    result.aborted = m_aborted;

    if (m_client)
        m_client->m_active.remove(this);

    emit finished(result);
    deleteLater();
}

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

HttpClient::~HttpClient()
{
    // 先删除仍在途的 HttpRequest，再让 QObject 子对象销毁 QNetworkAccessManager，
    // 避免 HttpRequest 中的 reply 指针在管理器之后失效。
    const auto active = m_active;
    m_active.clear();
    for (HttpRequest *request : active) {
        if (request) {
            QObject::disconnect(request, nullptr, this, nullptr);
            delete request;
        }
    }
}

void HttpClient::configure(const AppConfig &config)
{
    m_userAgent = config.effectiveUserAgent();
    if (m_userAgent.trimmed().isEmpty())
        m_userAgent = AppConfig::defaultUserAgent();

    if (config.useSystemProxy) {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        m_manager->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
    } else {
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy proxy(QNetworkProxy::HttpProxy, config.proxyHost, config.proxyPort);
        m_manager->setProxy(proxy);
    }
}

HttpRequest *HttpClient::get(const QUrl &url,
                             const QHash<QByteArray, QByteArray> &extraHeaders,
                             int timeoutMs)
{
    int timeout = qMax(1, timeoutMs);
    auto *request = new HttpRequest(this, url, extraHeaders, timeout, this);
    m_active.insert(request);
    connect(request, &QObject::destroyed, this, [this, request]() {
        m_active.remove(request);
    });
    return request;
}

void HttpClient::abortAll()
{
    const auto active = m_active;
    for (HttpRequest *request : active) {
        if (request)
            request->abort();
    }
}
