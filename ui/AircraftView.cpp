#include "AircraftView.h"

#include "services/ApplicationService.h"
#include "ui/AircraftDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

AircraftView::AircraftView(ApplicationService *service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    setupUi();

    if (m_service) {
        connect(m_service, &ApplicationService::aircraftChanged, this, &AircraftView::refreshAircraft);
    }
}

void AircraftView::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // ── 1. Header Row ────────────────────────────────────────────────────────
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QVBoxLayout *titleTextLayout = new QVBoxLayout;

    QLabel *titleLabel = new QLabel(QStringLiteral("Aircraft Fleet Management"), this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #0f172a;");

    QLabel *subLabel = new QLabel(QStringLiteral("Manage airport aircraft inventory, capacity, and operational status in MySQL"), this);
    subLabel->setStyleSheet("font-size: 13px; color: #64748b;");

    titleTextLayout->addWidget(titleLabel);
    titleTextLayout->addWidget(subLabel);
    headerLayout->addLayout(titleTextLayout);
    headerLayout->addStretch();

    m_countBadge = new QLabel(QStringLiteral("0 Aircraft"), this);
    m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; "
                               "padding: 6px 14px; border-radius: 12px; border: 1px solid #cbd5e1;");
    headerLayout->addWidget(m_countBadge);

    m_permNotice = new QLabel(this);
    m_permNotice->setStyleSheet("background-color: #fef3c7; color: #92400e; font-size: 11px; font-weight: 600; "
                               "padding: 6px 12px; border-radius: 12px; border: 1px solid #fde68a;");
    m_permNotice->setText(QStringLiteral("👁 Read-Only Mode"));
    m_permNotice->hide();
    headerLayout->addWidget(m_permNotice);

    mainLayout->addLayout(headerLayout);

    // ── 2. Toolbar Row ───────────────────────────────────────────────────────
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(10);

    // Search input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("🔍 Search by Registration or Aircraft Type..."));
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
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AircraftView::onSearchChanged);
    toolbarLayout->addWidget(m_searchEdit);

    // Status filter
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem(QStringLiteral("All Statuses"));
    m_statusFilter->addItems({
        QStringLiteral("Active"),
        QStringLiteral("Maintenance"),
        QStringLiteral("Retired")
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
            this, &AircraftView::onStatusFilterChanged);
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
    connect(m_refreshButton, &QPushButton::clicked, this, &AircraftView::refreshAircraft);
    toolbarLayout->addWidget(m_refreshButton);

    toolbarLayout->addStretch();

    // Add button (Admin only)
    m_addButton = new QPushButton(QStringLiteral("➕ Add Aircraft"), this);
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
    connect(m_addButton, &QPushButton::clicked, this, &AircraftView::onAddAircraftClicked);
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
    connect(m_editButton, &QPushButton::clicked, this, &AircraftView::onEditAircraftClicked);
    toolbarLayout->addWidget(m_editButton);

    // Delete button (Admin only)
    m_deleteButton = new QPushButton(QStringLiteral("🗑 Delete"), this);
    m_deleteButton->setEnabled(false);
    m_deleteButton->setStyleSheet(R"(
        QPushButton {
            background-color: #dc2626;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #b91c1c;
        }
        QPushButton:disabled {
            background-color: #cbd5e1;
            color: #94a3b8;
        }
    )");
    connect(m_deleteButton, &QPushButton::clicked, this, &AircraftView::onDeleteAircraftClicked);
    toolbarLayout->addWidget(m_deleteButton);

    mainLayout->addLayout(toolbarLayout);

    // ── 3. Content Area (Stacked: 0=Table, 1=Empty/Error State) ──────────────
    m_contentStack = new QStackedWidget(this);

    // Table view
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(6);
    m_tableWidget->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("Registration #"),
        QStringLiteral("Aircraft Type / Model"),
        QStringLiteral("Passenger Capacity"),
        QStringLiteral("Operational Status"),
        QStringLiteral("Linked Flights")
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
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

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

    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, &AircraftView::onSelectionChanged);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &AircraftView::onTableDoubleClicked);

    m_contentStack->addWidget(m_tableWidget);

    // Empty / Error State Widget
    m_stateWidget = new QWidget(this);
    QVBoxLayout *stateLayout = new QVBoxLayout(m_stateWidget);
    stateLayout->setAlignment(Qt::AlignCenter);

    m_stateIcon = new QLabel(QStringLiteral("🛩"), this);
    m_stateIcon->setStyleSheet("font-size: 48px; color: #94a3b8;");
    m_stateIcon->setAlignment(Qt::AlignCenter);

    m_stateTitle = new QLabel(QStringLiteral("No Aircraft Found"), this);
    m_stateTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #334155; margin-top: 8px;");
    m_stateTitle->setAlignment(Qt::AlignCenter);

    m_stateMessage = new QLabel(QStringLiteral("No aircraft match your search criteria or the database is empty."), this);
    m_stateMessage->setStyleSheet("font-size: 13px; color: #64748b; margin-top: 4px;");
    m_stateMessage->setAlignment(Qt::AlignCenter);

    m_stateButton = new QPushButton(QStringLiteral("Add Aircraft"), this);
    m_stateButton->setStyleSheet("background-color: #2563eb; color: #ffffff; border-radius: 6px; padding: 8px 16px; margin-top: 14px; font-weight: 600;");
    connect(m_stateButton, &QPushButton::clicked, this, &AircraftView::onAddAircraftClicked);

    stateLayout->addWidget(m_stateIcon);
    stateLayout->addWidget(m_stateTitle);
    stateLayout->addWidget(m_stateMessage);
    stateLayout->addWidget(m_stateButton);

    m_contentStack->addWidget(m_stateWidget);
    mainLayout->addWidget(m_contentStack);
}

void AircraftView::updatePermissions()
{
    if (!m_service) {
        return;
    }

    const Session &session = m_service->currentSession();
    bool canAdmin = session.hasPermission(QStringLiteral("action:AdministerAircraft"));
    bool canManage = session.hasPermission(QStringLiteral("action:ManageAircraft"));

    m_addButton->setVisible(canAdmin);
    m_deleteButton->setVisible(canAdmin);
    m_editButton->setVisible(canManage);
    m_stateButton->setVisible(canAdmin);

    if (!canManage) {
        m_permNotice->setText(QStringLiteral("👁 Read-Only Mode"));
        m_permNotice->setStyleSheet("background-color: #f1f5f9; color: #475569; font-size: 11px; font-weight: 600; "
                                   "padding: 6px 12px; border-radius: 12px; border: 1px solid #cbd5e1;");
        m_permNotice->show();
    } else if (!canAdmin) {
        m_permNotice->setText(QStringLiteral("🛠 Operator Mode: Status updates only"));
        m_permNotice->setStyleSheet("background-color: #eff6ff; color: #1d4ed8; font-size: 11px; font-weight: 600; "
                                   "padding: 6px 12px; border-radius: 12px; border: 1px solid #bfdbfe;");
        m_permNotice->show();
    } else {
        m_permNotice->hide();
    }

    onSelectionChanged();
}

void AircraftView::refreshAircraft()
{
    if (!m_service) {
        return;
    }

    updatePermissions();

    QString search = m_searchEdit->text();
    QString status = m_statusFilter->currentText();
    QString err;

    m_currentAircraft = m_service->getAllAircraft(search, status, &err);

    if (!err.isEmpty() && m_currentAircraft.isEmpty()) {
        m_stateIcon->setText(QStringLiteral("⚠"));
        m_stateTitle->setText(QStringLiteral("Database Error"));
        m_stateMessage->setText(err);
        m_stateButton->setText(QStringLiteral("Retry Connection"));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("Error"));
        m_countBadge->setStyleSheet("background-color: #fee2e2; color: #991b1b; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px;");
        return;
    }

    if (m_currentAircraft.isEmpty()) {
        m_stateIcon->setText(QStringLiteral("🛩"));
        m_stateTitle->setText(QStringLiteral("No Aircraft Found"));
        m_stateMessage->setText(search.isEmpty() ?
            QStringLiteral("No aircraft are currently registered in MySQL. Click 'Add Aircraft' to register one.") :
            QStringLiteral("No aircraft matched your filter '%1'.").arg(search));
        m_stateButton->setText(QStringLiteral("➕ Add Aircraft"));
        m_contentStack->setCurrentWidget(m_stateWidget);
        m_countBadge->setText(QStringLiteral("0 Aircraft"));
        m_countBadge->setStyleSheet("background-color: #e2e8f0; color: #334155; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px;");
        return;
    }

    renderAircraftTable(m_currentAircraft);
    m_contentStack->setCurrentWidget(m_tableWidget);
    m_countBadge->setText(QStringLiteral("%1 Aircraft").arg(m_currentAircraft.size()));
    m_countBadge->setStyleSheet("background-color: #e0f2fe; color: #0369a1; font-size: 12px; font-weight: 600; padding: 6px 14px; border-radius: 12px; border: 1px solid #bae6fd;");
}

void AircraftView::renderAircraftTable(const QList<Models::Aircraft> &aircraftList)
{
    m_tableWidget->setRowCount(0);
    m_tableWidget->setRowCount(aircraftList.size());

    for (int row = 0; row < aircraftList.size(); ++row) {
        const Models::Aircraft &ac = aircraftList[row];

        // 0: ID
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(ac.id));
        idItem->setTextAlignment(Qt::AlignCenter);

        // 1: Registration Number
        QTableWidgetItem *regItem = new QTableWidgetItem(ac.registrationNumber);
        QFont boldFont = regItem->font();
        boldFont.setBold(true);
        regItem->setFont(boldFont);
        regItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 2: Aircraft Type / Model
        QTableWidgetItem *typeItem = new QTableWidgetItem(ac.aircraftType);
        typeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // 3: Capacity
        QTableWidgetItem *capItem = new QTableWidgetItem(QStringLiteral("%1 seats").arg(ac.capacity));
        capItem->setTextAlignment(Qt::AlignCenter);

        // 4: Operational Status
        QTableWidgetItem *statusItem = new QTableWidgetItem(ac.status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (ac.status == QStringLiteral("Active")) {
            statusItem->setForeground(QColor(22, 101, 52)); // Green
            statusItem->setText(QStringLiteral("● Active"));
        } else if (ac.status == QStringLiteral("Maintenance")) {
            statusItem->setForeground(QColor(180, 83, 9)); // Amber
            statusItem->setText(QStringLiteral("▲ Maintenance"));
        } else if (ac.status == QStringLiteral("Retired")) {
            statusItem->setForeground(QColor(100, 116, 139)); // Slate
            statusItem->setText(QStringLiteral("✖ Retired"));
        } else {
            statusItem->setForeground(QColor(30, 41, 59));
        }

        // 5: Linked Flights Count
        QString flightStr = (ac.assignedFlightsCount > 0) ?
            QStringLiteral("%1 assigned flight%2").arg(ac.assignedFlightsCount).arg(ac.assignedFlightsCount > 1 ? "s" : "") :
            QStringLiteral("None");
        QTableWidgetItem *countItem = new QTableWidgetItem(flightStr);
        countItem->setTextAlignment(Qt::AlignCenter);
        if (ac.assignedFlightsCount > 0) {
            countItem->setForeground(QColor(37, 99, 235)); // Blue
        } else {
            countItem->setForeground(QColor(148, 163, 184)); // Slate
        }

        m_tableWidget->setItem(row, 0, idItem);
        m_tableWidget->setItem(row, 1, regItem);
        m_tableWidget->setItem(row, 2, typeItem);
        m_tableWidget->setItem(row, 3, capItem);
        m_tableWidget->setItem(row, 4, statusItem);
        m_tableWidget->setItem(row, 5, countItem);
    }
}

void AircraftView::onSearchChanged(const QString &text)
{
    Q_UNUSED(text)
    refreshAircraft();
}

void AircraftView::onStatusFilterChanged(int index)
{
    Q_UNUSED(index)
    refreshAircraft();
}

void AircraftView::onSelectionChanged()
{
    bool hasSelection = (m_tableWidget->currentRow() >= 0);
    bool canManage = m_service && m_service->currentSession().hasPermission(QStringLiteral("action:ManageAircraft"));
    bool canAdmin = m_service && m_service->currentSession().hasPermission(QStringLiteral("action:AdministerAircraft"));

    m_editButton->setEnabled(hasSelection && canManage);
    m_deleteButton->setEnabled(hasSelection && canAdmin);
}

Models::Aircraft* AircraftView::getSelectedAircraft()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentAircraft.size()) {
        return nullptr;
    }
    return &m_currentAircraft[row];
}

void AircraftView::onAddAircraftClicked()
{
    if (!m_service || !m_service->currentSession().hasPermission(QStringLiteral("action:AdministerAircraft"))) {
        QMessageBox::warning(this, QStringLiteral("Access Denied"),
                             QStringLiteral("You do not have permission to register new aircraft."));
        return;
    }

    AircraftDialog dialog(m_service, AircraftDialog::Mode::Add, this);
    if (dialog.exec() == QDialog::Accepted) {
        Models::Aircraft newAc = dialog.aircraft();
        QString error;
        if (m_service->createAircraft(newAc, &error)) {
            QMessageBox::information(this, QStringLiteral("Aircraft Registered"),
                                     QStringLiteral("Aircraft %1 successfully registered in MySQL.").arg(newAc.registrationNumber));
            refreshAircraft();
        } else {
            QMessageBox::critical(this, QStringLiteral("Registration Error"),
                                  QStringLiteral("Could not register aircraft:\n%1").arg(error));
        }
    }
}

void AircraftView::onEditAircraftClicked()
{
    Models::Aircraft *selected = getSelectedAircraft();
    if (!selected) {
        return;
    }

    if (!m_service || !m_service->currentSession().hasPermission(QStringLiteral("action:ManageAircraft"))) {
        QMessageBox::warning(this, QStringLiteral("Access Denied"),
                             QStringLiteral("You do not have permission to edit aircraft records."));
        return;
    }

    AircraftDialog dialog(m_service, AircraftDialog::Mode::Edit, this);
    dialog.setAircraft(*selected);

    if (dialog.exec() == QDialog::Accepted) {
        Models::Aircraft updatedAc = dialog.aircraft();
        QString error;
        if (m_service->updateAircraft(updatedAc, &error)) {
            QMessageBox::information(this, QStringLiteral("Aircraft Updated"),
                                     QStringLiteral("Aircraft %1 successfully updated.").arg(updatedAc.registrationNumber));
            refreshAircraft();
        } else {
            QMessageBox::critical(this, QStringLiteral("Update Error"),
                                  QStringLiteral("Could not update aircraft:\n%1").arg(error));
        }
    }
}

void AircraftView::onDeleteAircraftClicked()
{
    Models::Aircraft *selected = getSelectedAircraft();
    if (!selected) {
        return;
    }

    if (!m_service || !m_service->currentSession().hasPermission(QStringLiteral("action:AdministerAircraft"))) {
        QMessageBox::warning(this, QStringLiteral("Access Denied"),
                             QStringLiteral("You do not have permission to delete aircraft records."));
        return;
    }

    // Step 1: Pre-deletion reference check for user-friendly feedback
    QString checkReason;
    bool canDelete = m_service->canDeleteAircraft(selected->id, &checkReason);

    if (!canDelete) {
        QMessageBox::StandardButton retireOpt = QMessageBox::question(
            this,
            QStringLiteral("Safe Deletion Blocked"),
            QStringLiteral("%1\n\nTo preserve historical operational flight records, referenced aircraft cannot be deleted.\n\n"
                           "Would you like to retire this aircraft instead by updating its operational status to 'Retired'?")
                .arg(checkReason),
            QMessageBox::Yes | QMessageBox::No
        );

        if (retireOpt == QMessageBox::Yes) {
            QString statusErr;
            if (m_service->updateAircraftStatus(selected->id, QStringLiteral("Retired"), &statusErr)) {
                QMessageBox::information(this, QStringLiteral("Status Updated"),
                                         QStringLiteral("Aircraft %1 operational status set to 'Retired'.").arg(selected->registrationNumber));
                refreshAircraft();
            } else {
                QMessageBox::critical(this, QStringLiteral("Status Update Failed"),
                                      QStringLiteral("Failed to update status: %1").arg(statusErr));
            }
        }
        return;
    }

    // Step 2: Explicit confirmation dialog for unreferenced aircraft
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("Confirm Aircraft Deletion"),
        QStringLiteral("Are you sure you want to permanently delete aircraft '%1' (ID %2)?\n\n"
                       "This record is not currently assigned to any flights and will be permanently removed.")
            .arg(selected->registrationNumber, QString::number(selected->id)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString error;
    bool success = m_service->deleteAircraft(selected->id, &error);

    if (success) {
        QMessageBox::information(this, QStringLiteral("Aircraft Deleted"),
                                 QStringLiteral("Aircraft '%1' was successfully removed from the fleet.").arg(selected->registrationNumber));
        refreshAircraft();
    } else {
        QMessageBox::critical(this, QStringLiteral("Delete Failed"),
                              QStringLiteral("Could not delete aircraft:\n%1").arg(error));
    }
}

void AircraftView::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (m_service && m_service->currentSession().hasPermission(QStringLiteral("action:ManageAircraft"))) {
        onEditAircraftClicked();
    } else {
        Models::Aircraft *selected = getSelectedAircraft();
        if (selected) {
            QMessageBox::information(this, QStringLiteral("Aircraft Details"),
                QStringLiteral("Registration Number: %1\nAircraft Type: %2\nPassenger Capacity: %3 seats\nOperational Status: %4\nLinked Flights: %5")
                    .arg(selected->registrationNumber,
                         selected->aircraftType,
                         QString::number(selected->capacity),
                         selected->status,
                         QString::number(selected->assignedFlightsCount))
            );
        }
    }
}

void AircraftView::onQuickStatusChanged(int index)
{
    Q_UNUSED(index)
}
