#include "AppConfig.h"

#include <QSettings>

namespace {
namespace Defaults {
const QString UserAgent = QStringLiteral(
    "MinecraftModScanner/1.1.0 (Qt; contact:yang458coder@163.com)");
const QStringList LegacyUserAgents = {
    QStringLiteral("MinecraftModScanner/1.0.0 (Qt; contact:yang458coder@163.com)"),
    QStringLiteral("MinecraftModScanner/1.0.0 (Qt; contact: your-email@example.com)"),
};
const QString ProxyHost = QStringLiteral("127.0.0.1");
constexpr int ProxyPort = 8080;
const QString ExportFormat = QStringLiteral("CSV");
constexpr int ModrinthAttempts = 3;
constexpr int ModrinthTimeout = 15;
constexpr int ModrinthConcurrency = 2;
constexpr int ModrinthInterval = 300;
constexpr int CurseForgeAttempts = 3;
constexpr int CurseForgeTimeout = 15;
constexpr int CurseForgeConcurrency = 1;
constexpr int CurseForgeInterval = 500;
} // namespace Defaults

int boundedOrDefault(int value, int low, int high, int fallback)
{
    return (value < low || value > high) ? fallback : value;
}
} // namespace

AppConfig::AppConfig()
{
    userAgent = Defaults::UserAgent;
    useBrowserUserAgent = false;
    useSystemProxy = true;
    proxyHost = Defaults::ProxyHost;
    proxyPort = Defaults::ProxyPort;

    modrinth.maxAttempts = Defaults::ModrinthAttempts;
    modrinth.timeoutSeconds = Defaults::ModrinthTimeout;
    modrinth.concurrency = Defaults::ModrinthConcurrency;
    modrinth.minIntervalMs = Defaults::ModrinthInterval;

    curseForge.maxAttempts = Defaults::CurseForgeAttempts;
    curseForge.timeoutSeconds = Defaults::CurseForgeTimeout;
    curseForge.concurrency = Defaults::CurseForgeConcurrency;
    curseForge.minIntervalMs = Defaults::CurseForgeInterval;

    recursiveScan = false;
    enableMcModWiki = false;
    defaultExportFormat = Defaults::ExportFormat;
    rememberCurseForgeKey = false;
    curseForgeApiKey.clear();
}

AppConfig AppConfig::defaults()
{
    return AppConfig();
}

QString AppConfig::defaultUserAgent()
{
    return Defaults::UserAgent;
}

QString AppConfig::browserUserAgent()
{
    return QStringLiteral(
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36");
}

QStringList AppConfig::loaderWords()
{
    return {
        QStringLiteral("fabric"),
        QStringLiteral("forge"),
        QStringLiteral("neoforge"),
        QStringLiteral("quilt"),
        QStringLiteral("liteloader"),
        QStringLiteral("rift"),
        QStringLiteral("modloader"),
    };
}

QStringList AppConfig::exportFormats()
{
    return {QStringLiteral("CSV"), QStringLiteral("Markdown"), QStringLiteral("Excel")};
}

QString AppConfig::effectiveUserAgent() const
{
    if (useBrowserUserAgent)
        return browserUserAgent();
    const QString trimmed = userAgent.trimmed();
    return trimmed.isEmpty() ? defaultUserAgent() : trimmed;
}

void AppConfig::sanitize()
{
    const AppConfig d = defaults();

    if (userAgent.trimmed().isEmpty())
        userAgent = d.userAgent;
    else
        userAgent = userAgent.trimmed();

    proxyHost = proxyHost.trimmed();
    if (proxyPort < kProxyPortMin || proxyPort > kProxyPortMax)
        proxyPort = d.proxyPort;
    if (!useSystemProxy && proxyHost.isEmpty()) {
        proxyHost = d.proxyHost;
        useSystemProxy = true;
    }

    auto sanitizePlatform = [&](PlatformSettings &p, const PlatformSettings &def) {
        p.maxAttempts = boundedOrDefault(p.maxAttempts, kAttemptMin, kAttemptMax,
                                         def.maxAttempts);
        p.timeoutSeconds = boundedOrDefault(p.timeoutSeconds, kTimeoutMin, kTimeoutMax,
                                            def.timeoutSeconds);
        p.concurrency = boundedOrDefault(p.concurrency, kConcurrencyMin, kConcurrencyMax,
                                         def.concurrency);
        p.minIntervalMs = boundedOrDefault(p.minIntervalMs, kIntervalMin, kIntervalMax,
                                           def.minIntervalMs);
    };
    sanitizePlatform(modrinth, d.modrinth);
    sanitizePlatform(curseForge, d.curseForge);

    if (!exportFormats().contains(defaultExportFormat, Qt::CaseInsensitive))
        defaultExportFormat = d.defaultExportFormat;
    else {
        for (const QString &format : exportFormats()) {
            if (format.compare(defaultExportFormat, Qt::CaseInsensitive) == 0) {
                defaultExportFormat = format;
                break;
            }
        }
    }

    curseForgeApiKey = curseForgeApiKey.trimmed();
    // 未勾选“记住”时运行时仍保留 Key，但 save() 不会持久化它。
}

AppConfig AppConfig::load(const QSettings &settings)
{
    AppConfig c = defaults();

    c.userAgent = settings.value(QStringLiteral("network/userAgent"), c.userAgent).toString();
    // 旧版默认 UA 自动迁移到新默认值，避免已有 settings.ini 继续使用旧版本号。
    if (Defaults::LegacyUserAgents.contains(c.userAgent))
        c.userAgent = Defaults::UserAgent;
    c.useBrowserUserAgent =
        settings.value(QStringLiteral("network/useBrowserUserAgent"), c.useBrowserUserAgent).toBool();
    c.useSystemProxy =
        settings.value(QStringLiteral("network/useSystemProxy"), c.useSystemProxy).toBool();
    c.proxyHost = settings.value(QStringLiteral("network/proxyHost"), c.proxyHost).toString();
    c.proxyPort = settings.value(QStringLiteral("network/proxyPort"), c.proxyPort).toInt();

    auto readPlatform = [&settings](const QString &prefix, PlatformSettings &p) {
        p.maxAttempts = settings.value(prefix + QStringLiteral("/maxAttempts"), p.maxAttempts).toInt();
        p.timeoutSeconds =
            settings.value(prefix + QStringLiteral("/timeoutSeconds"), p.timeoutSeconds).toInt();
        p.concurrency =
            settings.value(prefix + QStringLiteral("/concurrency"), p.concurrency).toInt();
        p.minIntervalMs =
            settings.value(prefix + QStringLiteral("/minIntervalMs"), p.minIntervalMs).toInt();
    };
    readPlatform(QStringLiteral("modrinth"), c.modrinth);
    readPlatform(QStringLiteral("curseforge"), c.curseForge);

    c.recursiveScan =
        settings.value(QStringLiteral("other/recursiveScan"), c.recursiveScan).toBool();
    c.enableMcModWiki =
        settings.value(QStringLiteral("other/enableMcModWiki"), c.enableMcModWiki).toBool();
    c.defaultExportFormat =
        settings.value(QStringLiteral("other/defaultExportFormat"), c.defaultExportFormat).toString();
    c.rememberCurseForgeKey =
        settings.value(QStringLiteral("other/rememberCurseForgeKey"), c.rememberCurseForgeKey).toBool();
    if (c.rememberCurseForgeKey)
        c.curseForgeApiKey =
            settings.value(QStringLiteral("curseforge/apiKey")).toString();

    c.sanitize();
    return c;
}

void AppConfig::save(QSettings &settings) const
{
    settings.setValue(QStringLiteral("network/userAgent"), userAgent);
    settings.setValue(QStringLiteral("network/useBrowserUserAgent"), useBrowserUserAgent);
    settings.setValue(QStringLiteral("network/useSystemProxy"), useSystemProxy);
    settings.setValue(QStringLiteral("network/proxyHost"), proxyHost);
    settings.setValue(QStringLiteral("network/proxyPort"), proxyPort);

    settings.setValue(QStringLiteral("modrinth/maxAttempts"), modrinth.maxAttempts);
    settings.setValue(QStringLiteral("modrinth/timeoutSeconds"), modrinth.timeoutSeconds);
    settings.setValue(QStringLiteral("modrinth/concurrency"), modrinth.concurrency);
    settings.setValue(QStringLiteral("modrinth/minIntervalMs"), modrinth.minIntervalMs);

    settings.setValue(QStringLiteral("curseforge/maxAttempts"), curseForge.maxAttempts);
    settings.setValue(QStringLiteral("curseforge/timeoutSeconds"), curseForge.timeoutSeconds);
    settings.setValue(QStringLiteral("curseforge/concurrency"), curseForge.concurrency);
    settings.setValue(QStringLiteral("curseforge/minIntervalMs"), curseForge.minIntervalMs);

    settings.setValue(QStringLiteral("other/recursiveScan"), recursiveScan);
    settings.setValue(QStringLiteral("other/enableMcModWiki"), enableMcModWiki);
    settings.setValue(QStringLiteral("other/defaultExportFormat"), defaultExportFormat);
    settings.setValue(QStringLiteral("other/rememberCurseForgeKey"), rememberCurseForgeKey);

    // 仅在用户明确选择“记住”时明文保存；否则确保旧 Key 被删除。
    if (rememberCurseForgeKey && !curseForgeApiKey.trimmed().isEmpty())
        settings.setValue(QStringLiteral("curseforge/apiKey"), curseForgeApiKey.trimmed());
    else
        settings.remove(QStringLiteral("curseforge/apiKey"));

    settings.sync();
}
