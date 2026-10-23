#pragma once

#include "AppConfig.h"
#include "SearchTypes.h"

#include <QDialog>
#include <QSet>
#include <QVector>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class NetworkTaskManager;
class ResultModel;

class ManualSearchDialog : public QDialog
{
    Q_OBJECT
public:
    ManualSearchDialog(const AppConfig &config, ResultModel *model, const QString &prefill,
                       quint64 updateRowId = 0, int fixedPlatform = -1,
                       QWidget *parent = nullptr);
    ~ManualSearchDialog() override;

protected:
    void reject() override;

private:
    struct ResultRow
    {
        Platform platform = Platform::Modrinth;
        SearchCandidate candidate;
    };

    void buildUi(const QString &prefill);
    void startSearch();
    void onSearchStarted(const QString &tag, int platform, const QString &query, int attempt,
                         int maxAttempts);
    void onSearchRetry(const QString &tag, int platform, const QString &query, int nextAttempt,
                       int maxAttempts, int delayMs, const QString &reason);
    void onSearchFinished(const QString &tag, int platform, const PlatformResult &result);
    void finishSearchIfDone();
    void acceptSelection();
    void setBusy(bool busy);
    void appendStatus(const QString &text);
    QVector<ResultRow> selectedResults() const;
    quint64 clickedRowId() const { return m_updateRowId; }

    AppConfig m_config;
    ResultModel *m_model = nullptr;
    quint64 m_updateRowId = 0;
    int m_fixedPlatform = -1;

    QLineEdit *m_nameEdit = nullptr;
    QCheckBox *m_modrinthCheck = nullptr;
    QCheckBox *m_curseForgeCheck = nullptr;
    QPushButton *m_searchButton = nullptr;
    QPushButton *m_addButton = nullptr;
    QTableWidget *m_table = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel *m_statusLabel = nullptr;

    NetworkTaskManager *m_manager = nullptr;
    QSet<int> m_pendingPlatforms;
    QVector<ResultRow> m_resultRows;
    bool m_busy = false;
};
