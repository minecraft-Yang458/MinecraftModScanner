#pragma once

#include "SearchTypes.h"

#include <QByteArray>
#include <QDialog>
#include <QPair>
#include <QVector>

class DetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DetailsDialog(const QString &title, const QString &subtitle,
                           const QVector<QPair<QString, QString>> &fields,
                           const QByteArray &rawResponse, QWidget *parent = nullptr);

    static void showPlatformDetails(QWidget *parent, const ModEntry &entry,
                                    Platform platform);

private:
    QString buildPlainText() const;
    void copyAll();

    QString m_title;
    QString m_subtitle;
    QVector<QPair<QString, QString>> m_fields;
    QByteArray m_raw;
};
