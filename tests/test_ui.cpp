#include "AppConfig.h"
#include "MainWindow.h"
#include "ManualSearchDialog.h"
#include "ResultModel.h"
#include "SettingsDialog.h"
#include "SummaryDialog.h"
#include "UrlLauncher.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTextStream>
#include <QtTest>

class UiTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void urlNormalization();
    void renderMainWindow();
};

void UiTests::initTestCase()
{
    QLocale::setDefault(QLocale::c());
    QFont appFont = QApplication::font();
    appFont.setFamilies({
        QStringLiteral("Segoe UI"),
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Noto Sans"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Arial"),
        QStringLiteral("DejaVu Sans"),
    });
    appFont.setStyleHint(QFont::SansSerif);
    QApplication::setFont(appFont);

    QFile file(QStringLiteral(":/style.qss"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        qApp->setStyleSheet(stream.readAll());
    }
}

static void verifyAsciiDigits(const QString &text)
{
    for (QChar ch : text) {
        if (ch.isDigit()) {
            QVERIFY2(ch.unicode() >= '0' && ch.unicode() <= '9',
                     qPrintable(QStringLiteral("Non-ASCII digit in UI text: %1").arg(text)));
        }
    }
}

static void verifyWidgetDigits(QWidget *root)
{
    for (QLabel *label : root->findChildren<QLabel *>())
        verifyAsciiDigits(label->text());
    for (QAbstractButton *button : root->findChildren<QAbstractButton *>())
        verifyAsciiDigits(button->text());
    for (QSpinBox *spin : root->findChildren<QSpinBox *>())
        verifyAsciiDigits(spin->text());
}

void UiTests::urlNormalization()
{
    const QUrl full = UrlLauncher::normalize(
        QStringLiteral("https://modrinth.com/mod/jei"));
    QCOMPARE(full.scheme(), QStringLiteral("https"));
    QCOMPARE(full.host(), QStringLiteral("modrinth.com"));

    const QUrl bare = UrlLauncher::normalize(QStringLiteral("www.example.com/mod"));
    QCOMPARE(bare.scheme(), QStringLiteral("https"));
    QCOMPARE(bare.host(), QStringLiteral("www.example.com"));

    QVERIFY(UrlLauncher::normalize(QStringLiteral("   ")).isEmpty());
}

static PlatformResult successResult(Platform platform, const QString &name, const QString &id,
                                    const QString &author, const QString &license,
                                    const QString &url)
{
    PlatformResult result;
    result.state = SearchState::Success;
    result.name = name;
    result.id = id;
    result.author = author;
    result.license = license;
    result.url = url;
    result.attempts = 1;
    result.httpStatus = 200;
    result.elapsedMs = 123;
    result.timestamp = QDateTime::currentDateTime();
    result.requestUrl = platform == Platform::Modrinth
        ? QStringLiteral("https://api.modrinth.com/v2/search?query=%1").arg(name)
        : QStringLiteral("https://api.curseforge.com/v1/mods/search?searchFilter=%1").arg(name);
    result.rawResponse =
        QByteArrayLiteral("{\"mock\":true,\"platform\":\"demo\",\"name\":\"demo\"}");
    SearchCandidate candidate;
    candidate.name = name;
    candidate.id = id;
    candidate.author = author;
    candidate.license = license;
    candidate.url = url;
    result.candidates.append(candidate);
    return result;
}

void UiTests::renderMainWindow()
{
    const QString outputDir = qEnvironmentVariable(
        "MMS_UI_SCREENSHOT_DIR", QDir::currentPath());
    QDir().mkpath(outputDir);

    MainWindow window;
    window.resize(1600, 900);
    window.show();

    auto *model = window.findChild<ResultModel *>();
    QVERIFY(model);

    QVector<ModEntry> entries;

    ModEntry sodium;
    sodium.originalFileName = QStringLiteral("sodium-fabric-0.5.8.jar");
    sodium.baseCleanName = QStringLiteral("sodium-fabric");
    sodium.searchName = QStringLiteral("sodium");
    sodium.cleanerNote = QStringLiteral("已移除加载器：fabric");
    sodium.results[0] = successResult(
        Platform::Modrinth, QStringLiteral("Sodium"), QStringLiteral("AANobbMI"),
        QStringLiteral("jellysquid3"), QStringLiteral("LGPL-3.0"),
        QStringLiteral("https://modrinth.com/mod/sodium"));
    sodium.results[1] = successResult(
        Platform::CurseForge, QStringLiteral("Sodium"), QStringLiteral("394468"),
        QStringLiteral("jellysquid3"), QStringLiteral("LGPL-3.0"),
        QStringLiteral("https://www.curseforge.com/minecraft/mc-mods/sodium"));
    sodium.mcmodUrl = QStringLiteral("https://www.mcmod.cn/class/1234.html");
    sodium.mcmodTitle = QStringLiteral("钠 (Sodium)");
    sodium.mcmodStatus = QStringLiteral("已匹配");
    entries.append(sodium);

    ModEntry jei;
    jei.originalFileName = QStringLiteral("jei-forge-1.20.1.jar");
    jei.baseCleanName = QStringLiteral("jei-forge");
    jei.searchName = QStringLiteral("jei");
    jei.cleanerNote = QStringLiteral("已移除加载器：forge");
    jei.results[0] = successResult(
        Platform::Modrinth, QStringLiteral("Just Enough Items"), QStringLiteral("u6dRKJwZ"),
        QStringLiteral("mezz"), QStringLiteral("MIT"),
        QStringLiteral("https://modrinth.com/mod/jei"));
    jei.results[1].state = SearchState::Failed;
    jei.results[1].error = QStringLiteral("请求超时（15 秒）（已尝试 3 次，达到最大尝试次数）");
    jei.results[1].attempts = 3;
    jei.results[1].httpStatus = 0;
    jei.results[1].elapsedMs = 45123;
    jei.results[1].timestamp = QDateTime::currentDateTime();
    jei.results[1].requestUrl =
        QStringLiteral("https://api.curseforge.com/v1/mods/search?searchFilter=jei");
    entries.append(jei);

    ModEntry miss;
    miss.originalFileName = QStringLiteral("mystery-beta-1.0.jar");
    miss.baseCleanName = QStringLiteral("mystery");
    miss.searchName = QStringLiteral("mystery");
    miss.cleanerNote = QStringLiteral("已移除版本/测试标记");
    miss.results[0].state = SearchState::NotFound;
    miss.results[0].error = QStringLiteral("未找到匹配的模组（HTTP 404）");
    miss.results[0].attempts = 1;
    miss.results[0].httpStatus = 404;
    miss.results[0].timestamp = QDateTime::currentDateTime();
    miss.results[1].state = SearchState::Skipped;
    miss.results[1].error = QStringLiteral("未提供 CurseForge API Key，已跳过");
    entries.append(miss);

    model->setEntries(entries);

    ManualRowData manual;
    manual.searchName = QStringLiteral("create");
    manual.modrinth = successResult(
        Platform::Modrinth, QStringLiteral("Create"), QStringLiteral("LNytGWDc"),
        QStringLiteral("simibubi"), QStringLiteral("MIT"),
        QStringLiteral("https://modrinth.com/mod/create"));
    model->addManualRow(manual);

    QTest::qWait(200);
    QVERIFY2(window.grab().save(QDir(outputDir).filePath(QStringLiteral("ui-data.png"))),
             "Could not save main window screenshot");

    if (auto *table = window.findChild<QTableView *>()) {
        table->scrollTo(model->index(0, ResultModel::Operation));
        QTest::qWait(100);
        QVERIFY2(window.grab().save(
                     QDir(outputDir).filePath(QStringLiteral("ui-data-actions.png"))),
                 "Could not save actions screenshot");
        table->scrollTo(model->index(0, 0));
    }

    if (auto *progress = window.findChild<QProgressBar *>()) {
        progress->setRange(0, 100);
        progress->setValue(100);
        progress->setFormat(QStringLiteral("100 / 100（主检索完成）"));
        qInfo().noquote() << "Progress text:" << progress->text()
                          << "font:" << progress->font().family();
        QCOMPARE(progress->text(), QStringLiteral("100 / 100（主检索完成）"));
        verifyAsciiDigits(progress->text());
        QTest::qWait(100);
        QVERIFY2(window.grab().save(
                     QDir(outputDir).filePath(QStringLiteral("ui-progress.png"))),
                 "Could not save progress screenshot");
    }
    verifyWidgetDigits(&window);

    SummaryDialog summary(model);
    summary.resize(1080, 700);
    summary.show();
    QTest::qWait(200);
    if (auto *tabs = summary.findChild<QTabWidget *>())
        tabs->setCurrentIndex(1);
    QTest::qWait(100);
    QVERIFY2(summary.grab().save(QDir(outputDir).filePath(QStringLiteral("ui-summary-status.png"))),
             "Could not save summary screenshot");
    if (auto *tabs = summary.findChild<QTabWidget *>())
        tabs->setCurrentIndex(0);
    QTest::qWait(50);
    QVERIFY2(summary.grab().save(QDir(outputDir).filePath(QStringLiteral("ui-summary.png"))),
             "Could not save summary screenshot");
    verifyWidgetDigits(&summary);

    SettingsDialog settings(AppConfig::defaults());
    settings.show();
    QTest::qWait(150);
    verifyWidgetDigits(&settings);
    QVERIFY2(settings.grab().save(QDir(outputDir).filePath(QStringLiteral("ui-settings.png"))),
             "Could not save settings screenshot");
    settings.close();

    ManualSearchDialog manualDialog(AppConfig::defaults(), model, QStringLiteral("sodium"),
                                    0, -1);
    manualDialog.show();
    QTest::qWait(150);
    verifyWidgetDigits(&manualDialog);
    QVERIFY2(manualDialog.grab().save(
                 QDir(outputDir).filePath(QStringLiteral("ui-manual.png"))),
             "Could not save manual search screenshot");
    manualDialog.close();

    summary.close();
    window.close();
}

QTEST_MAIN(UiTests)

#include "test_ui.moc"
