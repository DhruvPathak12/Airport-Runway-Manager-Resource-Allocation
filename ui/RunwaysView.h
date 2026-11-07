#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

#include "models/Runway.h"
#include "resource/ConcurrentAllocationDemo.h"

class ApplicationService;

/**
 * @brief GUI view for viewing, filtering, and managing runway shared resources
 * and maintenance, and demonstrating concurrent multithreaded resource
 * allocation and mutual exclusion (Day 20).
 */
class RunwaysView : public QWidget {
  Q_OBJECT

public:
  explicit RunwaysView(ApplicationService *service, QWidget *parent = nullptr);
  ~RunwaysView() override;

  /**
   * @brief Updates UI controls and action availability based on session
   * permissions.
   */
  void updatePermissions();

public slots:
  /**
   * @brief Refreshes the runway list from MySQL.
   */
  void refreshRunways();

private slots:
  // Live Tab slots
  void onSearchChanged(const QString &text);
  void onStatusFilterChanged(int index);
  void onSelectionChanged();
  void onToggleMaintenanceClicked();
  void onAddRunwayClicked();
  void onEditRunwayClicked();
  void onTableDoubleClicked(int row, int column);
  void onAllocateClicked();
  void onReleaseClicked();

  // Demonstration Tab slots (Day 20)
  void onStartConcurrentTestClicked();
  void onResetDemoClicked();
  void onRefreshResourceStateClicked();
  void onDemoStarted(int runwayId, int totalThreads);
  void
  onThreadStateChanged(const ConcurrentAllocationDemo::ThreadResult &result);
  void onDemoFinished(const ConcurrentAllocationDemo::DemoReport &report);

private:
  void setupUi();
  QWidget *createLiveTab();
  QWidget *createConcurrentTab();
  void renderRunwayTable(const QList<Models::Runway> &runwayList);
  void updateDemoRunwayCombo();
  Models::Runway *getSelectedRunway();

  ApplicationService *m_service = nullptr;
  QTabWidget *m_tabWidget = nullptr;

  // ── Live Tab Controls ────────────────────────────────────────────────────
  QLineEdit *m_searchEdit = nullptr;
  QComboBox *m_statusFilter = nullptr;
  QPushButton *m_refreshButton = nullptr;
  QPushButton *m_allocateButton = nullptr;
  QPushButton *m_releaseButton = nullptr;
  QPushButton *m_toggleMaintenanceBtn = nullptr;
  QPushButton *m_addButton = nullptr;
  QPushButton *m_editButton = nullptr;
  QLabel *m_countBadge = nullptr;
  QLabel *m_permNotice = nullptr;
  QLabel *m_csBanner = nullptr;

  // Content stack (0=Table, 1=Empty / Error)
  QStackedWidget *m_contentStack = nullptr;
  QTableWidget *m_tableWidget = nullptr;
  QWidget *m_stateWidget = nullptr;
  QLabel *m_stateIcon = nullptr;
  QLabel *m_stateTitle = nullptr;
  QLabel *m_stateMessage = nullptr;
  QPushButton *m_stateButton = nullptr;

  // ── Demonstration Tab Controls (Day 20) ──────────────────────────────────
  QComboBox *m_demoRunwayCombo = nullptr;
  QSpinBox *m_demoThreadSpin = nullptr;
  QPushButton *m_startDemoBtn = nullptr;
  QPushButton *m_resetDemoBtn = nullptr;
  QPushButton *m_refreshDemoBtn = nullptr;
  QLabel *m_demoStatusLabel = nullptr;

  // Dashboard Cards
  QLabel *m_totalReqCard = nullptr;
  QLabel *m_successCard = nullptr;
  QLabel *m_rejectedCard = nullptr;
  QLabel *m_winnerCard = nullptr;

  QTableWidget *m_demoTable = nullptr;
  ConcurrentAllocationDemo *m_demoEngine = nullptr;

  QList<Models::Runway> m_currentRunways;
};
