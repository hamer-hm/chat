// client/loginwindow.h
#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include "protocol.h"

class LoginWindow : public QDialog {
    Q_OBJECT
public:
    explicit LoginWindow(QWidget *parent = nullptr);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onConnected();
    void onPacket(Proto::Type t, const QByteArray &body);

private:
    void doSend();
    QLineEdit *m_host, *m_port, *m_user, *m_pwd, *m_nick;
    QLabel    *m_tip;
    int        m_action = 0;      // 1=登录 2=注册
};
