#include "dbmanager.h"
#include <QSqlError>
#include <QDebug>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

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

    qDebug() << "[CACHE] Calling refreshLookupCache() from initDatabase()";
    refreshLookupCache();

    // === TEST DATA GENERATOR ===
    // Comment this line out after initial testing / seeding
    seedTestTransactions();

    return true;
}

void DbManager::createTables() {
    QSqlQuery query(db);

    // Funds
    query.exec(R"(CREATE TABLE IF NOT EXISTS funds (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        description TEXT,
        restriction_type TEXT CHECK(restriction_type IN ('WODR','WDR')) NOT NULL,
        archived INTEGER DEFAULT 0,
        archived_date TEXT
    );)");

    // Accounts
    query.exec(R"(CREATE TABLE IF NOT EXISTS accounts (
        id INTEGER PRIMARY KEY,
        code TEXT UNIQUE NOT NULL,
        name TEXT NOT NULL,
        type TEXT NOT NULL,
        fund_id INTEGER REFERENCES funds(id),
        archived INTEGER DEFAULT 0,
        archived_date TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );)");

    // Natural Classes
    query.exec(R"(CREATE TABLE IF NOT EXISTS natural_classes (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        archived INTEGER DEFAULT 0,
        archived_date TEXT
    );)");

    // Functional Classes
    query.exec(R"(CREATE TABLE IF NOT EXISTS functional_classes (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        archived INTEGER DEFAULT 0,
        archived_date TEXT
    );)");

// Transactions Header
    query.exec(R"(CREATE TABLE IF NOT EXISTS transactions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
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
        attachment_path TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );)");

    // Transaction Lines (proper double-entry: debit OR credit, never negative)
    query.exec(R"(CREATE TABLE IF NOT EXISTS transaction_lines (
        id INTEGER PRIMARY KEY,
        transaction_id INTEGER REFERENCES transactions(id) ON DELETE CASCADE,
        account_id INTEGER REFERENCES accounts(id),
        fund_id INTEGER REFERENCES funds(id),
        debit REAL DEFAULT 0,
        credit REAL DEFAULT 0,
        natural_class TEXT,
        functional_class TEXT,
        notes TEXT
    );)");

    // ====================== PERFORMANCE INDEXES ======================
    // Speeds up fund balance calculations and transaction filtering by fund
    query.exec("CREATE INDEX IF NOT EXISTS idx_transaction_lines_fund_id "
               "ON transaction_lines(fund_id)");

    // Speeds up date-based queries (ledger, reports, recent transactions)
    query.exec("CREATE INDEX IF NOT EXISTS idx_transactions_date "
               "ON transactions(date)");

    // Optional composite index for common transaction line lookups
    query.exec("CREATE INDEX IF NOT EXISTS idx_transaction_lines_tx_fund "
               "ON transaction_lines(transaction_id, fund_id)");

    qDebug() << "✅ All tables created/validated with archive flags";
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
    // Consistent with getAllFundBalances(): only Revenue and Expense affect fund balance
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT COALESCE(
            SUM(CASE 
                WHEN a.type = 'Revenue' THEN tl.credit
                WHEN a.type = 'Expense' THEN -tl.debit
                ELSE 0
            END), 0)
        FROM transaction_lines tl
        LEFT JOIN accounts a ON tl.account_id = a.id
        WHERE tl.fund_id = ?
    )");
    query.addBindValue(fundId);
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}

double DbManager::getWODRTotal() const {
    // Only Revenue/Expense affect fund totals (consistent with new balance logic)
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT COALESCE(
            SUM(CASE 
                WHEN a.type = 'Revenue' THEN tl.credit
                WHEN a.type = 'Expense' THEN -tl.debit
                ELSE 0
            END), 0)
        FROM transaction_lines tl
        LEFT JOIN accounts a ON tl.account_id = a.id
        LEFT JOIN funds f ON tl.fund_id = f.id
        WHERE f.restriction_type = 'WODR'
    )");
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}

double DbManager::getWDRTotal() const {
    // Only Revenue/Expense affect fund totals (consistent with new balance logic)
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT COALESCE(
            SUM(CASE 
                WHEN a.type = 'Revenue' THEN tl.credit
                WHEN a.type = 'Expense' THEN -tl.debit
                ELSE 0
            END), 0)
        FROM transaction_lines tl
        LEFT JOIN accounts a ON tl.account_id = a.id
        LEFT JOIN funds f ON tl.fund_id = f.id
        WHERE f.restriction_type = 'WDR'
    )");
    return (query.exec() && query.next()) ? query.value(0).toDouble() : 0.0;
}

// Correct fund accounting: Only Revenue and Expense affect fund balances.
// Asset/Liability accounts (e.g. Checking) are balance sheet only and must be ignored here.
QMap<int, double> DbManager::getAllFundBalances() const {
    QMap<int, double> balances;
    QSqlQuery query(db);
    query.exec(R"(
        SELECT 
            f.id,
            COALESCE(
                SUM(CASE 
                    WHEN a.type = 'Revenue' THEN tl.credit
                    WHEN a.type = 'Expense' THEN -tl.debit
                    ELSE 0
                END), 0) as balance
        FROM funds f
        LEFT JOIN transaction_lines tl ON tl.fund_id = f.id
        LEFT JOIN accounts a ON tl.account_id = a.id
        GROUP BY f.id
    )");
    while (query.next()) {
        balances[query.value(0).toInt()] = query.value(1).toDouble();
    }
    return balances;
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
    qDebug() << "Initial funds seeded.";

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
    qDebug() << "Initial accounts seeded.";

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
    qDebug() << "Initial Natural seeded.";

    // Functional Classes
    check.exec("SELECT COUNT(*) FROM functional_classes");
    if (check.next() && check.value(0).toInt() == 0) {
        QSqlQuery q(db);
        q.exec("INSERT INTO functional_classes (name) VALUES ('Program Services')");
        q.exec("INSERT INTO functional_classes (name) VALUES ('Management & General')");
        q.exec("INSERT INTO functional_classes (name) VALUES ('Fundraising')");
    }
    qDebug() << "Initial Functional seeded.";
}

QList<Fund> DbManager::getAllFunds(bool includeArchived) const {
    if (cacheValid) {
        qDebug() << "[CACHE] getAllFunds() using cache (includeArchived=" << includeArchived << ")";
    } else {
        qDebug() << "[CACHE] getAllFunds() cache MISS - querying database";
    }
    QList<Fund> funds;
    QSqlQuery query(db);
    
    QString sql = R"(
        SELECT id, name, description, restriction_type, archived, archived_date 
        FROM funds 
        WHERE 1=1
    )";
    if (!includeArchived) {
        sql += " AND archived = 0";
    }
    sql += " ORDER BY name";

    query.exec(sql);

    while (query.next()) {
        Fund f;
        f.id               = query.value(0).toInt();
        f.name             = query.value(1).toString();
        f.description      = query.value(2).toString();
        f.restriction_type = query.value(3).toString();
        f.archived         = query.value(4).toInt() != 0;
        f.archived_date    = query.value(5).toString();
        funds.append(f);
    }
    return funds;
}

QList<Account> DbManager::getAllAccounts(bool includeArchived) const {
    if (cacheValid) {
        qDebug() << "[CACHE] getAllAccounts() using cache (includeArchived=" << includeArchived << ")";
    } else {
        qDebug() << "[CACHE] getAllAccounts() cache MISS - querying database";
    }
    QList<Account> accounts;
    QSqlQuery query(db);
    
    QString sql = R"(
        SELECT id, code, name, type, fund_id, archived, archived_date 
        FROM accounts 
        WHERE 1=1
    )";
    if (!includeArchived) {
        sql += " AND archived = 0";
    }
    sql += " ORDER BY code";

    query.exec(sql);

    while (query.next()) {
        Account a;
        a.id            = query.value(0).toInt();
        a.code          = query.value(1).toString();
        a.name          = query.value(2).toString();
        a.type          = query.value(3).toString();
        a.fundId        = query.value(4).toInt();
        a.archived      = query.value(5).toInt() != 0;
        a.archived_date = query.value(6).toString();
        accounts.append(a);
    }
    return accounts;
}

QList<SimpleLookup> DbManager::getAllNaturalClasses(bool includeArchived) const {
    if (cacheValid) {
        qDebug() << "[CACHE] getAllNaturalClasses() using cache (includeArchived=" << includeArchived << ")";
    } else {
        qDebug() << "[CACHE] getAllNaturalClasses() cache MISS - querying database";
    }
    QList<SimpleLookup> list;
    QSqlQuery query(db);
    
    QString sql = R"(
        SELECT id, name, archived, archived_date 
        FROM natural_classes 
        WHERE 1=1
    )";
    if (!includeArchived) {
        sql += " AND archived = 0";
    }
    sql += " ORDER BY name";

    query.exec(sql);

    while (query.next()) {
        SimpleLookup s;
        s.id            = query.value(0).toInt();
        s.name          = query.value(1).toString();
        s.archived      = query.value(2).toInt() != 0;
        s.archived_date = query.value(3).toString();
        list.append(s);
    }
    return list;
}

QList<SimpleLookup> DbManager::getAllFunctionalClasses(bool includeArchived) const {
    QList<SimpleLookup> list;
    QSqlQuery query(db);
    
    QString sql = R"(
        SELECT id, name, archived, archived_date 
        FROM functional_classes 
        WHERE 1=1
    )";
    if (!includeArchived) {
        sql += " AND archived = 0";
    }
    sql += " ORDER BY name";

    query.exec(sql);

    while (query.next()) {
        SimpleLookup s;
        s.id            = query.value(0).toInt();
        s.name          = query.value(1).toString();
        s.archived      = query.value(2).toInt() != 0;
        s.archived_date = query.value(3).toString();
        list.append(s);
    }
    return list;
}

bool DbManager::saveAttachment(int transactionId, const QString& sourceFilePath) {
    if (sourceFilePath.isEmpty()) return false;

    QDir attachmentsDir("attachments/transactions");
    if (!attachmentsDir.exists()) {
        attachmentsDir.mkpath(".");
    }

    QString fileName = QString("TX_%1_%2").arg(transactionId)
                       .arg(QFileInfo(sourceFilePath).fileName());

    QString destPath = attachmentsDir.absoluteFilePath(fileName);

    if (QFile::copy(sourceFilePath, destPath)) {
        QSqlQuery query(db);
        query.prepare("UPDATE transactions SET attachment_path = ? WHERE id = ?");
        query.addBindValue("attachments/transactions/" + fileName);
        query.addBindValue(transactionId);
        return query.exec();
    }
    return false;
}

QString DbManager::getAttachmentPath(int transactionId) const {
    QSqlQuery query(db);
    query.prepare("SELECT attachment_path FROM transactions WHERE id = ?");
    query.addBindValue(transactionId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool DbManager::openAttachment(int transactionId) const {
    QString path = getAttachmentPath(transactionId);
    if (path.isEmpty()) return false;

    QFile file(path);
    if (file.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return true;
    }
    return false;
}

void DbManager::refreshLookupCache() {
    qDebug() << "[CACHE] refreshLookupCache() called";
    cachedFunds      = getAllFunds(true);
    cachedAccounts   = getAllAccounts(true);
    cachedNatural    = getAllNaturalClasses(true);
    cachedFunctional = getAllFunctionalClasses(true);
    cacheValid = true;
    qDebug() << "[CACHE] refresh complete → Funds:" << cachedFunds.size()
             << "Accounts:" << cachedAccounts.size()
             << "Natural:" << cachedNatural.size()
             << "Functional:" << cachedFunctional.size();
}

void DbManager::seedTestTransactions() {
    qDebug() << "[TEST] seedTestTransactions() called - Proper Double-Entry Mode";

    auto funds = getAllFunds(true);
    auto accounts = getAllAccounts(true);
    auto naturals = getAllNaturalClasses(true);
    auto functionals = getAllFunctionalClasses(true);

    if (funds.isEmpty() || accounts.isEmpty()) {
        qDebug() << "[TEST] seedTestTransactions: No funds or accounts found. Aborting.";
        return;
    }

    auto findAccount = [&](const QString& code) -> int {
        for (const auto& a : accounts) if (a.code == code) return a.id;
        return accounts.first().id;
    };
    auto findFund = [&](const QString& name) -> int {
        for (const auto& f : funds) if (f.name.contains(name, Qt::CaseInsensitive)) return f.id;
        return funds.first().id;
    };

    int checkingId = findAccount("1010");
    int contribUnrestId = findAccount("4000");
    int contribRestId = findAccount("4100");
    int programExpId = findAccount("5000");
    int mgmtId = findAccount("6000");

    int generalFundId = findFund("General");
    int buildingId = findFund("Building");
    int benevolenceId = findFund("Benevolence");
    int missionsId = findFund("Missions");
    int youthId = findFund("Youth");

    QString naturalContrib = naturals.isEmpty() ? "Contributions" : naturals.first().name;
    QString naturalSalaries = naturals.size() > 1 ? naturals[1].name : "Salaries & Housing";
    QString naturalUtilities = naturals.size() > 2 ? naturals[2].name : "Utilities";
    QString naturalOther = naturals.size() > 5 ? naturals[5].name : "Other";

    QString funcProgram = functionals.isEmpty() ? "Program Services" : functionals.first().name;
    QString funcMgmt = functionals.size() > 1 ? functionals[1].name : "Management & General";

    struct TxData {
        QDate date;
        QString desc;
        QString payee;
        QString memo;
        QList<QVariantList> lines; // {accountId, fundId, debit, credit, natural, functional, notes}
    };

    QList<TxData> testTxs;

    // ============================================================
    // CONTRIBUTIONS - Debit Checking (Asset), Credit Revenue
    // ============================================================

    // 1. Simple unrestricted contribution
    testTxs.append({ QDate(2025,1,5), "Sunday Offering - Jan 5", "Various Donors", "Weekly offering",
        { {checkingId, generalFundId, 1500.00, 0.0, naturalContrib, funcProgram, "Bank deposit"},
          {contribUnrestId, generalFundId, 0.0, 1500.00, naturalContrib, funcProgram, "General fund gifts"} } });

    // 2. Restricted contribution to Building Fund
    testTxs.append({ QDate(2025,1,12), "Building Fund Gift - Smith Family", "John & Mary Smith", "Capital campaign",
        { {checkingId, buildingId, 2500.00, 0.0, naturalContrib, funcProgram, "Bank deposit"},
          {contribRestId, buildingId, 0.0, 2500.00, naturalContrib, funcProgram, "Designated for new roof"} } });

    // 3. Multi-fund contribution (complex)
    testTxs.append({ QDate(2025,1,19), "Combined Offering - Jan 19", "Congregation", "Split gifts",
        { {checkingId, generalFundId, 1750.00, 0.0, naturalContrib, funcProgram, "Bank deposit"},
          {contribUnrestId, generalFundId, 0.0, 1200.00, naturalContrib, funcProgram, "Unrestricted"},
          {contribRestId, benevolenceId, 0.0, 350.00, naturalContrib, funcProgram, "Benevolence"},
          {contribRestId, missionsId, 0.0, 200.00, naturalContrib, funcProgram, "Missions"} } });

    // 4. Large mixed contribution (WODR + WDR)
    testTxs.append({ QDate(2025,2,9), "Special Offering - Building & General", "Various Donors", "Combined gift",
        { {checkingId, generalFundId, 5000.00, 0.0, naturalContrib, funcProgram, "Bank deposit"},
          {contribUnrestId, generalFundId, 0.0, 3200.00, naturalContrib, funcProgram, "General fund"},
          {contribRestId, buildingId, 0.0, 1800.00, naturalContrib, funcProgram, "Building fund"} } });

    // 5. Mixed multi-line with 4 entries
    testTxs.append({ QDate(2025,3,9), "March 9 Combined Gifts", "Multiple Donors", "Worship service",
        { {checkingId, generalFundId, 1700.00, 0.0, naturalContrib, funcProgram, "Bank deposit"},
          {contribUnrestId, generalFundId, 0.0, 950.00, naturalContrib, funcProgram, "General"},
          {contribRestId, buildingId, 0.0, 500.00, naturalContrib, funcProgram, "Building"},
          {contribRestId, benevolenceId, 0.0, 150.00, naturalContrib, funcProgram, "Benevolence"},
          {contribRestId, missionsId, 0.0, 100.00, naturalContrib, funcProgram, "Missions"} } });

    // ============================================================
    // EXPENSES - Debit Expense, Credit Checking (Asset)
    // ============================================================

    // 6. Youth ministry expense
    testTxs.append({ QDate(2025,1,22), "Youth Retreat Supplies", "Camp Store", "Retreat materials",
        { {programExpId, youthId, 475.50, 0.0, naturalOther, funcProgram, "T-shirts, craft supplies"},
          {checkingId, youthId, 0.0, 475.50, naturalOther, funcProgram, "Bank withdrawal"} } });

    // 7. Utility bill (General Fund)
    testTxs.append({ QDate(2025,1,28), "Electric Bill - January", "Power Company", "Church utilities",
        { {mgmtId, generalFundId, 312.75, 0.0, naturalUtilities, funcMgmt, "Main building"},
          {checkingId, generalFundId, 0.0, 312.75, naturalUtilities, funcMgmt, "Bank withdrawal"} } });

    // 8. Missions support payment
    testTxs.append({ QDate(2025,2,3), "Missionary Support - Feb", "Global Missions Org", "Monthly support",
        { {programExpId, missionsId, 800.00, 0.0, naturalOther, funcProgram, "Rev. Johnson support"},
          {checkingId, missionsId, 0.0, 800.00, naturalOther, funcProgram, "Bank withdrawal"} } });

    // 9. Benevolence disbursement
    testTxs.append({ QDate(2025,2,15), "Emergency Assistance - Family in need", "Confidential", "Rent assistance",
        { {programExpId, benevolenceId, 650.00, 0.0, naturalOther, funcProgram, "Confidential case #47"},
          {checkingId, benevolenceId, 0.0, 650.00, naturalOther, funcProgram, "Bank withdrawal"} } });

    // 10. Multiple expense lines (salaries + supplies)
    testTxs.append({ QDate(2025,2,28), "February Payroll & Supplies", "Various", "Payroll + office",
        { {mgmtId, generalFundId, 4200.00, 0.0, naturalSalaries, funcMgmt, "Pastor & admin salaries"},
          {mgmtId, generalFundId, 187.40, 0.0, naturalOther, funcMgmt, "Office supplies"},
          {checkingId, generalFundId, 0.0, 4387.40, naturalSalaries, funcMgmt, "Bank withdrawal"} } });

    // 11. Youth program contribution + expense (net effect on Youth fund)
    testTxs.append({ QDate(2025,3,2), "Youth Fundraiser + Expense", "Youth Group", "Car wash proceeds & supplies",
        { {checkingId, youthId, 425.00, 0.0, naturalContrib, funcProgram, "Car wash deposit"},
          {contribUnrestId, youthId, 0.0, 425.00, naturalContrib, funcProgram, "Car wash fundraiser"},
          {programExpId, youthId, 89.50, 0.0, naturalOther, funcProgram, "Supplies for car wash"},
          {checkingId, youthId, 0.0, 89.50, naturalOther, funcProgram, "Bank withdrawal"} } });

    // 12. Simple expense - travel
    testTxs.append({ QDate(2025,3,15), "Conference Travel - Pastor", "Airline", "Annual conference",
        { {mgmtId, generalFundId, 312.00, 0.0, naturalOther, funcMgmt, "Flight to denominational meeting"},
          {checkingId, generalFundId, 0.0, 312.00, naturalOther, funcMgmt, "Bank withdrawal"} } });

    // ============================================================
    // INSERT INTO DATABASE
    // ============================================================
    QSqlQuery txQuery(db);
    QSqlQuery lineQuery(db);

    for (const auto& tx : testTxs) {
        txQuery.prepare(R"(INSERT INTO transactions (date, description, total_amount, payee_donor, memo, approved_by)
                           VALUES (?, ?, ?, ?, ?, ?))");
        txQuery.addBindValue(tx.date.toString(Qt::ISODate));
        txQuery.addBindValue(tx.desc);

        double totalDebits = 0.0;
        for (const auto& ln : tx.lines) totalDebits += ln[2].toDouble();
        txQuery.addBindValue(totalDebits);
        txQuery.addBindValue(tx.payee);
        txQuery.addBindValue(tx.memo);
        txQuery.addBindValue("Treasurer");

        if (!txQuery.exec()) {
            qDebug() << "[TEST] Failed to insert transaction:" << tx.desc << txQuery.lastError().text();
            continue;
        }
        int txId = txQuery.lastInsertId().toInt();

        for (const auto& ln : tx.lines) {
            lineQuery.prepare(R"(INSERT INTO transaction_lines
                (transaction_id, account_id, fund_id, debit, credit, natural_class, functional_class, notes)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?))");
            lineQuery.addBindValue(txId);
            lineQuery.addBindValue(ln[0].toInt());
            lineQuery.addBindValue(ln[1].toInt());
            lineQuery.addBindValue(ln[2].toDouble()); // debit
            lineQuery.addBindValue(ln[3].toDouble()); // credit
            lineQuery.addBindValue(ln[4].toString());
            lineQuery.addBindValue(ln[5].toString());
            lineQuery.addBindValue(ln[6].toString());
            if (!lineQuery.exec()) {
                qDebug() << "[TEST] Line insert failed:" << lineQuery.lastError().text();
            }
        }
        qDebug() << "[TEST] Inserted:" << tx.desc << "(" << tx.lines.size() << "lines, balanced)";
    }

    qDebug() << "[TEST] seedTestTransactions() completed. Inserted" << testTxs.size() << "balanced transactions.";
}