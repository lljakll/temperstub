#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDate>
#include <QList>

struct Fund {
    int id = 0;
    QString name;
    QString description;
    QString restriction_type; // "WODR" or "WDR"
};

struct Transaction {
    int id = 0;
    QDate date;
    QString description;
    double amount = 0.0;           // >0 receipt, <0 disbursement
    int fundId = 0;
    QString naturalClass;
    QString functionalClass;       // Program Services / Management & General / Fundraising
    QString notes;
    QString transactionType;       // contribution / disbursement / release / adjustment
    QDateTime createdAt;
};

struct Account {
    int id;
    QString code;
    QString name;
    QString type;
    int fundId;
};

struct SimpleLookup {
    int id;
    QString name;
};

class DbManager : public QObject {
    Q_OBJECT
public:
    explicit DbManager(QObject* parent = nullptr);
    bool initDatabase();
    QList<Fund> getAllFunds() const;
    QList<Transaction> getAllTransactions() const;
    QString generateNextTransactionId() const;    
    bool addTransaction(const Transaction& t);
    double getFundBalance(int fundId) const;
    double getWODRTotal() const;
    double getWDRTotal() const;
    QSqlDatabase db;
    QList<Account> getAllAccounts() const;
    QString getFundName(int fundId) const;
    QList<SimpleLookup> getAllNaturalClasses() const;
    QList<SimpleLookup> getAllFunctionalClasses() const;

private:
    void createTables();
    void seedDefaultLookups();
};