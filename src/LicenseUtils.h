#pragma once

#include <QString>
#include <QStringList>

struct ModEntry;

namespace LicenseUtils {

QString normalized(const QString &license);
QStringList categoriesForLicense(const QString &license);
QStringList categoriesForRow(const ModEntry &entry);

QStringList licenseGroupOrder();

} // namespace LicenseUtils
