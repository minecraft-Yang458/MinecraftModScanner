#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct CleanedModFile
{
    QString filePath;
    QString originalFileName;
    QString baseCleanName;
    QString searchName;
    QString cleanerNote;
    bool cleanAnomaly = false;
};

struct ScanOutcome
{
    QVector<CleanedModFile> files;
    QStringList warnings;
};

class ScanCleaner
{
public:
    static ScanOutcome scanDirectory(const QString &directory, bool recursive);
    static CleanedModFile cleanFileName(const QString &fileName);

    // 供测试和复用的中间步骤。
    static QString removeBracketSegments(const QString &text);
    static QString removeModifierTokens(const QString &text);
    static QString removeVersionSegments(const QString &text);
    static QString removeTrailingMetadata(const QString &text);
    static QString loaderCleanup(const QString &text, QString *note, bool *fallbackUsed);
    static QString baseSeparators(const QString &text);
    static QString finalSeparators(const QString &text);
};
