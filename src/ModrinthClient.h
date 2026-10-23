#pragma once

#include "BasePlatformClient.h"

class ModrinthClient : public BasePlatformClient
{
    Q_OBJECT
public:
    explicit ModrinthClient(QObject *parent = nullptr);

    void checkConnection() override;

protected:
    QUrl buildSearchUrl(const QString &query) const override;
    QHash<QByteArray, QByteArray> requestHeaders() const override;
    bool validateCredentials(QString *error) const override;
    bool parseSearchResponse(const QByteArray &body, const QUrl &url, const QString &query,
                             PlatformResult *result, QString *error) const override;
    QString httpErrorText(int statusCode, const QByteArray &body) const override;
    QString displayName() const override;
};
