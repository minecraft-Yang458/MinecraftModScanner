#include "DetailsDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

DetailsDialog::DetailsDialog(const QString &title, const QString &subtitle,
                             const QVector<QPair<QString, QString>> &fields,
                             const QByteArray &rawResponse, QWidget *parent)
    : QDialog(parent)
    , m_title(title)
    , m_subtitle(subtitle)
    , m_fields(fields)
    , m_raw(rawResponse)
{
    setWindowTitle(title);
    setModal(true);
    resize(820, 640);

    auto *layout = new QVBoxLayout(this);

    auto *heading = new QLabel(QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped()), this);
    layout->addWidget(heading);
    if (!subtitle.isEmpty()) {
        auto *subLabel = new QLabel(subtitle, this);
        subLabel->setWordWrap(true);
        subLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(subLabel);
    }

    auto *formWidget = new QWidget(this);
    auto *form = new QFormLayout(formWidget);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    for (const auto &field : fields) {
        auto *label = new QLabel(field.second, formWidget);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setTextFormat(Qt::PlainText);
        form->addRow(field.first + QStringLiteral("："), label);
    }
    layout->addWidget(formWidget);

    auto *rawLabel = new QLabel(QStringLiteral("原始响应 / 原始信息"), this);
    layout->addWidget(rawLabel);

    auto *rawEdit = new QPlainTextEdit(this);
    rawEdit->setReadOnly(true);
    rawEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    rawEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    QString rawText;
    if (!m_raw.isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(m_raw);
        if (!document.isNull())
            rawText = QString::fromUtf8(document.toJson(QJsonDocument::Indented));
        else
            rawText = QString::fromUtf8(m_raw);
    }
    if (rawText.isEmpty())
        rawText = QStringLiteral("(无响应内容)");
    if (rawText.size() > 2 * 1024 * 1024)
        rawText = rawText.left(2 * 1024 * 1024) + QStringLiteral("\n…（内容过长，已截断显示）");
    rawEdit->setPlainText(rawText);
    layout->addWidget(rawEdit, 1);

    auto *buttons = new QDialogButtonBox(this);
    auto *copyButton = buttons->addButton(QStringLiteral("复制全部"), QDialogButtonBox::ActionRole);
    auto *copyRawButton =
        buttons->addButton(QStringLiteral("复制原始响应"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(copyButton, &QPushButton::clicked, this, &DetailsDialog::copyAll);
    connect(copyRawButton, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(QString::fromUtf8(m_raw));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString DetailsDialog::buildPlainText() const
{
    QStringList lines;
    lines.append(m_title);
    if (!m_subtitle.isEmpty())
        lines.append(m_subtitle);
    lines.append(QString());
    for (const auto &field : m_fields)
        lines.append(QStringLiteral("%1：%2").arg(field.first, field.second));
    lines.append(QString());
    lines.append(QStringLiteral("--- 原始响应 ---"));
    lines.append(QString::fromUtf8(m_raw));
    return lines.join(QLatin1Char('\n'));
}

void DetailsDialog::copyAll()
{
    QGuiApplication::clipboard()->setText(buildPlainText());
}

void DetailsDialog::showPlatformDetails(QWidget *parent, const ModEntry &entry,
                                        Platform platform)
{
    const PlatformResult &result = entry.result(platform);
    const bool success = result.isSuccess();
    const QString title = success ? QStringLiteral("详细检索原始信息 — %1").arg(platformName(platform))
                                  : QStringLiteral("错误详情 — %1").arg(platformName(platform));
    const QString subtitle =
        QStringLiteral("文件：%1\n检索用名称：%2")
            .arg(entry.originalFileName, entry.searchName);

    QVector<QPair<QString, QString>> fields;
    fields.append({QStringLiteral("平台"), platformName(platform)});
    fields.append({QStringLiteral("状态"), stateText(result.state)});
    fields.append({QStringLiteral("请求 URL"), result.requestUrl});
    fields.append({QStringLiteral("HTTP 状态码"),
                   result.httpStatus > 0 ? QString::number(result.httpStatus)
                                         : QStringLiteral("无（网络层失败）")});
    fields.append({QStringLiteral("耗时"), QStringLiteral("%1 ms").arg(result.elapsedMs)});
    fields.append({QStringLiteral("总尝试次数"), QString::number(result.attempts)});
    fields.append({QStringLiteral("其中重试次数"), QString::number(result.retryCount())});
    fields.append({QStringLiteral("请求时间"),
                   result.timestamp.isValid()
                       ? QLocale::c().toString(result.timestamp,
                                               QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                       : QStringLiteral("未知")});
    if (!result.error.trimmed().isEmpty())
        fields.append({QStringLiteral("错误"), result.error});
    if (success) {
        fields.append({QStringLiteral("名称"), result.name});
        fields.append({QStringLiteral("ID"), result.id});
        fields.append({QStringLiteral("作者"), result.author});
        fields.append({QStringLiteral("许可证"), result.displayLicense()});
        fields.append({QStringLiteral("直链"), result.url});
    }

    DetailsDialog dialog(title, subtitle, fields, result.rawResponse, parent);
    dialog.exec();
}
