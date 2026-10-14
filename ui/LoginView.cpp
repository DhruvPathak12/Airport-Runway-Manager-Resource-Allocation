#include "LoginView.h"

#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>
#include <QApplication>

LoginView::LoginView(ApplicationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    setupUi();
}

void LoginView::setupUi()
{
    // Background and central container
    setStyleSheet("background-color: #1a1a2e;");

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setAlignment(Qt::AlignCenter);

    // Card frame
    QFrame *card = new QFrame;
    card->setFixedWidth(380);
    card->setStyleSheet(R"(
        QFrame {
            background-color: #ffffff;
            border-radius: 8px;
            padding: 30px;
        }
    )");

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(14);

    // Title & Icon
    QLabel *iconLabel = new QLabel(QStringLiteral("✈"));
    QFont iconFont = iconLabel->font();
    iconFont.setPointSize(32);
    iconLabel->setFont(iconFont);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("color: #1a1a2e;");

    QLabel *titleLabel = new QLabel(QStringLiteral("SmartATC System"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #1a1a2e;");

    QLabel *subTitleLabel = new QLabel(QStringLiteral("Air Traffic Control & Resource Allocation"));
    QFont subFont = subTitleLabel->font();
    subFont.setPointSize(10);
    subTitleLabel->setFont(subFont);
    subTitleLabel->setAlignment(Qt::AlignCenter);
    subTitleLabel->setStyleSheet("color: #666; margin-bottom: 8px;");

    // Inputs
    QLabel *userLabel = new QLabel(QStringLiteral("Username"));
    userLabel->setStyleSheet("color: #333; font-weight: bold; font-size: 11px;");

    m_usernameInput = new QLineEdit;
    m_usernameInput->setPlaceholderText(QStringLiteral("Enter your username"));
    m_usernameInput->setStyleSheet(R"(
        QLineEdit {
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 8px 10px;
            font-size: 12px;
            background-color: #fcfcfc;
        }
        QLineEdit:focus {
            border: 1px solid #3949ab;
            background-color: #ffffff;
        }
    )");

    QLabel *passLabel = new QLabel(QStringLiteral("Password"));
    passLabel->setStyleSheet("color: #333; font-weight: bold; font-size: 11px;");

    m_passwordInput = new QLineEdit;
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setPlaceholderText(QStringLiteral("Enter your password"));
    m_passwordInput->setStyleSheet(R"(
        QLineEdit {
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 8px 10px;
            font-size: 12px;
            background-color: #fcfcfc;
        }
        QLineEdit:focus {
            border: 1px solid #3949ab;
            background-color: #ffffff;
        }
    )");

    // Feedback message label
    m_feedbackLabel = new QLabel;
    m_feedbackLabel->setWordWrap(true);
    m_feedbackLabel->setAlignment(Qt::AlignCenter);
    m_feedbackLabel->setStyleSheet("font-size: 11px; color: #d32f2f; font-weight: bold;");
    m_feedbackLabel->hide();

    // Login button
    m_loginButton = new QPushButton(QStringLiteral("Log In"));
    m_loginButton->setCursor(Qt::PointingHandCursor);
    m_loginButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3949ab;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 10px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #283593;
        }
        QPushButton:pressed {
            background-color: #1a237e;
        }
        QPushButton:disabled {
            background-color: #9fa8da;
        }
    )");

    // Database connection status footnote
    m_dbStatusLabel = new QLabel;
    m_dbStatusLabel->setAlignment(Qt::AlignCenter);
    m_dbStatusLabel->setStyleSheet("font-size: 10px; color: #888;");
    DatabaseManager &db = DatabaseManager::instance();
    m_dbStatusLabel->setText(db.statusDescription());

    // Layout assembly
    cardLayout->addWidget(iconLabel);
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(subTitleLabel);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(userLabel);
    cardLayout->addWidget(m_usernameInput);
    cardLayout->addWidget(passLabel);
    cardLayout->addWidget(m_passwordInput);
    cardLayout->addWidget(m_feedbackLabel);
    cardLayout->addSpacing(4);
    cardLayout->addWidget(m_loginButton);
    cardLayout->addSpacing(4);
    cardLayout->addWidget(m_dbStatusLabel);

    outerLayout->addWidget(card);

    // Keyboard navigation: Enter triggers login
    connect(m_usernameInput, &QLineEdit::returnPressed, this, &LoginView::onLoginClicked);
    connect(m_passwordInput, &QLineEdit::returnPressed, this, &LoginView::onLoginClicked);
    connect(m_loginButton, &QPushButton::clicked, this, &LoginView::onLoginClicked);
}

void LoginView::resetForm()
{
    m_usernameInput->clear();
    m_passwordInput->clear();
    m_feedbackLabel->hide();
    m_loginButton->setEnabled(true);
    m_loginButton->setText(QStringLiteral("Log In"));
    m_dbStatusLabel->setText(DatabaseManager::instance().statusDescription());
    m_usernameInput->setFocus();
}

void LoginView::setErrorMessage(const QString &message)
{
    setStatusMessage(message, true);
}

void LoginView::setStatusMessage(const QString &message, bool isError)
{
    if (message.isEmpty()) {
        m_feedbackLabel->hide();
        return;
    }

    m_feedbackLabel->setText(message);
    if (isError) {
        m_feedbackLabel->setStyleSheet("font-size: 11px; color: #d32f2f; font-weight: bold;");
    } else {
        m_feedbackLabel->setStyleSheet("font-size: 11px; color: #2e7d32; font-weight: bold;");
    }
    m_feedbackLabel->show();
}

void LoginView::onLoginClicked()
{
    m_feedbackLabel->hide();
    QString username = m_usernameInput->text().trimmed();
    QString password = m_passwordInput->text();

    if (username.isEmpty() || password.isEmpty()) {
        setErrorMessage(QStringLiteral("Please enter both username and password."));
        return;
    }

    m_loginButton->setEnabled(false);
    m_loginButton->setText(QStringLiteral("Authenticating..."));
    qApp->processEvents();

    QString errorMsg;
    bool success = m_service ? m_service->login(username, password, &errorMsg) : false;

    m_loginButton->setEnabled(true);
    m_loginButton->setText(QStringLiteral("Log In"));

    if (success) {
        m_passwordInput->clear();
        emit loginSuccessful();
    } else {
        setErrorMessage(errorMsg.isEmpty() ? QStringLiteral("Invalid username or password.") : errorMsg);
        m_passwordInput->selectAll();
        m_passwordInput->setFocus();
    }
}
