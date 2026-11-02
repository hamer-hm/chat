// client/loginwindow.cpp
#include "loginwindow.h"
#include "networkclient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>

LoginWindow::LoginWindow(QWidget *parent) : QDialog(parent) {
    setWindowTitle("WeChat 登录");
    setFixedSize(340, 420);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(32, 32, 32, 32);
    lay->setSpacing(12);

    auto *title = new QLabel("WeChat");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:30px;font-weight:bold;color:#07c160;");
    lay->addWidget(title);
    lay->addSpacing(12);

    auto mkEdit = [&](const QString &ph, QLineEdit::EchoMode mode = QLineEdit::Normal) {
        auto *e = new QLineEdit;
        e->setPlaceholderText(ph);
        e->setEchoMode(mode);
        e->setStyleSheet("padding:9px;border:1px solid #ddd;border-radius:4px;font-size:13px;");
        lay->addWidget(e);
        return e;
    };

    m_host = mkEdit("服务器地址");
    m_host->setText("127.0.0.1");
    m_port = mkEdit("端口");
    m_port->setText("8888");
    m_user = mkEdit("用户名");
    m_pwd  = mkEdit("密码", QLineEdit::Password);
    m_nick = mkEdit("昵称（注册时填写）");

    auto *row = new QHBoxLayout;
    auto *btnLogin = new QPushButton("登 录");
    auto *btnReg   = new QPushButton("注 册");
    btnLogin->setStyleSheet("background:#07c160;color:white;padding:9px;"
                            "border:none;border-radius:4px;font-size:14px;");
    btnReg->setStyleSheet("padding:9px;border:1px solid #ddd;border-radius:4px;font-size:14px;");
    row->addWidget(btnLogin, 1);
    row->addWidget(btnReg);
    lay->addSpacing(6);
    lay->addLayout(row);

    m_tip = new QLabel;
    m_tip->setAlignment(Qt::AlignCenter);
    m_tip->setStyleSheet("color:#e64340;font-size:12px;");
    m_tip->setWordWrap(true);
    lay->addWidget(m_tip);
    lay->addStretch();

    connect(btnLogin, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(btnReg,   &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    connect(&NetworkClient::inst(), &NetworkClient::packet, this, &LoginWindow::onPacket);
}

void LoginWindow::onLoginClicked() {
    m_action = 1;
    m_tip->setText("连接中…");
    auto &nc = NetworkClient::inst();
    if (nc.isConnected()) {
        doSend();
        return;
    }
    connect(&nc, &NetworkClient::connected, this, &LoginWindow::onConnected,
            Qt::UniqueConnection);
    nc.connectTo(m_host->text().trimmed(), quint16(m_port->text().toUShort()));
}

void LoginWindow::onRegisterClicked() {
    m_action = 2;
    m_tip->setText("连接中…");
    auto &nc = NetworkClient::inst();
    if (nc.isConnected()) {
        doSend();
        return;
    }
    connect(&nc, &NetworkClient::connected, this, &LoginWindow::onConnected,
            Qt::UniqueConnection);
    nc.connectTo(m_host->text().trimmed(), quint16(m_port->text().toUShort()));
}

void LoginWindow::onConnected() {
    doSend();
}

void LoginWindow::doSend() {
    auto &nc = NetworkClient::inst();
    //登录
    if (m_action == 1) {
        nc.sendJson(Proto::Type::Login, {
                                         {"username", m_user->text().trimmed()},
                                         {"password", m_pwd->text()},
                                         });
    } else {
        //注册
        nc.sendJson(Proto::Type::Register, {
                                            {"username", m_user->text().trimmed()},
                                            {"password", m_pwd->text()},
                                            {"nickname", m_nick->text().trimmed()},
                                            });
    }
}

void LoginWindow::onPacket(Proto::Type t, const QByteArray &body) {
    const QJsonObject o = QJsonDocument::fromJson(body).object();

    if (t == Proto::Type::RegisterResp) {
        m_tip->setStyleSheet(o["ok"].toBool() ? "color:#07c160;font-size:12px;"
                                              : "color:#e64340;font-size:12px;");
        m_tip->setText(o["msg"].toString());
        m_action = 0;
        return;
    }

    if (t == Proto::Type::LoginResp) {
        if (!o["ok"].toBool()) {
            m_tip->setText(o["msg"].toString());
            m_action = 0;
            return;
        }
        auto &nc = NetworkClient::inst();
        nc.myId   = o["userId"].toInt();
        nc.myName = o["nickname"].toString();
        nc.users  = o["users"].toArray();
        nc.groups = o["groups"].toArray();
        nc.sessions = o["sessions"].toArray();

        nc.userNames.clear();
        for (const auto &v : nc.users) {
            const QJsonObject u = v.toObject();
            nc.userNames.insert(u["id"].toInt(), u["nickname"].toString());
        }
        nc.groupNames.clear();
        for (const auto &v : nc.groups) {
            const QJsonObject g = v.toObject();
            nc.groupNames.insert(g["id"].toInt(), g["name"].toString());
        }
        accept();
    }
}
