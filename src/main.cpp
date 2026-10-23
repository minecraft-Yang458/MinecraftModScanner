#include "AppLog.h"
#include "MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QTextStream>

namespace {
QString loadStyleSheet()
{
    const QStringList candidates = {
        QStringLiteral(":/style.qss"),
        QStringLiteral(":/resources/style.qss"),
    };
    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            return stream.readAll();
        }
    }
    return {};
}
} // namespace

int main(int argc, char *argv[])
{
    QLocale::setDefault(QLocale::c());

    QApplication app(argc, argv);
    QLocale::setDefault(QLocale::c());

    QFont appFont = QApplication::font();
    appFont.setFamilies({
        QStringLiteral("Segoe UI"),
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Noto Sans"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Arial"),
        QStringLiteral("DejaVu Sans"),
    });
    appFont.setStyleHint(QFont::SansSerif);
    QApplication::setFont(appFont);

    QApplication::setOrganizationName(QStringLiteral("MinecraftModScanner"));
    QApplication::setOrganizationDomain(QStringLiteral("mcmodscanner.local"));
    QApplication::setApplicationName(QStringLiteral("MinecraftModScanner"));
    QApplication::setApplicationDisplayName(QStringLiteral("Minecraft 模组自动检索"));
    QApplication::setApplicationVersion(QStringLiteral(MMS_VERSION));

    const QIcon icon(QStringLiteral(":/app.svg"));
    if (!icon.isNull())
        QApplication::setWindowIcon(icon);

    AppLog::install();
    qInfo().noquote() << QStringLiteral("Starting MinecraftModScanner %1")
                             .arg(QStringLiteral(MMS_VERSION));
    qInfo().noquote() << QStringLiteral("Qt runtime: %1").arg(QString::fromLatin1(qVersion()));
    qInfo().noquote() << QStringLiteral("Log file: %1").arg(AppLog::logFilePath());

    const QString style = loadStyleSheet();
    if (!style.isEmpty())
        app.setStyleSheet(style);

    MainWindow window;
    window.show();

    const int result = app.exec();
    AppLog::shutdown();
    return result;
}
