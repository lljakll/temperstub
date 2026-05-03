#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QTableView>
#include <QComboBox>
#include "dbmanager.h"
#include <QStandardItemModel>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onNewTransaction();
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
};