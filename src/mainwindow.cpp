#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QLabel>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QSplitter>
#include <QDebug>
#include <QEvent>
#include <QCheckBox>
#include <QTextEdit>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    dbManager = new DbManager(this);
    if (!dbManager->initDatabase()) {
        QMessageBox::critical(this, "Database Error", "Could not initialize database.");
        close();
        return;
    }
    setupUI();
    refreshAll();
    setWindowTitle("TemperStub – Hope Baptist Church Treasurer (Guide Rev 1.0)");
    resize(1200, 800);
    qDebug() << "✅ MainWindow fully initialized with Treasurer’s Guide principles";
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    auto* central = new QWidget;
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    sidebar = new QListWidget;
    sidebar->setFixedWidth(240);
    sidebar->setStyleSheet(R"(
        QListWidget { background: #2c3e50; color: #ecf0f1; font-size: 15px; }
        QListWidget::item { padding: 14px 12px; border-bottom: 1px solid #34495e; }
        QListWidget::item:selected { background: #3498db; color: white; }
    )");
    // Sidebar items
    sidebar->addItem("Dashboard");
    sidebar->addItem("New Transaction");
    sidebar->addItem("Manage Lookups");           // Manage Lookups
    sidebar->addItem("Transactions");
    sidebar->addItem("Reports");

    connect(sidebar, &QListWidget::currentRowChanged, this, &MainWindow::switchPage);

    stack = new QStackedWidget;

    // Dashboard Page
    dashboardPage = new QWidget;
    auto* dashL = new QVBoxLayout(dashboardPage);
    dashL->setContentsMargins(24, 24, 24, 24);
    dashL->setSpacing(16);

    auto* balBox = new QGroupBox("Net Asset Balances — Treasurer’s Guide §2");
    auto* balF = new QFormLayout(balBox);
    wodrLabel = new QLabel("$0.00");
    wdrLabel  = new QLabel("$0.00");
    totalLabel = new QLabel("$0.00");
    balF->addRow("Without Donor Restrictions (WODR)", wodrLabel);
    balF->addRow("With Donor Restrictions (WDR)", wdrLabel);
    balF->addRow("Total Net Assets", totalLabel);
    dashL->addWidget(balBox);

    QPushButton* newBtn = new QPushButton("➕ New Transaction (Guide §3, §4, §10)");
    newBtn->setStyleSheet("font-size: 16px; padding: 14px;");
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::onNewTransaction);
    dashL->addWidget(newBtn);

    dashL->addWidget(new QLabel("Recent Transactions (Guide §9)"));
    recentTxTable = new QTableView;
    recentTxTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dashL->addWidget(recentTxTable);

    stack->addWidget(dashboardPage);

    // Funds Page
    fundsPage = new QWidget;
    auto* fundsL = new QVBoxLayout(fundsPage);
    fundsL->setContentsMargins(24, 24, 24, 24);
    auto* fundTable = new QTableWidget(0, 3);
    fundTable->setHorizontalHeaderLabels({"Fund", "Restriction", "Balance"});
    fundTable->horizontalHeader()->setStretchLastSection(true);
    fundsL->addWidget(fundTable);
    stack->addWidget(fundsPage);

    // Transactions Page
    transactionsPage = new QWidget;
    auto* txL = new QVBoxLayout(transactionsPage);
    txL->setContentsMargins(24, 24, 24, 24);
    txL->addWidget(new QLabel("Full Transaction Ledger – coming soon"));
    stack->addWidget(transactionsPage);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(sidebar);
    splitter->addWidget(stack);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
    setCentralWidget(central);

    sidebar->setCurrentRow(0);
}

void MainWindow::switchPage(int index) {
    stack->setCurrentIndex(index);   // ← use 'stack' not sidebar

    if (index == 1) {           // New Transaction
        onNewTransaction();
        sidebar->setCurrentRow(0); // optional: return to Dashboard after opening dialog
    } 
    else if (index == 2) {      // Lookups
        onManageLookups();
        sidebar->setCurrentRow(0);  // Return to dashboard after closing dialog
    } 
}

void MainWindow::onNewTransaction() {
    qDebug() << "Double-Entry Dialog opened";

    QDialog dlg(this);
    dlg.setWindowTitle("New Transaction Entry - Hope Baptist Church");
    QVBoxLayout* mainL = new QVBoxLayout(&dlg);
    mainL->setContentsMargins(16, 16, 16, 16);
    mainL->setSpacing(12);

    // Transaction Details
    QGroupBox* detailsBox = new QGroupBox("Transaction Details");
    QFormLayout* detailsF = new QFormLayout(detailsBox);
    QDateEdit* dateEdit = new QDateEdit(QDate::currentDate());
    dateEdit->setCalendarPopup(true);
    detailsF->addRow("Date", dateEdit);

    QLineEdit* descEdit = new QLineEdit;
    detailsF->addRow("Description", descEdit);

    QLineEdit* payeeEdit = new QLineEdit;
    detailsF->addRow("Payee/Donor", payeeEdit);

    QLineEdit* refEdit = new QLineEdit;
    detailsF->addRow("Reference", refEdit);

    QLineEdit* approvedEdit = new QLineEdit;
    detailsF->addRow("Approved By", approvedEdit);

    mainL->addWidget(detailsBox);

    // Reconcile label
    QLabel* reconcileLabel = new QLabel("Reconcile: $0.00");
    mainL->addWidget(reconcileLabel);

    // Lists declared early
    QList<QDoubleSpinBox*> fromSpins;
    QList<QDoubleSpinBox*> toSpins;

    auto updateReconcile = [&]() {
        double fromSum = 0.0, toSum = 0.0;
        for (auto* s : fromSpins) fromSum += s->value();
        for (auto* s : toSpins) toSum += s->value();
        double diff = toSum - fromSum;
        reconcileLabel->setText(QString("Reconcile: $%1").arg(diff, 0, 'f', 2));
        reconcileLabel->setStyleSheet(qAbs(diff) < 0.01 ? "color: green; font-weight: bold;" : "color: red; font-weight: bold;");
    };

    // === FROM (Debits) ===
    QGroupBox* fromBox = new QGroupBox("From (Debits)");
    QVBoxLayout* fromL = new QVBoxLayout(fromBox);

    auto addFromLine = [&]() {
        QHBoxLayout* line = new QHBoxLayout;
        QComboBox* accountCombo = new QComboBox;
        QList<Account> accts = dbManager->getAllAccounts();
        for (const Account& a : accts) accountCombo->addItem(a.name + " (" + a.code + ")", a.id);

        QDoubleSpinBox* amt = new QDoubleSpinBox;
        amt->setRange(0, 9999999.99);
        amt->setDecimals(2);
        amt->setPrefix("$ ");
        amt->setValue(0.0);

        fromSpins.append(amt);
        connect(amt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateReconcile);

        QComboBox* nat = new QComboBox; nat->addItems({"Contributions","Salaries & Housing","Utilities","Supplies","Travel","Other"});
        QComboBox* func = new QComboBox; func->addItems({"Program Services","Management & General","Fundraising"});

        line->addWidget(accountCombo);
        line->addWidget(amt);
        line->addWidget(nat);
        line->addWidget(func);
        fromL->addLayout(line);
        updateReconcile();
    };

    QPushButton* addFromBtn = new QPushButton("+ Add From Line");
    connect(addFromBtn, &QPushButton::clicked, addFromLine);
    fromL->addWidget(addFromBtn);
    addFromLine(); // start with one
    mainL->addWidget(fromBox);

    // === TO (Credits) ===
    QGroupBox* toBox = new QGroupBox("To (Credits)");
    QVBoxLayout* toL = new QVBoxLayout(toBox);

    auto addToLine = [&]() {
        QHBoxLayout* line = new QHBoxLayout;
        QComboBox* accountCombo = new QComboBox;
        QList<Account> accts = dbManager->getAllAccounts();
        for (const Account& a : accts) accountCombo->addItem(a.name + " (" + a.code + ")", a.id);

        QDoubleSpinBox* amt = new QDoubleSpinBox;
        amt->setRange(0, 9999999.99);
        amt->setDecimals(2);
        amt->setPrefix("$ ");
        amt->setValue(0.0);

        toSpins.append(amt);
        connect(amt, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateReconcile);

        QComboBox* nat = new QComboBox; nat->addItems({"Contributions","Salaries & Housing","Utilities","Supplies","Travel","Other"});
        QComboBox* func = new QComboBox; func->addItems({"Program Services","Management & General","Fundraising"});

        line->addWidget(accountCombo);
        line->addWidget(amt);
        line->addWidget(nat);
        line->addWidget(func);
        toL->addLayout(line);
        updateReconcile();
    };

    QPushButton* addToBtn = new QPushButton("+ Add To Line");
    connect(addToBtn, &QPushButton::clicked, addToLine);
    toL->addWidget(addToBtn);
    addToLine(); // start with one
    mainL->addWidget(toBox);

    // Buttons
    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainL->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    while (true) {
        if (dlg.exec() != QDialog::Accepted) break;

        if (descEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Missing", "Description required.");
            continue;
        }

        double fromSum = 0.0, toSum = 0.0;
        for (auto* s : fromSpins) fromSum += s->value();
        for (auto* s : toSpins) toSum += s->value();
        if (qAbs(toSum - fromSum) > 0.01) {
            QMessageBox::warning(this, "Unbalanced", "Transaction must balance to $0.00");
            continue;
        }

        // Save header
        QString txId = dbManager->generateNextTransactionId();
        QSqlQuery header(dbManager->db);
        header.prepare("INSERT INTO transactions (id, date, description, total_amount, payee_donor, reference, approved_by) VALUES (?, ?, ?, ?, ?, ?, ?)");
        header.addBindValue(txId);
        header.addBindValue(dateEdit->date().toString(Qt::ISODate));
        header.addBindValue(descEdit->text().trimmed());
        header.addBindValue(toSum);
        header.addBindValue(payeeEdit->text().trimmed());
        header.addBindValue(refEdit->text().trimmed());
        header.addBindValue(approvedEdit->text().trimmed());
        header.exec();

        QMessageBox::information(this, "✅ Success", "Transaction " + txId + " saved!");
        refreshAll();
        break;
    }
}

void MainWindow::onManageLookups() {
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Lookups");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QTabWidget* tabs = new QTabWidget;

    // === FUNDS TAB ===
    QWidget* fundsTab = new QWidget;
    QVBoxLayout* fundsL = new QVBoxLayout(fundsTab);

    QTableView* fundsTable = new QTableView;
    QStandardItemModel* fundsModel = new QStandardItemModel(this);
    fundsModel->setHorizontalHeaderLabels({"ID", "Name", "Description", "Donor Restricted"});
    fundsTable->setModel(fundsModel);

    refreshFundTable(fundsModel);

    fundsTable->resizeColumnsToContents();
    fundsTable->horizontalHeader()->setStretchLastSection(true);
    fundsTable->setColumnWidth(0, 60);
    fundsTable->setColumnWidth(1, 200);
    fundsTable->setColumnWidth(2, 350);
    fundsTable->setColumnWidth(3, 120);
    fundsTable->setMinimumWidth(700);
    fundsTable->setAlternatingRowColors(true);

    fundsL->addWidget(fundsTable);

    QHBoxLayout* fundsBtnL = new QHBoxLayout;
    QPushButton* fundsAddBtn = new QPushButton("Add");
    QPushButton* fundsEditBtn = new QPushButton("Edit");
    QPushButton* fundsDelBtn = new QPushButton("Delete");
    fundsBtnL->addWidget(fundsAddBtn);
    fundsBtnL->addWidget(fundsEditBtn);
    fundsBtnL->addWidget(fundsDelBtn);
    fundsL->addLayout(fundsBtnL);

    connect(fundsAddBtn, &QPushButton::clicked, this, [this, fundsModel]() { showFundDialog(false, -1, fundsModel); });
    connect(fundsEditBtn, &QPushButton::clicked, this, [this, fundsTable, fundsModel]() {
        QModelIndex idx = fundsTable->currentIndex();
        if (idx.isValid()) {
            int fundId = fundsModel->item(idx.row(), 0)->text().toInt();
            showFundDialog(true, fundId, fundsModel);
        }
    });
    connect(fundsDelBtn, &QPushButton::clicked, this, [this, fundsTable, fundsModel]() {
        QModelIndex idx = fundsTable->currentIndex();
        if (idx.isValid() && QMessageBox::question(this, "Delete", "Delete this fund?") == QMessageBox::Yes) {
            int fundId = fundsModel->item(idx.row(), 0)->text().toInt();
            QSqlQuery query(dbManager->db);
            query.prepare("DELETE FROM funds WHERE id = ?");
            query.addBindValue(fundId);
            if (query.exec()) {
                refreshFundTable(fundsModel);
            }
        }
    });

    tabs->addTab(fundsTab, "Funds");

    // === ACCOUNTS TAB ===
    QWidget* accountsTab = new QWidget;
    QVBoxLayout* accountsL = new QVBoxLayout(accountsTab);

    QTableView* accountsTable = new QTableView;
    QStandardItemModel* accountsModel = new QStandardItemModel(this);
    accountsModel->setHorizontalHeaderLabels({"ID", "Code", "Name", "Type", "Fund"});
    accountsTable->setModel(accountsModel);

    refreshAccountsTable(accountsModel);

    accountsTable->resizeColumnsToContents();
    accountsTable->horizontalHeader()->setStretchLastSection(true);
    accountsTable->setColumnWidth(0, 60);
    accountsTable->setColumnWidth(1, 100);
    accountsTable->setColumnWidth(2, 250);
    accountsTable->setColumnWidth(3, 150);
    accountsTable->setColumnWidth(4, 150);
    accountsTable->setMinimumWidth(1200);
    accountsTable->setAlternatingRowColors(true);

    accountsL->addWidget(accountsTable);

    QHBoxLayout* accountsBtnL = new QHBoxLayout;
    QPushButton* accountsAddBtn = new QPushButton("Add");
    QPushButton* accountsEditBtn = new QPushButton("Edit");
    QPushButton* accountsDelBtn = new QPushButton("Delete");
    accountsBtnL->addWidget(accountsAddBtn);
    accountsBtnL->addWidget(accountsEditBtn);
    accountsBtnL->addWidget(accountsDelBtn);
    accountsL->addLayout(accountsBtnL);

    connect(accountsAddBtn, &QPushButton::clicked, this, [this, accountsModel]() { 
        showAccountDialog(false, -1, accountsModel); 
    });
    connect(accountsEditBtn, &QPushButton::clicked, this, [this, accountsTable, accountsModel]() {
        QModelIndex idx = accountsTable->currentIndex();
        if (idx.isValid()) {
            int accountId = accountsModel->item(idx.row(), 0)->text().toInt();
            showAccountDialog(true, accountId, accountsModel);
        }
    });
    connect(accountsDelBtn, &QPushButton::clicked, this, [this, accountsTable, accountsModel]() {
        QModelIndex idx = accountsTable->currentIndex();
        if (idx.isValid() && QMessageBox::question(this, "Delete", "Delete this account?") == QMessageBox::Yes) {
            int accountId = accountsModel->item(idx.row(), 0)->text().toInt();
            QSqlQuery query(dbManager->db);
            query.prepare("DELETE FROM accounts WHERE id = ?");
            query.addBindValue(accountId);
            if (query.exec()) {
                refreshAccountsTable(accountsModel);
            }
        }
    });

    tabs->addTab(accountsTab, "Accounts");

    // === NATURAL CLASSES TAB ===
    QWidget* naturalTab = new QWidget;
    QVBoxLayout* naturalL = new QVBoxLayout(naturalTab);

    QTableView* naturalTable = new QTableView;
    QStandardItemModel* naturalModel = new QStandardItemModel(this);
    naturalModel->setHorizontalHeaderLabels({"ID", "Name"});
    naturalTable->setModel(naturalModel);
    naturalTable->setAlternatingRowColors(true);
    naturalL->addWidget(naturalTable);

    refreshSimpleLookupTable(naturalModel, "natural_classes");

    QHBoxLayout* naturalBtnL = new QHBoxLayout;
    QPushButton* naturalAddBtn = new QPushButton("Add");
    QPushButton* naturalEditBtn = new QPushButton("Edit");
    QPushButton* naturalDelBtn = new QPushButton("Delete");
    naturalBtnL->addWidget(naturalAddBtn);
    naturalBtnL->addWidget(naturalEditBtn);
    naturalBtnL->addWidget(naturalDelBtn);
    naturalL->addLayout(naturalBtnL);

    connect(naturalAddBtn, &QPushButton::clicked, this, [this, naturalModel]() {
        showSimpleLookupDialog("Add Natural Class", naturalModel, "natural_classes");
    });
    connect(naturalEditBtn, &QPushButton::clicked, this, [this, naturalTable, naturalModel]() {
        QModelIndex idx = naturalTable->currentIndex();
        if (idx.isValid()) {
            int id = naturalModel->item(idx.row(), 0)->text().toInt();
            showSimpleLookupDialog("Edit Natural Class", naturalModel, "natural_classes", id);
        }
    });
    connect(naturalDelBtn, &QPushButton::clicked, this, [this, naturalTable, naturalModel]() {
        QModelIndex idx = naturalTable->currentIndex();
        if (idx.isValid() && QMessageBox::question(this, "Delete", "Delete this natural class?") == QMessageBox::Yes) {
            int id = naturalModel->item(idx.row(), 0)->text().toInt();
            QSqlQuery query(dbManager->db);
            query.prepare("DELETE FROM natural_classes WHERE id = ?");
            query.addBindValue(id);
            if (query.exec()) refreshSimpleLookupTable(naturalModel, "natural_classes");
        }
    });

    tabs->addTab(naturalTab, "Natural Classes");

    // === FUNCTIONAL CLASSES TAB ===
    QWidget* functionalTab = new QWidget;
    QVBoxLayout* functionalL = new QVBoxLayout(functionalTab);

    QTableView* functionalTable = new QTableView;
    QStandardItemModel* functionalModel = new QStandardItemModel(this);
    functionalModel->setHorizontalHeaderLabels({"ID", "Name"});
    functionalTable->setModel(functionalModel);
    functionalTable->setAlternatingRowColors(true);
    functionalL->addWidget(functionalTable);

    refreshSimpleLookupTable(functionalModel, "functional_classes");

    QHBoxLayout* functionalBtnL = new QHBoxLayout;
    QPushButton* functionalAddBtn = new QPushButton("Add");
    QPushButton* functionalEditBtn = new QPushButton("Edit");
    QPushButton* functionalDelBtn = new QPushButton("Delete");
    functionalBtnL->addWidget(functionalAddBtn);
    functionalBtnL->addWidget(functionalEditBtn);
    functionalBtnL->addWidget(functionalDelBtn);
    functionalL->addLayout(functionalBtnL);

    connect(functionalAddBtn, &QPushButton::clicked, this, [this, functionalModel]() {
        showSimpleLookupDialog("Add Functional Class", functionalModel, "functional_classes");
    });
    connect(functionalEditBtn, &QPushButton::clicked, this, [this, functionalTable, functionalModel]() {
        QModelIndex idx = functionalTable->currentIndex();
        if (idx.isValid()) {
            int id = functionalModel->item(idx.row(), 0)->text().toInt();
            showSimpleLookupDialog("Edit Functional Class", functionalModel, "functional_classes", id);
        }
    });
    connect(functionalDelBtn, &QPushButton::clicked, this, [this, functionalTable, functionalModel]() {
        QModelIndex idx = functionalTable->currentIndex();
        if (idx.isValid() && QMessageBox::question(this, "Delete", "Delete this functional class?") == QMessageBox::Yes) {
            int id = functionalModel->item(idx.row(), 0)->text().toInt();
            QSqlQuery query(dbManager->db);
            query.prepare("DELETE FROM functional_classes WHERE id = ?");
            query.addBindValue(id);
            if (query.exec()) refreshSimpleLookupTable(functionalModel, "functional_classes");
        }
    });

    tabs->addTab(functionalTab, "Functional Classes");

    layout->addWidget(tabs);
    dlg.resize(1000, 650);

    QDialogButtonBox* closeBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(closeBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(closeBox);

    dlg.exec();
}

void MainWindow::onManageFunds() {
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Funds");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QTableView* table = new QTableView;
    QStandardItemModel* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"ID", "Name", "Description", "Restricted"});
    table->setModel(model);

    refreshFundTable(model);

    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);

    table->setColumnWidth(0, 60);   // ID (narrow)
    table->setColumnWidth(1, 200);  // Name
    table->setColumnWidth(2, 820);  // Description (largest)
    table->setColumnWidth(3, 120);  // Donor Restricted (Yes/No)

    table->setMinimumWidth(1200);

    layout->addWidget(table);

    QHBoxLayout* btnL = new QHBoxLayout;
    QPushButton* addBtn = new QPushButton("Add");
    QPushButton* editBtn = new QPushButton("Edit");
    QPushButton* delBtn = new QPushButton("Delete");
    btnL->addWidget(addBtn);
    btnL->addWidget(editBtn);
    btnL->addWidget(delBtn);
    layout->addLayout(btnL);

    connect(addBtn, &QPushButton::clicked, this, [this, model]() { 
        showFundDialog(false, -1, model); 
    });
    connect(editBtn, &QPushButton::clicked, this, [this, table, model]() {
        QModelIndex idx = table->currentIndex();
        if (idx.isValid()) {
            int fundId = model->item(idx.row(), 0)->text().toInt();
            showFundDialog(true, fundId, model);
        }
    });
    connect(delBtn, &QPushButton::clicked, this, [this, table, model]() {
        QModelIndex idx = table->currentIndex();
        if (idx.isValid() && QMessageBox::question(this, "Delete", "Delete this fund?") == QMessageBox::Yes) {
            int fundId = model->item(idx.row(), 0)->text().toInt();
            QSqlQuery query(dbManager->db);
            query.prepare("DELETE FROM funds WHERE id = ?");
            query.addBindValue(fundId);
            if (query.exec()) {
                refreshFundTable(model);
            }
        }
    });

    dlg.adjustSize();   // auto-size dialog to content
    dlg.exec();
}

void MainWindow::showFundDialog(bool isEdit, int fundId, QStandardItemModel* model) {
    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? "Edit Fund" : "Add New Fund");
    QFormLayout* form = new QFormLayout(&dlg);
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setProperty("tabChangesFocus", true);
    nameEdit->setMinimumWidth(300);


    QTextEdit* descEdit = new QTextEdit;
    descEdit->setProperty("tabChangesFocus", true);
    descEdit->setMinimumWidth(300);
    descEdit->setMaximumHeight(100);


    QCheckBox* restrictedCheck = new QCheckBox("Donor Restricted (WDR)");

    form->addRow("Name", nameEdit);
    form->addRow("Description", descEdit);
    form->addRow("", restrictedCheck);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (isEdit) {
        QSqlQuery query(dbManager->db);
        query.prepare("SELECT name, description, restriction_type FROM funds WHERE id = ?");
        query.addBindValue(fundId);
        if (query.exec() && query.next()) {
            nameEdit->setText(query.value(0).toString());
            descEdit->setPlainText(query.value(1).toString());
            restrictedCheck->setChecked(query.value(2).toString() == "WDR");
        }
    }

    if (dlg.exec() == QDialog::Accepted) {
        QString restriction = restrictedCheck->isChecked() ? "WDR" : "WODR";

        QSqlQuery query(dbManager->db);
        if (isEdit) {
            query.prepare("UPDATE funds SET name = ?, description = ?, restriction_type = ? WHERE id = ?");
            query.addBindValue(nameEdit->text());
            query.addBindValue(descEdit->toPlainText());
            query.addBindValue(restriction);
            query.addBindValue(fundId);
        } else {
            query.prepare("INSERT INTO funds (name, description, restriction_type) VALUES (?, ?, ?)");
            query.addBindValue(nameEdit->text());
            query.addBindValue(descEdit->toPlainText());
            query.addBindValue(restriction);
        }

        if (query.exec()) {
            refreshFundTable(model);
        } else {
            QMessageBox::critical(this, "Error", "Save failed.");
        }
    }
}

void MainWindow::refreshFundTable(QStandardItemModel* model) {
    model->removeRows(0, model->rowCount());
    QList<Fund> funds = dbManager->getAllFunds();
    for (const Fund& f : funds) {
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(f.id))
            << new QStandardItem(f.name)
            << new QStandardItem(f.description)
            << new QStandardItem(f.restrictionType == "WDR" ? "Yes" : "No");
        model->appendRow(row);
    }
}

void MainWindow::onManageAccounts() {
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Chart of Accounts");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QTableView* table = new QTableView;
    QStandardItemModel* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"ID", "Code", "Name", "Type", "Fund"});
    table->setModel(model);

    refreshAccountsTable(model);

    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);

    table->setColumnWidth(0, 60);   // ID
    table->setColumnWidth(1, 100);  // Code
    table->setColumnWidth(2, 250);  // Name
    table->setColumnWidth(3, 150);  // Type
    table->setColumnWidth(4, 150);  // Fund

    table->setMinimumWidth(1200);

    layout->addWidget(table);

    QHBoxLayout* btnL = new QHBoxLayout;
    QPushButton* addBtn = new QPushButton("Add");
    QPushButton* editBtn = new QPushButton("Edit");
    QPushButton* delBtn = new QPushButton("Delete");
    btnL->addWidget(addBtn);
    btnL->addWidget(editBtn);
    btnL->addWidget(delBtn);
    layout->addLayout(btnL);

    connect(addBtn, &QPushButton::clicked, this, [this, model]() { 
        showAccountDialog(false, -1, model); 
    });
    connect(editBtn, &QPushButton::clicked, this, [this, table, model]() {
        QModelIndex idx = table->currentIndex();
        if (idx.isValid()) {
            int accountId = model->item(idx.row(), 0)->text().toInt();
            showAccountDialog(true, accountId, model);
        }
    });
    connect(delBtn, &QPushButton::clicked, this, [this, table, model]() {
        QModelIndex idx = table->currentIndex();
        if (idx.isValid() && QMessageBox::question(this, "Delete", "Delete this account?") == QMessageBox::Yes) {
            int accountId = model->item(idx.row(), 0)->text().toInt();
            QSqlQuery query(dbManager->db);
            query.prepare("DELETE FROM accounts WHERE id = ?");
            query.addBindValue(accountId);
            if (query.exec()) {
                refreshAccountsTable(model);
            }
        }
    });

    dlg.exec();
}

void MainWindow::showAccountDialog(bool isEdit, int accountId, QStandardItemModel* model) {
    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? "Edit Account" : "Add New Account");
    QFormLayout* form = new QFormLayout(&dlg);

    QLineEdit* codeEdit = new QLineEdit;
    codeEdit->setProperty("tabChangesFocus", true);
    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setProperty("tabChangesFocus", true);
    QComboBox* typeCombo = new QComboBox;
    typeCombo->addItems({"Asset", "Liability", "NetAsset", "Revenue", "Expense"});

    QComboBox* fundCombo = new QComboBox;
    populateFundsCombo(fundCombo);              // your existing function
    fundCombo->setCurrentIndex(0);              // force "(None)" as default

    form->addRow("Code", codeEdit);
    form->addRow("Name", nameEdit);
    form->addRow("Type", typeCombo);
    form->addRow("Fund (optional)", fundCombo);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (isEdit) {
        QSqlQuery query(dbManager->db);
        query.prepare("SELECT code, name, type, fund_id FROM accounts WHERE id = ?");
        query.addBindValue(accountId);
        if (query.exec() && query.next()) {
            codeEdit->setText(query.value(0).toString());
            nameEdit->setText(query.value(1).toString());
            typeCombo->setCurrentText(query.value(2).toString());
            fundCombo->setCurrentIndex(fundCombo->findData(query.value(3).toInt()));
        }
    }

    if (dlg.exec() == QDialog::Accepted) {
        QSqlQuery query(dbManager->db);
        if (isEdit) {
            query.prepare("UPDATE accounts SET code = ?, name = ?, type = ?, fund_id = ? WHERE id = ?");
            query.addBindValue(codeEdit->text());
            query.addBindValue(nameEdit->text());
            query.addBindValue(typeCombo->currentText());
            query.addBindValue(fundCombo->currentData().toInt());
            query.addBindValue(accountId);
        } else {
            query.prepare("INSERT INTO accounts (code, name, type, fund_id) VALUES (?, ?, ?, ?)");
            query.addBindValue(codeEdit->text());
            query.addBindValue(nameEdit->text());
            query.addBindValue(typeCombo->currentText());
            query.addBindValue(fundCombo->currentData().toInt());
        }

        if (query.exec()) {
            refreshAccountsTable(model);
        } else {
            QMessageBox::critical(this, "Error", "Save failed.");
        }
    }
}

void MainWindow::refreshAccountsTable(QStandardItemModel* model) {
    model->removeRows(0, model->rowCount());
    QList<Account> accounts = dbManager->getAllAccounts();
    for (const Account& a : accounts) {
        QString fundName = (a.fundId > 0) ? dbManager->getFundName(a.fundId) : "(None)";

        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(a.id))
            << new QStandardItem(a.code)
            << new QStandardItem(a.name)
            << new QStandardItem(a.type)
            << new QStandardItem(fundName);
        model->appendRow(row);
    }
}

void MainWindow::refreshAll() {
    double wodr = dbManager->getWODRTotal();
    double wdr  = dbManager->getWDRTotal();
    wodrLabel->setText(QString("$%1").arg(wodr, 0, 'f', 2));
    wdrLabel->setText(QString("$%1").arg(wdr, 0, 'f', 2));
    totalLabel->setText(QString("$%1").arg(wodr + wdr, 0, 'f', 2));

    auto txs = dbManager->getAllTransactions();
    QStandardItemModel* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"Date", "Description", "Amount", "Fund", "Natural", "Functional", "Type"});
    for (int i = 0; i < qMin(15, txs.size()); ++i) {
        const auto& t = txs[i];
        QList<QStandardItem*> row;
        row << new QStandardItem(t.date.toString("yyyy-MM-dd"))
            << new QStandardItem(t.description)
            << new QStandardItem(QString("$%1").arg(t.amount, 0, 'f', 2))
            << new QStandardItem(getFundName(t.fundId))
            << new QStandardItem(t.naturalClass)
            << new QStandardItem(t.functionalClass)
            << new QStandardItem(t.transactionType);
        model->appendRow(row);
    }
    recentTxTable->setModel(model);
    recentTxTable->resizeColumnsToContents();
}

void MainWindow::populateFundsCombo(QComboBox* combo) {
    combo->clear();
    combo->addItem("", QVariant());   // default blank option (null fund_id)
    auto funds = dbManager->getAllFunds();
    qDebug() << "Populating" << funds.size() << "funds in dropdown";
    for (const auto& f : funds) {
        combo->addItem(f.name, f.id);
    }
}

QString MainWindow::getFundName(int fundId) const {
    auto funds = dbManager->getAllFunds();
    for (const auto& f : funds) {
        if (f.id == fundId) return f.name;
    }
    return "Unknown";
}

void MainWindow::showSimpleLookupDialog(const QString& title, QStandardItemModel* model, const QString& tableName, int id) {
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    QFormLayout* form = new QFormLayout(&dlg);

    QLineEdit* nameEdit = new QLineEdit;
    form->addRow("Name", nameEdit);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (id > 0) {
        QSqlQuery query(dbManager->db);
        query.prepare(QString("SELECT name FROM %1 WHERE id = ?").arg(tableName));
        query.addBindValue(id);
        if (query.exec() && query.next()) {
            nameEdit->setText(query.value(0).toString());
        }
    }

    if (dlg.exec() == QDialog::Accepted) {
        QSqlQuery query(dbManager->db);
        if (id > 0) {
            query.prepare(QString("UPDATE %1 SET name = ? WHERE id = ?").arg(tableName));
            query.addBindValue(nameEdit->text().trimmed());
            query.addBindValue(id);
        } else {
            query.prepare(QString("INSERT INTO %1 (name) VALUES (?)").arg(tableName));
            query.addBindValue(nameEdit->text().trimmed());
        }
        if (query.exec()) {
            refreshSimpleLookupTable(model, tableName);
        }
    }
}

void MainWindow::refreshSimpleLookupTable(QStandardItemModel* model, const QString& tableName) {
    model->removeRows(0, model->rowCount());
    QSqlQuery query(dbManager->db);
    query.exec(QString("SELECT id, name FROM %1").arg(tableName));
    while (query.next()) {
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(query.value(0).toInt()))
            << new QStandardItem(query.value(1).toString());
        model->appendRow(row);
    }
}

void MainWindow::refreshFundsPage() {}