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
        if (arg == QStringLiteral("--hash-password") ||
            arg == QStringLiteral("--provision-account") ||
            arg == QStringLiteral("--cli-help")) {
            isCli = true;
            break;
        }
    }

    if (!isCli) {
        return -1; // Proceed with GUI launch
    }

    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SmartATC Developer Provisioning Tool"));
    parser.addHelpOption();

    QCommandLineOption hashOpt(QStringLiteral("hash-password"),
                               QStringLiteral("Generate a SHA-256 password hash using PasswordUtils."),
                               QStringLiteral("password"));
    parser.addOption(hashOpt);

    QCommandLineOption provOpt(QStringLiteral("provision-account"),
                               QStringLiteral("Provision or update a demo user account (requires db.conf). Format: username:password:role:fullname"),
                               QStringLiteral("credentials"));
    parser.addOption(provOpt);

    parser.process(app);

    if (parser.isSet(hashOpt)) {
        QString plain = parser.value(hashOpt);
        if (plain.isEmpty()) {
            std::cerr << "Error: Password cannot be empty." << std::endl;
            return 1;
        }
        QString hashed = PasswordUtils::hash(plain);
        std::cout << hashed.toStdString() << std::endl;
        return 0;
    }

    if (parser.isSet(provOpt)) {
        QString val = parser.value(provOpt);
        QStringList parts = val.split(QLatin1Char(':'));
        if (parts.size() < 4) {
            std::cerr << "Error: Format must be username:password:role:fullname" << std::endl;
            return 1;
        }
        QString username = parts[0].trimmed();
        QString password = parts[1];
        QString roleName = parts[2].trimmed();
        QString fullName = parts.mid(3).join(QLatin1Char(':')).trimmed();

        if (username.isEmpty() || password.isEmpty() || roleName.isEmpty()) {
            std::cerr << "Error: Username, password, and role cannot be empty." << std::endl;
            return 1;
        }

        // Initialize DB
        if (!DatabaseManager::instance().loadConfig()) {
            std::cerr << "Error: Failed to load db.conf." << std::endl;
            return 1;
        }
        if (!DatabaseManager::instance().connect()) {
            std::cerr << "Error: Database connection failed: "
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
            std::cerr << "Error executing query: " << q.lastError().text().toStdString() << std::endl;
            return 1;
        }

        std::cout << "SUCCESS: Account '" << username.toStdString() << "' provisioned with role '"
                  << roleName.toStdString() << "'." << std::endl;
        DatabaseManager::instance().disconnect();
        return 0;
    }

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
