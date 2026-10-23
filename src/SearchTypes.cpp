#include "SearchTypes.h"

QString platformName(Platform platform)
{
    switch (platform) {
    case Platform::Modrinth:
        return QStringLiteral("Modrinth");
    case Platform::CurseForge:
        return QStringLiteral("CurseForge");
    }
    return QStringLiteral("未知平台");
}

QString platformShortName(Platform platform)
{
    switch (platform) {
    case Platform::Modrinth:
        return QStringLiteral("MR");
    case Platform::CurseForge:
        return QStringLiteral("CF");
    }
    return QStringLiteral("?");
}

QString stateText(SearchState state)
{
    switch (state) {
    case SearchState::NotSearched:
        return QStringLiteral("未检索");
    case SearchState::Searching:
        return QStringLiteral("检索中");
    case SearchState::RetryWaiting:
        return QStringLiteral("等待重试");
    case SearchState::Success:
        return QStringLiteral("成功");
    case SearchState::NotFound:
        return QStringLiteral("404 未找到");
    case SearchState::Failed:
        return QStringLiteral("请求失败");
    case SearchState::Skipped:
        return QStringLiteral("已跳过");
    case SearchState::Cancelled:
        return QStringLiteral("已取消");
    }
    return QStringLiteral("未知状态");
}

QString stateSummaryText(SearchState state)
{
    switch (state) {
    case SearchState::Success:
        return QStringLiteral("成功");
    case SearchState::NotFound:
        return QStringLiteral("404 未找到");
    case SearchState::Failed:
        return QStringLiteral("请求失败");
    case SearchState::Skipped:
        return QStringLiteral("跳过");
    case SearchState::Cancelled:
        return QStringLiteral("用户取消");
    case SearchState::Searching:
    case SearchState::RetryWaiting:
        return QStringLiteral("检索中");
    case SearchState::NotSearched:
        return QStringLiteral("待检索");
    }
    return QStringLiteral("未知状态");
}

Platform platformFromInt(int value)
{
    return value == platformToInt(Platform::CurseForge) ? Platform::CurseForge
                                                        : Platform::Modrinth;
}

int platformToInt(Platform platform)
{
    return platform == Platform::CurseForge ? 1 : 0;
}

bool PlatformResult::hasTerminalState() const
{
    return state == SearchState::Success || state == SearchState::NotFound
        || state == SearchState::Failed || state == SearchState::Skipped
        || state == SearchState::Cancelled;
}

QString PlatformResult::displayLicense() const
{
    if (!isSuccess())
        return {};
    return license.trimmed().isEmpty() ? QStringLiteral("未知") : license.trimmed();
}
