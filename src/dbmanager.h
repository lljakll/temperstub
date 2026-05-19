#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDate>
#include <QList>
#include <QMap>

struct Fund {
    int id = 0;
    QString name;
    QString description;
    QString restriction_type; // "WODR" or "WDR"
    bool archived = false;  
    QString archived_date;      
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
    bool archived = false;
    QString archived_date;
};

struct SimpleLookup {
    int id;
    QString name;
    bool archived = false;
    QString archived_date;
};

class DbManager : public QObject {
    Q_OBJECT
public:
    explicit DbManager(QObject* parent = nullptr);
    bool initDatabase();
    QList<Fund> getAllFunds(bool includeArchived = false) const;
    QList<Transaction> getAllTransactions() const;
    QString generateNextTransactionId() const;    
    bool addTransaction(const Transaction& t);
    double getFundBalance(int fundId) const;
    double getWODRTotal() const;
    double getWDRTotal() const;
    QSqlDatabase db;
    QList<Account> getAllAccounts(bool includeArchived = false) const;
    QString getFundName(int fundId) const;
    QList<SimpleLookup> getAllNaturalClasses(bool includeArchived = false) const;
    QList<SimpleLookup> getAllFunctionalClasses(bool includeArchived = false) const;
    bool saveAttachment(int transactionId, const QString& sourceFilePath);
    QString getAttachmentPath(int transactionId) const;
    bool openAttachment(int transactionId) const;
    QMap<int, double> getAllFundBalances() const;   // NEW: efficient balance lookup

    void refreshLookupCache();                    // NEW: refresh cached lookups
    void seedTestTransactions();                  // Test data generator (comment out after use)

private:
    void createTables();
    void seedDefaultLookups();

    // Simple lookup cache
    mutable QList<Fund>         cachedFunds;
    mutable QList<Account>      cachedAccounts;
    mutable QList<SimpleLookup> cachedNatural;
    mutable QList<SimpleLookup> cachedFunctional;
    bool cacheValid = false;
};