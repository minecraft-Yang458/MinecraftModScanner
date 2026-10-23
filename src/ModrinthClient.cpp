#include "ModrinthClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrlQuery>

ModrinthClient::ModrinthClient(QObject *parent)
    : BasePlatformClient(Platform::Modrinth, parent)
{
}

QUrl ModrinthClient::buildSearchUrl(const QString &query) const
{
    QUrl url(QStringLiteral("https://api.modrinth.com/v2/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"), query);
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
    q.addQueryItem(QStringLiteral("index"), QStringLiteral("relevance"));
    url.setQuery(q);
    return url;
}

QHash<QByteArray, QByteArray> ModrinthClient::requestHeaders() const
{
    return {};
}

bool ModrinthClient::validateCredentials(QString *error) const
{
    Q_UNUSED(error);
    return true;
}

bool ModrinthClient::parseSearchResponse(const QByteArray &body, const QUrl &url,
                                         const QString &query, PlatformResult *result,
                                         QString *error) const
{
    Q_UNUSED(url);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("JSON 无效：%1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    if (!root.contains(QStringLiteral("hits"))) {
        if (error)
            *error = QStringLiteral("响应中缺少 hits 字段");
        return false;
    }

    QVector<SearchCandidate> candidates;
    QSet<QString> seenIds;
    const QJsonArray hits = root.value(QStringLiteral("hits")).toArray();
    for (const QJsonValue &value : hits) {
        if (!value.isObject())
            continue;
        const QJsonObject hit = value.toObject();
        SearchCandidate candidate;
        candidate.name = hit.value(QStringLiteral("title")).toString().trimmed();
        candidate.id = hit.value(QStringLiteral("project_id")).toString().trimmed();
        candidate.author = hit.value(QStringLiteral("author")).toString().trimmed();

        const QJsonValue licenseValue = hit.value(QStringLiteral("license"));
        if (licenseValue.isString()) {
            candidate.license = licenseValue.toString().trimmed();
        } else if (licenseValue.isObject()) {
            const QJsonObject licenseObject = licenseValue.toObject();
            candidate.license = licenseObject.value(QStringLiteral("name")).toString();
            if (candidate.license.isEmpty())
                candidate.license = licenseObject.value(QStringLiteral("id")).toString();
        }

        const QString slug = hit.value(QStringLiteral("slug")).toString().trimmed();
        const QString path = slug.isEmpty() ? candidate.id : slug;
        if (!path.isEmpty()) {
            QUrl projectUrl(QStringLiteral("https://modrinth.com/mod/"));
            projectUrl.setPath(QStringLiteral("/mod/") + path);
            candidate.url = projectUrl.toString();
        }

        if (candidate.name.isEmpty() || candidate.id.isEmpty()
            || seenIds.contains(candidate.id)) {
            continue;
        }
        seenIds.insert(candidate.id);
        candidates.append(candidate);
    }

    result->candidates = candidates;
    if (candidates.isEmpty()) {
        result->state = SearchState::NotFound;
        result->error = QStringLiteral("没有匹配的 Modrinth 项目");
        return true;
    }

    const SearchCandidate chosen = primaryCandidate(candidates, query);
    result->state = SearchState::Success;
    result->name = chosen.name;
    result->id = chosen.id;
    result->author = chosen.author;
    result->license = chosen.license;
    result->url = chosen.url;
    result->error.clear();
    return true;
}

QString ModrinthClient::httpErrorText(int statusCode, const QByteArray &body) const
{
    Q_UNUSED(body);
    switch (statusCode) {
    case 400:
        return QStringLiteral("Modrinth 请求参数无效（HTTP 400）");
    case 401:
        return QStringLiteral("Modrinth 需要身份验证（HTTP 401）");
    case 403:
        return QStringLiteral("Modrinth 拒绝访问（HTTP 403），可能被防火墙或代理拦截");
    case 404:
        return QStringLiteral("Modrinth API 端点不存在（HTTP 404）");
    case 429:
        return QStringLiteral("Modrinth 请求过于频繁（HTTP 429）");
    default:
        if (statusCode >= 500)
            return QStringLiteral("Modrinth 服务器错误（HTTP %1）").arg(statusCode);
        return QStringLiteral("Modrinth 请求失败（HTTP %1）").arg(statusCode);
    }
}

QString ModrinthClient::displayName() const
{
    return QStringLiteral("Modrinth");
}

void ModrinthClient::checkConnection()
{
    abortProbe();

    QUrl url(QStringLiteral("https://api.modrinth.com/v2/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"), QStringLiteral("sodium"));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
    url.setQuery(q);

    HttpRequest *request =
        httpClient()->get(url, requestHeaders(), currentConfig().modrinth.timeoutSeconds * 1000);
    m_probe = request;
    connect(request, &HttpRequest::finished, this, [this, request](const HttpResult &http) {
        if (m_probe != request)
            return;
        m_probe = nullptr;
        if (http.statusCode >= 200 && http.statusCode < 300) {
            reportConnection(true, QStringLiteral("Modrinth 连接正常"), http.statusCode);
            return;
        }
        QString message;
        if (http.statusCode == 0) {
            message = transportErrorText(http);
        } else {
            message = httpErrorText(http.statusCode, http.body);
        }
        reportConnection(false, message, http.statusCode);
    });
}
