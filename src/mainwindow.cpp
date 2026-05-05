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
#include <QSqlError>

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
    qDebug() << "New Transaction Dialog opened";

    QDialog dlg(this);
    dlg.setWindowTitle("New Transaction - Hope Baptist Church");
    dlg.resize(1280, 680);
    dlg.setMinimumSize(1280, 680);

    QVBoxLayout* mainL = new QVBoxLayout(&dlg);
    mainL->setContentsMargins(16, 16, 16, 16);
    mainL->setSpacing(12);

    // ====================== HEADER ======================
    QHBoxLayout* headerL = new QHBoxLayout;
    QFormLayout* left = new QFormLayout;
    QDateEdit* dateEdit = new QDateEdit(QDate::currentDate());
    dateEdit->setCalendarPopup(true);
    QLineEdit* descEdit = new QLineEdit;
    QLineEdit* payeeEdit = new QLineEdit;
    QLineEdit* approvedEdit = new QLineEdit;

    left->addRow("Date", dateEdit);
    left->addRow("Description", descEdit);
    left->addRow("Payee/Donor", payeeEdit);
    left->addRow("Approved By", approvedEdit);
    headerL->addLayout(left);

    QFormLayout* right = new QFormLayout;
    QLineEdit* refEdit = new QLineEdit;

    QComboBox* statusCombo = new QComboBox;
    statusCombo->addItems({"", "C - Cleared", "R - Reconciled"});

    right->addRow("Reference #", refEdit);
    right->addRow("Status", statusCombo);
    headerL->addLayout(right);
    mainL->addLayout(headerL);

    // ====================== TRANSACTION LINES TABLE ======================
    QTableWidget* txTable = new QTableWidget(0, 9);
    txTable->setHorizontalHeaderLabels({
        "Line", "Account", "Fund", "Type", "Natural Class",
        "Functional Class", "Memo/Note", "Debit", "Credit"
    });
    txTable->setAlternatingRowColors(true);
    txTable->horizontalHeader()->setStretchLastSection(false);

    txTable->setColumnWidth(0, 50);
    txTable->setColumnWidth(1, 220);
    txTable->setColumnWidth(2, 180);   // Fund column
    txTable->setColumnWidth(3, 110);
    txTable->setColumnWidth(4, 150);
    txTable->setColumnWidth(5, 150);
    txTable->setColumnWidth(7, 100);
    txTable->setColumnWidth(8, 100);
    txTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch); // Memo

    mainL->addWidget(txTable);

    QPushButton* addBtn = new QPushButton("+ Add Line");
    mainL->addWidget(addBtn);

    QLabel* reconcileLabel = new QLabel("Reconcile: $0.00");
    reconcileLabel->setAlignment(Qt::AlignRight);
    mainL->addWidget(reconcileLabel);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainL->addWidget(btns);

    // ====================== RECONCILE HELPER ======================
    auto updateReconcile = [&]() {
        double debits = 0.0, credits = 0.0;
        for (int r = 0; r < txTable->rowCount(); ++r) {
            debits += txTable->item(r, 7) ? txTable->item(r, 7)->text().toDouble() : 0.0;
            credits += txTable->item(r, 8) ? txTable->item(r, 8)->text().toDouble() : 0.0;
        }
        double diff = debits - credits;
        reconcileLabel->setText(QString("Reconcile: $%1").arg(diff, 0, 'f', 2));
        reconcileLabel->setStyleSheet(qAbs(diff) < 0.01 ?
            "color: green; font-weight: bold;" : "color: red; font-weight: bold;");
    };

    // ====================== ADD LINE ======================
    connect(addBtn, &QPushButton::clicked, this, [&]() {
        int row = txTable->rowCount();
        txTable->insertRow(row);

        txTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        txTable->setItem(row, 6, new QTableWidgetItem(""));   // Memo
        txTable->setItem(row, 7, new QTableWidgetItem("0.00")); // Debit
        txTable->setItem(row, 8, new QTableWidgetItem("0.00")); // Credit

        // Account Combo
        QComboBox* acctCombo = new QComboBox;
        acctCombo->addItem("(Select Account)", -1);
        for (const Account& a : dbManager->getAllAccounts()) {
            acctCombo->addItem(a.name + " (" + a.code + ")", QVariant::fromValue(a));
        }
        txTable->setCellWidget(row, 1, acctCombo);

        // Fund Combo (NEW)
        QComboBox* fundCombo = new QComboBox;
        fundCombo->addItem("(Select Fund)", -1);
        for (const auto& f : dbManager->getAllFunds()) {   // You'll need this method
            fundCombo->addItem(f.name, QVariant::fromValue(f.id));
        }
        txTable->setCellWidget(row, 2, fundCombo);

        // Natural & Functional Class Combos
        QComboBox* natCombo = new QComboBox; natCombo->addItem("(None)");
        QSqlQuery natQ(dbManager->db); natQ.exec("SELECT name FROM natural_classes ORDER BY name");
        while (natQ.next()) natCombo->addItem(natQ.value(0).toString());
        txTable->setCellWidget(row, 4, natCombo);

        QComboBox* funcCombo = new QComboBox; funcCombo->addItem("(None)");
        QSqlQuery funcQ(dbManager->db); funcQ.exec("SELECT name FROM functional_classes ORDER BY name");
        while (funcQ.next()) funcCombo->addItem(funcQ.value(0).toString());
        txTable->setCellWidget(row, 5, funcCombo);

        // Account type update lambda
        connect(acctCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [txTable, row, updateReconcile](int idx) {
                if (idx <= 0) {
                    txTable->setItem(row, 3, new QTableWidgetItem(""));
                    updateReconcile();
                    return;
                }
                QComboBox* combo = qobject_cast<QComboBox*>(txTable->cellWidget(row, 1));
                Account a = qvariant_cast<Account>(combo->currentData());

                QString typeStr = a.type;
                if (a.type == "Asset" || a.type == "Expense")
                    typeStr += " (D)";
                else
                    typeStr += " (C)";

                txTable->setItem(row, 3, new QTableWidgetItem(typeStr));
                updateReconcile();
            });

        updateReconcile();
    });

    connect(txTable, &QTableWidget::itemChanged, this, updateReconcile);

    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // ====================== SAVE LOOP ======================
    while (true) {
        if (dlg.exec() != QDialog::Accepted) break;

        if (descEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Required", "Description is required.");
            continue;
        }

        // Balance check
        double debits = 0.0, credits = 0.0;
        for (int r = 0; r < txTable->rowCount(); ++r) {
            debits += txTable->item(r, 7) ? txTable->item(r, 7)->text().toDouble() : 0.0;
            credits += txTable->item(r, 8) ? txTable->item(r, 8)->text().toDouble() : 0.0;
        }
        if (qAbs(debits - credits) > 0.01) {
            QMessageBox::warning(this, "Unbalanced", "Debits must equal Credits.");
            continue;
        }

        QString userReference = refEdit->text().trimmed();
        if (userReference.isEmpty()) {
            userReference = "REF-" + QDate::currentDate().toString("yyMMdd");
        }

        // ====================== SAVE LOGIC ======================
        if (!dbManager->db.transaction()) {
            QMessageBox::critical(this, "DB Error", "Failed to start transaction");
            continue;
        }

        bool success = false;
        int linesSaved = 0;
        qlonglong txId = 0;

        // Save Header
        QSqlQuery header(dbManager->db);
        header.prepare(R"(
            INSERT INTO transactions 
            (date, description, total_amount, payee_donor, reference, approved_by)
            VALUES (?, ?, ?, ?, ?, ?)
        )");

        header.addBindValue(dateEdit->date().toString(Qt::ISODate));
        header.addBindValue(descEdit->text().trimmed());
        header.addBindValue(debits);
        header.addBindValue(payeeEdit->text().trimmed());
        header.addBindValue(userReference);
        header.addBindValue(approvedEdit->text().trimmed());

        if (!header.exec()) {
            QMessageBox::critical(this, "Save Error", "Failed to save header:\n" + header.lastError().text());
            dbManager->db.rollback();
            continue;
        }

        txId = header.lastInsertId().toLongLong();
        if (txId <= 0) {
            QMessageBox::critical(this, "Error", "Failed to retrieve transaction ID");
            dbManager->db.rollback();
            continue;
        }

        qDebug() << "✅ Generated Transaction ID:" << txId;

        // Handle Status
        QString statusText = statusCombo->currentText();
        int cleared = 0, reconciled = 0;
        QString clearedDate, reconciledDate;

        if (statusText.startsWith("C")) {
            cleared = 1; clearedDate = QDate::currentDate().toString(Qt::ISODate);
        } else if (statusText.startsWith("R")) {
            reconciled = 1; reconciledDate = QDate::currentDate().toString(Qt::ISODate);
        }

        QSqlQuery statusQ(dbManager->db);
        statusQ.prepare(R"(
            UPDATE transactions SET cleared = ?, cleared_date = ?,
                reconciled = ?, reconciled_date = ? WHERE id = ?
        )");
        statusQ.addBindValue(cleared);
        statusQ.addBindValue(clearedDate);
        statusQ.addBindValue(reconciled);
        statusQ.addBindValue(reconciledDate);
        statusQ.addBindValue(txId);
        statusQ.exec();

        // Save Lines (with real Fund selection)
        for (int r = 0; r < txTable->rowCount(); ++r) {
            QComboBox* acctCombo = qobject_cast<QComboBox*>(txTable->cellWidget(r, 1));
            QComboBox* fundCombo = qobject_cast<QComboBox*>(txTable->cellWidget(r, 2));

            if (!acctCombo || acctCombo->currentIndex() <= 0) continue;
            if (!fundCombo || fundCombo->currentIndex() <= 0) {
                QMessageBox::warning(this, "Missing Fund", "Please select a Fund for every line.");
                dbManager->db.rollback();
                goto save_failed;   // simple way to break out
            }

            Account a = qvariant_cast<Account>(acctCombo->currentData());
            int fundId = fundCombo->currentData().toInt();

            QString nat = "", func = "";
            if (QComboBox* natC = qobject_cast<QComboBox*>(txTable->cellWidget(r, 4)))
                if (natC->currentIndex() > 0) nat = natC->currentText();
            if (QComboBox* funcC = qobject_cast<QComboBox*>(txTable->cellWidget(r, 5)))
                if (funcC->currentIndex() > 0) func = funcC->currentText();

            double debitAmt = txTable->item(r, 7) ? txTable->item(r, 7)->text().toDouble() : 0.0;
            double creditAmt = txTable->item(r, 8) ? txTable->item(r, 8)->text().toDouble() : 0.0;
            QString memo = txTable->item(r, 6) ? txTable->item(r, 6)->text().trimmed() : "";

            double finalAmount = debitAmt - creditAmt;

            QSqlQuery line(dbManager->db);
            line.prepare(R"(
                INSERT INTO transaction_lines 
                (transaction_id, account_id, fund_id, amount, natural_class, functional_class, notes)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            )");

            line.addBindValue(txId);
            line.addBindValue(a.id);
            line.addBindValue(fundId);
            line.addBindValue(finalAmount);
            line.addBindValue(nat);
            line.addBindValue(func);
            line.addBindValue(memo);

            if (line.exec()) {
                linesSaved++;
            } else {
                qDebug() << "Line" << r << "failed:" << line.lastError().text();
            }
        }

        if (linesSaved == 0) {
            QMessageBox::warning(this, "Warning", "No valid lines were saved.");
            dbManager->db.rollback();
            continue;
        }

        if (dbManager->db.commit()) {
            success = true;
        } else {
            dbManager->db.rollback();
            continue;
        }

        QMessageBox::information(this, "✅ Success",
            QString("Transaction saved!\n\nID: %1\nReference: %2\nLines saved: %3")
                .arg(QString::number(txId), userReference, QString::number(linesSaved)));

        refreshAll();
        break;

    save_failed:
        continue;
    } // end while(true)
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
            << new QStandardItem(f.restriction_type == "WDR" ? "Yes" : "No");
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

        QString fundName = (t.fundId > 0) 
            ? getFundName(t.fundId) 
            : "";                     // Blank for no fund

        QList<QStandardItem*> row;
        row << new QStandardItem(t.date.toString("yyyy-MM-dd"))
            << new QStandardItem(t.description)
            << new QStandardItem(QString("$%1").arg(t.amount, 0, 'f', 2))
            << new QStandardItem(fundName)           // ← Fixed
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