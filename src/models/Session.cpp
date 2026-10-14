#include "Session.h"

Session::Session(int userId, const QString &username, const QString &fullName, const QString &role)
    : m_userId(userId)
    , m_username(username)
    , m_fullName(fullName)
    , m_role(role)
{
}

void Session::clear()
{
    m_userId = 0;
    m_username.clear();
    m_fullName.clear();
    m_role.clear();
}

bool Session::hasPermission(const QString &permission) const
{
    if (!isValid()) {
        return false;
    }

    // Centralized RBAC permission policy
    // Admin: Dashboard, Flights (view), Aircraft (view), Runways (view), Scheduling (view), Transactions, Logs, Users
    // ATC: Dashboard, Flights (manage), Runways (monitor/allocate), Scheduling (queue/execute), Logs (view)
    // Airport Operator: Dashboard, Aircraft (manage), Runways (maintenance), Flights (view), Logs (view)

    if (permission == QStringLiteral("page:Dashboard")) {
        return true;
    }

    if (permission == QStringLiteral("page:Users") || permission == QStringLiteral("action:ManageUsers")) {
        return isAdmin();
    }

    if (permission == QStringLiteral("page:Scheduling") || permission == QStringLiteral("action:ManageScheduling")) {
        return isAdmin() || isATC();
    }

    if (permission == QStringLiteral("page:Flights")) {
        return isAdmin() || isATC() || isOperator();
    }

    if (permission == QStringLiteral("action:ManageFlights")) {
        return isATC();
    }

    if (permission == QStringLiteral("page:Aircraft")) {
        return isAdmin() || isOperator() || isATC();
    }

    if (permission == QStringLiteral("action:ManageAircraft")) {
        return isAdmin() || isOperator();
    }

    if (permission == QStringLiteral("action:AdministerAircraft")) {
        return isAdmin();
    }

    if (permission == QStringLiteral("page:Runways")) {
        return isAdmin() || isATC() || isOperator();
    }

    if (permission == QStringLiteral("action:RunwayAllocation")) {
        return isATC();
    }

    if (permission == QStringLiteral("action:RunwayMaintenance")) {
        return isOperator();
    }

    if (permission == QStringLiteral("page:Transactions")) {
        return isAdmin();
    }

    if (permission == QStringLiteral("page:Logs")) {
        return isAdmin() || isATC() || isOperator();
    }

    return false;
}

QStringList Session::allowedNavItems() const
{
    QStringList items;
    if (!isValid()) {
        return items;
    }

    // Canonical nav items: Dashboard, Flights, Aircraft, Runways, Scheduling, Transactions, Logs, Users
    if (hasPermission(QStringLiteral("page:Dashboard")))    items << QStringLiteral("Dashboard");
    if (hasPermission(QStringLiteral("page:Flights")))      items << QStringLiteral("Flights");
    if (hasPermission(QStringLiteral("page:Aircraft")))     items << QStringLiteral("Aircraft");
    if (hasPermission(QStringLiteral("page:Runways")))      items << QStringLiteral("Runways");
    if (hasPermission(QStringLiteral("page:Scheduling")))   items << QStringLiteral("Scheduling");
    if (hasPermission(QStringLiteral("page:Transactions"))) items << QStringLiteral("Transactions");
    if (hasPermission(QStringLiteral("page:Logs")))         items << QStringLiteral("Logs");
    if (hasPermission(QStringLiteral("page:Users")))        items << QStringLiteral("Users");

    return items;
}
