#include "dbmanager.h"
#include <QSqlError>
#include <QDebug>
#include <QFile>

DbManager::DbManager(QObject* parent) : QObject(parent) {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("treasurer_stub.db");
}

bool DbManager::initDatabase() {
    if (!db.open()) {
        qDebug() << "Database open failed:" << db.lastError().text();
        return false;
    }
    createTables();
    seedInitialFunds();
    return true;
}

void DbManager::createTables() {
    QSqlQuery query(db);
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS funds (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE,
            description TEXT,
            restriction_type TEXT CHECK(restriction_type IN ('WODR','WDR')) NOT NULL
        );
    )");
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY,
            date TEXT NOT NULL,
            description TEXT NOT NULL,
            amount REAL NOT NULL,
            fund_id INTEGER REFERENCES funds(id),
            natural_class TEXT,
            functional_class TEXT CHECK(functional_class IN ('Program Services','Management & General','Fundraising')),
            notes TEXT,
            transaction_type TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )");
}

void DbManager::seedInitialFunds() { // Treasurer’s Guide §5.1
    QSqlQuery check(db);
    check.exec("SELECT COUNT(*) FROM funds");
    if (check.next() && check.value(0).toInt() == 0) {
        QSqlQuery q(db);
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES "
               "('Tithes and Offerings', 'General operating – unrestricted (Guide §5.1.1)', 'WODR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES "
               "('Designated Gifts', 'Temporarily restricted for specific ministries/projects (§5.1.2)', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES "
               "('Benevolence', 'Restricted for assistance to those in need (§5.1.3)', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES "
               "('Missions', 'Restricted for missionary support and outreach (§5.1.4)', 'WDR')");
        q.exec("INSERT INTO funds (name, description, restriction_type) VALUES "
               "('Capital / Building', 'Restricted for facilities and major acquisitions (§5.1.5)', 'WDR')");
    }
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
    query.exec(R"(SELECT id, date, description, amount, fund_id, natural_class, 
                         functional_class, notes, transaction_type, created_at 
                  FROM transactions ORDER BY date DESC, id DESC)");
    while (query.next()) {
        Transaction t;
        t.id = query.value(0).toInt();
        t.date = QDate::fromString(query.value(1).toString(), Qt::ISODate);
        t.description = query.value(2).toString();
        t.amount = query.value(3).toDouble();
        t.fundId = query.value(4).toInt();
        t.naturalClass = query.value(5).toString();
        t.functionalClass = query.value(6).toString();
        t.notes = query.value(7).toString();
        t.transactionType = query.value(8).toString();
        t.createdAt = query.value(9).toDateTime();
        txs.append(t);
    }
    return txs;
}

bool DbManager::addTransaction(const Transaction& t) {
    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO transactions (date, description, amount, fund_id, natural_class,
                                  functional_class, notes, transaction_type)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(t.date.toString(Qt::ISODate));
    query.addBindValue(t.description);
    query.addBindValue(t.amount);
    query.addBindValue(t.fundId);
    query.addBindValue(t.naturalClass);
    query.addBindValue(t.functionalClass);
    query.addBindValue(t.notes);
    query.addBindValue(t.transactionType);
    return query.exec();
}

double DbManager::getFundBalance(int fundId) const {
    QSqlQuery query(db);
    query.prepare("SELECT SUM(amount) FROM transactions WHERE fund_id = ?");
    query.addBindValue(fundId);
    if (query.exec() && query.next()) return query.value(0).toDouble();
    return 0.0;
}

double DbManager::getWODRTotal() const {
    QSqlQuery query(db);
    query.prepare(R"(SELECT SUM(t.amount) FROM transactions t 
                     JOIN funds f ON t.fund_id = f.id 
                     WHERE f.restriction_type = 'WODR')");
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}

double DbManager::getWDRTotal() const {
    QSqlQuery query(db);
    query.prepare(R"(SELECT SUM(t.amount) FROM transactions t 
                     JOIN funds f ON t.fund_id = f.id 
                     WHERE f.restriction_type = 'WDR')");
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}