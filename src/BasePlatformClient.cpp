#include "BasePlatformClient.h"

#include "AppLog.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <climits>

namespace {
QString retryReasonForStatus(int status)
{
    if (status == 429)
        return QStringLiteral("请求被限流（HTTP 429）");
    if (status == 408)
        return QStringLiteral("请求超时（HTTP 408）");
    if (status == 425)
        return QStringLiteral("请求过早（HTTP 425）");
    if (status >= 500)
        return QStringLiteral("平台服务器错误（HTTP %1）").arg(status);
    return QStringLiteral("HTTP %1").arg(status);
}
} // namespace

BasePlatformClient::BasePlatformClient(Platform platform, QObject *parent)
    : QObject(parent)
    , m_platform(platform)
    , m_http(new HttpClient(this))
{
    m_clock.start();
    m_pumpTimer = new QTimer(this);
    m_pumpTimer->setSingleShot(true);
    connect(m_pumpTimer, &QTimer::timeout, this, &BasePlatformClient::pump);
}

BasePlatformClient::~BasePlatformClient()
{
    cancelAll();
}

void BasePlatformClient::configure(const AppConfig &config)
{
    m_config = config;
    m_config.sanitize();
    m_http->configure(m_config);
}

int BasePlatformClient::platformTimeoutMs() const
{
    const PlatformSettings &settings =
        m_platform == Platform::Modrinth ? m_config.modrinth : m_config.curseForge;
    return settings.timeoutSeconds * 1000;
}

QString BasePlatformClient::transportErrorText(const HttpResult &http) const
{
    if (http.timedOut) {
        const PlatformSettings &settings =
            m_platform == Platform::Modrinth ? m_config.modrinth : m_config.curseForge;
        return QStringLiteral("请求超时（%1 秒）").arg(settings.timeoutSeconds);
    }

    switch (http.networkError) {
    case QNetworkReply::HostNotFoundError:
        return QStringLiteral("无法解析主机 %1，请检查网络或代理设置")
            .arg(http.url.host());
    case QNetworkReply::ConnectionRefusedError:
        return QStringLiteral("连接被拒绝（%1），请检查代理设置").arg(http.url.host());
    case QNetworkReply::RemoteHostClosedError:
        return QStringLiteral("远端服务器提前关闭了连接");
    case QNetworkReply::SslHandshakeFailedError:
        return QStringLiteral("TLS/SSL 握手失败，请检查系统时间、代理或网络环境");
    case QNetworkReply::TimeoutError:
        return QStringLiteral("网络请求超时");
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
        return QStringLiteral("无法连接代理服务器，请检查代理地址和端口");
    case QNetworkReply::OperationCanceledError:
        return http.aborted ? QStringLiteral("请求已取消") : QStringLiteral("请求被中止");
    default:
        break;
    }

    const QString detail = http.errorString.trimmed();
    if (detail.isEmpty())
        return QStringLiteral("网络错误（错误码 %1）").arg(static_cast<int>(http.networkError));
    return QStringLiteral("网络错误：%1").arg(detail);
}

QString BasePlatformClient::normalizedForMatch(const QString &value)
{
    QString result = value.toLower().trimmed();
    result.remove(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N}]+)")));
    return result;
}

SearchCandidate BasePlatformClient::primaryCandidate(
    const QVector<SearchCandidate> &candidates, const QString &query)
{
    if (candidates.isEmpty())
        return {};

    const QString normalizedQuery = normalizedForMatch(query);
    for (const SearchCandidate &candidate : candidates) {
        if (normalizedForMatch(candidate.name) == normalizedQuery)
            return candidate;
    }
    return candidates.first();
}

QString BasePlatformClient::submitSearch(const QString &query, const QString &tag)
{
    if (query.trimmed().isEmpty())
        return {};

    m_cancelled = false;
    Task task;
    task.tag = tag.trimmed();
    if (task.tag.isEmpty())
        task.tag = QStringLiteral("%1-%2").arg(platformShortName(m_platform)).arg(++m_tagCounter);
    task.query = query;
    m_queue.enqueue(task);
    schedulePump(0);
    return task.tag;
}

void BasePlatformClient::cancelAll()
{
    m_cancelled = true;
    m_pumpTimer->stop();
    m_queue.clear();

    const auto active = m_active;
    for (auto it = active.constBegin(); it != active.constEnd(); ++it) {
        HttpRequest *request = it.key();
        QObject::disconnect(request, nullptr, this, nullptr);
        request->abort();
    }
    m_active.clear();
    abortProbe();
}

void BasePlatformClient::abortProbe()
{
    if (!m_probe)
        return;
    HttpRequest *request = m_probe;
    m_probe = nullptr;
    QObject::disconnect(request, nullptr, this, nullptr);
    request->abort();
}

bool BasePlatformClient::isBusy() const
{
    return !m_queue.isEmpty() || !m_active.isEmpty() || m_probe != nullptr;
}

int BasePlatformClient::pendingCount() const
{
    return m_queue.size() + m_active.size();
}

void BasePlatformClient::reportConnection(bool ok, const QString &message, int httpStatus)
{
    emit connectionChecked(platformToInt(m_platform), ok, message, httpStatus);
}

void BasePlatformClient::schedulePump(int delayMs)
{
    if (m_cancelled)
        return;
    const int delay = qMax(0, delayMs);
    if (!m_pumpTimer->isActive() || delay < m_pumpTimer->remainingTime())
        m_pumpTimer->start(delay);
}

int BasePlatformClient::firstReadyIndex(qint64 nowMs) const
{
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).readyAtMs <= nowMs)
            return i;
    }
    return -1;
}

void BasePlatformClient::pump()
{
    if (m_cancelled)
        return;
    if (m_pumping) {
        m_pumpAgain = true;
        return;
    }

    m_pumping = true;
    do {
        m_pumpAgain = false;
        if (m_cancelled)
            break;

        const PlatformSettings &settings =
            m_platform == Platform::Modrinth ? m_config.modrinth : m_config.curseForge;

        while (m_active.size() < settings.concurrency && !m_queue.isEmpty()) {
            if (m_cancelled)
                break;
            const qint64 now = m_clock.elapsed();
            const int index = firstReadyIndex(now);
            if (index < 0)
                break;

            if (m_lastStart.isValid()) {
                const qint64 sinceLast = m_clock.elapsed() - m_lastStart.elapsed();
                const qint64 wait = settings.minIntervalMs - sinceLast;
                if (wait > 0) {
                    schedulePump(static_cast<int>(wait) + 2);
                    break;
                }
            }

            Task task = m_queue.takeAt(index);
            startTask(task);
        }

        if (m_cancelled)
            break;

        if (!m_queue.isEmpty() && m_active.size() < settings.concurrency) {
            const qint64 now = m_clock.elapsed();
            qint64 wait = LLONG_MAX;
            for (const Task &task : m_queue) {
                const qint64 remaining = task.readyAtMs - now;
                if (remaining <= 0) {
                    wait = 0;
                    break;
                }
                wait = qMin(wait, remaining);
            }

            if (m_lastStart.isValid() && settings.minIntervalMs > 0) {
                const qint64 remaining =
                    settings.minIntervalMs - (m_clock.elapsed() - m_lastStart.elapsed());
                if (remaining > 0)
                    wait = qMin(wait, remaining);
            }

            if (wait != LLONG_MAX) {
                // wait == 0 表示此刻可以开始；上面的主循环只有在并发已满时才会留下任务。
                schedulePump(static_cast<int>(qMax<qint64>(1, wait)) + 2);
            }
        }
    } while (m_pumpAgain);
    m_pumping = false;
}

void BasePlatformClient::startTask(const Task &task)
{
    Task attemptTask = task;
    ++attemptTask.attempts;

    const PlatformSettings &settings =
        m_platform == Platform::Modrinth ? m_config.modrinth : m_config.curseForge;

    QString credentialError;
    if (!validateCredentials(&credentialError)) {
        PlatformResult result;
        result.state = SearchState::Failed;
        result.error = credentialError;
        result.attempts = 0;
        result.timestamp = QDateTime::currentDateTime();
        finishTask(attemptTask, result);
        return;
    }

    const QUrl url = buildSearchUrl(attemptTask.query);
    emit searchStarted(attemptTask.tag, platformToInt(m_platform), attemptTask.query,
                       attemptTask.attempts, settings.maxAttempts);

    HttpRequest *request =
        m_http->get(url, requestHeaders(), settings.timeoutSeconds * 1000);
    m_active.insert(request, attemptTask);
    m_lastStart.start();

    connect(request, &HttpRequest::finished, this,
            [this, request, attemptTask](const HttpResult &http) {
                if (!m_active.contains(request))
                    return;
                m_active.remove(request);
                handleAttempt(attemptTask, http);
            });
}

void BasePlatformClient::logSummary(const QString &query, const HttpResult &http,
                                    int attempt) const
{
    qInfo().noquote() << QStringLiteral(
                             "%1 search \"%2\": HTTP %3, %4 ms, attempt %5, networkError=%6")
                             .arg(displayName(), query)
                             .arg(http.statusCode)
                             .arg(http.elapsedMs)
                             .arg(attempt)
                             .arg(static_cast<int>(http.networkError));
}

void BasePlatformClient::handleAttempt(const Task &task, const HttpResult &http)
{
    logSummary(task.query, http, task.attempts);

    Task current = task;
    current.totalElapsedMs += http.elapsedMs;

    PlatformResult result;
    result.attempts = current.attempts;
    result.httpStatus = http.statusCode;
    result.timestamp = http.timestamp;
    result.requestUrl = http.url.toString();
    result.rawResponse = http.body;
    result.elapsedMs = current.totalElapsedMs;

    bool retryable = false;
    bool terminal = false;
    QString retryReason;

    if (http.statusCode >= 200 && http.statusCode < 300) {
        QString parseError;
        if (parseSearchResponse(http.body, http.url, task.query, &result, &parseError)) {
            if (result.state == SearchState::Success
                || result.state == SearchState::NotFound) {
                result.attempts = current.attempts;
                result.elapsedMs = current.totalElapsedMs;
                result.rawResponse = http.body;
                finishTask(current, result);
                return;
            }
            result.state = SearchState::Failed;
            result.error = parseError.isEmpty() ? QStringLiteral("响应内容无法识别")
                                                 : parseError;
            terminal = true;
        } else {
            retryable = true;
            retryReason = parseError.isEmpty() ? QStringLiteral("响应解析失败")
                                                : QStringLiteral("响应解析失败：%1").arg(parseError);
        }
    } else if (http.statusCode == 404) {
        result.state = SearchState::NotFound;
        result.error = QStringLiteral("未找到匹配的模组（HTTP 404）");
        terminal = true;
    } else if (http.statusCode == 429) {
        retryable = true;
        retryReason = retryReasonForStatus(429);
    } else if (http.statusCode == 408 || http.statusCode == 425
               || (http.statusCode >= 500 && http.statusCode < 600)) {
        retryable = true;
        retryReason = retryReasonForStatus(http.statusCode);
    } else if (http.statusCode >= 400) {
        result.state = SearchState::Failed;
        result.error = httpErrorText(http.statusCode, http.body);
        terminal = true;
    } else if (http.timedOut || http.networkError != QNetworkReply::NoError
               || http.statusCode == 0) {
        retryable = true;
        retryReason = transportErrorText(http);
    } else {
        result.state = SearchState::Failed;
        result.error = QStringLiteral("无法识别的 HTTP 响应（状态码 %1）").arg(http.statusCode);
        terminal = true;
    }

    if (retryable && !terminal) {
        const PlatformSettings &settings =
            m_platform == Platform::Modrinth ? m_config.modrinth : m_config.curseForge;
        if (current.attempts < settings.maxAttempts) {
            int delayMs = 0;
            if (http.statusCode == 429 && http.retryAfterMs > 0) {
                delayMs = http.retryAfterMs;
                retryReason += QStringLiteral("，遵守 Retry-After");
            } else {
                const int exponent = qMin(current.attempts - 1, 6);
                delayMs = qMin(10000, 800 * (1 << exponent));
                delayMs += QRandomGenerator::global()->bounded(250);
            }
            enqueueRetry(current, delayMs, retryReason);
            return;
        }

        result.state = SearchState::Failed;
        result.error =
            QStringLiteral("%1（已尝试 %2 次，达到最大尝试次数）").arg(retryReason).arg(current.attempts);
        finishTask(current, result);
        return;
    }

    if (terminal) {
        finishTask(current, result);
        return;
    }

    result.state = SearchState::Failed;
    result.error = QStringLiteral("请求失败（未知原因）");
    finishTask(current, result);
}

void BasePlatformClient::enqueueRetry(Task task, int delayMs, const QString &reason)
{
    const PlatformSettings &settings =
        m_platform == Platform::Modrinth ? m_config.modrinth : m_config.curseForge;
    const int nextAttempt = task.attempts + 1;
    task.readyAtMs = m_clock.elapsed() + qMax(0, delayMs);
    m_queue.enqueue(task);
    emit searchRetry(task.tag, platformToInt(m_platform), task.query, nextAttempt,
                     settings.maxAttempts, delayMs, reason);
    schedulePump(delayMs + 2);
    pump();
}

void BasePlatformClient::finishTask(const Task &task, PlatformResult result)
{
    if (m_cancelled)
        return;
    result.attempts = task.attempts;
    result.elapsedMs = task.totalElapsedMs;
    if (result.timestamp.isNull())
        result.timestamp = QDateTime::currentDateTime();
    if (!result.isSuccess() && result.state != SearchState::NotFound
        && result.error.trimmed().isEmpty()) {
        result.error = QStringLiteral("请求失败");
    }
    emit searchFinished(task.tag, platformToInt(m_platform), result);
    pump();
}
