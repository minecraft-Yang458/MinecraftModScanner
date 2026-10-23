#include "McModWikiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace {
constexpr int kTimeoutMs = 15000;
constexpr int kMaxAttempts = 2;
constexpr int kMinIntervalMs = 1200;

QString normalizedText(const QString &value)
{
    QString result = value.toLower().trimmed();
    result.remove(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N}]+)")));
    return result;
}

QString spacedText(const QString &value)
{
    QString result = value.toLower().trimmed();
    result.replace(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N}]+)")),
                   QStringLiteral(" "));
    result.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return result.trimmed();
}
} // namespace

McModWikiClient::McModWikiClient(QObject *parent)
    : QObject(parent)
    , m_http(new HttpClient(this))
{
    m_clock.start();
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &McModWikiClient::pump);
}

void McModWikiClient::configure(const AppConfig &config)
{
    m_config = config;
    m_config.sanitize();
    m_http->configure(m_config);
}

bool McModWikiClient::busy() const
{
    return m_currentRequest != nullptr || !m_queue.isEmpty();
}

QString McModWikiClient::searchPageUrl(const QString &name)
{
    QUrl url(QStringLiteral("https://search.mcmod.cn/s"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), name);
    url.setQuery(query);
    return url.toString();
}

QString McModWikiClient::apiUrl(const QString &name)
{
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(name));
    return QStringLiteral("https://mcmod-api.zkitefly.eu.org/s/key=%1").arg(encoded);
}

QString McModWikiClient::normalized(const QString &value)
{
    return normalizedText(value);
}

void McModWikiClient::lookup(const QString &tag, const QString &name)
{
    if (name.trimmed().isEmpty())
        return;

    m_cancelled = false;
    const QString cacheKey = normalized(name);
    if (m_cache.contains(cacheKey)) {
        McModWikiResult result = m_cache.value(cacheKey);
        result.tag = tag;
        result.name = name;
        const quint64 generation = m_generation;
        QTimer::singleShot(0, this, [this, result, generation]() {
            if (!m_cancelled && generation == m_generation)
                emit lookupFinished(result);
        });
        return;
    }

    Task task;
    task.tag = tag;
    task.name = name;
    m_queue.enqueue(task);
    schedulePump(0);
}

void McModWikiClient::schedulePump(int delayMs)
{
    if (m_cancelled)
        return;
    const int delay = qMax(0, delayMs);
    if (!m_timer->isActive() || delay < m_timer->remainingTime())
        m_timer->start(delay);
}

void McModWikiClient::pump()
{
    if (m_cancelled || m_currentRequest || m_queue.isEmpty())
        return;

    const qint64 now = m_clock.elapsed();
    const Task &front = m_queue.head();
    if (front.readyAtMs > now) {
        schedulePump(static_cast<int>(front.readyAtMs - now) + 2);
        return;
    }

    if (m_lastStart.isValid()) {
        const qint64 sinceLast = m_clock.elapsed() - m_lastStart.elapsed();
        const qint64 wait = kMinIntervalMs - sinceLast;
        if (wait > 0) {
            schedulePump(static_cast<int>(wait) + 2);
            return;
        }
    }

    const Task task = m_queue.dequeue();
    startTask(task);
}

void McModWikiClient::startTask(const Task &input)
{
    Task task = input;
    ++task.attempts;
    m_currentTask = task;

    HttpRequest *request =
        m_http->get(QUrl(apiUrl(task.name)), {}, kTimeoutMs);
    m_currentRequest = request;
    m_lastStart.start();

    connect(request, &HttpRequest::finished, this, [this, request](const HttpResult &http) {
        if (m_currentRequest != request)
            return;
        m_currentRequest = nullptr;
        const Task task = m_currentTask;

        qInfo().noquote() << QStringLiteral(
                                 "MC百科 API lookup \"%1\": HTTP %2, %3 ms, attempt %4")
                                 .arg(task.name)
                                 .arg(http.statusCode)
                                 .arg(http.elapsedMs)
                                 .arg(task.attempts);

        const bool retryable = http.statusCode == 0 || http.timedOut
            || http.statusCode == 429 || http.statusCode == 408
            || http.statusCode >= 500;
        if (retryable && task.attempts < kMaxAttempts) {
            Task retry = task;
            int delay = 1500 * task.attempts;
            if (http.statusCode == 429 && http.retryAfterMs > 0)
                delay = http.retryAfterMs;
            retry.readyAtMs = m_clock.elapsed() + delay;
            m_queue.enqueue(retry);
            schedulePump(delay + 2);
            pump();
            return;
        }

        McModWikiResult result;
        if (http.statusCode >= 200 && http.statusCode < 300) {
            result = parseResponse(http.body, task);
        } else {
            result.tag = task.tag;
            result.name = task.name;
            result.url = searchPageUrl(task.name);
            result.fallbackSearch = true;
            result.error = QStringLiteral("MC百科 API 请求失败（HTTP %1），已回退搜索页")
                               .arg(http.statusCode);
            if (http.statusCode == 0)
                result.error = QStringLiteral("MC百科 API 网络请求失败，已回退搜索页");
        }

        if (result.url.isEmpty()) {
            result.tag = task.tag;
            result.name = task.name;
            result.url = searchPageUrl(task.name);
            result.fallbackSearch = true;
            if (result.error.isEmpty())
                result.error = QStringLiteral("未找到匹配的百科条目，已回退搜索页");
        }

        finishTask(task, result);
    });
}

McModWikiResult McModWikiClient::parseResponse(const QByteArray &body, const Task &task) const
{
    McModWikiResult result;
    result.tag = task.tag;
    result.name = task.name;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        result.url = searchPageUrl(task.name);
        result.fallbackSearch = true;
        result.error = QStringLiteral("MC百科 API 响应解析失败，已回退搜索页");
        return result;
    }

    const QString query = normalizedText(task.name);
    const QString querySpaced = spacedText(task.name);
    QString bestUrl;
    QString bestTitle;
    int bestScore = -1;

    const QJsonArray array = document.array();
    int order = 0;
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        const QString address = object.value(QStringLiteral("address")).toString().trimmed();
        if (!address.contains(QStringLiteral("/class/")))
            continue;

        const QString title = object.value(QStringLiteral("title")).toString().trimmed();
        const QJsonObject data = object.value(QStringLiteral("data")).toObject();
        const QString abbr = data.value(QStringLiteral("abbr")).toString().trimmed();
        const QString chinese = data.value(QStringLiteral("chinese_name")).toString().trimmed();
        const QString sub = data.value(QStringLiteral("sub_name")).toString().trimmed();

        int score = 0;
        const struct
        {
            QString text;
            int weight;
        } fields[] = {
            {abbr, 100},
            {chinese, 92},
            {sub, 88},
            {title, 72},
        };
        for (const auto &field : fields) {
            const QString tight = normalizedText(field.text);
            if (tight.isEmpty())
                continue;
            const QString spaced = spacedText(field.text);
            if (tight == query) {
                const int valueScore = field.weight + (spaced == querySpaced ? 10 : 0);
                score = qMax(score, valueScore);
            } else if (spaced == querySpaced) {
                score = qMax(score, field.weight + 6);
            } else if (tight.startsWith(query)) {
                score = qMax(score, field.weight - 10);
            } else if (tight.contains(query)) {
                score = qMax(score, field.weight - 22);
            }
        }
        if (score <= 0)
            continue;

        const QString allNames = (title + QLatin1Char(' ') + chinese + QLatin1Char(' ')
                                 + sub + QLatin1Char(' ') + abbr)
                                     .toLower();
        if (allNames.contains(QStringLiteral("legacy"))
            && !querySpaced.contains(QStringLiteral("legacy"))) {
            score -= 25;
        }
        // API 已按相关性排序；同分时保留更靠前的结果。
        score += qMax(0, 8 - order);
        ++order;

        if (score > bestScore) {
            bestScore = score;
            bestUrl = address;
            bestTitle = title;
        }
    }

    if (bestUrl.isEmpty()) {
        result.url = searchPageUrl(task.name);
        result.fallbackSearch = true;
        result.error = QStringLiteral("MC百科未找到精确匹配条目，已回退搜索页");
        return result;
    }

    if (bestUrl.startsWith(QStringLiteral("//")))
        bestUrl = QStringLiteral("https:") + bestUrl;
    else if (bestUrl.startsWith(QStringLiteral("http://")))
        bestUrl.replace(0, 7, QStringLiteral("https://"));

    result.url = bestUrl;
    result.title = bestTitle;
    result.matched = true;
    return result;
}

void McModWikiClient::finishTask(const Task &task, const McModWikiResult &result)
{
    if (m_cancelled)
        return;
    Q_UNUSED(task);
    m_cache.insert(normalized(result.name), result);
    emit lookupFinished(result);
    pump();
}

void McModWikiClient::cancelAll()
{
    ++m_generation;
    m_cancelled = true;
    m_timer->stop();
    m_queue.clear();
    if (m_currentRequest) {
        HttpRequest *request = m_currentRequest;
        m_currentRequest = nullptr;
        QObject::disconnect(request, nullptr, this, nullptr);
        request->abort();
    }
}
