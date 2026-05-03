#include "dbmanager.h"
#include <QSqlError>
#include <QDebug>
#include <QDate>

DbManager::DbManager(QObject* parent) : QObject(parent) {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("treasurer_stub.db");
    qDebug() << "DbManager: Path =" << db.databaseName();
}

bool DbManager::initDatabase() {
    if (!db.open()) {
        qDebug() << "❌ DB open failed:" << db.lastError().text();
        return false;
    }
    qDebug() << "✅ DB opened";
    createTables();
    qDebug() << "✅ Tables Created or Validated";
    seedDefaultLookups();
    qDebug() << "✅ DB initialized";
    return true;
}

void DbManager::createTables() {
    QSqlQuery query(db);

    // Funds
    query.exec(R"(CREATE TABLE IF NOT EXISTS funds (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        description TEXT,
        restriction_type TEXT CHECK(restriction_type IN ('WODR','WDR')) NOT NULL
    );)");

    // Chart of Accounts
    query.exec(R"(CREATE TABLE IF NOT EXISTS accounts (
        id INTEGER PRIMARY KEY,
        code TEXT UNIQUE NOT NULL,
        name TEXT NOT NULL,
        type TEXT NOT NULL,
        fund_id INTEGER REFERENCES funds(id),
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );)");

    // Transactions Header
    query.exec(R"(CREATE TABLE IF NOT EXISTS transactions (
        id TEXT PRIMARY KEY,
        date TEXT NOT NULL,
        description TEXT NOT NULL,
        total_amount REAL NOT NULL,
        memo TEXT,
        notes TEXT,
        payee_donor TEXT,
        reference TEXT,
        approved_by TEXT,
        cleared INTEGER DEFAULT 0,
        cleared_date TEXT,
        reconciled INTEGER DEFAULT 0,
        reconciled_date TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );)");

    // Transaction Lines
    query.exec(R"(CREATE TABLE IF NOT EXISTS transaction_lines (
        id INTEGER PRIMARY KEY,
        transaction_id TEXT REFERENCES transactions(id) ON DELETE CASCADE,
        account_id INTEGER REFERENCES accounts(id),
        fund_id INTEGER REFERENCES funds(id),
        amount REAL NOT NULL,
        natural_class TEXT,
        functional_class TEXT,
        transaction_type TEXT,
        notes TEXT
    );)");
    
    // Natural Classes
        query.exec(R"(CREATE TABLE IF NOT EXISTS natural_classes (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE
    );)");

    // Functional Classes
    query.exec(R"(CREATE TABLE IF NOT EXISTS functional_classes (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE
    );)");
}

QString DbManager::generateNextTransactionId() const {
    QDate today = QDate::currentDate();
    QString yearPrefix = today.toString("yy");

    QSqlQuery query(db);
    query.prepare("SELECT id FROM transactions WHERE id LIKE ? ORDER BY id DESC LIMIT 1");
    query.addBindValue(yearPrefix + "____");
    if (query.exec() && query.next()) {
        QString lastId = query.value(0).toString();
        int seq = lastId.mid(2).toInt() + 1;
        return yearPrefix + QString("%1").arg(seq, 4, 10, QChar('0'));
    }
    return yearPrefix + "0001";
}

QList<Fund> DbManager::getAllFunds() const {
    QList<Fund> funds;
    QSqlQuery query(db);
    query.exec("SELECT id, name, description, restriction_type FROM funds");
    while (query.next()) {
        Fund f;
        f.id = query.value(0).toInt();
        f.name = query.value(1).toString();
        f.description = query.value(2).toString();
        f.restrictionType = query.value(3).toString();
        funds.append(f);
    }
    return funds;
}

QList<Transaction> DbManager::getAllTransactions() const {
    QList<Transaction> txs;
    QSqlQuery query(db);
    query.exec(R"(SELECT id, date, description, total_amount, memo, notes, payee_donor, reference, approved_by, cleared, cleared_date, reconciled, reconciled_date 
                  FROM transactions ORDER BY date DESC, id DESC)");
    while (query.next()) {
        Transaction t;
        t.id = query.value(0).toInt();
        t.date = QDate::fromString(query.value(1).toString(), Qt::ISODate);
        t.description = query.value(2).toString();
        t.amount = query.value(3).toDouble();
        t.notes = query.value(5).toString();
        txs.append(t);
    }
    return txs;
}

bool DbManager::addTransaction(const Transaction& t) {
    QSqlQuery query(db);
    query.prepare(R"(INSERT INTO transactions (date, description, total_amount) VALUES (?, ?, ?))");
    query.addBindValue(t.date.toString(Qt::ISODate));
    query.addBindValue(t.description);
    query.addBindValue(t.amount);
    bool success = query.exec();
    qDebug() << "addTransaction success:" << success << "Amount" << t.amount;
    return success;
}

double DbManager::getFundBalance(int fundId) const {
    QSqlQuery query(db);
    query.prepare("SELECT SUM(amount) FROM transaction_lines WHERE fund_id = ?");
    query.addBindValue(fundId);
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}

double DbManager::getWODRTotal() const {
    QSqlQuery query(db);
    query.prepare(R"(SELECT SUM(l.amount) FROM transaction_lines l 
                     JOIN funds f ON l.fund_id = f.id 
                     WHERE f.restriction_type = 'WODR')");
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}

double DbManager::getWDRTotal() const {
    QSqlQuery query(db);
    query.prepare(R"(SELECT SUM(l.amount) FROM transaction_lines l 
                     JOIN funds f ON l.fund_id = f.id 
                     WHERE f.restriction_type = 'WDR')");
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}

QList<Account> DbManager::getAllAccounts() const {
    QList<Account> accounts;
    QSqlQuery query(db);
    query.exec("SELECT id, code, name, type, fund_id FROM accounts");
    while (query.next()) {
        Account a;
        a.id = query.value(0).toInt();
        a.code = query.value(1).toString();
        a.name = query.value(2).toString();
        a.type = query.value(3).toString();
        a.fundId = query.value(4).toInt();
        accounts.append(a);
    }
    return accounts;
}

QString DbManager::getFundName(int fundId) const {
    QSqlQuery query(db);
    query.prepare("SELECT name FROM funds WHERE id = ?");
    query.addBindValue(fundId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "(None)";
}

QList<SimpleLookup> DbManager::getAllNaturalClasses() const {
    QList<SimpleLookup> list;
    QSqlQuery query(db);
    query.exec("SELECT id, name FROM natural_classes");
    while (query.next()) {
        SimpleLookup s;
        s.id = query.value(0).toInt();
        s.name = query.value(1).toString();
        list.append(s);
    }
    return list;
}

QList<SimpleLookup> DbManager::getAllFunctionalClasses() const {
    QList<SimpleLookup> list;
    QSqlQuery query(db);
    query.exec("SELECT id, name FROM functional_classes");
    while (query.next()) {
        SimpleLookup s;
        s.id = query.value(0).toInt();
        s.name = query.value(1).toString();
        list.append(s);
    }
    return list;
}

void DbManager::seedDefaultLookups() {
    QSqlQuery check(db);

    // Funds
    check.exec("SELECT COUNT(*) FROM funds");
    if (check.next() && check.value(0).toInt() == 0) {
        QSqlQuery q(db);
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('General Fund', 'Unrestricted operations', 'WODR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Building Fund', 'Capital projects', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Benevolence', 'Help for those in need', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Missions', 'Outreach and missions', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Youth', 'Youth ministry', 'WDR')");
    }

    // Accounts
    check.exec("SELECT COUNT(*) FROM accounts");
    if (check.next() && check.value(0).toInt() == 0) {
        QSqlQuery q(db);
        q.exec("INSERT INTO accounts (code, name, type) VALUES ('1010', 'Checking Account', 'Asset')");
        q.exec("INSERT INTO accounts (code, name, type) VALUES ('4000', 'Contributions - Unrestricted', 'Revenue')");
        q.exec("INSERT INTO accounts (code, name, type) VALUES ('4100', 'Contributions - Restricted', 'Revenue')");
        q.exec("INSERT INTO accounts (code, name, type) VALUES ('5000', 'Program Expenses', 'Expense')");
        q.exec("INSERT INTO accounts (code, name, type) VALUES ('6000', 'Management & General', 'Expense')");
    }

    // Natural Classes
    check.exec("SELECT COUNT(*) FROM natural_classes");
    if (check.next() && check.value(0).toInt() == 0) {
        QSqlQuery q(db);
        q.exec("INSERT INTO natural_classes (name) VALUES ('Contributions')");
        q.exec("INSERT INTO natural_classes (name) VALUES ('Salaries & Housing')");
        q.exec("INSERT INTO natural_classes (name) VALUES ('Utilities')");
        q.exec("INSERT INTO natural_classes (name) VALUES ('Supplies')");
        q.exec("INSERT INTO natural_classes (name) VALUES ('Travel')");
        q.exec("INSERT INTO natural_classes (name) VALUES ('Other')");
    }

    // Functional Classes
    check.exec("SELECT COUNT(*) FROM functional_classes");
    if (check.next() && check.value(0).toInt() == 0) {
        QSqlQuery q(db);
        q.exec("INSERT INTO functional_classes (name) VALUES ('Program Services')");
        q.exec("INSERT INTO functional_classes (name) VALUES ('Management & General')");
        q.exec("INSERT INTO functional_classes (name) VALUES ('Fundraising')");
    }
}