#ifndef SQLITEDB_H
#define SQLITEDB_H

#include <QObject>
#include <QtSql>

class SQLiteDB : public QObject
{
    Q_OBJECT
public:
    explicit SQLiteDB(QObject *parent = 0);
    ~SQLiteDB();

private:
    QSqlDatabase myDB;

public slots:
    void AddContact(QString, QString, int, QString, QString);
    QString FindInDB(QString);
    bool CorrectInput(QString, QString);
};

#endif // SQLITEDB_H
