#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

enum class ExportFormat {
    Csv,
    Markdown,
    Excel,
};

class ExportService
{
public:
    static ExportFormat formatFromString(const QString &name);
    static QString displayName(ExportFormat format);
    static QString fileFilter(ExportFormat format);
    static QString defaultSuffix(ExportFormat format);

    static bool exportToFile(const QString &path, ExportFormat format,
                             const QStringList &headers,
                             const QVector<QStringList> &rows, QString *error);

    static QByteArray buildCsv(const QStringList &headers,
                               const QVector<QStringList> &rows);
    static QByteArray buildMarkdown(const QStringList &headers,
                                    const QVector<QStringList> &rows);
    static QByteArray buildXlsx(const QStringList &headers,
                                const QVector<QStringList> &rows);
};
