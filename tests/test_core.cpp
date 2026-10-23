#include "AppConfig.h"
#include "ExportService.h"
#include "LicenseUtils.h"
#include "ScanCleaner.h"
#include "SearchTypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QtTest>

class CoreTests : public QObject
{
    Q_OBJECT

private slots:
    void cleaningExamples();
    void cleaningFallbacks();
    void loaderRules();
    void advancedMetadataFiltering();
    void scanDirectory();
    void configSanitize();
    void configPersistence();
    void licenseNormalization();
    void csvExport();
    void markdownExport();
    void xlsxExport();
};

void CoreTests::cleaningExamples()
{
    const auto sodium = ScanCleaner::cleanFileName(QStringLiteral("sodium-fabric-0.5.8.jar"));
    QCOMPARE(sodium.baseCleanName, QStringLiteral("sodium-fabric"));
    QCOMPARE(sodium.searchName, QStringLiteral("sodium"));
    QVERIFY(!sodium.cleanAnomaly);

    const auto jei = ScanCleaner::cleanFileName(QStringLiteral("jei-forge-1.20.1.jar"));
    QCOMPARE(jei.searchName, QStringLiteral("jei"));

    const auto plus = ScanCleaner::cleanFileName(QStringLiteral("sodium+fabric.jar"));
    QCOMPARE(plus.searchName, QStringLiteral("sodium"));

    const auto api = ScanCleaner::cleanFileName(QStringLiteral("fabric-api.jar"));
    QCOMPARE(api.baseCleanName, QStringLiteral("fabric-api"));
    QCOMPARE(api.searchName, QStringLiteral("fabric api"));

    QCOMPARE(ScanCleaner::cleanFileName(QStringLiteral("neoforge.jar")).searchName,
             QStringLiteral("neoforge"));
    QCOMPARE(ScanCleaner::cleanFileName(QStringLiteral("ForgeConfigAPIPort-v1.2.3.jar"))
                 .searchName,
             QStringLiteral("ForgeConfigAPIPort"));

    const auto bracket = ScanCleaner::cleanFileName(
        QStringLiteral("[TaC: 绿葡萄附属包] lradd-1.20.1-0.3.0.jar"));
    QCOMPARE(bracket.baseCleanName, QStringLiteral("lradd"));
    QCOMPARE(bracket.searchName, QStringLiteral("lradd"));

    const auto map = ScanCleaner::cleanFileName(
        QStringLiteral("[Xaero的世界地图] xaeroworldmap-forge-1.20.1-1.46.0.jar"));
    QCOMPARE(map.searchName, QStringLiteral("xaeroworldmap"));

    const auto fullWidth = ScanCleaner::cleanFileName(
        QStringLiteral("【安全工艺】[1.20.1] SecurityCraft-v1.10.2.1.jar"));
    QCOMPARE(fullWidth.searchName, QStringLiteral("SecurityCraft"));

    const auto multiple = ScanCleaner::cleanFileName(
        QStringLiteral("[道路中国] [Forge 1.20.1] RoadChina-1.0.2.jar"));
    QCOMPARE(multiple.searchName, QStringLiteral("RoadChina"));
}

void CoreTests::cleaningFallbacks()
{
    const auto empty = ScanCleaner::cleanFileName(QStringLiteral(".jar"));
    QVERIFY(empty.cleanAnomaly);
    QCOMPARE(empty.searchName, QStringLiteral(".jar"));

    const auto beta = ScanCleaner::cleanFileName(QStringLiteral("alpha.jar"));
    QVERIFY(beta.cleanAnomaly);
    QCOMPARE(beta.searchName, QStringLiteral("alpha.jar"));

    const auto dotted = ScanCleaner::cleanFileName(QStringLiteral("Mr.Crayfish.jar"));
    QCOMPARE(dotted.searchName, QStringLiteral("Mr.Crayfish"));

    const auto onlyBracket =
        ScanCleaner::cleanFileName(QStringLiteral("[中文名].jar"));
    QVERIFY(onlyBracket.cleanAnomaly);
    QCOMPARE(onlyBracket.searchName, QStringLiteral("[中文名].jar"));
}

void CoreTests::loaderRules()
{
    const auto token = ScanCleaner::cleanFileName(QStringLiteral("ForgeConfigAPIPort.jar"));
    QCOMPARE(token.searchName, QStringLiteral("ForgeConfigAPIPort"));

    const auto underscore = ScanCleaner::cleanFileName(QStringLiteral("foo_fabric.jar"));
    QCOMPARE(underscore.baseCleanName, QStringLiteral("foo_fabric"));
    QCOMPARE(underscore.searchName, QStringLiteral("foo"));

    const auto upperUnderscore =
        ScanCleaner::cleanFileName(QStringLiteral("CreativeCore_FORGE_v2.12.40_mc1.20.1.jar"));
    QCOMPARE(upperUnderscore.baseCleanName, QStringLiteral("CreativeCore_FORGE"));
    QCOMPARE(upperUnderscore.searchName, QStringLiteral("CreativeCore"));

    const auto two = ScanCleaner::cleanFileName(QStringLiteral("foo-forge+fabric.jar"));
    QCOMPARE(two.searchName, QStringLiteral("foo"));

    QString note;
    bool fallback = false;
    const QString result = ScanCleaner::loaderCleanup(QStringLiteral("-fabric"), &note, &fallback);
    QCOMPARE(result, QStringLiteral("-fabric"));
    QVERIFY(fallback);
    QVERIFY(note.contains(QStringLiteral("已回退")));
}

void CoreTests::advancedMetadataFiltering()
{
    struct Case
    {
        const char *file;
        const char *base;
        const char *search;
    };
    const Case cases[] = {
        {"tacz-1.20.1-1.1.8-hotfix.jar", "tacz", "tacz"},
        {"rhino-forge-2001.2.3-build.10.jar", "rhino-forge", "rhino"},
        {"ModernUI-Forge-1.20.1-3.12.0.1-universal.jar", "ModernUI-Forge", "ModernUI"},
        {"I18nUpdateMod-3.7.0-all.jar", "I18nUpdateMod", "I18nUpdateMod"},
        {"rubidium-extra-0.5.4.4+mc1.20.1-build.131.jar", "rubidium-extra",
         "rubidium extra"},
        {"endofdays-1.9.0-RE-slim.jar", "endofdays", "endofdays"},
        {"fancymenu_forge_3.9.14_MC_1.20.1.jar", "fancymenu_forge", "fancymenu"},
        {"MTS Official Pack-1.20.1-V29.jar", "MTS Official Pack", "MTS Official Pack"},
        {"NaturalDecorMod_NOBIOMES_1.20.1_V1.9.5.jar", "NaturalDecorMod",
         "NaturalDecorMod"},
        {"foo-all.jar", "foo", "foo"},
        {"AllTheMods-1.20.1.jar", "AllTheMods", "AllTheMods"},
    };

    for (const Case &testCase : cases) {
        const CleanedModFile cleaned =
            ScanCleaner::cleanFileName(QString::fromLatin1(testCase.file));
        QCOMPARE(cleaned.baseCleanName, QString::fromLatin1(testCase.base));
        QCOMPARE(cleaned.searchName, QString::fromLatin1(testCase.search));
    }
}

void CoreTests::scanDirectory()
{
    const QString root = QDir::current().filePath(
        QStringLiteral("test-scan-%1").arg(QCoreApplication::applicationPid()));
    QDir(root).removeRecursively();
    QVERIFY(QDir().mkpath(root + QStringLiteral("/nested")));

    auto touch = [](const QString &path) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write("jar");
    };
    touch(root + QStringLiteral("/sodium-fabric-0.5.8.jar"));
    touch(root + QStringLiteral("/JEI-FORGE-1.20.1.JAR"));
    touch(root + QStringLiteral("/nested/create-fabric-0.5.jar"));
    touch(root + QStringLiteral("/not-a-jar.zip"));

    const ScanOutcome flat = ScanCleaner::scanDirectory(root, false);
    QCOMPARE(flat.files.size(), 2);
    const ScanOutcome recursive = ScanCleaner::scanDirectory(root, true);
    QCOMPARE(recursive.files.size(), 3);

    bool foundSodium = false;
    for (const CleanedModFile &file : recursive.files) {
        if (file.searchName == QStringLiteral("sodium"))
            foundSodium = true;
        QVERIFY(!file.filePath.isEmpty());
    }
    QVERIFY(foundSodium);

    QDir(root).removeRecursively();
}

void CoreTests::configSanitize()
{
    QCOMPARE(AppConfig::defaultUserAgent(),
             QStringLiteral("MinecraftModScanner/1.1.0 (Qt; contact:yang458coder@163.com)"));

    AppConfig config = AppConfig::defaults();
    config.userAgent = QStringLiteral("   ");
    config.modrinth.maxAttempts = 0;
    config.modrinth.timeoutSeconds = 999;
    config.modrinth.concurrency = 0;
    config.modrinth.minIntervalMs = -1;
    config.curseForge.maxAttempts = 99;
    config.proxyPort = 0;
    config.useSystemProxy = false;
    config.proxyHost.clear();
    config.defaultExportFormat = QStringLiteral("bogus");
    config.sanitize();

    QCOMPARE(config.userAgent, AppConfig::defaultUserAgent());
    QCOMPARE(config.modrinth.maxAttempts, 3);
    QCOMPARE(config.modrinth.timeoutSeconds, 15);
    QCOMPARE(config.modrinth.concurrency, 2);
    QCOMPARE(config.modrinth.minIntervalMs, 300);
    QCOMPARE(config.curseForge.maxAttempts, 3);
    QCOMPARE(config.proxyPort, 8080);
    QVERIFY(config.useSystemProxy);
    QCOMPARE(config.defaultExportFormat, QStringLiteral("CSV"));

    config.useBrowserUserAgent = true;
    QCOMPARE(config.effectiveUserAgent(), AppConfig::browserUserAgent());
    config.useBrowserUserAgent = false;
    config.userAgent = QStringLiteral("CustomUA");
    QCOMPARE(config.effectiveUserAgent(), QStringLiteral("CustomUA"));
}

void CoreTests::configPersistence()
{
    const QString path = QDir::current().filePath(QStringLiteral("test-settings.ini"));
    QFile::remove(path);

    AppConfig config = AppConfig::defaults();
    config.userAgent = QStringLiteral("Test UA");
    config.rememberCurseForgeKey = true;
    config.curseForgeApiKey = QStringLiteral("secret-key");
    config.defaultExportFormat = QStringLiteral("Markdown");
    {
        QSettings settings(path, QSettings::IniFormat);
        config.save(settings);
    }

    {
        QSettings settings(path, QSettings::IniFormat);
        const AppConfig loaded = AppConfig::load(settings);
        QCOMPARE(loaded.userAgent, QStringLiteral("Test UA"));
        QCOMPARE(loaded.curseForgeApiKey, QStringLiteral("secret-key"));
        QCOMPARE(loaded.defaultExportFormat, QStringLiteral("Markdown"));
    }

    config.rememberCurseForgeKey = false;
    {
        QSettings settings(path, QSettings::IniFormat);
        config.save(settings);
    }
    {
        QSettings settings(path, QSettings::IniFormat);
        const AppConfig loaded = AppConfig::load(settings);
        QVERIFY(loaded.curseForgeApiKey.isEmpty());
    }

    {
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("network/userAgent"),
                          QStringLiteral("MinecraftModScanner/1.0.0 "
                                         "(Qt; contact:yang458coder@163.com)"));
        settings.sync();
        const AppConfig loaded = AppConfig::load(settings);
        QCOMPARE(loaded.userAgent, AppConfig::defaultUserAgent());
    }
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("network/userAgent"),
                          QStringLiteral("MinecraftModScanner/1.0.0 "
                                         "(Qt; contact: your-email@example.com)"));
        settings.sync();
        const AppConfig loaded = AppConfig::load(settings);
        QCOMPARE(loaded.userAgent, AppConfig::defaultUserAgent());
    }
    QFile::remove(path);
}

void CoreTests::licenseNormalization()
{
    QCOMPARE(LicenseUtils::categoriesForLicense(QStringLiteral("MIT")),
             QStringList{QStringLiteral("MIT")});
    QCOMPARE(LicenseUtils::categoriesForLicense(QStringLiteral("MIT OR Apache-2.0")),
             (QStringList{QStringLiteral("MIT"), QStringLiteral("Apache-2.0")}));
    QCOMPARE(LicenseUtils::categoriesForLicense(QStringLiteral("LGPL-3.0")),
             QStringList{QStringLiteral("LGPL")});
    QCOMPARE(LicenseUtils::categoriesForLicense(QStringLiteral("All Rights Reserved")),
             QStringList{QStringLiteral("All Rights Reserved")});

    ModEntry entry;
    entry.results[0].state = SearchState::Success;
    entry.results[0].license = QStringLiteral("MIT");
    entry.results[1].state = SearchState::Success;
    entry.results[1].license = QStringLiteral("Apache-2.0");
    const QStringList categories = LicenseUtils::categoriesForRow(entry);
    QVERIFY(categories.contains(QStringLiteral("MIT")));
    QVERIFY(categories.contains(QStringLiteral("Apache-2.0")));
    QVERIFY(categories.contains(QStringLiteral("多许可证")));

    ModEntry pending;
    QCOMPARE(LicenseUtils::categoriesForRow(pending),
             QStringList{QStringLiteral("未检索/未找到")});
}

void CoreTests::csvExport()
{
    const QStringList headers = {QStringLiteral("名称"), QStringLiteral("备注")};
    const QVector<QStringList> rows = {
        {QStringLiteral("A,B"), QStringLiteral("含\"引号\"")},
        {QStringLiteral("换行"), QStringLiteral("第一行\n第二行")},
    };
    const QByteArray csv = ExportService::buildCsv(headers, rows);
    QVERIFY(csv.startsWith("\xEF\xBB\xBF"));
    QVERIFY(csv.contains("\"A,B\""));
    QVERIFY(csv.contains("\"含\"\"引号\"\"\""));
    QVERIFY(csv.contains("\r\n"));
}

void CoreTests::markdownExport()
{
    const QStringList headers = {QStringLiteral("名称"), QStringLiteral("链接")};
    const QVector<QStringList> rows = {
        {QStringLiteral("a|b"), QStringLiteral("https://example.com/x")},
    };
    const QByteArray markdown = ExportService::buildMarkdown(headers, rows);
    QVERIFY(markdown.contains("| 名称 | 链接 |"));
    QVERIFY(markdown.contains("a\\|b"));
    QVERIFY(markdown.contains("| --- | --- |"));
}

void CoreTests::xlsxExport()
{
    const QStringList headers = {QStringLiteral("名称"), QStringLiteral("状态")};
    const QVector<QStringList> rows = {
        {QStringLiteral("测试 & <Mod>"), QStringLiteral("成功")},
    };
    const QByteArray xlsx = ExportService::buildXlsx(headers, rows);
    QVERIFY(xlsx.startsWith("PK\x03\x04"));
    QVERIFY(xlsx.contains("[Content_Types].xml"));
    QVERIFY(xlsx.contains("xl/worksheets/sheet1.xml"));
    QVERIFY(xlsx.contains(QStringLiteral("测试 &amp; &lt;Mod&gt;").toUtf8()));

    const QString outputPath = qEnvironmentVariable("MMS_XLSX_OUTPUT");
    if (!outputPath.isEmpty()) {
        QString error;
        QVERIFY2(ExportService::exportToFile(outputPath, ExportFormat::Excel, headers, rows,
                                             &error),
                 qPrintable(error));
    }
}

QTEST_GUILESS_MAIN(CoreTests)

#include "test_core.moc"
