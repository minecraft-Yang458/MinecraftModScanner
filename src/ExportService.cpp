#include "ExportService.h"

#include <QDateTime>
#include <QSaveFile>
#include <QVector>
#include <algorithm>

namespace {
QString csvField(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(value);
}

QString markdownField(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    value.replace(QLatin1Char('\r'), QString());
    value.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return value;
}

QString xmlEscape(QString value)
{
    QString cleaned;
    cleaned.reserve(value.size());
    for (QChar ch : value) {
        const ushort code = ch.unicode();
        if (code < 0x20 && ch != QLatin1Char('\t') && ch != QLatin1Char('\n')
            && ch != QLatin1Char('\r')) {
            cleaned.append(QLatin1Char(' '));
        } else if (code == 0xFFFE || code == 0xFFFF) {
            continue;
        } else {
            cleaned.append(ch);
        }
    }
    if (cleaned.size() > 32767)
        cleaned.truncate(32767);
    cleaned.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    cleaned.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    cleaned.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    cleaned.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    cleaned.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
    return cleaned;
}

QString columnName(int index)
{
    QString name;
    int value = index + 1;
    while (value > 0) {
        const int remainder = (value - 1) % 26;
        name.prepend(QChar(QLatin1Char('A').unicode() + remainder));
        value = (value - 1) / 26;
    }
    return name;
}

quint32 crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }

    quint32 crc = 0xFFFFFFFFU;
    for (unsigned char byte : data)
        crc = table[(crc ^ byte) & 0xFFU] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

void appendU16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>(value & 0xFFU));
    out.append(static_cast<char>((value >> 8) & 0xFFU));
}

void appendU32(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>(value & 0xFFU));
    out.append(static_cast<char>((value >> 8) & 0xFFU));
    out.append(static_cast<char>((value >> 16) & 0xFFU));
    out.append(static_cast<char>((value >> 24) & 0xFFU));
}

struct ZipEntry
{
    QByteArray name;
    QByteArray data;
    quint32 crc = 0;
    quint32 offset = 0;
};

QByteArray buildStoredZip(const QVector<QPair<QByteArray, QByteArray>> &files)
{
    QVector<ZipEntry> entries;
    entries.reserve(files.size());

    const QDateTime now = QDateTime::currentDateTime();
    const quint16 dosTime = static_cast<quint16>(
        (now.time().hour() << 11) | (now.time().minute() << 5) | (now.time().second() / 2));
    const quint16 dosDate = static_cast<quint16>(
        ((now.date().year() - 1980) << 9) | (now.date().month() << 5) | now.date().day());

    QByteArray out;
    out.reserve(64 * 1024);
    for (const auto &file : files) {
        ZipEntry entry;
        entry.name = file.first;
        entry.data = file.second;
        entry.crc = crc32(entry.data);
        entry.offset = static_cast<quint32>(out.size());

        appendU32(out, 0x04034B50U);
        appendU16(out, 20);
        appendU16(out, 0x0800); // UTF-8 names
        appendU16(out, 0);      // stored
        appendU16(out, dosTime);
        appendU16(out, dosDate);
        appendU32(out, entry.crc);
        appendU32(out, static_cast<quint32>(entry.data.size()));
        appendU32(out, static_cast<quint32>(entry.data.size()));
        appendU16(out, static_cast<quint16>(entry.name.size()));
        appendU16(out, 0);
        out.append(entry.name);
        out.append(entry.data);
        entries.append(entry);
    }

    const quint32 centralOffset = static_cast<quint32>(out.size());
    for (const ZipEntry &entry : entries) {
        appendU32(out, 0x02014B50U);
        appendU16(out, 20);
        appendU16(out, 20);
        appendU16(out, 0x0800);
        appendU16(out, 0);
        appendU16(out, dosTime);
        appendU16(out, dosDate);
        appendU32(out, entry.crc);
        appendU32(out, static_cast<quint32>(entry.data.size()));
        appendU32(out, static_cast<quint32>(entry.data.size()));
        appendU16(out, static_cast<quint16>(entry.name.size()));
        appendU16(out, 0);
        appendU16(out, 0);
        appendU16(out, 0);
        appendU16(out, 0);
        appendU32(out, 0);
        appendU32(out, entry.offset);
        out.append(entry.name);
    }
    const quint32 centralSize = static_cast<quint32>(out.size()) - centralOffset;

    appendU32(out, 0x06054B50U);
    appendU16(out, 0);
    appendU16(out, 0);
    appendU16(out, static_cast<quint16>(entries.size()));
    appendU16(out, static_cast<quint16>(entries.size()));
    appendU32(out, centralSize);
    appendU32(out, centralOffset);
    appendU16(out, 0);
    return out;
}

QByteArray stylesXml()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<numFmts count=\"0\"/>"
        "<fonts count=\"2\">"
        "<font><sz val=\"11\"/><name val=\"Calibri\"/><family val=\"2\"/></font>"
        "<font><b/><sz val=\"11\"/><name val=\"Calibri\"/><family val=\"2\"/></font>"
        "</fonts>"
        "<fills count=\"3\">"
        "<fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFDDEBF7\"/>"
        "<bgColor indexed=\"64\"/></patternFill></fill>"
        "</fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>"
        "</cellStyleXfs>"
        "<cellXfs count=\"2\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"0\" xfId=\"0\" "
        "applyFont=\"1\" applyFill=\"1\"/>"
        "</cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/>"
        "</cellStyles>"
        "</styleSheet>");
}
} // namespace

ExportFormat ExportService::formatFromString(const QString &name)
{
    if (name.compare(QStringLiteral("Markdown"), Qt::CaseInsensitive) == 0
        || name.compare(QStringLiteral("MD"), Qt::CaseInsensitive) == 0) {
        return ExportFormat::Markdown;
    }
    if (name.compare(QStringLiteral("Excel"), Qt::CaseInsensitive) == 0
        || name.compare(QStringLiteral("XLSX"), Qt::CaseInsensitive) == 0) {
        return ExportFormat::Excel;
    }
    return ExportFormat::Csv;
}

QString ExportService::displayName(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Csv:
        return QStringLiteral("CSV");
    case ExportFormat::Markdown:
        return QStringLiteral("Markdown");
    case ExportFormat::Excel:
        return QStringLiteral("Excel");
    }
    return QStringLiteral("CSV");
}

QString ExportService::fileFilter(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Csv:
        return QStringLiteral("CSV 文件 (*.csv)");
    case ExportFormat::Markdown:
        return QStringLiteral("Markdown 文件 (*.md)");
    case ExportFormat::Excel:
        return QStringLiteral("Excel 工作簿 (*.xlsx)");
    }
    return QStringLiteral("所有文件 (*)");
}

QString ExportService::defaultSuffix(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Csv:
        return QStringLiteral("csv");
    case ExportFormat::Markdown:
        return QStringLiteral("md");
    case ExportFormat::Excel:
        return QStringLiteral("xlsx");
    }
    return QStringLiteral("csv");
}

QByteArray ExportService::buildCsv(const QStringList &headers,
                                   const QVector<QStringList> &rows)
{
    QByteArray out("\xEF\xBB\xBF", 3); // 带 BOM，便于 Excel 直接打开 UTF-8。
    QStringList headerFields;
    for (const QString &header : headers)
        headerFields.append(csvField(header));
    out.append(headerFields.join(QLatin1Char(',')).toUtf8());
    out.append("\r\n");

    for (const QStringList &row : rows) {
        QStringList fields;
        fields.reserve(headers.size());
        for (int i = 0; i < headers.size(); ++i)
            fields.append(csvField(i < row.size() ? row.at(i) : QString()));
        out.append(fields.join(QLatin1Char(',')).toUtf8());
        out.append("\r\n");
    }
    return out;
}

QByteArray ExportService::buildMarkdown(const QStringList &headers,
                                        const QVector<QStringList> &rows)
{
    QString out;
    out.append(QStringLiteral("| "));
    QStringList headerFields;
    for (const QString &header : headers)
        headerFields.append(markdownField(header));
    out.append(headerFields.join(QStringLiteral(" | ")));
    out.append(QStringLiteral(" |\n| "));
    QStringList separators;
    for (int i = 0; i < headers.size(); ++i)
        separators.append(QStringLiteral("---"));
    out.append(separators.join(QStringLiteral(" | ")));
    out.append(QStringLiteral(" |\n"));

    for (const QStringList &row : rows) {
        QStringList fields;
        fields.reserve(headers.size());
        for (int i = 0; i < headers.size(); ++i)
            fields.append(markdownField(i < row.size() ? row.at(i) : QString()));
        out.append(QStringLiteral("| "));
        out.append(fields.join(QStringLiteral(" | ")));
        out.append(QStringLiteral(" |\n"));
    }
    return out.toUtf8();
}

QByteArray ExportService::buildXlsx(const QStringList &headers,
                                    const QVector<QStringList> &rows)
{
    const int columnCount = qMax(1, headers.size());
    const int rowCount = rows.size() + 1;

    QString sheet;
    sheet.reserve(4096 + rowCount * columnCount * 32);
    sheet.append(QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"));
    sheet.append(QStringLiteral("<dimension ref=\"A1:%1%2\"/>")
                     .arg(columnName(columnCount - 1))
                     .arg(rowCount));
    sheet.append(QStringLiteral(
        "<sheetViews><sheetView workbookViewId=\"0\"><pane ySplit=\"1\" topLeftCell=\"A2\" "
        "activePane=\"bottomLeft\" state=\"frozen\"/></sheetView></sheetViews>"));
    sheet.append(QStringLiteral("<sheetFormatPr defaultRowHeight=\"15\"/>"));

    const QVector<double> widths = {26, 18, 18, 26, 16, 16, 14, 42,
                                    26, 16, 16, 14, 42, 34, 46, 9};
    sheet.append(QStringLiteral("<cols>"));
    for (int col = 0; col < columnCount; ++col) {
        const double width = col < widths.size() ? widths.at(col) : 18.0;
        sheet.append(QStringLiteral("<col min=\"%1\" max=\"%1\" width=\"%2\" customWidth=\"1\"/>")
                         .arg(col + 1)
                         .arg(width));
    }
    sheet.append(QStringLiteral("</cols><sheetData>"));

    auto appendRow = [&sheet, columnCount](const QStringList &values, int rowIndex,
                                           bool header) {
        sheet.append(QStringLiteral("<row r=\"%1\">").arg(rowIndex));
        for (int col = 0; col < columnCount; ++col) {
            const QString text = col < values.size() ? values.at(col) : QString();
            if (text.isEmpty())
                continue;
            const QString reference =
                QStringLiteral("%1%2").arg(columnName(col)).arg(rowIndex);
            sheet.append(QStringLiteral("<c r=\"%1\" t=\"inlineStr\"%2><is><t xml:space=\"preserve\">%3</t></is></c>")
                             .arg(reference,
                                  header ? QStringLiteral(" s=\"1\"") : QString(),
                                  xmlEscape(text)));
        }
        sheet.append(QStringLiteral("</row>"));
    };

    appendRow(headers, 1, true);
    for (int i = 0; i < rows.size(); ++i)
        appendRow(rows.at(i), i + 2, false);

    sheet.append(QStringLiteral("</sheetData>"));
    if (rowCount > 1) {
        sheet.append(QStringLiteral("<autoFilter ref=\"A1:%1%2\"/>")
                         .arg(columnName(columnCount - 1))
                         .arg(rowCount));
    }
    sheet.append(QStringLiteral("</worksheet>"));

    const QString workbook = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"检索结果\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
        "</workbook>");
    const QString workbookRels = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
        "Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
        "Target=\"styles.xml\"/>"
        "</Relationships>");
    const QString rootRels = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"xl/workbook.xml\"/>"
        "</Relationships>");
    const QString contentTypes = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "</Types>");

    QVector<QPair<QByteArray, QByteArray>> files;
    files.append({QByteArrayLiteral("[Content_Types].xml"), contentTypes.toUtf8()});
    files.append({QByteArrayLiteral("_rels/.rels"), rootRels.toUtf8()});
    files.append({QByteArrayLiteral("xl/workbook.xml"), workbook.toUtf8()});
    files.append({QByteArrayLiteral("xl/_rels/workbook.xml.rels"), workbookRels.toUtf8()});
    files.append({QByteArrayLiteral("xl/styles.xml"), stylesXml()});
    files.append({QByteArrayLiteral("xl/worksheets/sheet1.xml"), sheet.toUtf8()});
    return buildStoredZip(files);
}

bool ExportService::exportToFile(const QString &path, ExportFormat format,
                                 const QStringList &headers,
                                 const QVector<QStringList> &rows, QString *error)
{
    QByteArray data;
    switch (format) {
    case ExportFormat::Csv:
        data = buildCsv(headers, rows);
        break;
    case ExportFormat::Markdown:
        data = buildMarkdown(headers, rows);
        break;
    case ExportFormat::Excel:
        data = buildXlsx(headers, rows);
        break;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("无法写入文件：%1").arg(file.errorString());
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("写入文件失败：%1").arg(file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("保存文件失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}
