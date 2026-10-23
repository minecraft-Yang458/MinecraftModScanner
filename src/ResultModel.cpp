#include "ResultModel.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QStringList>

namespace {
constexpr int kPlatformFieldCount = 5;
const QColor kLinkColor(0x15, 0x65, 0xC0);
const QColor kErrorColor(0xC6, 0x28, 0x28);
const QColor kMutedColor(0x75, 0x75, 0x75);

QString shortText(const QString &text, int maxLength = 90)
{
    QString oneLine = text.simplified();
    if (oneLine.size() > maxLength)
        oneLine = oneLine.left(maxLength - 1) + QChar(0x2026);
    return oneLine;
}
} // namespace

ResultModel::ResultModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QStringList ResultModel::headers()
{
    return {
        QStringLiteral("原始文件名"),
        QStringLiteral("基础清洗名"),
        QStringLiteral("检索用名称"),
        QStringLiteral("Modrinth 名称"),
        QStringLiteral("Modrinth ID"),
        QStringLiteral("Modrinth 作者"),
        QStringLiteral("Modrinth 许可证"),
        QStringLiteral("Modrinth 直链"),
        QStringLiteral("CurseForge 名称"),
        QStringLiteral("CurseForge ID"),
        QStringLiteral("CurseForge 作者"),
        QStringLiteral("CurseForge 许可证"),
        QStringLiteral("CurseForge 直链"),
        QStringLiteral("MC百科链接"),
        QStringLiteral("状态/备注"),
        QStringLiteral("操作"),
    };
}

int ResultModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int ResultModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};
    if (section < 0 || section >= ColumnCount)
        return {};
    if (role == Qt::DisplayRole)
        return headers().at(section);
    if (role == Qt::TextAlignmentRole)
        return QVariant::fromValue<int>(Qt::AlignLeft | Qt::AlignVCenter);
    return {};
}

Qt::ItemFlags ResultModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QString ResultModel::platformCell(const PlatformResult &result, int field) const
{
    if (field < 0 || field >= kPlatformFieldCount)
        return {};

    if (result.state == SearchState::Success) {
        switch (field) {
        case 0:
            return result.name;
        case 1:
            return result.id;
        case 2:
            return result.author;
        case 3:
            return result.displayLicense();
        case 4:
            return result.url;
        default:
            return {};
        }
    }

    if (field != 0)
        return {};
    switch (result.state) {
    case SearchState::Searching:
        return QStringLiteral("检索中…");
    case SearchState::RetryWaiting:
        return QStringLiteral("等待重试…");
    case SearchState::NotFound:
        return QStringLiteral("未找到");
    case SearchState::Failed:
        return QStringLiteral("请求失败");
    case SearchState::Skipped:
        return QStringLiteral("已跳过");
    case SearchState::Cancelled:
        return QStringLiteral("已取消");
    case SearchState::NotSearched:
    case SearchState::Success:
        break;
    }
    return {};
}

QString ResultModel::statusText(const ModEntry &entry) const
{
    QStringList parts;
    if (!entry.cleanerNote.trimmed().isEmpty())
        parts.append(entry.cleanerNote);
    if (!entry.rowNote.trimmed().isEmpty() && entry.rowNote != entry.cleanerNote)
        parts.append(entry.rowNote);

    for (int i = 0; i < kPlatformCount; ++i) {
        const PlatformResult &result = entry.results[i];
        if (result.state == SearchState::NotSearched)
            continue;
        const Platform platform = platformFromInt(i);
        QString text = QStringLiteral("%1：%2").arg(platformName(platform), stateText(result.state));
        if ((result.state == SearchState::Failed || result.state == SearchState::Skipped
             || result.state == SearchState::Cancelled)
            && !result.error.trimmed().isEmpty()) {
            text += QStringLiteral("（%1）").arg(shortText(result.error, 60));
        }
        parts.append(text);
    }

    if (!entry.mcmodStatus.trimmed().isEmpty())
        parts.append(QStringLiteral("MC百科：%1").arg(entry.mcmodStatus));

    if (parts.isEmpty())
        return QStringLiteral("待检索");
    return parts.join(QStringLiteral("；"));
}

QString ResultModel::statusToolTip(const ModEntry &entry) const
{
    QStringList parts;
    parts.append(statusText(entry));

    if (entry.cleanAnomaly)
        parts.append(QStringLiteral("清洗异常：已回退原始文件名。"));
    for (int i = 0; i < kPlatformCount; ++i) {
        const PlatformResult &result = entry.results[i];
        if (result.state == SearchState::Failed || result.state == SearchState::NotFound
            || result.state == SearchState::Skipped
            || result.state == SearchState::Cancelled) {
            const Platform platform = platformFromInt(i);
            parts.append(QStringLiteral("%1：%2").arg(platformName(platform), result.error));
            if (!result.requestUrl.isEmpty())
                parts.append(QStringLiteral("%1 请求 URL：%2")
                                 .arg(platformName(platform), result.requestUrl));
            parts.append(QStringLiteral("%1 尝试次数：%2").arg(platformName(platform)).arg(result.attempts));
        }
    }
    if (!entry.mcmodUrl.isEmpty())
        parts.append(QStringLiteral("MC百科链接：%1").arg(entry.mcmodUrl));
    return parts.join(QLatin1Char('\n'));
}

QVariant ResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const ModEntry &entry = m_entries.at(index.row());
    const int column = index.column();

    if (role == Qt::UserRole)
        return QVariant::fromValue<qulonglong>(entry.uid);

    if (role == Qt::DisplayRole) {
        switch (column) {
        case OriginalFile:
            return entry.originalFileName;
        case BaseCleanName:
            return entry.baseCleanName;
        case SearchName:
            return entry.searchName;
        case ModrinthName:
            return platformCell(entry.results[0], 0);
        case ModrinthId:
            return platformCell(entry.results[0], 1);
        case ModrinthAuthor:
            return platformCell(entry.results[0], 2);
        case ModrinthLicense:
            return platformCell(entry.results[0], 3);
        case ModrinthUrl:
            return platformCell(entry.results[0], 4);
        case CurseForgeName:
            return platformCell(entry.results[1], 0);
        case CurseForgeId:
            return platformCell(entry.results[1], 1);
        case CurseForgeAuthor:
            return platformCell(entry.results[1], 2);
        case CurseForgeLicense:
            return platformCell(entry.results[1], 3);
        case CurseForgeUrl:
            return platformCell(entry.results[1], 4);
        case McModUrl:
            if (!entry.mcmodUrl.isEmpty())
                return entry.mcmodUrl;
            if (entry.mcmodStatus.contains(QStringLiteral("检索中")))
                return QStringLiteral("检索中…");
            return {};
        case Status:
            return statusText(entry);
        case Operation:
            return QString();
        default:
            return {};
        }
    }

    if (role == Qt::ToolTipRole) {
        switch (column) {
        case Status:
            return statusToolTip(entry);
        case McModUrl:
            if (!entry.mcmodTitle.isEmpty() || !entry.mcmodStatus.isEmpty()) {
                QStringList tip;
                if (!entry.mcmodTitle.isEmpty())
                    tip.append(entry.mcmodTitle);
                if (!entry.mcmodStatus.isEmpty())
                    tip.append(entry.mcmodStatus);
                if (!entry.mcmodUrl.isEmpty()) {
                    tip.append(entry.mcmodUrl);
                    tip.append(QStringLiteral("（单击打开浏览器）"));
                }
                return tip.join(QLatin1Char('\n'));
            }
            return {};
        case ModrinthName:
        case ModrinthId:
        case ModrinthAuthor:
        case ModrinthLicense:
        case ModrinthUrl:
        case CurseForgeName:
        case CurseForgeId:
        case CurseForgeAuthor:
        case CurseForgeLicense:
        case CurseForgeUrl: {
            const int platformIndex = (column >= ModrinthName && column <= ModrinthUrl) ? 0 : 1;
            const PlatformResult &result = entry.results[platformIndex];
            if (!result.error.isEmpty())
                return result.error;
            if (!result.url.isEmpty()) {
                if (column == ModrinthUrl || column == CurseForgeUrl)
                    return result.url + QStringLiteral("\n（单击打开浏览器）");
                return result.url;
            }
            return {};
        }
        default:
            return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        if (column == Operation)
            return QVariant::fromValue<int>(Qt::AlignCenter);
        return QVariant::fromValue<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        if (column == ModrinthUrl || column == CurseForgeUrl) {
            if (entry.results[column == ModrinthUrl ? 0 : 1].state == SearchState::Success)
                return QBrush(kLinkColor);
        }
        if (column == McModUrl && !entry.mcmodUrl.isEmpty())
            return QBrush(kLinkColor);
        if (column == ModrinthName || column == CurseForgeName) {
            const SearchState state = entry.results[column == ModrinthName ? 0 : 1].state;
            if (state == SearchState::Failed)
                return QBrush(kErrorColor);
            if (state == SearchState::NotFound || state == SearchState::Skipped
                || state == SearchState::Cancelled) {
                return QBrush(kMutedColor);
            }
        }
        if (column == Status && entry.cleanAnomaly)
            return QBrush(kErrorColor);
        return {};
    }

    if (role == Qt::FontRole) {
        if (column == OriginalFile && entry.manualRow) {
            QFont font;
            font.setItalic(true);
            return font;
        }
        return {};
    }

    return {};
}

void ResultModel::setEntries(QVector<ModEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    for (ModEntry &entry : m_entries) {
        if (entry.uid == 0)
            entry.uid = allocateUid();
        else
            m_nextUid = qMax(m_nextUid, entry.uid + 1);
    }
    endResetModel();
    emit rowCountChanged();
}

void ResultModel::clearRows()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit rowCountChanged();
}

const ModEntry *ResultModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return nullptr;
    return &m_entries.at(row);
}

const ModEntry *ResultModel::entryById(quint64 uid) const
{
    const int row = rowIndexById(uid);
    return row < 0 ? nullptr : &m_entries.at(row);
}

int ResultModel::rowIndexById(quint64 uid) const
{
    return indexOfUid(uid);
}

int ResultModel::indexOfUid(quint64 uid) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).uid == uid)
            return i;
    }
    return -1;
}

quint64 ResultModel::allocateUid()
{
    while (m_nextUid == 0)
        ++m_nextUid;
    return m_nextUid++;
}

int ResultModel::countState(Platform platform, SearchState state) const
{
    int count = 0;
    for (const ModEntry &entry : m_entries) {
        if (entry.result(platform).state == state)
            ++count;
    }
    return count;
}

int ResultModel::successRowCount() const
{
    int count = 0;
    for (const ModEntry &entry : m_entries) {
        if (entry.results[0].isSuccess() || entry.results[1].isSuccess())
            ++count;
    }
    return count;
}

bool ResultModel::containsPlatformId(Platform platform, const QString &id) const
{
    if (id.trimmed().isEmpty())
        return false;
    for (const ModEntry &entry : m_entries) {
        const PlatformResult &result = entry.result(platform);
        if (result.isSuccess() && result.id == id)
            return true;
    }
    return false;
}

void ResultModel::preparePlatformForSearch(quint64 uid, Platform platform)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    PlatformResult &result = m_entries[row].result(platform);
    result = PlatformResult();
    notifyRow(row);
}

void ResultModel::markPlatformSkipped(quint64 uid, Platform platform, const QString &reason)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    PlatformResult &result = m_entries[row].result(platform);
    result = PlatformResult();
    result.state = SearchState::Skipped;
    result.error = reason;
    result.timestamp = QDateTime::currentDateTime();
    notifyRow(row);
}

void ResultModel::markPlatformCancelled(quint64 uid, Platform platform, const QString &reason)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    PlatformResult &result = m_entries[row].result(platform);
    result.state = SearchState::Cancelled;
    if (!reason.isEmpty())
        result.error = reason;
    result.timestamp = QDateTime::currentDateTime();
    notifyRow(row);
}

void ResultModel::setPlatformState(quint64 uid, Platform platform, SearchState state,
                                   int attempts, const QString &error)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    PlatformResult &result = m_entries[row].result(platform);
    result.state = state;
    if (attempts >= 0)
        result.attempts = attempts;
    if (!error.isEmpty())
        result.error = error;
    notifyRow(row);
}

void ResultModel::setPlatformResult(quint64 uid, Platform platform,
                                    const PlatformResult &result)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    m_entries[row].result(platform) = result;
    notifyRow(row);
}

void ResultModel::clearPlatformResult(quint64 uid, Platform platform)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    PlatformResult &result = m_entries[row].result(platform);
    result = PlatformResult();
    QString note = QStringLiteral("已移除 %1 结果").arg(platformName(platform));
    if (!m_entries[row].rowNote.isEmpty())
        note = m_entries[row].rowNote + QStringLiteral("；") + note;
    m_entries[row].rowNote = note;
    notifyRow(row);
}

void ResultModel::setMcModLookup(quint64 uid, const QString &url, const QString &title,
                                 const QString &status)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    m_entries[row].mcmodUrl = url;
    m_entries[row].mcmodTitle = title;
    m_entries[row].mcmodStatus = status;
    notifyRow(row);
}

void ResultModel::setRowNote(quint64 uid, const QString &note)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return;
    m_entries[row].rowNote = note;
    notifyRow(row);
}

bool ResultModel::addManualRow(const ManualRowData &data)
{
    if (data.searchName.trimmed().isEmpty())
        return false;

    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    ModEntry entry;
    entry.uid = allocateUid();
    entry.originalFileName = QStringLiteral("（手动）");
    entry.baseCleanName.clear();
    entry.searchName = data.searchName;
    entry.manualRow = true;
    entry.rowNote = QStringLiteral("手动添加");
    entry.results[0] = data.modrinth;
    entry.results[1] = data.curseforge;
    m_entries.append(entry);
    endInsertRows();
    emit rowCountChanged();
    return true;
}

bool ResultModel::removeRowById(quint64 uid)
{
    const int row = indexOfUid(uid);
    if (row < 0)
        return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();
    emit rowCountChanged();
    return true;
}

void ResultModel::notifyRow(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

QStringList ResultModel::exportHeaders() const
{
    return headers();
}

QVector<QStringList> ResultModel::exportRows() const
{
    QVector<QStringList> rows;
    rows.reserve(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        QStringList values;
        values.reserve(ColumnCount);
        for (int column = 0; column < ColumnCount; ++column)
            values.append(data(index(row, column), Qt::DisplayRole).toString());
        rows.append(values);
    }
    return rows;
}

void ResultModel::setActionsEnabled(bool enabled)
{
    if (m_actionsEnabled == enabled)
        return;
    m_actionsEnabled = enabled;
    emit actionsEnabledChanged(enabled);
    if (rowCount() > 0)
        emit dataChanged(index(0, Operation), index(rowCount() - 1, Operation));
}
