#include "CurseForgeClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrlQuery>

CurseForgeClient::CurseForgeClient(QObject *parent)
    : BasePlatformClient(Platform::CurseForge, parent)
{
}

QUrl CurseForgeClient::buildSearchUrl(const QString &query) const
{
    QUrl url(QStringLiteral("https://api.curseforge.com/v1/mods/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("gameId"), QStringLiteral("432"));
    q.addQueryItem(QStringLiteral("classId"), QStringLiteral("6"));
    q.addQueryItem(QStringLiteral("searchFilter"), query);
    q.addQueryItem(QStringLiteral("sortField"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("desc"));
    q.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("10"));
    q.addQueryItem(QStringLiteral("index"), QStringLiteral("0"));
    url.setQuery(q);
    return url;
}

QHash<QByteArray, QByteArray> CurseForgeClient::requestHeaders() const
{
    QHash<QByteArray, QByteArray> headers;
    headers.insert(QByteArrayLiteral("x-api-key"),
                   currentConfig().curseForgeApiKey.toUtf8());
    return headers;
}

bool CurseForgeClient::validateCredentials(QString *error) const
{
    if (currentConfig().curseForgeApiKey.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("未配置 CurseForge API Key，已跳过 CurseForge 检索");
        return false;
    }
    return true;
}

bool CurseForgeClient::parseSearchResponse(const QByteArray &body, const QUrl &url,
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
    if (!root.contains(QStringLiteral("data"))) {
        if (error)
            *error = QStringLiteral("响应中缺少 data 字段");
        return false;
    }

    QVector<SearchCandidate> candidates;
    QSet<QString> seenIds;
    const QJsonArray data = root.value(QStringLiteral("data")).toArray();
    for (const QJsonValue &value : data) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        SearchCandidate candidate;
        candidate.name = object.value(QStringLiteral("name")).toString().trimmed();
        const QJsonValue idValue = object.value(QStringLiteral("id"));
        candidate.id = idValue.isString() ? idValue.toString()
                                          : QString::number(idValue.toVariant().toLongLong());

        const QJsonArray authors = object.value(QStringLiteral("authors")).toArray();
        if (!authors.isEmpty()) {
            const QJsonValue first = authors.first();
            if (first.isObject())
                candidate.author =
                    first.toObject().value(QStringLiteral("name")).toString().trimmed();
            else if (first.isString())
                candidate.author = first.toString().trimmed();
        }

        const QJsonValue licenseValue = object.value(QStringLiteral("license"));
        if (licenseValue.isString()) {
            candidate.license = licenseValue.toString().trimmed();
        } else if (licenseValue.isObject()) {
            const QJsonObject licenseObject = licenseValue.toObject();
            candidate.license = licenseObject.value(QStringLiteral("name")).toString().trimmed();
            if (candidate.license.isEmpty())
                candidate.license = licenseObject.value(QStringLiteral("slug")).toString().trimmed();
        }
        if (candidate.license.isEmpty())
            candidate.license = object.value(QStringLiteral("licenseName")).toString().trimmed();

        const QJsonObject links = object.value(QStringLiteral("links")).toObject();
        candidate.url = links.value(QStringLiteral("websiteUrl")).toString().trimmed();
        if (candidate.url.isEmpty()) {
            const QString slug = object.value(QStringLiteral("slug")).toString().trimmed();
            if (!slug.isEmpty()) {
                QUrl projectUrl(QStringLiteral("https://www.curseforge.com/minecraft/mc-mods/"));
                projectUrl.setPath(QStringLiteral("/minecraft/mc-mods/") + slug);
                candidate.url = projectUrl.toString();
            }
        }

        if (candidate.name.isEmpty() || candidate.id.isEmpty() || candidate.id == QStringLiteral("0")
            || seenIds.contains(candidate.id)) {
            continue;
        }
        seenIds.insert(candidate.id);
        candidates.append(candidate);
    }

    result->candidates = candidates;
    if (candidates.isEmpty()) {
        result->state = SearchState::NotFound;
        result->error = QStringLiteral("没有匹配的 CurseForge 项目");
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

QString CurseForgeClient::httpErrorText(int statusCode, const QByteArray &body) const
{
    Q_UNUSED(body);
    switch (statusCode) {
    case 400:
        return QStringLiteral("CurseForge 请求参数无效（HTTP 400）");
    case 401:
    case 403:
        return QStringLiteral(
            "CurseForge API Key 无效、已过期或没有权限（HTTP %1），请在 CurseForge 开发者控制台检查")
            .arg(statusCode);
    case 404:
        return QStringLiteral("CurseForge API 端点或资源不存在（HTTP 404）");
    case 429:
        return QStringLiteral("CurseForge 请求过于频繁（HTTP 429）");
    default:
        if (statusCode >= 500)
            return QStringLiteral("CurseForge 服务器错误（HTTP %1）").arg(statusCode);
        return QStringLiteral("CurseForge 请求失败（HTTP %1）").arg(statusCode);
    }
}

QString CurseForgeClient::displayName() const
{
    return QStringLiteral("CurseForge");
}

void CurseForgeClient::checkConnection()
{
    abortProbe();

    if (currentConfig().curseForgeApiKey.trimmed().isEmpty()) {
        reportConnection(false, QStringLiteral("未配置 CurseForge API Key"));
        return;
    }

    QUrl url(QStringLiteral("https://api.curseforge.com/v1/games/432"));
    HttpRequest *request = httpClient()->get(
        url, requestHeaders(), currentConfig().curseForge.timeoutSeconds * 1000);
    m_probe = request;
    connect(request, &HttpRequest::finished, this, [this, request](const HttpResult &http) {
        if (m_probe != request)
            return;
        m_probe = nullptr;
        if (http.statusCode >= 200 && http.statusCode < 300) {
            reportConnection(true, QStringLiteral("CurseForge API Key 有效，连接正常"),
                             http.statusCode);
            return;
        }
        QString message;
        if (http.statusCode == 0)
            message = transportErrorText(http);
        else
            message = httpErrorText(http.statusCode, http.body);
        reportConnection(false, message, http.statusCode);
    });
}
