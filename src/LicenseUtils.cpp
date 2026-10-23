#include "LicenseUtils.h"

#include "SearchTypes.h"

#include <QRegularExpression>
#include <QSet>

namespace {
QStringList splitLicenseParts(const QString &license)
{
    static const QRegularExpression separator(
        QStringLiteral(R"(\s*(?:;|\||,|/|\+|\band\b|\bor\b)\s*)"),
        QRegularExpression::CaseInsensitiveOption);
    return license.split(separator, Qt::SkipEmptyParts);
}

QString classifyPart(const QString &part)
{
    const QString s = LicenseUtils::normalized(part);
    if (s.isEmpty())
        return {};

    if (s.contains(QStringLiteral("lgpl"))
        || s.contains(QStringLiteral("lesser general public"))) {
        return QStringLiteral("LGPL");
    }
    if (s.contains(QStringLiteral("apache"))) {
        return QStringLiteral("Apache-2.0");
    }
    if (s.contains(QStringLiteral("all rights reserved"))
        || s.contains(QStringLiteral("proprietary"))
        || s.contains(QStringLiteral("closed source"))) {
        return QStringLiteral("All Rights Reserved");
    }
    if (QRegularExpression(QStringLiteral(R"(\bmit\b)"),
                           QRegularExpression::CaseInsensitiveOption)
            .match(s)
            .hasMatch()) {
        return QStringLiteral("MIT");
    }
    if (s.contains(QStringLiteral("gpl"))
        && (s.contains(QStringLiteral("3")) || s.contains(QStringLiteral("v3"))
            || s.contains(QStringLiteral("version 3")))) {
        return QStringLiteral("GPL-3.0");
    }
    if (s.contains(QStringLiteral("general public license"))
        && (s.contains(QStringLiteral("3")) || s.contains(QStringLiteral("v3"))
            || s.contains(QStringLiteral("version 3")))) {
        return QStringLiteral("GPL-3.0");
    }
    return {};
}
} // namespace

namespace LicenseUtils {

QString normalized(const QString &license)
{
    QString s = license.toLower().trimmed();
    s.replace(QChar(0x00A0), QLatin1Char(' '));
    s.replace(QRegularExpression(QStringLiteral(R"([\s_]+)")), QStringLiteral(" "));
    return s.trimmed();
}

QStringList categoriesForLicense(const QString &license)
{
    QSet<QString> result;
    for (const QString &part : splitLicenseParts(license)) {
        const QString category = classifyPart(part);
        if (!category.isEmpty())
            result.insert(category);
    }

    // 没有分隔符时也把整个字符串当成一段再试一次（例如 "MIT License"）。
    if (result.isEmpty()) {
        const QString category = classifyPart(license);
        if (!category.isEmpty())
            result.insert(category);
    }

    QStringList ordered;
    for (const QString &category : licenseGroupOrder()) {
        if (result.contains(category))
            ordered.append(category);
    }
    return ordered;
}

QStringList categoriesForRow(const ModEntry &entry)
{
    int successCount = 0;
    QSet<QString> categories;

    for (int i = 0; i < kPlatformCount; ++i) {
        const PlatformResult &r = entry.results[i];
        if (!r.isSuccess())
            continue;
        ++successCount;
        const QStringList found = categoriesForLicense(r.license);
        for (const QString &category : found)
            categories.insert(category);
    }

    if (successCount == 0)
        return {QStringLiteral("未检索/未找到")};

    QStringList ordered;
    for (const QString &category : licenseGroupOrder()) {
        if (categories.contains(category))
            ordered.append(category);
    }

    if (ordered.isEmpty())
        return {QStringLiteral("未知许可证")};

    if (ordered.size() > 1)
        ordered.append(QStringLiteral("多许可证"));
    return ordered;
}

QStringList licenseGroupOrder()
{
    return {
        QStringLiteral("MIT"),
        QStringLiteral("Apache-2.0"),
        QStringLiteral("GPL-3.0"),
        QStringLiteral("LGPL"),
        QStringLiteral("All Rights Reserved"),
        QStringLiteral("多许可证"),
        QStringLiteral("未知许可证"),
        QStringLiteral("未检索/未找到"),
    };
}

} // namespace LicenseUtils
