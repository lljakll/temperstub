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
    QString restrictionType; // "WODR" or "WDR"
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

class DbManager : public QObject {
    Q_OBJECT
public:
    explicit DbManager(QObject* parent = nullptr);
    bool initDatabase();
    QList<Fund> getAllFunds() const;
    QList<Transaction> getAllTransactions() const;
    bool addTransaction(const Transaction& t);
    double getFundBalance(int fundId) const;
    double getWODRTotal() const;
    double getWDRTotal() const;
    QSqlDatabase db;

private:
    void createTables();
    void seedInitialFunds(); // Guide §5.1
};