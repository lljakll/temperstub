#include "dbmanager.h"
#include <QSqlError>
#include <QDebug>

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
    seedInitialFunds();
    return true;
}

void DbManager::createTables() {
    QSqlQuery query(db);

    // Header: one row per logical transaction
    query.exec(R"(CREATE TABLE IF NOT EXISTS transactions (
        id INTEGER PRIMARY KEY,
        date TEXT NOT NULL,
        description TEXT NOT NULL,
        total_amount REAL NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );)");

    // Lines: one row per fund allocation
    query.exec(R"(CREATE TABLE IF NOT EXISTS transaction_lines (
        id INTEGER PRIMARY KEY,
        transaction_id INTEGER REFERENCES transactions(id) ON DELETE CASCADE,
        fund_id INTEGER REFERENCES funds(id),
        amount REAL NOT NULL,
        natural_class TEXT,
        functional_class TEXT,
        transaction_type TEXT,
        notes TEXT
    );)");

    // Keep old table for compatibility during transition
    query.exec(R"(CREATE TABLE IF NOT EXISTS transactions_old (
        id INTEGER PRIMARY KEY,
        date TEXT NOT NULL,
        description TEXT NOT NULL,
        amount REAL NOT NULL,
        fund_id INTEGER,
        natural_class TEXT,
        functional_class TEXT,
        notes TEXT,
        transaction_type TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );)");
}

void DbManager::seedInitialFunds() {
    QSqlQuery check(db);
    check.exec("SELECT COUNT(*) FROM funds");
    int count = check.next() ? check.value(0).toInt() : 0;
    qDebug() << "Funds count:" << count;

    if (count == 0) {
        QSqlQuery q(db);
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Tithes and Offerings', 'General operating – unrestricted (§5.1.1)', 'WODR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Designated Gifts', 'Temporarily restricted (§5.1.2)', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Benevolence', 'Restricted for assistance (§5.1.3)', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Missions', 'Restricted for missionary (§5.1.4)', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES ('Capital / Building', 'Restricted for facilities (§5.1.5)', 'WDR')");
        qDebug() << "✅ 5 funds seeded per Treasurer’s Guide §5.1";
    }
}

QList<Fund> DbManager::getAllFunds() const { /* your existing code */ }

QList<Transaction> DbManager::getAllTransactions() const { /* your existing code */ }

bool DbManager::addTransaction(const Transaction& t) { /* your existing code */ }

double DbManager::getFundBalance(int fundId) const { /* your existing code */ }

double DbManager::getWODRTotal() const { /* your existing code */ }

double DbManager::getWDRTotal() const { /* your existing code */ }