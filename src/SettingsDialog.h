#pragma once

#include "AppConfig.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const AppConfig &config, QWidget *parent = nullptr);

    AppConfig config() const { return m_result; }

protected:
    void accept() override;

private:
    void buildUi();
    void applyToUi(const AppConfig &config);
    AppConfig collect() const;
    void restoreDefaults();
    void updateUserAgentState();
    void updateProxyState();
    void updateRememberWarning();

    AppConfig m_original;
    AppConfig m_result;

    QLineEdit *m_userAgent = nullptr;
    QPushButton *m_restoreUserAgent = nullptr;
    QCheckBox *m_browserUserAgent = nullptr;
    QLabel *m_userAgentHint = nullptr;
    QComboBox *m_proxyMode = nullptr;
    QLineEdit *m_proxyHost = nullptr;
    QSpinBox *m_proxyPort = nullptr;
    QSpinBox *m_mrAttempts = nullptr;
    QSpinBox *m_mrTimeout = nullptr;
    QSpinBox *m_mrConcurrency = nullptr;
    QSpinBox *m_mrInterval = nullptr;
    QSpinBox *m_cfAttempts = nullptr;
    QSpinBox *m_cfTimeout = nullptr;
    QSpinBox *m_cfConcurrency = nullptr;
    QSpinBox *m_cfInterval = nullptr;
    QCheckBox *m_recursive = nullptr;
    QCheckBox *m_mcmod = nullptr;
    QComboBox *m_exportFormat = nullptr;
    QCheckBox *m_rememberKey = nullptr;
    QLabel *m_rememberWarning = nullptr;
};
