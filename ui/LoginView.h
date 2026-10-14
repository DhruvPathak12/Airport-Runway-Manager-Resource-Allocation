#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

class ApplicationService;

/**
 * @brief Dedicated login dialog / widget for user authentication.
 *
 * Requirements:
 * - Application title: "Smart Air Traffic Control Management System"
 * - Username field.
 * - Password field with masked characters.
 * - Login button (Enter key submits).
 * - Feedback label for sanitized error messages.
 * - Loading / busy state during authentication attempt.
 * - Clean visual styling consistent with application theme.
 */
class LoginView : public QWidget
{
    Q_OBJECT

public:
    explicit LoginView(ApplicationService *service, QWidget *parent = nullptr);
    ~LoginView() override = default;

    void resetForm();
    void setErrorMessage(const QString &message);
    void setStatusMessage(const QString &message, bool isError = false);

signals:
    void loginSuccessful();

private slots:
    void onLoginClicked();

private:
    void setupUi();

    ApplicationService *m_service       = nullptr;
    QLineEdit          *m_usernameInput = nullptr;
    QLineEdit          *m_passwordInput = nullptr;
    QPushButton        *m_loginButton   = nullptr;
    QLabel             *m_feedbackLabel = nullptr;
    QLabel             *m_dbStatusLabel = nullptr;
};
