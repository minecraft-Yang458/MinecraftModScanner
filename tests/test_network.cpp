#include "AppConfig.h"
#include "McModWikiClient.h"
#include "NetworkTaskManager.h"

#include <QEventLoop>
#include <QTimer>
#include <QtTest>

class NetworkTests : public QObject
{
    Q_OBJECT
private slots:
    void modrinthSearch();
    void connectionChecks();
    void mcmodApi();
};

void NetworkTests::modrinthSearch()
{
    NetworkTaskManager manager;
    AppConfig config = AppConfig::defaults();
    config.useSystemProxy = true;
    manager.configure(config);

    PlatformResult captured;
    bool done = false;
    QEventLoop loop;
    connect(&manager, &NetworkTaskManager::searchFinished, &loop,
            [&](const QString &, int, const PlatformResult &result) {
                captured = result;
                done = true;
                loop.quit();
            });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    manager.submitSearch(Platform::Modrinth, QStringLiteral("jei"), QStringLiteral("net-test"));
    loop.exec();

    QVERIFY2(done, "Modrinth request did not finish in time");
    QCOMPARE(captured.state, SearchState::Success);
    QVERIFY(!captured.id.isEmpty());
    QVERIFY(!captured.name.isEmpty());
    qInfo().noquote() << "Modrinth smoke:" << captured.name << captured.id << captured.url;
}

void NetworkTests::connectionChecks()
{
    NetworkTaskManager manager;
    AppConfig config = AppConfig::defaults();
    config.useSystemProxy = true;
    manager.configure(config);

    bool checked = false;
    bool checkOk = false;
    bool finished = false;
    QEventLoop loop;
    connect(&manager, &NetworkTaskManager::connectionChecked, &loop,
            [&](int platform, bool ok, const QString &message, int) {
                qInfo().noquote() << "Connection check:" << platform << ok << message;
                if (platform == platformToInt(Platform::Modrinth)) {
                    checked = true;
                    checkOk = ok;
                }
            });
    connect(&manager, &NetworkTaskManager::connectionCheckFinished, &loop, [&]() {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    manager.checkConnections(true, false);
    loop.exec();

    QVERIFY2(finished, "Connection check did not finish in time");
    QVERIFY(checked);
    QVERIFY(checkOk);

    bool cfChecked = false;
    bool cfOk = true;
    bool cfFinished = false;
    QEventLoop cfLoop;
    connect(&manager, &NetworkTaskManager::connectionChecked, &cfLoop,
            [&](int platform, bool ok, const QString &, int) {
                if (platform == platformToInt(Platform::CurseForge)) {
                    cfChecked = true;
                    cfOk = ok;
                }
            });
    connect(&manager, &NetworkTaskManager::connectionCheckFinished, &cfLoop, [&]() {
        cfFinished = true;
        cfLoop.quit();
    });
    QTimer::singleShot(10000, &cfLoop, &QEventLoop::quit);
    manager.checkConnections(false, true);
    cfLoop.exec();
    QVERIFY(cfFinished);
    QVERIFY(cfChecked);
    QVERIFY(!cfOk);
}

void NetworkTests::mcmodApi()
{
    McModWikiClient client;
    AppConfig config = AppConfig::defaults();
    config.useSystemProxy = true;
    client.configure(config);

    McModWikiResult captured;
    bool done = false;
    QEventLoop loop;
    connect(&client, &McModWikiClient::lookupFinished, &loop,
            [&](const McModWikiResult &result) {
                captured = result;
                done = true;
                loop.quit();
            });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    client.lookup(QStringLiteral("1"), QStringLiteral("Just Enough Items"));
    loop.exec();

    QVERIFY2(done, "MC百科 request did not finish in time");
    QVERIFY2(captured.matched, qPrintable(captured.error));
    QVERIFY(captured.url.contains(QStringLiteral("mcmod.cn/class/")));
    qInfo().noquote() << "MC百科 smoke:" << captured.title << captured.url;
}

QTEST_GUILESS_MAIN(NetworkTests)

#include "test_network.moc"
