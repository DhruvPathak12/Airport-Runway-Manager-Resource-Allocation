#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>

class ApplicationService;

/**
 * @brief The main application window with navigation sidebar + content area.
 *
 * Structure:
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │ Smart Air Traffic Control Management System                    [− □ ×]│
 *  ├──────────────────┬───────────────────────────────────────────────────┤
 *  │  Navigation      │                                                   │
 *  │  ─────────────   │            Content Area (QStackedWidget)          │
 *  │  Dashboard       │                                                   │
 *  │  Flights         │   (Placeholder pages — populated Day 6–14)        │
 *  │  Aircraft        │                                                   │
 *  │  Runways         │                                                   │
 *  │  Scheduling      │                                                   │
 *  │  Transactions    │                                                   │
 *  │  Logs            │                                                   │
 *  │  Users           │                                                   │
 *  │                  │                                                   │
 *  └──────────────────┴───────────────────────────────────────────────────┘
 *
 * Today (Day 2): Navigation + placeholder pages only.
 * Functional modules added per-day starting Day 6.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ApplicationService *service, QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onNavItemChanged(int row);

private:
    void setupUi();
    void setupNavigation();
    void setupPages();
    QWidget* makePlaceholderPage(const QString &title, const QString &description);

    // ── UI components ─────────────────────────────────────────────────────────
    QListWidget    *m_navList      = nullptr;
    QStackedWidget *m_pageStack    = nullptr;

    // ── Application service reference ─────────────────────────────────────────
    ApplicationService *m_service  = nullptr;
};
