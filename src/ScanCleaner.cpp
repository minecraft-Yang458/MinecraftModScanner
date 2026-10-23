#include "ScanCleaner.h"

#include "AppConfig.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace {
const QRegularExpression &modifierRegex()
{
    static const QRegularExpression regex(
        QStringLiteral(R"((?<![A-Za-z0-9])(?:beta|alpha|snapshot)(?![A-Za-z0-9]))"),
        QRegularExpression::CaseInsensitiveOption);
    return regex;
}

QString trimSeparators(QString value)
{
    while (!value.isEmpty()) {
        const QChar c = value.at(0);
        if (c.isSpace() || c == QLatin1Char('.') || c == QLatin1Char('_')
            || c == QLatin1Char('-') || c == QLatin1Char('+')) {
            value.remove(0, 1);
        } else {
            break;
        }
    }
    while (!value.isEmpty()) {
        const QChar c = value.at(value.size() - 1);
        if (c.isSpace() || c == QLatin1Char('.') || c == QLatin1Char('_')
            || c == QLatin1Char('-') || c == QLatin1Char('+')) {
            value.chop(1);
        } else {
            break;
        }
    }
    return value;
}

void removeEmptyBrackets(QString &value)
{
    static const QRegularExpression emptyPair(
        QStringLiteral(R"((\[\s*\]|\(\s*\)|\{\s*\}))"));
    for (int i = 0; i < 4; ++i) {
        const QString next = value;
        value.remove(emptyPair);
        if (value == next)
            break;
    }
}

QString loaderPattern()
{
    QStringList words = AppConfig::loaderWords();
    std::sort(words.begin(), words.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    QStringList escaped;
    for (const QString &word : words)
        escaped.append(QRegularExpression::escape(word));

    return QStringLiteral(R"((?<![A-Za-z0-9])(?:%1)(?![A-Za-z0-9]))")
        .arg(escaped.join(QLatin1Char('|')));
}

QStringList metadataSuffixes()
{
    return {
        QStringLiteral("hotfix"),
        QStringLiteral("universal"),
        QStringLiteral("all"),
        QStringLiteral("slim"),
        QStringLiteral("release"),
        QStringLiteral("re"),
        QStringLiteral("nobiomes"),
    };
}

bool containsLetterOrNumber(const QString &text)
{
    for (QChar ch : text) {
        if (ch.isLetterOrNumber())
            return true;
    }
    return false;
}
} // namespace

QString ScanCleaner::removeBracketSegments(const QString &text)
{
    QString value = text;
    static const QRegularExpression asciiBrackets(
        QStringLiteral(R"(\[[^\]]*\])"));
    static const QRegularExpression fullWidthBrackets(
        QStringLiteral(R"(【[^】]*】)"));
    static const QRegularExpression fullWidthSquareBrackets(
        QStringLiteral(R"(［[^］]*］)"));
    value.remove(asciiBrackets);
    value.remove(fullWidthBrackets);
    value.remove(fullWidthSquareBrackets);
    return value;
}

QString ScanCleaner::removeModifierTokens(const QString &text)
{
    QString value = text;
    value.remove(modifierRegex());
    return value;
}

QString ScanCleaner::removeVersionSegments(const QString &text)
{
    QString value = text;

    // build.10 / build-131 / build10 等构建号。
    static const QRegularExpression buildVersion(
        QStringLiteral(
            R"((?<![A-Za-z0-9])build[-_. ]?\d+(?:\.\d+)*(?![A-Za-z0-9.]))"),
        QRegularExpression::CaseInsensitiveOption);
    value.remove(buildVersion);

    // v1.2.3 / mc1.20.1 / MC_1.20.1 / MC-1.20.1（完整版本 token）。
    static const QRegularExpression labeledVersion(
        QStringLiteral(
            R"((?<![A-Za-z0-9])(?:mc|v)[-_. ]?\d+(?:\.\d+)+(?![A-Za-z0-9]))"),
        QRegularExpression::CaseInsensitiveOption);
    value.remove(labeledVersion);

    // 仍嵌在文本中的数字点号版本段，例如 mod1.2.3 -> mod。
    static const QRegularExpression embeddedVersion(
        QStringLiteral(R"(\d+(?:\.\d+)+)"));
    value.remove(embeddedVersion);

    // 不带点号的纯版本号，例如 V29、v2；排除 V1.9 这类已由上面的规则处理的形式。
    static const QRegularExpression shortVVersion(
        QStringLiteral(R"((?<![A-Za-z0-9])v\d+(?![A-Za-z0-9.]))"),
        QRegularExpression::CaseInsensitiveOption);
    value.remove(shortVVersion);

    return value;
}

QString ScanCleaner::removeTrailingMetadata(const QString &text)
{
    QString value = text;
    const QStringList suffixes = metadataSuffixes();

    for (;;) {
        int end = value.size();
        while (end > 0 && !value.at(end - 1).isLetterOrNumber())
            --end;
        if (end == 0)
            break;

        int start = end;
        while (start > 0 && value.at(start - 1).isLetterOrNumber())
            --start;
        if (start == 0)
            break;

        const QString token = value.mid(start, end - start).toLower();
        if (!suffixes.contains(token))
            break;

        const QString prefix = value.left(start);
        if (!containsLetterOrNumber(prefix))
            break;

        value = prefix;
    }
    return value;
}

QString ScanCleaner::baseSeparators(const QString &text)
{
    QString value = text;
    value.replace(QRegularExpression(QStringLiteral(R"([\t\r\n ]+)")),
                  QStringLiteral(" "));
    value.replace(QRegularExpression(QStringLiteral(R"(-{2,})")), QStringLiteral("-"));
    value.replace(QRegularExpression(QStringLiteral(R"(\+{2,})")), QStringLiteral("+"));
    value.replace(QRegularExpression(QStringLiteral(R"(\_{2,})")), QStringLiteral("_"));
    removeEmptyBrackets(value);
    value = value.trimmed();
    value = trimSeparators(value);
    return value;
}

QString ScanCleaner::finalSeparators(const QString &text)
{
    QString value = text;
    value.replace(QLatin1Char('+'), QLatin1Char(' '));
    value.replace(QLatin1Char('-'), QLatin1Char(' '));
    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    value.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    value = value.trimmed();
    return trimSeparators(value);
}

QString ScanCleaner::loaderCleanup(const QString &text, QString *note, bool *fallbackUsed)
{
    if (note)
        note->clear();
    if (fallbackUsed)
        *fallbackUsed = false;

    static const QRegularExpression regex(loaderPattern(),
                                          QRegularExpression::CaseInsensitiveOption);
    QVector<QRegularExpressionMatch> matches;
    auto it = regex.globalMatch(text);
    while (it.hasNext())
        matches.append(it.next());

    QString value = text;
    QStringList removed;
    for (int i = matches.size() - 1; i >= 0; --i) {
        const QRegularExpressionMatch &match = matches.at(i);
        const int start = match.capturedStart();
        if (start <= 0)
            continue;

        const QChar previous = value.at(start - 1);
        if (previous != QLatin1Char('-') && previous != QLatin1Char('+')
            && previous != QLatin1Char('_') && !previous.isSpace()) {
            continue;
        }

        removed.prepend(match.captured(0).toLower());
        value.remove(start, match.capturedLength());
    }

    if (finalSeparators(value).isEmpty()) {
        value = text;
        if (fallbackUsed)
            *fallbackUsed = true;
    }

    if (note) {
        if (!removed.isEmpty()) {
            *note = QStringLiteral("已移除加载器：%1").arg(removed.join(QStringLiteral(", ")));
        }
        if (fallbackUsed && *fallbackUsed) {
            if (!note->isEmpty())
                note->append(QStringLiteral("；"));
            note->append(QStringLiteral("加载器清理后为空，已回退删除前结果"));
        }
    }
    return value;
}

CleanedModFile ScanCleaner::cleanFileName(const QString &fileName)
{
    CleanedModFile result;
    result.originalFileName = fileName;

    QString stem = fileName;
    if (stem.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive))
        stem.chop(4);

    stem = removeBracketSegments(stem);

    QString intermediate = removeModifierTokens(stem);
    intermediate = removeVersionSegments(intermediate);
    intermediate = removeTrailingMetadata(intermediate);

    QString base = baseSeparators(intermediate);

    QString loaderNote;
    bool fallback = false;
    QString loaderCleaned = loaderCleanup(intermediate, &loaderNote, &fallback);
    QString search = finalSeparators(loaderCleaned);

    result.cleanerNote = loaderNote;
    result.baseCleanName = base;
    result.searchName = search;

    if (result.baseCleanName.isEmpty()) {
        result.baseCleanName = fileName;
        result.cleanAnomaly = true;
        if (!result.cleanerNote.isEmpty())
            result.cleanerNote.append(QStringLiteral("；"));
        result.cleanerNote.append(QStringLiteral("基础清洗结果为空，已保留原始文件名"));
    }

    if (result.searchName.isEmpty()) {
        result.searchName = fileName;
        result.cleanAnomaly = true;
        if (!result.cleanerNote.isEmpty())
            result.cleanerNote.append(QStringLiteral("；"));
        result.cleanerNote.append(QStringLiteral("清洗结果为空，已回退原始文件名并标记异常"));
    }

    return result;
}

ScanOutcome ScanCleaner::scanDirectory(const QString &directory, bool recursive)
{
    ScanOutcome outcome;
    QDir dir(directory);
    if (!dir.exists()) {
        outcome.warnings.append(QStringLiteral("目录不存在或不可访问。"));
        return outcome;
    }

    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
    if (recursive)
        flags |= QDirIterator::Subdirectories;

    QDirIterator it(dir.absolutePath(), QDir::Files | QDir::Readable, flags);
    QStringList paths;
    while (it.hasNext()) {
        const QString path = it.next();
        if (it.fileInfo().suffix().compare(QStringLiteral("jar"), Qt::CaseInsensitive) == 0)
            paths.append(path);
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(paths.begin(), paths.end(), [&collator](const QString &a, const QString &b) {
        return collator.compare(a, b) < 0;
    });

    for (const QString &path : paths) {
        const QFileInfo info(path);
        CleanedModFile cleaned = cleanFileName(info.fileName());
        cleaned.filePath = info.absoluteFilePath();
        outcome.files.append(cleaned);
    }

    return outcome;
}
