#include "UrlLauncher.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
QString targetForSystem(const QUrl &url)
{
    if (url.isLocalFile())
        return QDir::toNativeSeparators(url.toLocalFile());
    return url.toString(QUrl::FullyEncoded);
}

bool allowedScheme(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("file");
}

#ifdef Q_OS_WIN
QString shellErrorText(INT_PTR code)
{
    switch (code) {
    case 0:
        return QStringLiteral("ShellExecute 失败：系统资源不足");
    case 2:
        return QStringLiteral("ShellExecute 失败：目标文件不存在");
    case 3:
        return QStringLiteral("ShellExecute 失败：目标路径不存在");
    case 5:
        return QStringLiteral("ShellExecute 失败：访问被拒绝");
    case 26:
        return QStringLiteral("ShellExecute 失败：共享冲突");
    case 27:
        return QStringLiteral("ShellExecute 失败：文件关联不完整");
    case 28:
        return QStringLiteral("ShellExecute 失败：DDE 超时");
    case 29:
        return QStringLiteral("ShellExecute 失败：DDE 失败");
    case 30:
        return QStringLiteral("ShellExecute 失败：DDE 忙");
    case 31:
        return QStringLiteral(
            "ShellExecute 失败：没有可用的默认浏览器或默认应用关联");
    default:
        return QStringLiteral("ShellExecute 失败，错误码 %1").arg(code);
    }
}

bool openWithShell(const QString &target, QString *error)
{
    const HINSTANCE result =
        ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(target.utf16()),
                      nullptr, nullptr, SW_SHOWNORMAL);
    const INT_PTR code = reinterpret_cast<INT_PTR>(result);
    if (code > 32)
        return true;
    if (error)
        *error = shellErrorText(code);
    return false;
}
#endif
} // namespace

QUrl UrlLauncher::normalize(const QString &input)
{
    QString text = input.trimmed();
    if (text.isEmpty())
        return {};

    if (!text.contains(QStringLiteral("://"), Qt::CaseInsensitive)
        && !text.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
        && !text.startsWith(QStringLiteral("//"))) {
        text.prepend(QStringLiteral("https://"));
    } else if (text.startsWith(QStringLiteral("//"))) {
        text.prepend(QStringLiteral("https:"));
    }

    QUrl url(text, QUrl::StrictMode);
    if (!url.isValid())
        url = QUrl::fromUserInput(text);
    return url;
}

bool UrlLauncher::open(const QUrl &url, QString *error)
{
    if (!url.isValid()) {
        if (error)
            *error = QStringLiteral("链接无效");
        return false;
    }
    if (!allowedScheme(url)) {
        if (error)
            *error = QStringLiteral("不支持的链接协议：%1").arg(url.scheme());
        return false;
    }

    const QString target = targetForSystem(url);

#ifdef Q_OS_WIN
    QString shellError;
    if (openWithShell(target, &shellError))
        return true;

    if (QDesktopServices::openUrl(url))
        return true;

    if (QProcess::startDetached(QStringLiteral("explorer.exe"), {target}))
        return true;

    if (QProcess::startDetached(QStringLiteral("rundll32.exe"),
                                {QStringLiteral("url.dll,FileProtocolHandler"), target})) {
        return true;
    }

    if (error) {
        *error = shellError.isEmpty()
            ? QStringLiteral("系统没有成功启动默认浏览器或默认应用")
            : shellError;
    }
    return false;
#else
    if (QDesktopServices::openUrl(url))
        return true;
    if (error)
        *error = QStringLiteral("系统没有成功启动默认浏览器或默认应用");
    return false;
#endif
}

bool UrlLauncher::openInteractive(QWidget *parent, const QUrl &url,
                                  const QString &description)
{
    const QUrl normalized = normalize(url.toString(QUrl::FullyEncoded));
    QString error;
    if (open(normalized, &error))
        return true;

    const QString target = targetForSystem(normalized);
    const bool webLink = normalized.scheme().compare(QStringLiteral("http"),
                                                      Qt::CaseInsensitive) == 0
        || normalized.scheme().compare(QStringLiteral("https"),
                                       Qt::CaseInsensitive) == 0;

    if (!target.isEmpty())
        QGuiApplication::clipboard()->setText(target);

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(webLink ? QStringLiteral("无法打开默认浏览器")
                               : QStringLiteral("无法打开文件或文件夹"));
    box.setText(webLink
                    ? QStringLiteral("无法启动系统默认浏览器。")
                    : QStringLiteral("无法调用系统默认程序打开此目标。"));
    box.setInformativeText(
        QStringLiteral("%1\n\n%2已复制到剪贴板：\n%3\n\n"
                       "如果默认浏览器未设置，请在系统“默认应用”中重新设置后重试。\n"
                       "如果程序以管理员权限运行，也可尝试以普通权限运行。")
            .arg(description.isEmpty() ? QStringLiteral("目标")
                                       : description,
                 webLink ? QStringLiteral("链接") : QStringLiteral("路径"), target));
    if (!error.isEmpty())
        box.setDetailedText(error);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();

    qWarning().noquote() << QStringLiteral("Failed to open '%1': %2")
                                .arg(target, error);
    return false;
}
