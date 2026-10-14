#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QSqlQuery>
#include <QSqlError>
#include <iostream>

#include "services/ApplicationService.h"
#include "database/DatabaseManager.h"
#include "utils/PasswordUtils.h"
#include "ui/MainWindow.h"

static int handleDeveloperCli(int argc, char *argv[])
{
    // Check if CLI flags were passed before creating GUI QApplication
    bool isCli = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--provision-user") ||
            arg == QStringLiteral("--cli-help") ||
            arg == QStringLiteral("-h") ||
            arg == QStringLiteral("--help")) {
            isCli = true;
            break;
        }
    }

    if (!isCli) {
        return -1; // Proceed with GUI launch
    }

    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SmartATC Development Account Provisioning Tool\n"
                                                    "Explicitly restricted for local development and authorized administrative setup."));
    parser.addHelpOption();

    QCommandLineOption userOpt(QStringList() << QStringLiteral("u") << QStringLiteral("provision-user"),
                               QStringLiteral("Username of the development account to provision or update."),
                               QStringLiteral("username"));
    parser.addOption(userOpt);

    QCommandLineOption roleOpt(QStringList() << QStringLiteral("r") << QStringLiteral("role"),
                               QStringLiteral("Role for the account: 'Admin', 'ATC', or 'Airport Operator'."),
                               QStringLiteral("role"));
    parser.addOption(roleOpt);

    QCommandLineOption nameOpt(QStringList() << QStringLiteral("n") << QStringLiteral("fullname"),
                               QStringLiteral("Full display name of the account holder."),
                               QStringLiteral("name"));
    parser.addOption(nameOpt);

    parser.process(app);

    if (!parser.isSet(userOpt)) {
        parser.showHelp(1);
    }

    // Security check: Restrict provisioning to development environment
    QString devEnv = qEnvironmentVariable("SMARTATC_ENV");
    if (!devEnv.isEmpty() && devEnv.compare(QStringLiteral("production"), Qt::CaseInsensitive) == 0) {
        std::cerr << "FATAL ERROR: Account provisioning is strictly disabled in production environments." << std::endl;
        return 1;
    }

    QString username = parser.value(userOpt).trimmed();
    QString roleName = parser.value(roleOpt).trimmed();
    QString fullName = parser.value(nameOpt).trimmed();

    if (roleName.isEmpty()) {
        std::cerr << "Error: Role is required (--role <Admin|ATC|Airport Operator>)." << std::endl;
        return 1;
    }

    if (fullName.isEmpty()) {
        fullName = username;
    }

    std::cout << "=== SmartATC Development Account Provisioning ===" << std::endl;
    std::cout << "Username : " << username.toStdString() << std::endl;
    std::cout << "Role     : " << roleName.toStdString() << std::endl;
    std::cout << "Full Name: " << fullName.toStdString() << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    // Secure, non-echoing console input for password
    QString password = PasswordUtils::readPasswordSecurely(QStringLiteral("Enter Password: "));
    if (password.isEmpty()) {
        std::cerr << "Error: Password cannot be empty." << std::endl;
        return 1;
    }

    QString confirmPass = PasswordUtils::readPasswordSecurely(QStringLiteral("Confirm Password: "));
    if (password != confirmPass) {
        std::cerr << "Error: Passwords do not match. Provisioning aborted." << std::endl;
        return 1;
    }

    QString validationError;
    if (!PasswordUtils::validateAccountInput(username, roleName, fullName, password, &validationError)) {
        std::cerr << "Validation Error: " << validationError.toStdString() << std::endl;
        return 1;
    }

    // Initialize Database
    if (!DatabaseManager::instance().loadConfig()) {
        std::cerr << "Database Error: Failed to load db.conf. Local configuration is required." << std::endl;
        return 1;
    }

    if (!DatabaseManager::instance().connect()) {
        std::cerr << "Database Error: Connection failed: "
                  << DatabaseManager::instance().statusDescription().toStdString() << std::endl;
        return 1;
    }

    QSqlDatabase db = QSqlDatabase::database(DatabaseManager::CONNECTION_NAME);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password_hash, role_id, full_name, status) "
        "VALUES (:u, :p, (SELECT role_id FROM roles WHERE role_name = :r LIMIT 1), :f, 'Active') "
        "ON DUPLICATE KEY UPDATE "
        "password_hash = VALUES(password_hash), "
        "full_name = VALUES(full_name), "
        "role_id = (SELECT role_id FROM roles WHERE role_name = :r2 LIMIT 1), "
        "status = 'Active';"
    ));

    QString hashed = PasswordUtils::hash(password);
    q.bindValue(QStringLiteral(":u"), username);
    q.bindValue(QStringLiteral(":p"), hashed);
    q.bindValue(QStringLiteral(":r"), roleName);
    q.bindValue(QStringLiteral(":f"), fullName);
    q.bindValue(QStringLiteral(":r2"), roleName);

    if (!q.exec()) {
        std::cerr << "Database Execution Error: " << q.lastError().text().toStdString() << std::endl;
        DatabaseManager::instance().disconnect();
        return 1;
    }

    std::cout << "SUCCESS: Development account '" << username.toStdString() << "' provisioned successfully with role '"
              << roleName.toStdString() << "'." << std::endl;

    DatabaseManager::instance().disconnect();
    return 0;
}

int main(int argc, char *argv[])
{
    int cliResult = handleDeveloperCli(argc, argv);
    if (cliResult >= 0) {
        return cliResult;
    }

    QApplication app(argc, argv);
    app.setApplicationName("Smart Air Traffic Control Management System");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("ATC Project Team");

    // Initialize application service layer
    ApplicationService service;
    service.initialize();

    // Create and show the main window
    MainWindow window(&service);
    window.show();

    int result = app.exec();

    // Clean shutdown
    service.shutdown();

    return result;
}
