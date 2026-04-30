#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QTableView>
#include <QComboBox>
#include "dbmanager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onNewTransaction();
    void switchPage(int index);

private:
    void setupUI();
    void refreshAll();
    void populateFundsCombo(QComboBox* combo);
    QString getFundName(int fundId) const;
    void refreshFundsPage();

    DbManager* dbManager;

    QStackedWidget* stack;
    QListWidget* sidebar;

    // Page widgets
    QWidget* dashboardPage;
    QWidget* fundsPage;
    QWidget* transactionsPage;

    // Dashboard elements
    QLabel* wodrLabel;
    QLabel* wdrLabel;
    QLabel* totalLabel;
    QTableView* recentTxTable;
};