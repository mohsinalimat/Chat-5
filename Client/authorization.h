#ifndef AUTHORIZATION_H
#define AUTHORIZATION_H

#include <QDialog>
#include <QKeyEvent>
#include <QTcpSocket>
#include "registration.h"
#include <QTime>


namespace Ui {
class Authorization;
}

class Authorization : public QWidget
{
    Q_OBJECT

public:
    explicit Authorization(QWidget *parent = 0);
    virtual void keyPressEvent(QKeyEvent* event) { key = event->key(); }
    virtual void keyReleaseEvent(QKeyEvent* event);

    ~Authorization();
    bool ok;
    Ui::Authorization *ui;
    Registration *registration;

private:
    QPoint m_dragPosition;
    HashMD5 *passhash;
    int key;

protected:
    QTcpSocket *socket;
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);

signals:
    void sendData(QString str, QString pas, QString pubKey);
    void sendFindContact(QString str);

private slots:
    void onButtonSendUser();
    void recieveData(QString rStr);
    void getMessage();

private slots:
    void on_reg_button_clicked();
    void on_close_window_clicked();
    void on_turn_window_clicked();
    void on_log_in_clicked();
};

#endif // AUTHORIZATION_H
