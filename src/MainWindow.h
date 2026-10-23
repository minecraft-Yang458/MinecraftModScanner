#pragma once

#include "AppConfig.h"
#include "SearchTypes.h"

#include <QHash>
#include <QMainWindow>
#include <QPair>
#include <QPoint>
#include <QPointer>
#include <QSet>

enum class ExportFormat;
struct McModWikiResult;

class ActionButtonDelegate;
class McModWikiClient;
class NetworkTaskManager;
class QLabel;
class QLineEdit;
class QMenu;
class QProgressBar;
class QPushButton;
class QSettings;
class QTableView;
class QToolButton;
class ResultModel;
class SummaryDialog;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class Phase {
        Idle,
        Connecting,
        Searching,
        Wiki,
    };

    struct PendingTask
    {
        quint64 uid = 0;
        Platform platform = Platform::Modrinth;
    };

    void buildUi();
    void buildMenus();
    void connectSignals();
    void loadInitialSettings();
    void persistSettings();

    void chooseFolder();
    void rescanFolder();
    void startSearch();
    void cancelSearch();
    void beginSearchPhase();
    void finishPlatformPhase();
    void startWikiPhase();
    void finishJob(bool canceled);
    void abortBeforeSearch(const QString &message);

    void onConnectionChecked(int platform, bool ok, const QString &message, int httpStatus);
    void onConnectionCheckFinished();
    void onSearchStarted(const QString &tag, int platform, const QString &query, int attempt,
                         int maxAttempts);
    void onSearchRetry(const QString &tag, int platform, const QString &query, int nextAttempt,
                       int maxAttempts, int delayMs, const QString &reason);
    void onSearchFinished(const QString &tag, int platform, const PlatformResult &result);
    void onWikiLookupFinished(const McModWikiResult &result);

    void showRowMenu(const QModelIndex &index, const QPoint &globalPosition);
    void copyPlatformResult(quint64 uid, Platform platform);
    void openPlatformUrl(quint64 uid, Platform platform);
    void showPlatformDetails(quint64 uid, Platform platform);
    void clearPlatformResult(quint64 uid, Platform platform);
    void retryRow(quint64 uid, Platform platform);
    void manualSearchForRow(quint64 uid, Platform platform);
    void deleteRow(quint64 uid);
    void selectRowById(quint64 uid);

    void openStandaloneSearch();
    void openSummary();
    void openSettings();
    void validateCurseForgeKey();
    void exportResults(ExportFormat format);
    void exportQuick(ExportFormat format);
    bool writeExportFile(const QString &path, ExportFormat format, bool offerFallback);
    QString defaultExportDirectory() const;
    QString defaultExportFileName(ExportFormat format) const;
    void exportDefaultFormat();
    void showApiKeyHelp();
    void showAbout();
    void showLicense();
    void updateUiState();
    void updateProgressDisplay(int completed, int total);
    void setStatusLabel(const QString &text, const QString &tooltip = QString());
    void updateStartActionState();

    const ModEntry *entryById(quint64 uid) const;
    QString platformSkipReason(Platform platform) const;

    AppConfig m_config;
    AppConfig m_jobConfig;
    QSettings *m_settings = nullptr;
    QString m_currentDirectory;
    Phase m_phase = Phase::Idle;
    bool m_running = false;
    bool m_cancelRequested = false;
    bool m_singleRetryActive = false;
    bool m_modrinthEnabled = false;
    bool m_curseForgeEnabled = false;

    quint64 m_jobId = 0;
    int m_total = 0;
    int m_completed = 0;
    int m_successCount = 0;
    int m_notFoundCount = 0;
    int m_failedCount = 0;
    int m_wikiTotal = 0;
    int m_wikiDone = 0;
    int m_wikiMatched = 0;
    QHash<QString, PendingTask> m_pendingTasks;
    QHash<int, QPair<bool, QString>> m_connectionResults;

    ResultModel *m_model = nullptr;
    ActionButtonDelegate *m_actionDelegate = nullptr;
    NetworkTaskManager *m_searchManager = nullptr;
    NetworkTaskManager *m_retryManager = nullptr;
    McModWikiClient *m_wikiClient = nullptr;
    QPointer<SummaryDialog> m_summaryDialog;

    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    QPushButton *m_chooseButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_validateButton = nullptr;
    QToolButton *m_exportButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableView *m_table = nullptr;

    QAction *m_startAction = nullptr;
    QAction *m_manualAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_exportCsvAction = nullptr;
    QAction *m_exportMarkdownAction = nullptr;
    QAction *m_exportExcelAction = nullptr;
    QAction *m_exportQuickAction = nullptr;
};
