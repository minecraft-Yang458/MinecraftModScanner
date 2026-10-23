#include "SummaryDialog.h"

#include "LicenseUtils.h"
#include "ResultModel.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int kUidRole = Qt::UserRole;
constexpr int kPlatformRole = Qt::UserRole + 1;

QString shortError(const QString &error)
{
    const QString text = error.simplified();
    return text.size() > 55 ? text.left(54) + QChar(0x2026) : text;
}
} // namespace

SummaryDialog::SummaryDialog(ResultModel *model, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(QStringLiteral("总结视图"));
    setModal(false);
    resize(1080, 700);

    auto *root = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    m_statsLabel = new QLabel(this);
    m_statsLabel->setWordWrap(true);
    top->addWidget(m_statsLabel, 1);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    top->addWidget(refreshButton);
    root->addLayout(top);

    auto *tabs = new QTabWidget(this);
    m_licenseTree = createTree();
    m_licenseTree->setHeaderLabels(
        {QStringLiteral("条目"), QStringLiteral("成功平台"),
         QStringLiteral("许可证"), QStringLiteral("状态")});
    tabs->addTab(m_licenseTree, QStringLiteral("按许可证"));

    m_statusTree = createTree();
    m_statusTree->setHeaderLabels(
        {QStringLiteral("条目"), QStringLiteral("平台"),
         QStringLiteral("检索状态"), QStringLiteral("操作")});
    tabs->addTab(m_statusTree, QStringLiteral("按检索状态"));
    root->addWidget(tabs, 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(180);
    connect(m_refreshTimer, &QTimer::timeout, this, &SummaryDialog::refresh);

    connect(refreshButton, &QPushButton::clicked, this, &SummaryDialog::refresh);
    connect(m_licenseTree, &QTreeWidget::itemDoubleClicked, this,
            &SummaryDialog::onItemDoubleClicked);
    connect(m_statusTree, &QTreeWidget::itemDoubleClicked, this,
            &SummaryDialog::onItemDoubleClicked);

    connect(m_model, &QAbstractItemModel::dataChanged, this,
            [this]() { scheduleRefresh(); });
    connect(m_model, &QAbstractItemModel::modelReset, this,
            [this]() { scheduleRefresh(); });
    connect(m_model, &QAbstractItemModel::rowsInserted, this,
            [this]() { scheduleRefresh(); });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this,
            [this]() { scheduleRefresh(); });

    refresh();
}

QTreeWidget *SummaryDialog::createTree()
{
    auto *tree = new QTreeWidget(this);
    tree->setColumnCount(4);
    tree->setRootIsDecorated(true);
    tree->setAlternatingRowColors(true);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setUniformRowHeights(false);
    tree->setAnimated(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    tree->setColumnWidth(3, 300);
    return tree;
}

void SummaryDialog::scheduleRefresh()
{
    if (m_refreshTimer && !m_refreshTimer->isActive())
        m_refreshTimer->start();
}

QTreeWidgetItem *SummaryDialog::addGroup(QTreeWidget *tree, const QString &title)
{
    auto *group = new QTreeWidgetItem(tree);
    group->setFirstColumnSpanned(true);
    QFont font = group->font(0);
    font.setBold(true);
    group->setFont(0, font);
    group->setText(0, title);
    group->setExpanded(true);
    group->setFlags(Qt::ItemIsEnabled);
    return group;
}

void SummaryDialog::updateGroupTitle(QTreeWidgetItem *group, int count)
{
    const QString base = group->text(0).section(QStringLiteral("（"), 0, 0);
    group->setText(0, QStringLiteral("%1（%2）").arg(base).arg(count));
}

void SummaryDialog::refresh()
{
    if (!m_model)
        return;

    int success = 0;
    int notFound = 0;
    int failed = 0;
    int skipped = 0;
    int cancelled = 0;
    for (const ModEntry &entry : m_model->entries()) {
        for (int i = 0; i < kPlatformCount; ++i) {
            switch (entry.results[i].state) {
            case SearchState::Success:
                ++success;
                break;
            case SearchState::NotFound:
                ++notFound;
                break;
            case SearchState::Failed:
                ++failed;
                break;
            case SearchState::Skipped:
                ++skipped;
                break;
            case SearchState::Cancelled:
                ++cancelled;
                break;
            default:
                break;
            }
        }
    }

    int mcmodLinks = 0;
    for (const ModEntry &entry : m_model->entries()) {
        if (!entry.mcmodUrl.isEmpty())
            ++mcmodLinks;
    }

    m_statsLabel->setText(
        QStringLiteral("统计摘要：共 %1 行；成功平台 %2，404 未找到 %3，请求失败 %4，"
                       "跳过 %5，用户取消 %6；MC百科链接 %7。双击条目可跳转主表格。")
            .arg(m_model->rowCount())
            .arg(success)
            .arg(notFound)
            .arg(failed)
            .arg(skipped)
            .arg(cancelled)
            .arg(mcmodLinks));

    populateLicenseTree();
    populateStatusTree();
}

void SummaryDialog::populateLicenseTree()
{
    m_licenseTree->clear();

    if (m_model->rowCount() == 0) {
        QTreeWidgetItem *group = addGroup(m_licenseTree, QStringLiteral("待检索"));
        auto *hint = new QTreeWidgetItem(group);
        hint->setFirstColumnSpanned(true);
        hint->setText(0, QStringLiteral("尚未检索：请先扫描 mods 文件夹并开始检索。"));
        hint->setFlags(Qt::ItemIsEnabled);
        return;
    }

    bool anySearched = false;
    for (const ModEntry &entry : m_model->entries()) {
        for (int i = 0; i < kPlatformCount; ++i) {
            if (entry.results[i].state != SearchState::NotSearched) {
                anySearched = true;
                break;
            }
        }
        if (anySearched)
            break;
    }

    if (!anySearched) {
        QTreeWidgetItem *group = addGroup(m_licenseTree, QStringLiteral("待检索"));
        auto *hint = new QTreeWidgetItem(group);
        hint->setFirstColumnSpanned(true);
        hint->setText(0, QStringLiteral("尚未检索：当前所有行都没有检索结果。"));
        hint->setFlags(Qt::ItemIsEnabled);
        return;
    }

    QHash<QString, QTreeWidgetItem *> groups;
    for (const QString &title : LicenseUtils::licenseGroupOrder())
        groups.insert(title, addGroup(m_licenseTree, title));

    QHash<QString, int> counts;
    for (const ModEntry &entry : m_model->entries()) {
        const QStringList categories = LicenseUtils::categoriesForRow(entry);
        for (const QString &category : categories) {
            QTreeWidgetItem *group = groups.value(category);
            if (!group)
                continue;
            addLicenseRow(group, entry.uid);
            counts[category] += 1;
        }
    }

    for (const QString &title : LicenseUtils::licenseGroupOrder()) {
        QTreeWidgetItem *group = groups.value(title);
        if (group)
            updateGroupTitle(group, counts.value(title));
    }
}

void SummaryDialog::addLicenseRow(QTreeWidgetItem *group, quint64 uid)
{
    const ModEntry *entry = m_model->entryById(uid);
    if (!entry)
        return;

    auto *item = new QTreeWidgetItem(group);
    QString label = entry->originalFileName;
    if (!entry->searchName.isEmpty() && entry->searchName != entry->originalFileName)
        label += QStringLiteral("  →  %1").arg(entry->searchName);
    item->setText(0, label);

    QStringList platforms;
    QStringList licenses;
    for (int i = 0; i < kPlatformCount; ++i) {
        const PlatformResult &result = entry->results[i];
        if (!result.isSuccess())
            continue;
        platforms.append(platformName(platformFromInt(i)));
        licenses.append(result.displayLicense());
    }
    item->setText(1, platforms.join(QStringLiteral(" / ")));
    item->setText(2, licenses.join(QStringLiteral(" / ")));
    item->setText(3, entry->cleanAnomaly ? QStringLiteral("清洗异常")
                                         : QStringLiteral("已检索"));
    item->setData(0, kUidRole, QVariant::fromValue<qulonglong>(uid));
    item->setData(0, kPlatformRole, -1);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
}

void SummaryDialog::populateStatusTree()
{
    m_statusTree->clear();

    if (m_model->rowCount() == 0) {
        QTreeWidgetItem *group = addGroup(m_statusTree, QStringLiteral("待检索"));
        auto *hint = new QTreeWidgetItem(group);
        hint->setFirstColumnSpanned(true);
        hint->setText(0, QStringLiteral("尚未检索：请先扫描 mods 文件夹并开始检索。"));
        hint->setFlags(Qt::ItemIsEnabled);
        return;
    }

    bool anySearched = false;
    for (const ModEntry &entry : m_model->entries()) {
        for (int i = 0; i < kPlatformCount; ++i) {
            if (entry.results[i].state != SearchState::NotSearched) {
                anySearched = true;
                break;
            }
        }
        if (anySearched)
            break;
    }

    const QStringList groupOrder = {
        QStringLiteral("成功"),
        QStringLiteral("404未找到"),
        QStringLiteral("请求失败"),
        QStringLiteral("跳过"),
        QStringLiteral("清洗异常"),
        QStringLiteral("待检索"),
        QStringLiteral("用户取消"),
    };
    QHash<QString, QTreeWidgetItem *> groups;
    for (const QString &title : groupOrder)
        groups.insert(title, addGroup(m_statusTree, title));

    if (!anySearched) {
        for (QTreeWidgetItem *group : groups)
            group->setHidden(group->text(0).section(QStringLiteral("（"), 0, 0)
                             != QStringLiteral("待检索"));
        auto *hint = new QTreeWidgetItem(groups.value(QStringLiteral("待检索")));
        hint->setFirstColumnSpanned(true);
        hint->setText(0, QStringLiteral("尚未检索。以下是等待检索的行，可逐条手动检索。"));
        hint->setFlags(Qt::ItemIsEnabled);
    }

    QHash<QString, int> counts;
    for (const ModEntry &entry : m_model->entries()) {
        if (entry.cleanAnomaly) {
            auto *item = addStatusRow(groups.value(QStringLiteral("清洗异常")), entry.uid, -1,
                                      QStringLiteral("清洗异常，已回退原始文件名"),
                                      entry.cleanerNote);
            Q_UNUSED(item);
            counts[QStringLiteral("清洗异常")] += 1;
        }

        for (int i = 0; i < kPlatformCount; ++i) {
            const PlatformResult &result = entry.results[i];
            QString groupName;
            QString status;
            QString tooltip = result.error;
            switch (result.state) {
            case SearchState::Success:
                groupName = QStringLiteral("成功");
                status = QStringLiteral("成功");
                tooltip.clear();
                break;
            case SearchState::NotFound:
                groupName = QStringLiteral("404未找到");
                status = QStringLiteral("404 未找到");
                break;
            case SearchState::Failed:
                groupName = QStringLiteral("请求失败");
                status = QStringLiteral("请求失败%1")
                             .arg(result.error.isEmpty()
                                      ? QString()
                                      : QStringLiteral("：%1").arg(shortError(result.error)));
                break;
            case SearchState::Skipped:
                groupName = QStringLiteral("跳过");
                status = QStringLiteral("跳过");
                break;
            case SearchState::Cancelled:
                groupName = QStringLiteral("用户取消");
                status = QStringLiteral("用户取消");
                break;
            case SearchState::Searching:
            case SearchState::RetryWaiting:
                groupName = QStringLiteral("待检索");
                status = QStringLiteral("检索中");
                break;
            case SearchState::NotSearched:
                groupName = QStringLiteral("待检索");
                status = QStringLiteral("未检索");
                break;
            }

            QTreeWidgetItem *group = groups.value(groupName);
            if (!group)
                continue;
            addStatusRow(group, entry.uid, i, status, tooltip);
            counts[groupName] += 1;
        }
    }

    for (const QString &title : groupOrder)
        updateGroupTitle(groups.value(title), counts.value(title));
}

QTreeWidgetItem *SummaryDialog::addStatusRow(QTreeWidgetItem *group, quint64 uid, int platform,
                                             const QString &statusText,
                                             const QString &tooltip)
{
    const ModEntry *entry = m_model->entryById(uid);
    if (!entry || !group)
        return nullptr;

    auto *item = new QTreeWidgetItem(group);
    QString label = entry->originalFileName;
    if (!entry->searchName.isEmpty() && entry->searchName != entry->originalFileName)
        label += QStringLiteral("  →  %1").arg(entry->searchName);
    item->setText(0, label);
    item->setText(1, platform < 0 ? QStringLiteral("整行") : platformName(platformFromInt(platform)));
    item->setText(2, statusText);
    if (!tooltip.trimmed().isEmpty())
        item->setToolTip(2, tooltip);
    item->setData(0, kUidRole, QVariant::fromValue<qulonglong>(uid));
    item->setData(0, kPlatformRole, platform);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    if (statusText != QStringLiteral("成功")) {
        QWidget *actions = createActionWidget(uid, platform);
        m_statusTree->setItemWidget(item, 3, actions);
        item->setSizeHint(3, QSize(0, 28));
    }
    return item;
}

QWidget *SummaryDialog::createActionWidget(quint64 uid, int platform)
{
    auto *widget = new QWidget(m_statusTree);
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(2, 1, 2, 1);
    layout->setSpacing(4);

    auto addButton = [this, widget, layout, uid, platform](const QString &text,
                                                           bool retry) {
        auto *button = new QToolButton(widget);
        button->setText(text);
        button->setAutoRaise(true);
        button->setStyleSheet(QStringLiteral("QToolButton { padding: 1px 5px; }"));
        layout->addWidget(button);
        if (retry) {
            connect(button, &QToolButton::clicked, this, [this, uid, platform, button]() {
                askRetry(uid, platform, button);
            });
        } else if (text == QStringLiteral("复制名称")) {
            connect(button, &QToolButton::clicked, this,
                    [this, uid]() { copySearchName(uid); });
        } else {
            connect(button, &QToolButton::clicked, this, [this, uid, platform, button]() {
                askManual(uid, platform, button);
            });
        }
    };

    addButton(QStringLiteral("重新检索"), true);
    addButton(QStringLiteral("复制名称"), false);
    addButton(QStringLiteral("手动检索"), false);
    layout->addStretch(1);
    return widget;
}

void SummaryDialog::askRetry(quint64 uid, int platform, QWidget *anchor)
{
    if (platform >= 0) {
        emit retryRequested(uid, platform);
        return;
    }

    QMenu menu(this);
    QAction *modrinth = menu.addAction(QStringLiteral("重新检索 Modrinth"));
    QAction *curseForge = menu.addAction(QStringLiteral("重新检索 CurseForge"));
    QAction *chosen = menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
    if (chosen == modrinth)
        emit retryRequested(uid, platformToInt(Platform::Modrinth));
    else if (chosen == curseForge)
        emit retryRequested(uid, platformToInt(Platform::CurseForge));
}

void SummaryDialog::askManual(quint64 uid, int platform, QWidget *anchor)
{
    if (platform >= 0) {
        emit manualSearchRequested(uid, platform);
        return;
    }

    QMenu menu(this);
    QAction *modrinth = menu.addAction(QStringLiteral("手动检索 Modrinth"));
    QAction *curseForge = menu.addAction(QStringLiteral("手动检索 CurseForge"));
    QAction *chosen = menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
    if (chosen == modrinth)
        emit manualSearchRequested(uid, platformToInt(Platform::Modrinth));
    else if (chosen == curseForge)
        emit manualSearchRequested(uid, platformToInt(Platform::CurseForge));
}

void SummaryDialog::copySearchName(quint64 uid)
{
    const ModEntry *entry = m_model->entryById(uid);
    if (!entry)
        return;
    QGuiApplication::clipboard()->setText(entry->searchName);
}

void SummaryDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item)
        return;
    const QVariant uidValue = item->data(0, kUidRole);
    if (!uidValue.isValid())
        return;
    emit rowActivated(uidValue.toULongLong());
}
