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
#include <QFileDialog>
#include <QFileInfo>

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

    // Sidebar items - exact order you want
    sidebar->addItem("Dashboard");        // index 0
    sidebar->addItem("New Transaction");  // index 1
    sidebar->addItem("Manage Lookups");   // index 2
    sidebar->addItem("Transactions");     // index 3
    sidebar->addItem("Funds");            // index 4

    connect(sidebar, &QListWidget::currentRowChanged, this, &MainWindow::switchPage);

    stack = new QStackedWidget;

    // Dashboard Page (index 0)
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

    // Placeholder for Manage Lookups (index 2)
    stack->addWidget(new QWidget());   // index 1 - will be replaced by onNewTransaction logic
    stack->addWidget(new QWidget());   // index 2 - Manage Lookups

    // Transactions Ledger will be inserted at index 3

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(sidebar);
    splitter->addWidget(stack);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
    setCentralWidget(central);

    sidebar->setCurrentRow(0);

    // ====================== FINAL SETUP ======================
    setupTransactionsPage();    // Insert at index 3
    loadTransactions();

    setupFundsBalancesPage();   // Insert at index 4
    loadFundBalances();
}

void MainWindow::switchPage(int index) {
    stack->setCurrentIndex(index);

    if (index == 1) {           // New Transaction
        onNewTransaction();
        sidebar->setCurrentRow(0);
    } 
    else if (index == 2) {      // Manage Lookups
        onManageLookups();
        sidebar->setCurrentRow(0);
    }
    else if (index == 3) {      // Transactions Ledger
        loadTransactions();     // Refresh the list when we switch to it
        // sidebar->setCurrentRow(0);  // optional
    }
    else if (index == 4) {           // Now Funds Balances
    loadFundBalances();
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

    QPushButton* attachBtn = new QPushButton("📎 Attach Document");
    QString attachedFilePath;   // Will hold the selected file path

    right->addRow("Reference #", refEdit);
    right->addRow("Status", statusCombo);
    right->addRow("", attachBtn);   // Attachment button
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
    txTable->setColumnWidth(2, 180);
    txTable->setColumnWidth(3, 110);
    txTable->setColumnWidth(4, 150);
    txTable->setColumnWidth(5, 150);
    txTable->setColumnWidth(7, 100);
    txTable->setColumnWidth(8, 100);
    txTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    mainL->addWidget(txTable);

    QPushButton* addBtn = new QPushButton("+ Add Line");
    mainL->addWidget(addBtn);

    QLabel* reconcileLabel = new QLabel("Reconcile: $0.00");
    reconcileLabel->setAlignment(Qt::AlignRight);
    mainL->addWidget(reconcileLabel);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainL->addWidget(btns);

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

    // ====================== ATTACH BUTTON ======================
    connect(attachBtn, &QPushButton::clicked, this, [&]() {
        QString file = QFileDialog::getOpenFileName(this, "Attach Document", "",
            "PDF Files (*.pdf);;Images (*.png *.jpg *.jpeg);;All Files (*)");
        if (!file.isEmpty()) {
            attachedFilePath = file;
            attachBtn->setText("📎 " + QFileInfo(file).fileName());
        }
    });

    // ====================== ADD LINE ======================
    connect(addBtn, &QPushButton::clicked, this, [&]() {
        int row = txTable->rowCount();
        txTable->insertRow(row);

        txTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        txTable->setItem(row, 6, new QTableWidgetItem(""));
        txTable->setItem(row, 7, new QTableWidgetItem("0.00"));
        txTable->setItem(row, 8, new QTableWidgetItem("0.00"));

        // Account Combo
        QComboBox* acctCombo = new QComboBox;
        acctCombo->addItem("(Select Account)", -1);
        for (const Account& a : dbManager->getAllAccounts(false)) {
            acctCombo->addItem(a.name + " (" + a.code + ")", QVariant::fromValue(a));
        }
        txTable->setCellWidget(row, 1, acctCombo);

        // Fund Combo
        QComboBox* fundCombo = new QComboBox;
        fundCombo->addItem("(Select Fund)", -1);
        for (const auto& f : dbManager->getAllFunds(false)) {
            fundCombo->addItem(f.name, QVariant::fromValue(f.id));
        }
        txTable->setCellWidget(row, 2, fundCombo);

        // Natural + Functional (using getters)
        QComboBox* natCombo = new QComboBox; natCombo->addItem("(None)");
        for (const SimpleLookup& n : dbManager->getAllNaturalClasses(false)) {
            natCombo->addItem(n.name);
        }
        txTable->setCellWidget(row, 4, natCombo);

        QComboBox* funcCombo = new QComboBox; funcCombo->addItem("(None)");
        for (const SimpleLookup& f : dbManager->getAllFunctionalClasses(false)) {
            funcCombo->addItem(f.name);
        }
        txTable->setCellWidget(row, 5, funcCombo);

        // Smart Fund Logic
        connect(acctCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [txTable, row, fundCombo](int idx) {
                if (idx <= 0) {
                    txTable->setItem(row, 3, new QTableWidgetItem(""));
                    return;
                }

                QComboBox* combo = qobject_cast<QComboBox*>(txTable->cellWidget(row, 1));
                Account a = qvariant_cast<Account>(combo->currentData());

                QString typeStr = a.type;
                if (a.type == "Asset" || a.type == "Liability") {
                    typeStr += " (D/C)";
                    fundCombo->setCurrentIndex(0);
                    fundCombo->setEnabled(false);
                } else {
                    typeStr += (a.type == "Expense" ? " (D)" : " (C)");
                    fundCombo->setEnabled(true);
                }

                txTable->setItem(row, 3, new QTableWidgetItem(typeStr));
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

        if (!dbManager->db.transaction()) {
            QMessageBox::critical(this, "DB Error", "Failed to start transaction");
            continue;
        }

        bool success = false;
        int linesSaved = 0;
        qlonglong txId = 0;

        // ====================== SAVE HEADER ======================
        QSqlQuery header(dbManager->db);
        header.prepare(R"(
            INSERT INTO transactions 
            (date, description, total_amount, payee_donor, reference, approved_by, attachment_path)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )");

        header.addBindValue(dateEdit->date().toString(Qt::ISODate));
        header.addBindValue(descEdit->text().trimmed());
        header.addBindValue(debits);
        header.addBindValue(payeeEdit->text().trimmed());
        header.addBindValue(userReference);
        header.addBindValue(approvedEdit->text().trimmed());
        header.addBindValue(attachedFilePath.isEmpty() ? QVariant() : attachedFilePath);  // NULL if no file

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

        // ====================== SAVE ATTACHMENT ======================
        if (!attachedFilePath.isEmpty()) {
            dbManager->saveAttachment(txId, attachedFilePath);
        }

        // Save Lines
        for (int r = 0; r < txTable->rowCount(); ++r) {
            QComboBox* acctCombo = qobject_cast<QComboBox*>(txTable->cellWidget(r, 1));
            QComboBox* fundCombo = qobject_cast<QComboBox*>(txTable->cellWidget(r, 2));

            if (!acctCombo || acctCombo->currentIndex() <= 0) continue;

            Account a = qvariant_cast<Account>(acctCombo->currentData());
            int fundId = (fundCombo && fundCombo->isEnabled() && fundCombo->currentIndex() > 0) 
                       ? fundCombo->currentData().toInt() : 0;

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
    }
}

void MainWindow::onManageLookups() {
    QDialog dlg(this);
    dlg.setWindowTitle("Manage Lookups");
    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QTabWidget* tabs = new QTabWidget;

    // ====================== FUNDS TAB ======================
    QWidget* fundsTab = new QWidget;
    QVBoxLayout* fundsL = new QVBoxLayout(fundsTab);

    QTableView* fundsTable = new QTableView;
    QStandardItemModel* fundsModel = new QStandardItemModel(this);
    fundsModel->setHorizontalHeaderLabels({"ID", "Name", "Description", "Restricted", "Archived", "Archived Date"});
    fundsTable->setModel(fundsModel);

    refreshFundTable(fundsModel);

    fundsTable->resizeColumnsToContents();
    fundsTable->horizontalHeader()->setStretchLastSection(true);
    fundsTable->setColumnWidth(0, 60);
    fundsTable->setColumnWidth(1, 180);
    fundsTable->setColumnWidth(2, 300);
    fundsTable->setColumnWidth(3, 100);
    fundsTable->setColumnWidth(4, 80);
    fundsTable->setColumnWidth(5, 140);

    fundsL->addWidget(fundsTable);

    QHBoxLayout* fundsBtnL = new QHBoxLayout;
    QPushButton* addBtn = new QPushButton("Add");
    QPushButton* editBtn = new QPushButton("Edit");
    QPushButton* archiveBtn = new QPushButton("Archive / Unarchive");
    fundsBtnL->addWidget(addBtn);
    fundsBtnL->addWidget(editBtn);
    fundsBtnL->addWidget(archiveBtn);
    fundsL->addLayout(fundsBtnL);

    connect(addBtn, &QPushButton::clicked, this, [this, fundsModel]() { 
        showFundDialog(false, -1, fundsModel); 
    });
    connect(editBtn, &QPushButton::clicked, this, [this, fundsTable, fundsModel]() {
        QModelIndex idx = fundsTable->currentIndex();
        if (idx.isValid()) {
            int fundId = fundsModel->item(idx.row(), 0)->text().toInt();
            showFundDialog(true, fundId, fundsModel);
        }
    });
    connect(archiveBtn, &QPushButton::clicked, this, [this, fundsTable, fundsModel]() {
        QModelIndex idx = fundsTable->currentIndex();
        if (!idx.isValid()) return;

        int fundId = fundsModel->item(idx.row(), 0)->text().toInt();
        bool isArchived = fundsModel->item(idx.row(), 4)->text() == "Yes";

        QString msg = isArchived ? "Unarchive this fund?" : "Archive this fund? It will be hidden from dropdowns and new transactions.";
        if (QMessageBox::question(this, "Archive Fund", msg) == QMessageBox::Yes) {
            QSqlQuery query(dbManager->db);
            if (isArchived) {
                query.prepare("UPDATE funds SET archived = 0, archived_date = NULL WHERE id = ?");
            } else {
                query.prepare("UPDATE funds SET archived = 1, archived_date = ? WHERE id = ?");
                query.addBindValue(QDate::currentDate().toString(Qt::ISODate));
            }
            query.addBindValue(fundId);
            if (query.exec()) {
                refreshFundTable(fundsModel);
            }
        }
    });

    tabs->addTab(fundsTab, "Funds");

    // ====================== ACCOUNTS TAB ======================
    QWidget* accountsTab = new QWidget;
    QVBoxLayout* accountsL = new QVBoxLayout(accountsTab);

    QTableView* accountsTable = new QTableView;
    QStandardItemModel* accountsModel = new QStandardItemModel(this);
    accountsModel->setHorizontalHeaderLabels({"ID", "Code", "Name", "Type", "Fund", "Archived", "Archived Date"});
    accountsTable->setModel(accountsModel);

    refreshAccountsTable(accountsModel);

    accountsTable->resizeColumnsToContents();
    accountsTable->horizontalHeader()->setStretchLastSection(true);

    accountsL->addWidget(accountsTable);

    QHBoxLayout* accountsBtnL = new QHBoxLayout;
    QPushButton* accAddBtn = new QPushButton("Add");
    QPushButton* accEditBtn = new QPushButton("Edit");
    QPushButton* accArchiveBtn = new QPushButton("Archive / Unarchive");
    accountsBtnL->addWidget(accAddBtn);
    accountsBtnL->addWidget(accEditBtn);
    accountsBtnL->addWidget(accArchiveBtn);
    accountsL->addLayout(accountsBtnL);

    connect(accAddBtn, &QPushButton::clicked, this, [this, accountsModel]() { 
        showAccountDialog(false, -1, accountsModel); 
    });
    connect(accEditBtn, &QPushButton::clicked, this, [this, accountsTable, accountsModel]() {
        QModelIndex idx = accountsTable->currentIndex();
        if (idx.isValid()) {
            int accountId = accountsModel->item(idx.row(), 0)->text().toInt();
            showAccountDialog(true, accountId, accountsModel);
        }
    });
    connect(accArchiveBtn, &QPushButton::clicked, this, [this, accountsTable, accountsModel]() {
        QModelIndex idx = accountsTable->currentIndex();
        if (!idx.isValid()) return;

        int accountId = accountsModel->item(idx.row(), 0)->text().toInt();
        bool isArchived = accountsModel->item(idx.row(), 5)->text() == "Yes";

        QString msg = isArchived ? "Unarchive this account?" : "Archive this account?";
        if (QMessageBox::question(this, "Archive Account", msg) == QMessageBox::Yes) {
            QSqlQuery query(dbManager->db);
            if (isArchived) {
                query.prepare("UPDATE accounts SET archived = 0, archived_date = NULL WHERE id = ?");
            } else {
                query.prepare("UPDATE accounts SET archived = 1, archived_date = ? WHERE id = ?");
                query.addBindValue(QDate::currentDate().toString(Qt::ISODate));
            }
            query.addBindValue(accountId);
            if (query.exec()) {
                refreshAccountsTable(accountsModel);
            }
        }
    });

    tabs->addTab(accountsTab, "Accounts");

    // ====================== NATURAL CLASSES TAB ======================
    QWidget* naturalTab = new QWidget;
    QVBoxLayout* naturalL = new QVBoxLayout(naturalTab);

    QTableView* naturalTable = new QTableView;
    QStandardItemModel* naturalModel = new QStandardItemModel(this);
    naturalModel->setHorizontalHeaderLabels({"ID", "Name", "Archived", "Archived Date"});
    naturalTable->setModel(naturalModel);

    refreshSimpleLookupTable(naturalModel, "natural_classes");

    naturalL->addWidget(naturalTable);

    QHBoxLayout* naturalBtnL = new QHBoxLayout;
    QPushButton* nAddBtn = new QPushButton("Add");
    QPushButton* nEditBtn = new QPushButton("Edit");
    QPushButton* nArchiveBtn = new QPushButton("Archive / Unarchive");
    naturalBtnL->addWidget(nAddBtn);
    naturalBtnL->addWidget(nEditBtn);
    naturalBtnL->addWidget(nArchiveBtn);
    naturalL->addLayout(naturalBtnL);

    connect(nAddBtn, &QPushButton::clicked, this, [this, naturalModel]() {
        showSimpleLookupDialog("Add Natural Class", naturalModel, "natural_classes");
    });
    connect(nEditBtn, &QPushButton::clicked, this, [this, naturalTable, naturalModel]() {
        QModelIndex idx = naturalTable->currentIndex();
        if (idx.isValid()) {
            int id = naturalModel->item(idx.row(), 0)->text().toInt();
            showSimpleLookupDialog("Edit Natural Class", naturalModel, "natural_classes", id);
        }
    });
    connect(nArchiveBtn, &QPushButton::clicked, this, [this, naturalTable, naturalModel]() {
        QModelIndex idx = naturalTable->currentIndex();
        if (!idx.isValid()) return;

        int id = naturalModel->item(idx.row(), 0)->text().toInt();
        bool isArchived = naturalModel->item(idx.row(), 2)->text() == "Yes";

        QString msg = isArchived ? "Unarchive this natural class?" : "Archive this natural class?";
        if (QMessageBox::question(this, "Archive", msg) == QMessageBox::Yes) {
            QSqlQuery query(dbManager->db);
            if (isArchived) {
                query.prepare("UPDATE natural_classes SET archived = 0, archived_date = NULL WHERE id = ?");
            } else {
                query.prepare("UPDATE natural_classes SET archived = 1, archived_date = ? WHERE id = ?");
                query.addBindValue(QDate::currentDate().toString(Qt::ISODate));
            }
            query.addBindValue(id);
            if (query.exec()) {
                refreshSimpleLookupTable(naturalModel, "natural_classes");
            }
        }
    });

    tabs->addTab(naturalTab, "Natural Classes");

    // ====================== FUNCTIONAL CLASSES TAB ======================
    QWidget* functionalTab = new QWidget;
    QVBoxLayout* functionalL = new QVBoxLayout(functionalTab);

    QTableView* functionalTable = new QTableView;
    QStandardItemModel* functionalModel = new QStandardItemModel(this);
    functionalModel->setHorizontalHeaderLabels({"ID", "Name", "Archived", "Archived Date"});
    functionalTable->setModel(functionalModel);

    refreshSimpleLookupTable(functionalModel, "functional_classes");

    functionalL->addWidget(functionalTable);

    QHBoxLayout* functionalBtnL = new QHBoxLayout;
    QPushButton* fAddBtn = new QPushButton("Add");
    QPushButton* fEditBtn = new QPushButton("Edit");
    QPushButton* fArchiveBtn = new QPushButton("Archive / Unarchive");
    functionalBtnL->addWidget(fAddBtn);
    functionalBtnL->addWidget(fEditBtn);
    functionalBtnL->addWidget(fArchiveBtn);
    functionalL->addLayout(functionalBtnL);

    connect(fAddBtn, &QPushButton::clicked, this, [this, functionalModel]() {
        showSimpleLookupDialog("Add Functional Class", functionalModel, "functional_classes");
    });
    connect(fEditBtn, &QPushButton::clicked, this, [this, functionalTable, functionalModel]() {
        QModelIndex idx = functionalTable->currentIndex();
        if (idx.isValid()) {
            int id = functionalModel->item(idx.row(), 0)->text().toInt();
            showSimpleLookupDialog("Edit Functional Class", functionalModel, "functional_classes", id);
        }
    });
    connect(fArchiveBtn, &QPushButton::clicked, this, [this, functionalTable, functionalModel]() {
        QModelIndex idx = functionalTable->currentIndex();
        if (!idx.isValid()) return;

        int id = functionalModel->item(idx.row(), 0)->text().toInt();
        bool isArchived = functionalModel->item(idx.row(), 2)->text() == "Yes";

        QString msg = isArchived ? "Unarchive this functional class?" : "Archive this functional class?";
        if (QMessageBox::question(this, "Archive", msg) == QMessageBox::Yes) {
            QSqlQuery query(dbManager->db);
            if (isArchived) {
                query.prepare("UPDATE functional_classes SET archived = 0, archived_date = NULL WHERE id = ?");
            } else {
                query.prepare("UPDATE functional_classes SET archived = 1, archived_date = ? WHERE id = ?");
                query.addBindValue(QDate::currentDate().toString(Qt::ISODate));
            }
            query.addBindValue(id);
            if (query.exec()) {
                refreshSimpleLookupTable(functionalModel, "functional_classes");
            }
        }
    });

    tabs->addTab(functionalTab, "Functional Classes");

    layout->addWidget(tabs);
    dlg.resize(1100, 700);
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

    QList<Fund> funds = dbManager->getAllFunds(true);  // include archived

    for (const Fund& f : funds) {
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(f.id))
            << new QStandardItem(f.name)
            << new QStandardItem(f.description)
            << new QStandardItem(f.restriction_type == "WDR" ? "Yes" : "No")
            << new QStandardItem(f.archived ? "Yes" : "No")
            << new QStandardItem(f.archived_date);   // Archived Date
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

    QList<Account> accounts = dbManager->getAllAccounts(true);

    for (const Account& a : accounts) {
        QString fundName = (a.fundId > 0) ? dbManager->getFundName(a.fundId) : "(None)";

        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(a.id))
            << new QStandardItem(a.code)
            << new QStandardItem(a.name)
            << new QStandardItem(a.type)
            << new QStandardItem(fundName)
            << new QStandardItem(a.archived ? "Yes" : "No")
            << new QStandardItem(a.archived_date);   // Archived Date
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
    auto funds = dbManager->getAllFunds(false);
    qDebug() << "Populating" << funds.size() << "funds in dropdown";
    for (const auto& f : funds) {
        combo->addItem(f.name, f.id);
    }
}

QString MainWindow::getFundName(int fundId) const {
    auto funds = dbManager->getAllFunds(false);
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

    QList<SimpleLookup> list;
    if (tableName == "natural_classes") {
        list = dbManager->getAllNaturalClasses(true);
    } else if (tableName == "functional_classes") {
        list = dbManager->getAllFunctionalClasses(true);
    }

    for (const SimpleLookup& s : list) {
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(s.id))
            << new QStandardItem(s.name)
            << new QStandardItem(s.archived ? "Yes" : "No")
            << new QStandardItem(s.archived_date);
        model->appendRow(row);
    }
}

void MainWindow::setupTransactionsPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    QLabel* title = new QLabel("Transaction Ledger");
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    // Main list - simple version
    txListTable = new QTableWidget(0, 6);
    txListTable->setHorizontalHeaderLabels({
        "ID", "Date", "Reference", "Description", "Total", "Status"
    });
    txListTable->setAlternatingRowColors(true);
    txListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    txListTable->setSelectionMode(QAbstractItemView::SingleSelection);
    txListTable->horizontalHeader()->setStretchLastSection(true);

    txListTable->setColumnWidth(0, 60);
    txListTable->setColumnWidth(1, 100);
    txListTable->setColumnWidth(2, 130);
    txListTable->setColumnWidth(4, 110);

    mainLayout->addWidget(txListTable, 2);

    // Details
    QGroupBox* detailsGroup = new QGroupBox("Transaction Details");
    QVBoxLayout* detailsVL = new QVBoxLayout(detailsGroup);

    txDetailsLabel = new QLabel("Select a transaction to view line items");
    txDetailsLabel->setWordWrap(true);
    detailsVL->addWidget(txDetailsLabel);

    txDetailsTable = new QTableWidget(0, 6);
    txDetailsTable->setHorizontalHeaderLabels({
        "Fund", "Account", "Amount", "Natural Class", "Functional Class", "Memo"
    });
    txDetailsTable->horizontalHeader()->setStretchLastSection(true);
    detailsVL->addWidget(txDetailsTable);

    mainLayout->addWidget(detailsGroup, 1);

    connect(txListTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        int row = txListTable->currentRow();
        if (row >= 0) onTransactionSelected(row);
    });

    stack->insertWidget(3, page);
}

void MainWindow::loadTransactions() {
    if (!txListTable) return;
    txListTable->setRowCount(0);

    QSqlQuery q(dbManager->db);
    q.exec(R"(
        SELECT id, date, reference, description, total_amount,
               CASE 
                   WHEN reconciled = 1 THEN 'R - Reconciled'
                   WHEN cleared = 1 THEN 'C - Cleared'
                   ELSE 'Pending' 
               END as status
        FROM transactions 
        ORDER BY date DESC, id DESC
    )");

    while (q.next()) {
        int row = txListTable->rowCount();
        txListTable->insertRow(row);

        qlonglong txId = q.value("id").toLongLong();

        QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(txId));
        idItem->setData(Qt::UserRole, txId);
        txListTable->setItem(row, 0, idItem);

        txListTable->setItem(row, 1, new QTableWidgetItem(q.value("date").toString()));
        txListTable->setItem(row, 2, new QTableWidgetItem(q.value("reference").toString()));
        txListTable->setItem(row, 3, new QTableWidgetItem(q.value("description").toString()));

        double total = q.value("total_amount").toDouble();
        QTableWidgetItem* totalItem = new QTableWidgetItem(QString::number(total, 'f', 2));
        totalItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        txListTable->setItem(row, 4, totalItem);

        txListTable->setItem(row, 5, new QTableWidgetItem(q.value("status").toString()));
    }
}

void MainWindow::onTransactionSelected(int row) {
    if (!txListTable || row < 0) return;

    qlonglong txId = txListTable->item(row, 0) ? 
                     txListTable->item(row, 0)->data(Qt::UserRole).toLongLong() : 0;

    if (txId <= 0) {
        txDetailsLabel->setText("Could not load transaction details.");
        txDetailsTable->setRowCount(0);
        return;
    }

    // Header info
    QString date = txListTable->item(row, 1) ? txListTable->item(row, 1)->text() : "";
    QString ref  = txListTable->item(row, 2) ? txListTable->item(row, 2)->text() : "";
    QString desc = txListTable->item(row, 3) ? txListTable->item(row, 3)->text() : "";
    QString totalStr = txListTable->item(row, 4) ? txListTable->item(row, 4)->text() : "0.00";

    txDetailsLabel->setText(QString("<b>Transaction #%1</b> — %2 — %3<br>"
                                    "<b>Description:</b> %4 &nbsp;&nbsp; <b>Total:</b> $%5")
                                .arg(QString::number(txId), date, ref, desc, totalStr));

    txDetailsTable->setRowCount(0);

    QSqlQuery q(dbManager->db);
    q.prepare(R"(
        SELECT 
            COALESCE(f.name, '(Asset/Liability - No Fund)') as fund_name,
            COALESCE(a.name, '(Unknown Account)') as account_name,
            tl.amount,
            COALESCE(tl.natural_class, '') as natural_class,
            COALESCE(tl.functional_class, '') as functional_class,
            COALESCE(tl.notes, '') as notes,
            a.type as account_type
        FROM transaction_lines tl
        LEFT JOIN funds f ON tl.fund_id = f.id
        LEFT JOIN accounts a ON tl.account_id = a.id
        WHERE tl.transaction_id = ?
        ORDER BY tl.id
    )");
    q.addBindValue(txId);
    q.exec();

    while (q.next()) {
        int r = txDetailsTable->rowCount();
        txDetailsTable->insertRow(r);

        txDetailsTable->setItem(r, 0, new QTableWidgetItem(q.value("fund_name").toString()));
        txDetailsTable->setItem(r, 1, new QTableWidgetItem(q.value("account_name").toString()));

        double rawAmt = q.value("amount").toDouble();
        double displayAmt = rawAmt;

        // Flip sign for better readability: positive = inflow to fund, negative = outflow
        QString accType = q.value("account_type").toString();
        if (accType == "Revenue") displayAmt = -rawAmt;
        else if (accType == "Expense") displayAmt = rawAmt;

        QTableWidgetItem* amtItem = new QTableWidgetItem(QString::number(displayAmt, 'f', 2));
        amtItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        txDetailsTable->setItem(r, 2, amtItem);

        txDetailsTable->setItem(r, 3, new QTableWidgetItem(q.value("natural_class").toString()));
        txDetailsTable->setItem(r, 4, new QTableWidgetItem(q.value("functional_class").toString()));
        txDetailsTable->setItem(r, 5, new QTableWidgetItem(q.value("notes").toString()));
    }

    txDetailsTable->resizeColumnsToContents();
    qDebug() << "=== Loaded details for tx" << txId << "- rows:" << txDetailsTable->rowCount() << "===";
}

void MainWindow::setupFundsBalancesPage() {
    QWidget* page = new QWidget;
    QVBoxLayout* mainL = new QVBoxLayout(page);
    mainL->setContentsMargins(24, 24, 24, 24);
    mainL->setSpacing(16);

    QLabel* title = new QLabel("Fund Balances");
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainL->addWidget(title);

    // Summary totals
    QHBoxLayout* summaryL = new QHBoxLayout;
    QGroupBox* summaryBox = new QGroupBox("Net Assets Summary");
    QFormLayout* form = new QFormLayout(summaryBox);

    totalWODRLabel = new QLabel("$0.00");
    totalWDRLabel  = new QLabel("$0.00");
    grandTotalLabel = new QLabel("$0.00");

    totalWODRLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #2ecc71;");
    totalWDRLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #e67e22;");
    grandTotalLabel->setStyleSheet("font-size: 20px; font-weight: bold;");

    form->addRow("Without Donor Restrictions (WODR):", totalWODRLabel);
    form->addRow("With Donor Restrictions (WDR):", totalWDRLabel);
    form->addRow("Total Net Assets:", grandTotalLabel);

    summaryL->addWidget(summaryBox);
    mainL->addLayout(summaryL);

    // Funds Table
    fundBalancesTable = new QTableWidget(0, 4);
    fundBalancesTable->setHorizontalHeaderLabels({
        "Fund", "Restriction Type", "Balance", "Notes"
    });
    fundBalancesTable->setAlternatingRowColors(true);
    fundBalancesTable->horizontalHeader()->setStretchLastSection(true);
    fundBalancesTable->setColumnWidth(0, 220);
    fundBalancesTable->setColumnWidth(1, 140);
    fundBalancesTable->setColumnWidth(2, 140);

    mainL->addWidget(fundBalancesTable, 1);

    QPushButton* refreshBtn = new QPushButton("↻ Refresh Balances");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::loadFundBalances);
    mainL->addWidget(refreshBtn);

    stack->insertWidget(4, page);   // Insert at index 4 (after Transactions)
}

void MainWindow::loadFundBalances() {
    if (!fundBalancesTable) return;
    fundBalancesTable->setRowCount(0);

    double totalWODR = 0.0;
    double totalWDR = 0.0;

    QSqlQuery q(dbManager->db);
    q.exec(R"(
        SELECT 
            f.name,
            f.restriction_type,
            COALESCE(
                SUM(CASE 
                    WHEN a.type = 'Revenue' THEN -tl.amount   -- Credit to revenue increases fund
                    WHEN a.type = 'Expense' THEN  tl.amount   -- Debit to expense decreases fund
                    ELSE tl.amount
                END), 0) as balance
        FROM funds f
        LEFT JOIN transaction_lines tl ON tl.fund_id = f.id
        LEFT JOIN accounts a ON tl.account_id = a.id
        WHERE tl.account_id IS NOT NULL
        GROUP BY f.id, f.name, f.restriction_type
        ORDER BY f.name
    )");

    while (q.next()) {
        int row = fundBalancesTable->rowCount();
        fundBalancesTable->insertRow(row);

        QString name = q.value("name").toString();
        QString type = q.value("restriction_type").toString();
        double bal = q.value("balance").toDouble();

        fundBalancesTable->setItem(row, 0, new QTableWidgetItem(name));
        fundBalancesTable->setItem(row, 1, new QTableWidgetItem(type));

        QTableWidgetItem* balItem = new QTableWidgetItem(QString::number(bal, 'f', 2));
        balItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fundBalancesTable->setItem(row, 2, balItem);

        fundBalancesTable->setItem(row, 3, new QTableWidgetItem(""));

        if (type == "WODR") 
            totalWODR += bal;
        else 
            totalWDR += bal;
    }

    double grandTotal = totalWODR + totalWDR;

    totalWODRLabel->setText(QString("$%1").arg(totalWODR, 0, 'f', 2));
    totalWDRLabel->setText(QString("$%1").arg(totalWDR, 0, 'f', 2));
    grandTotalLabel->setText(QString("$%1").arg(grandTotal, 0, 'f', 2));

    qDebug() << "Fund Balances refreshed - WODR:" << totalWODR << "WDR:" << totalWDR;
}

void MainWindow::refreshFundsPage() {}