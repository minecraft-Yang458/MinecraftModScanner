#pragma once

#include "SearchTypes.h"

#include <QAbstractTableModel>
#include <QVector>

struct ManualRowData
{
    QString searchName;
    PlatformResult modrinth;
    PlatformResult curseforge;
};

class ResultModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        OriginalFile = 0,
        BaseCleanName,
        SearchName,
        ModrinthName,
        ModrinthId,
        ModrinthAuthor,
        ModrinthLicense,
        ModrinthUrl,
        CurseForgeName,
        CurseForgeId,
        CurseForgeAuthor,
        CurseForgeLicense,
        CurseForgeUrl,
        McModUrl,
        Status,
        Operation,
        ColumnCount
    };

    explicit ResultModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setEntries(QVector<ModEntry> entries);
    void clearRows();

    const QVector<ModEntry> &entries() const { return m_entries; }
    const ModEntry *entryAt(int row) const;
    const ModEntry *entryById(quint64 uid) const;
    int rowIndexById(quint64 uid) const;

    int countState(Platform platform, SearchState state) const;
    int successRowCount() const;
    bool containsPlatformId(Platform platform, const QString &id) const;

    void preparePlatformForSearch(quint64 uid, Platform platform);
    void markPlatformSkipped(quint64 uid, Platform platform, const QString &reason);
    void markPlatformCancelled(quint64 uid, Platform platform, const QString &reason);
    void setPlatformState(quint64 uid, Platform platform, SearchState state, int attempts,
                          const QString &error = QString());
    void setPlatformResult(quint64 uid, Platform platform, const PlatformResult &result);
    void clearPlatformResult(quint64 uid, Platform platform);
    void setMcModLookup(quint64 uid, const QString &url, const QString &title,
                        const QString &status);
    void setRowNote(quint64 uid, const QString &note);

    bool addManualRow(const ManualRowData &data);
    bool removeRowById(quint64 uid);

    QStringList exportHeaders() const;
    QVector<QStringList> exportRows() const;

    void setActionsEnabled(bool enabled);
    bool actionsEnabled() const { return m_actionsEnabled; }

    static QStringList headers();

signals:
    void rowCountChanged();
    void actionsEnabledChanged(bool enabled);

private:
    int indexOfUid(quint64 uid) const;
    quint64 allocateUid();
    void notifyRow(int row);
    QString platformCell(const PlatformResult &result, int field) const;
    QString statusText(const ModEntry &entry) const;
    QString statusToolTip(const ModEntry &entry) const;

    QVector<ModEntry> m_entries;
    quint64 m_nextUid = 1;
    bool m_actionsEnabled = true;
};
