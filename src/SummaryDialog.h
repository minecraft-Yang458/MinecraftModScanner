#pragma once

#include <QDialog>

class QLabel;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class ResultModel;

class SummaryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SummaryDialog(ResultModel *model, QWidget *parent = nullptr);

public slots:
    void refresh();

signals:
    void retryRequested(quint64 uid, int platform);
    void manualSearchRequested(quint64 uid, int platform);
    void rowActivated(quint64 uid);

private:
    void populateLicenseTree();
    void populateStatusTree();
    void scheduleRefresh();
    QTreeWidget *createTree();
    QTreeWidgetItem *addGroup(QTreeWidget *tree, const QString &title);
    void updateGroupTitle(QTreeWidgetItem *group, int count);
    void addLicenseRow(QTreeWidgetItem *group, quint64 uid);
    QTreeWidgetItem *addStatusRow(QTreeWidgetItem *group, quint64 uid, int platform,
                                  const QString &statusText, const QString &tooltip);
    QWidget *createActionWidget(quint64 uid, int platform);
    void askRetry(quint64 uid, int platform, QWidget *anchor);
    void askManual(quint64 uid, int platform, QWidget *anchor);
    void copySearchName(quint64 uid);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

    ResultModel *m_model = nullptr;
    QLabel *m_statsLabel = nullptr;
    QTreeWidget *m_licenseTree = nullptr;
    QTreeWidget *m_statusTree = nullptr;
    QTimer *m_refreshTimer = nullptr;
};
