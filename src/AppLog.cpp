#include "AppLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdio>

namespace {
QMutex g_mutex;
QFile g_file;
QString g_path;
QtMessageHandler g_previousHandler = nullptr;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "LOG";
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString line = QStringLiteral("[%1] [%2] %3")
                             .arg(QLocale::c().toString(
                                      QDateTime::currentDateTime(),
                                      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  QString::fromLatin1(levelName(type)), message);

    {
        QMutexLocker locker(&g_mutex);
        if (g_file.isOpen()) {
            QTextStream stream(&g_file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << line << '\n';
            stream.flush();
        }
    }

    std::fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}
} // namespace

void AppLog::install()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
    QDir().mkpath(dir);
    g_path = dir + QStringLiteral("/mcmodscanner.log");

    QMutexLocker locker(&g_mutex);
    const QFileInfo info(g_path);
    if (info.exists() && info.size() > 2 * 1024 * 1024) {
        const QString oldPath = g_path + QStringLiteral(".1");
        QFile::remove(oldPath);
        QFile::rename(g_path, oldPath);
    }
    g_file.setFileName(g_path);
    if (!g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        std::fprintf(stderr, "Unable to open log file: %s\n", g_path.toLocal8Bit().constData());

    g_previousHandler = qInstallMessageHandler(messageHandler);
}

void AppLog::shutdown()
{
    qInstallMessageHandler(g_previousHandler);
    QMutexLocker locker(&g_mutex);
    if (g_file.isOpen()) {
        g_file.flush();
        g_file.close();
    }
}

QString AppLog::logFilePath()
{
    return g_path;
}
