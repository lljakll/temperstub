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
#include <QApplication>

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
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    auto* central = new QWidget;
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar
    sidebar = new QListWidget;
    sidebar->setFixedWidth(240);
    sidebar->setStyleSheet(R"(
        QListWidget { background: #2c3e50; color: #ecf0f1; font-size: 15px; }
        QListWidget::item { padding: 14px 12px; border-bottom: 1px solid #34495e; }
        QListWidget::item:selected { background: #3498db; color: white; }
    )");
    sidebar->addItem("📊 Dashboard");
    sidebar->addItem("➕ New Transaction");
    sidebar->addItem("📋 Funds");
    sidebar->addItem("📜 Transactions");
    connect(sidebar, &QListWidget::currentRowChanged, this, &MainWindow::switchPage);

    // Workspace
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

    QPushButton* newBtn = new QPushButton("New Transaction (Guide §3, §4, §10)");
    newBtn->setStyleSheet("font-size: 16px; padding: 14px;");
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::onNewTransaction);
    dashL->addWidget(newBtn);

    dashL->addWidget(new QLabel("Recent Transactions (Guide §9)"));
    recentTxTable = new QTableView;
    recentTxTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dashL->addWidget(recentTxTable);

    // Funds Page (basic for now)
    fundsPage = new QWidget;
    auto* fundsL = new QVBoxLayout(fundsPage);
    fundsL->setContentsMargins(24, 24, 24, 24);
    auto* fundTable = new QTableWidget(0, 3, fundsPage);
    fundTable->setHorizontalHeaderLabels({"Fund", "Restriction", "Balance"});
    fundTable->horizontalHeader()->setStretchLastSection(true);
    fundsL->addWidget(fundTable);

    // Transactions Page
    transactionsPage = new QWidget;
    auto* txL = new QVBoxLayout(transactionsPage);
    txL->setContentsMargins(24, 24, 24, 24);
    txL->addWidget(new QLabel("Full Transaction Ledger – will be expanded soon"));

    stack->addWidget(dashboardPage);   // 0
    stack->addWidget(new QWidget());   // 1 placeholder
    stack->addWidget(fundsPage);       // 2
    stack->addWidget(transactionsPage);// 3

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(sidebar);
    splitter->addWidget(stack);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);
    setCentralWidget(central);

    sidebar->setCurrentRow(0);
}

void MainWindow::switchPage(int index) {
    if (index == 1) {                    // New Transaction
        onNewTransaction();
        sidebar->setCurrentRow(0);
        return;
    }
    stack->setCurrentIndex(index == 0 ? 0 : index - 1);
}

void MainWindow::onNewTransaction() {
    QDialog dlg(this);
    dlg.setWindowTitle("New Transaction – Treasurer’s Guide");
    QFormLayout* form = new QFormLayout(&dlg);

    QDateEdit* dateEdit = new QDateEdit(QDate::currentDate()); dateEdit->setCalendarPopup(true);
    form->addRow("Date", dateEdit);

    QLineEdit* descEdit = new QLineEdit; form->addRow("Description *", descEdit);

    QDoubleSpinBox* amtSpin = new QDoubleSpinBox;
    amtSpin->setRange(-9999999.99, 9999999.99); amtSpin->setDecimals(2); amtSpin->setPrefix("$ ");
    form->addRow("Amount", amtSpin);

    QComboBox* fundCombo = new QComboBox; populateFundsCombo(fundCombo);
    form->addRow("Fund (Guide §5)", fundCombo);

    QComboBox* natCombo = new QComboBox; natCombo->addItems({"Contributions","Salaries & Housing","Utilities","Supplies","Travel","Other"});
    form->addRow("Natural (Guide §4.1.1)", natCombo);

    QComboBox* funcCombo = new QComboBox; funcCombo->addItems({"Program Services","Management & General","Fundraising"});
    form->addRow("Functional (Guide §4.1.2)", funcCombo);

    QLineEdit* notesEdit = new QLineEdit; form->addRow("Notes", notesEdit);

    QComboBox* typeCombo = new QComboBox; typeCombo->addItems({"contribution","disbursement","release","adjustment"});
    form->addRow("Type", typeCombo);

    QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(btns);

    if (dlg.exec() == QDialog::Accepted && !descEdit->text().trimmed().isEmpty() && fundCombo->currentIndex() >= 0) {
        Transaction t;
        t.date = dateEdit->date();
        t.description = descEdit->text().trimmed();
        t.amount = amtSpin->value();
        t.fundId = fundCombo->currentData().toInt();
        t.naturalClass = natCombo->currentText();
        t.functionalClass = funcCombo->currentText();
        t.notes = notesEdit->text();
        t.transactionType = typeCombo->currentText();

        if (dbManager->addTransaction(t)) {
            QMessageBox::information(this, "Success", "Transaction recorded.\n\n(Guide §10: Use separate release entry when needed)");
            refreshAll();
        } else {
            QMessageBox::critical(this, "Error", "Failed to save.");
        }
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
    auto funds = dbManager->getAllFunds();
    for (const auto& f : funds) {
        combo->addItem(f.name, f.id);
    }
}

QString MainWindow::getFundName(int fundId) const {
    auto funds = dbManager->getAllFunds();
    for (const auto& f : funds) if (f.id == fundId) return f.name;
    return "Unknown";
}

void MainWindow::refreshFundsPage() { /* placeholder */ }