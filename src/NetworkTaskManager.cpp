#include "NetworkTaskManager.h"

#include "CurseForgeClient.h"
#include "ModrinthClient.h"

NetworkTaskManager::NetworkTaskManager(QObject *parent)
    : QObject(parent)
    , m_modrinth(new ModrinthClient(this))
    , m_curseForge(new CurseForgeClient(this))
{
    connect(m_modrinth, &BasePlatformClient::searchStarted, this,
            &NetworkTaskManager::searchStarted);
    connect(m_modrinth, &BasePlatformClient::searchRetry, this,
            &NetworkTaskManager::searchRetry);
    connect(m_modrinth, &BasePlatformClient::searchFinished, this,
            &NetworkTaskManager::searchFinished);
    connect(m_curseForge, &BasePlatformClient::searchStarted, this,
            &NetworkTaskManager::searchStarted);
    connect(m_curseForge, &BasePlatformClient::searchRetry, this,
            &NetworkTaskManager::searchRetry);
    connect(m_curseForge, &BasePlatformClient::searchFinished, this,
            &NetworkTaskManager::searchFinished);

    connect(m_modrinth, &BasePlatformClient::connectionChecked, this,
            &NetworkTaskManager::onConnectionChecked);
    connect(m_curseForge, &BasePlatformClient::connectionChecked, this,
            &NetworkTaskManager::onConnectionChecked);
}

void NetworkTaskManager::configure(const AppConfig &config)
{
    m_modrinth->configure(config);
    m_curseForge->configure(config);
}

void NetworkTaskManager::submitSearch(Platform platform, const QString &query,
                                      const QString &tag)
{
    if (platform == Platform::Modrinth)
        m_modrinth->submitSearch(query, tag);
    else
        m_curseForge->submitSearch(query, tag);
}

void NetworkTaskManager::cancelAll()
{
    m_pendingChecks.clear();
    m_checksRunning = false;
    m_modrinth->cancelAll();
    m_curseForge->cancelAll();
}

void NetworkTaskManager::checkConnections(bool modrinth, bool curseForge)
{
    m_modrinth->abortProbe();
    m_curseForge->abortProbe();
    m_pendingChecks.clear();
    m_checksRunning = false;

    if (modrinth)
        m_pendingChecks.insert(platformToInt(Platform::Modrinth));
    if (curseForge)
        m_pendingChecks.insert(platformToInt(Platform::CurseForge));

    if (m_pendingChecks.isEmpty()) {
        emit connectionCheckFinished();
        return;
    }

    m_checksRunning = true;
    if (modrinth)
        m_modrinth->checkConnection();
    if (curseForge)
        m_curseForge->checkConnection();

    if (m_checksRunning && m_pendingChecks.isEmpty()) {
        m_checksRunning = false;
        emit connectionCheckFinished();
    }
}

void NetworkTaskManager::cancelChecks()
{
    m_pendingChecks.clear();
    m_checksRunning = false;
    m_modrinth->abortProbe();
    m_curseForge->abortProbe();
}

bool NetworkTaskManager::busy() const
{
    return m_modrinth->isBusy() || m_curseForge->isBusy();
}

void NetworkTaskManager::onConnectionChecked(int platform, bool ok, const QString &message,
                                             int httpStatus)
{
    if (!m_checksRunning || !m_pendingChecks.contains(platform))
        return;
    m_pendingChecks.remove(platform);
    emit connectionChecked(platform, ok, message, httpStatus);
    if (m_pendingChecks.isEmpty()) {
        m_checksRunning = false;
        emit connectionCheckFinished();
    }
}
