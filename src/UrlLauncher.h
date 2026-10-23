#pragma once

#include <QString>
#include <QUrl>

class QWidget;

namespace UrlLauncher {

QUrl normalize(const QString &input);
bool open(const QUrl &url, QString *error = nullptr);
bool openInteractive(QWidget *parent, const QUrl &url,
                     const QString &description = QString());

} // namespace UrlLauncher
