#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

enum class Platform {
    Modrinth = 0,
    CurseForge = 1,
};

inline constexpr int kPlatformCount = 2;

enum class SearchState {
    NotSearched,
    Searching,
    RetryWaiting,
    Success,
    NotFound,
    Failed,
    Skipped,
    Cancelled,
};

QString platformName(Platform platform);
QString platformShortName(Platform platform);
QString stateText(SearchState state);
QString stateSummaryText(SearchState state);
Platform platformFromInt(int value);
int platformToInt(Platform platform);

struct SearchCandidate
{
    QString name;
    QString id;
    QString author;
    QString license;
    QString url;
};

struct PlatformResult
{
    SearchState state = SearchState::NotSearched;
    QString name;
    QString id;
    QString author;
    QString license;
    QString url;

    QString error;
    int httpStatus = 0;
    int attempts = 0; // 已发出的总请求次数（含第一次）
    QString requestUrl;
    qint64 elapsedMs = 0;
    QDateTime timestamp;
    QByteArray rawResponse;
    QVector<SearchCandidate> candidates;

    bool isSuccess() const { return state == SearchState::Success; }
    bool hasTerminalState() const;
    int retryCount() const { return attempts > 0 ? attempts - 1 : 0; }
    QString displayLicense() const;
};

struct ModEntry
{
    quint64 uid = 0;
    QString originalFileName;
    QString baseCleanName;
    QString searchName;
    QString cleanerNote;
    QString rowNote;
    bool cleanAnomaly = false;
    bool manualRow = false;

    PlatformResult results[kPlatformCount];

    QString mcmodUrl;
    QString mcmodTitle;
    QString mcmodStatus;

    PlatformResult &result(Platform platform)
    {
        return results[platformToInt(platform)];
    }
    const PlatformResult &result(Platform platform) const
    {
        return results[platformToInt(platform)];
    }
};
