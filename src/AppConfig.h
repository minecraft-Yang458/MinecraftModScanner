#pragma once

#include <QString>
#include <QStringList>

class QSettings;

struct PlatformSettings
{
    int maxAttempts;      // 总尝试次数，不是额外重试次数
    int timeoutSeconds;   // 单次请求超时
    int concurrency;      // 同时在途请求数
    int minIntervalMs;    // 两次请求启动之间的最小间隔
};

struct AppConfig
{
    AppConfig();

    // ---- 网络 ----
    QString userAgent;
    bool useBrowserUserAgent;
    bool useSystemProxy;
    QString proxyHost;
    int proxyPort;

    // ---- 平台检索 ----
    PlatformSettings modrinth;
    PlatformSettings curseForge;

    // ---- 其他 ----
    bool recursiveScan;
    bool enableMcModWiki;
    QString defaultExportFormat;
    bool rememberCurseForgeKey;
    QString curseForgeApiKey;

    static AppConfig defaults();
    static AppConfig load(const QSettings &settings);
    void save(QSettings &settings) const;

    // 所有非法/越界值回退到唯一一处默认值。
    void sanitize();

    QString effectiveUserAgent() const;

    static QString defaultUserAgent();
    static QString browserUserAgent();
    static QStringList loaderWords();
    static QStringList exportFormats();

    // UI 范围（默认值不在此处定义，避免默认值散落）。
    static constexpr int kAttemptMin = 1;
    static constexpr int kAttemptMax = 5;
    static constexpr int kTimeoutMin = 3;
    static constexpr int kTimeoutMax = 60;
    static constexpr int kConcurrencyMin = 1;
    static constexpr int kConcurrencyMax = 8;
    static constexpr int kIntervalMin = 0;
    static constexpr int kIntervalMax = 5000;
    static constexpr int kProxyPortMin = 1;
    static constexpr int kProxyPortMax = 65535;
};
