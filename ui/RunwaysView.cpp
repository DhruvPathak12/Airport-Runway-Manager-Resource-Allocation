#include "RunwaysView.h"

#include "services/ApplicationService.h"
#include "ui/RunwayDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

RunwaysView::RunwaysView(ApplicationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    setupUi();

    if (m_service) {
        connect(m_service, &ApplicationService::runwaysChanged, this, &RunwaysView::refreshRunways);
    }
}

void RunwaysView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // ── 1. Header Row ────────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleTextLayout = new QVBoxLayout;

    QLabel *titleLabel = new QLabel(QStringLiteral("Runway Resource Management"), this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #0f172a;");

    QLabel *subLabel = new QLabel(QStringLiteral("Monitor shared runway resources, operational availability, and maintenance controls in MySQL"), this);
    subLabel->setStyleSheet("font-size: 13px; color: #64748b;");

    titleTextLayout->addWidget(titleLabel);
    titleTextLayout->addWidget(subLabel);
    headerLayout->addLayout(titleTextLayout);
    headerLayout->addStretch();

    m_countBadge = new QLabel(QStringLiteral("0 Runways"), this);
    m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; "
                               "padding: 6px 14px; border-radius: 12px; border: 1px solid #cbd5e1;");
    headerLayout->addWidget(m_countBadge);

    m_permNotice = new QLabel(this);
    m_permNotice->setStyleSheet("background-color: #fef3c7; color: #92400e; font-size: 11px; font-weight: 600; "
                               "padding: 6px 12px; border-radius: 12px; border: 1px solid #fde68a;");
    m_permNotice->setText(QStringLiteral("👁 Read-Only"));
    m_permNotice->hide();
    headerLayout->addWidget(m_permNotice);

    mainLayout->addLayout(headerLayout);

    // ── 2. Toolbar Row ───────────────────────────────────────────────────────
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(10);

    // Search input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("🔍 Search by Runway Designation or Type..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(320);
    m_searchEdit->setStyleSheet(R"(
        QLineEdit {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
        }
        QLineEdit:focus {
            border: 1px solid #3b82f6;
            background-color: #f8fafc;
        }
    )");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &RunwaysView::onSearchChanged);
    toolbarLayout->addWidget(m_searchEdit);

    // Status filter
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(QStringLiteral("All Statuses"));
    m_statusFilter->addItems({
        QStringLiteral("Available"),
        QStringLiteral("Occupied"),
        QStringLiteral("Maintenance")
    });
    m_statusFilter->setStyleSheet(R"(
        QComboBox {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 13px;
            color: #0f172a;
            min-width: 140px;
        }
    )");
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RunwaysView::onStatusFilterChanged);
    toolbarLayout->addWidget(m_statusFilter);

    // Refresh button
    m_refreshButton = new QPushButton(QStringLiteral("🔄 Refresh"), this);
    m_refreshButton->setStyleSheet(R"(
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 14px;
            font-size: 13px;
            font-weight: 500;
            color: #334155;
        }
        QPushButton:hover {
            background-color: #f1f5f9;
            border-color: #94a3b8;
        }
    )");
    connect(m_refreshButton, &QPushButton::clicked, this, &RunwaysView::refreshRunways);
    toolbarLayout->addWidget(m_refreshButton);

    toolbarLayout->addStretch();

    // Toggle Maintenance Button (Airport Operator & Admin)
    m_toggleMaintenanceBtn = new QPushButton(QStringLiteral("🛠 Set Maintenance"), this);
    m_toggleMaintenanceBtn->setEnabled(false);
    m_toggleMaintenanceBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #d97706;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #b45309;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_toggleMaintenanceBtn, &QPushButton::clicked, this, &RunwaysView::onToggleMaintenanceClicked);
    toolbarLayout->addWidget(m_toggleMaintenanceBtn);

    // Add button (Admin only)
    m_addButton = new QPushButton(QStringLiteral("➕ Add Runway"), this);
    m_addButton->setStyleSheet(R"(
        QPushButton {
            background-color: #16a34a;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #15803d;
        }
        QPushButton:disabled {
            background-color: #94a3b8;
        }
    )");
    connect(m_addButton, &QPushButton::clicked, this, &RunwaysView::onAddRunwayClicked);
    toolbarLayout->addWidget(m_addButton);

    // Edit button (Admin or Operator)
    m_editButton = new QPushButton(QStringLiteral("✏ Edit"), this);
    m_editButton->setEnabled(false);
    m_editButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #1d4ed8;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_editButton, &QPushButton::clicked, this, &RunwaysView::onEditRunwayClicked);
    toolbarLayout->addWidget(m_editButton);

    mainLayout->addLayout(toolbarLayout);

    // ── 3. Content Area (Stacked: 0=Table, 1=Empty/Error State) ──────────────
    m_contentStack = new QStackedWidget(this);

    // Table view
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(5);
    m_tableWidget->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("Runway Designation"),
        QStringLiteral("Permitted Operations"),
        QStringLiteral("Operational Status"),
        QStringLiteral("Current Allocation / Occupancy")
    });

    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    m_tableWidget->setStyleSheet(R"(
        QTableWidget {
            background-color: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            gridline-color: #f1f5f9;
            font-size: 13px;
        }
        QTableWidget::item {
            padding: 8px 12px;
            color: #1e293b;
            border-bottom: 1px solid #f1f5f9;
        }
        QTableWidget::item:selected {
            background-color: #e0e7ff;
            color: #1e1b4b;
        }
        QHeaderView::section {
            background-color: #f8fafc;
            color: #475569;
            font-weight: 600;
            font-size: 12px;
            padding: 10px 12px;
            border: none;
            border-bottom: 2px solid #cbd5e1;
        }
    )");

    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, &RunwaysView::onSelectionChanged);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &RunwaysView::onTableDoubleClicked);

    m_contentStack->addWidget(m_tableWidget);

    // Empty / Error State Widget
    m_stateWidget = new QWidget(this);
    QVBoxLayout *stateLayout = new QVBoxLayout(m_stateWidget);
    stateLayout->setAlignment(Qt::AlignCenter);

    m_stateIcon = new QLabel(QStringLiteral("🛫"), this);
    m_stateIcon->setStyleSheet("font-size: 48px; color: #94a3b8;");
    m_stateIcon->setAlignment(Qt::AlignCenter);

    m_stateTitle = new QLabel(QStringLiteral("No Runways Found"), this);
    m_stateTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #334155; margin-top: 8px;");
    m_stateTitle->setAlignment(Qt::AlignCenter);

    m_stateMessage = new QLabel(QStringLiteral("No runways match your search criteria or the database is unavailable."), this);
    m_stateMessage->setStyleSheet("font-size: 13px; color: #64748b; margin-top: 4px;");
    m_stateMessage->setAlignment(Qt::AlignCenter);

    m_stateButton = new QPushButton(QStringLiteral("Refresh Runways"), this);
    m_stateButton->setStyleSheet("background-color: #2563eb; color: #ffffff; border-radius: 6px; padding: 8px 16px; margin-top: 14px; font-weight: 600;");
    connect(m_stateButton, &QPushButton::clicked, this, &RunwaysView::refreshRunways);

    stateLayout->addWidget(m_stateIcon);
    stateLayout->addWidget(m_stateTitle);
    stateLayout->addWidget(m_stateMessage);
    stateLayout->addWidget(m_stateButton);

    m_contentStack->addWidget(m_stateWidget);
    mainLayout->addWidget(m_contentStack);
}

void RunwaysView::updatePermissions()
{
    if (!m_service) {
        return;
    }

    const Session &session = m_service->currentSession();
    bool canMaintenance = session.hasPermission(QStringLiteral("action:RunwayMaintenance"));
    bool isAdmin = session.isAdmin();

    m_addButton->setVisible(isAdmin);
    m_editButton->setVisible(isAdmin || canMaintenance);
    m_toggleMaintenanceBtn->setVisible(canMaintenance || isAdmin);

    if (isAdmin) {
        m_permNotice->setText(QStringLiteral("👑 Admin Mode"));
        m_permNotice->setStyleSheet("background-color: #dbeafe; color: #1e40af; font-size: 11px; font-weight: 600; padding: 6px 12px; border-radius: 12px; border: 1px solid #bfdbfe;");
        m_permNotice->show();
    } else if (canMaintenance) {
        m_permNotice->setText(QStringLiteral("🛠 Operator Mode (Maintenance Authorized)"));
        m_permNotice->setStyleSheet("background-color: #fef3c7; color: #92400e; font-size: 11px; font-weight: 600; padding: 6px 12px; border-radius: 12px; border: 1px solid #fde68a;");
        m_permNotice->show();
    } else {
        m_permNotice->setText(QStringLiteral("👁 ATC View / Monitor Mode"));
        m_permNotice->setStyleSheet("background-color: #f1f5f9; color: #475569; font-size: 11px; font-weight: 600; padding: 6px 12px; border-radius: 12px; border: 1px solid #e2e8f0;");
        m_permNotice->show();
    }

    onSelectionChanged();
}

void RunwaysView::refreshRunways()
{
    if (!m_service || !m_service->isAuthenticated()) {
        m_currentRunways.clear();
        m_tableWidget->setRowCount(0);
        m_stateTitle->setText(QStringLiteral("Authentication Required"));
        m_stateMessage->setText(QStringLiteral("Please sign in with authorized credentials to view runway data."));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Runways"));
        return;
    }

    QString search = m_searchEdit ? m_searchEdit->text() : QString();
    QString status = m_statusFilter ? m_statusFilter->currentText() : QString();
    QString error;

    m_currentRunways = m_service->getAllRunways(search, status, &error);

    if (!error.isEmpty() && m_currentRunways.isEmpty()) {
        m_tableWidget->setRowCount(0);
        m_stateTitle->setText(QStringLiteral("Database Unavailable"));
        m_stateMessage->setText(error);
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Runways"));
        m_countBadge->setStyleSheet("background-color: #fee2e2; color: #991b1b; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #fca5a5;");
        return;
    }

    if (m_currentRunways.isEmpty()) {
        m_tableWidget->setRowCount(0);
        m_stateTitle->setText(QStringLiteral("No Runways Found"));
        m_stateMessage->setText(QStringLiteral("No runways match your search query or filter."));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Runways"));
        m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #cbd5e1;");
        return;
    }

    renderRunwayTable(m_currentRunways);
    m_contentStack->setCurrentWidget(m_tableWidget);
    m_countBadge->setText(QStringLiteral("%1 Runways").arg(m_currentRunways.size()));
    m_countBadge->setStyleSheet("background-color: #e0f2fe; color: #0369a1; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #bae6fd;");
}

void RunwaysView::renderRunwayTable(const QList<Models::Runway> &runwayList)
{
    m_tableWidget->setRowCount(0);
    m_tableWidget->setRowCount(runwayList.size());

    for (int row = 0; row < runwayList.size(); ++row) {
        const Models::Runway &rw = runwayList[row];

        // 0: ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(rw.id));
        idItem->setTextAlignment(Qt::AlignCenter);

        // 1: Runway Designation
        QTableWidgetItem *numItem = new QTableWidgetItem(rw.runwayNumber);
        QFont boldFont = numItem->font();
        boldFont.setBold(true);
        numItem->setFont(boldFont);
        numItem->setTextAlignment(Qt::AlignCenter);

        // 2: Permitted Operations
        QString typeLabel = rw.runwayType;
        if (rw.runwayType == QStringLiteral("Both")) {
            typeLabel = QStringLiteral("Both (Landing / Takeoff)");
        } else if (rw.runwayType == QStringLiteral("Landing")) {
            typeLabel = QStringLiteral("Landing Only");
        } else if (rw.runwayType == QStringLiteral("Takeoff")) {
            typeLabel = QStringLiteral("Takeoff Only");
        }
        QTableWidgetItem *typeItem = new QTableWidgetItem(typeLabel);
        typeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 3: Operational Status (Colors + Explicit text labels)
        QTableWidgetItem *statusItem = new QTableWidgetItem(rw.status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (rw.isAvailable()) {
            statusItem->setForeground(QColor(22, 101, 52)); // Green
            statusItem->setText(QStringLiteral("● Available"));
        } else if (rw.isOccupied()) {
            statusItem->setForeground(QColor(37, 99, 235)); // Blue
            if (!rw.activeFlightNumber.isEmpty()) {
                statusItem->setText(QStringLiteral("■ Occupied (%1)").arg(rw.activeFlightNumber));
            } else {
                statusItem->setText(QStringLiteral("■ Occupied"));
            }
        } else if (rw.isMaintenance()) {
            statusItem->setForeground(QColor(180, 83, 9)); // Amber / Orange
            statusItem->setText(QStringLiteral("▲ Maintenance"));
        } else {
            statusItem->setForeground(QColor(100, 116, 139));
        }

        // 4: Occupancy / Active Allocation
        QString allocationStr;
        if (rw.isOccupied()) {
            if (!rw.activeFlightNumber.isEmpty()) {
                allocationStr = QStringLiteral("Occupied by Flight %1 (Active Assignment)").arg(rw.activeFlightNumber);
            } else {
                allocationStr = QStringLiteral("Occupied (Active Resource Hold)");
            }
        } else if (rw.isMaintenance()) {
            allocationStr = QStringLiteral("Offline for Scheduled / Safety Maintenance");
        } else {
            allocationStr = QStringLiteral("None (Free Shared Resource)");
        }

        QTableWidgetItem *allocItem = new QTableWidgetItem(allocationStr);
        allocItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        if (rw.isOccupied()) {
            allocItem->setForeground(QColor(29, 78, 216));
        } else if (rw.isMaintenance()) {
            allocItem->setForeground(QColor(146, 64, 14));
        } else {
            allocItem->setForeground(QColor(71, 85, 105));
        }

        m_tableWidget->setItem(row, 0, idItem);
        m_tableWidget->setItem(row, 1, numItem);
        m_tableWidget->setItem(row, 2, typeItem);
        m_tableWidget->setItem(row, 3, statusItem);
        m_tableWidget->setItem(row, 4, allocItem);
    }
}

void RunwaysView::onSearchChanged(const QString &text)
{
    Q_UNUSED(text)
    refreshRunways();
}

void RunwaysView::onStatusFilterChanged(int index)
{
    Q_UNUSED(index)
    refreshRunways();
}

void RunwaysView::onSelectionChanged()
{
    Models::Runway *rw = getSelectedRunway();
    bool hasSelection = (rw != nullptr);

    bool canMaintenance = m_service && m_service->currentSession().hasPermission(QStringLiteral("action:RunwayMaintenance"));
    bool isAdmin = m_service && m_service->currentSession().isAdmin();

    m_editButton->setEnabled(hasSelection && (isAdmin || canMaintenance));

    if (hasSelection && (canMaintenance || isAdmin)) {
        m_toggleMaintenanceBtn->setEnabled(true);
        if (rw->isMaintenance()) {
            m_toggleMaintenanceBtn->setText(QStringLiteral("✅ Return to Service"));
            m_toggleMaintenanceBtn->setStyleSheet(R"(
                QPushButton {
                    background-color: #16a34a;
                    color: #ffffff;
                    border: none;
                    border-radius: 6px;
                    padding: 8px 16px;
                    font-size: 13px;
                    font-weight: 600;
                }
                QPushButton:hover {
                    background-color: #15803d;
                }
            )");
        } else {
            m_toggleMaintenanceBtn->setText(QStringLiteral("🛠 Put in Maintenance"));
            m_toggleMaintenanceBtn->setStyleSheet(R"(
                QPushButton {
                    background-color: #d97706;
                    color: #ffffff;
                    border: none;
                    border-radius: 6px;
                    padding: 8px 16px;
                    font-size: 13px;
                    font-weight: 600;
                }
                QPushButton:hover {
                    background-color: #b45309;
                }
            )");
        }
    } else {
        m_toggleMaintenanceBtn->setEnabled(false);
        m_toggleMaintenanceBtn->setText(QStringLiteral("🛠 Set Maintenance"));
        m_toggleMaintenanceBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #cbd5e1;
                color: #94a3b8;
                border: none;
                border-radius: 6px;
                padding: 8px 16px;
                font-size: 13px;
                font-weight: 600;
            }
        )");
    }
}

Models::Runway* RunwaysView::getSelectedRunway()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentRunways.size()) {
        return nullptr;
    }
    return &m_currentRunways[row];
}

void RunwaysView::onToggleMaintenanceClicked()
{
    Models::Runway *rw = getSelectedRunway();
    if (!rw) {
        return;
    }

    bool targetMaintenance = !rw->isMaintenance();
    QString actionText = targetMaintenance ?
        QStringLiteral("place Runway %1 into Maintenance mode? Normal flight allocations will be suspended.").arg(rw->runwayNumber) :
        QStringLiteral("return Runway %1 to Available service?").arg(rw->runwayNumber);

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Confirm Runway Status Change"),
        actionText,
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString error;
    bool ok = m_service->setRunwayMaintenanceStatus(rw->id, targetMaintenance, &error);
    if (!ok) {
        QMessageBox::critical(
            this,
            QStringLiteral("Maintenance Update Failed"),
            QStringLiteral("Could not update runway status:\n%1").arg(error)
        );
        return;
    }

    refreshRunways();
}

void RunwaysView::onAddRunwayClicked()
{
    RunwayDialog dialog(m_service, RunwayDialog::Mode::Add, this);
    if (dialog.exec() == QDialog::Accepted) {
        Models::Runway newRw = dialog.runway();
        QString error;
        bool ok = m_service->createRunway(newRw, &error);
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("Creation Failed"),
                                  QStringLiteral("Failed to create runway:\n%1").arg(error));
        } else {
            refreshRunways();
        }
    }
}

void RunwaysView::onEditRunwayClicked()
{
    Models::Runway *rw = getSelectedRunway();
    if (!rw) {
        return;
    }

    RunwayDialog dialog(m_service, RunwayDialog::Mode::Edit, this);
    dialog.setRunway(*rw);
    if (dialog.exec() == QDialog::Accepted) {
        Models::Runway updatedRw = dialog.runway();
        QString error;
        bool ok = m_service->updateRunway(updatedRw, &error);
        if (!ok) {
            QMessageBox::critical(this, QStringLiteral("Update Failed"),
                                  QStringLiteral("Failed to update runway:\n%1").arg(error));
        } else {
            refreshRunways();
        }
    }
}

void RunwaysView::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(row)
    Q_UNUSED(column)
    onEditRunwayClicked();
}
