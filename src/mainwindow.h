#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QTableView>
#include <QComboBox>
#include "dbmanager.h"
#include <QStandardItemModel>
#include <QTableWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onNewTransaction(int editTxId);
    void onManageFunds();
    void onManageLookups();
    void onManageAccounts();
    void switchPage(int index);

private:
    void setupUI();
    void refreshAll();
    void populateFundsCombo(QComboBox* combo);
    QString getFundName(int fundId) const;
    void refreshFundsPage();
    void showFundDialog(bool isEdit = false, int fundId = -1, QStandardItemModel* model = nullptr);
    void refreshFundTable(QStandardItemModel* model);
    void showAccountDialog(bool isEdit = false, int accountId = -1, QStandardItemModel* model = nullptr);
    void refreshAccountsTable(QStandardItemModel* model);
    void showSimpleLookupDialog(const QString& title, QStandardItemModel* model, const QString& tableName, int id = -1);
    void refreshSimpleLookupTable(QStandardItemModel* model, const QString& tableName);
    void setupTransactionsPage();
    void loadTransactions();
    void onTransactionSelected(int row);
    void setupFundsBalancesPage();
    void loadFundBalances();
    void setupManageLookupsPage();
    void editTransaction(int txId);
    // Page indices - single source of truth
    static const int PAGE_DASHBOARD = 0;
    static const int PAGE_LEDGER    = 1;
    static const int PAGE_FUNDS     = 2;
    static const int PAGE_REPORTS   = 3;
    static const int PAGE_SETUP     = 4;

    DbManager* dbManager;

    QStackedWidget* stack;
    QListWidget* sidebar;

    QWidget* dashboardPage;
    QWidget* fundsPage;
    QWidget* transactionsPage;

    QLabel* wodrLabel;
    QLabel* wdrLabel;
    QLabel* totalLabel;
    QTableView* recentTxTable;

    QTableWidget* txListTable = nullptr;
    QTableWidget* txDetailsTable = nullptr;
    QLabel* txDetailsLabel = nullptr;

    QTableWidget* fundBalancesTable = nullptr;
    QLabel* totalWODRLabel = nullptr;
    QLabel* totalWDRLabel = nullptr;
    QLabel* grandTotalLabel = nullptr;

    // Theme-able colors for transaction rows
    QColor colorNormalEven;
    QColor colorNormalOdd;
    QColor colorCleared;
    QColor colorReconciled;
};