// server/server.cpp
#include "server.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

/* ================= ClientConnection ================= */

ClientConnection::ClientConnection(QTcpSocket *sock, Server *srv)
    : QObject(srv), m_sock(sock), m_srv(srv) {
    m_sock->setParent(this);
    connect(m_sock, &QTcpSocket::readyRead,    this, &ClientConnection::onReadyRead);
    connect(m_sock, &QTcpSocket::disconnected, this, &ClientConnection::onDisconnected);
    // connect(m_sock, &QTcpSocket::disconnected, this, &ClientConnection::sigDisconnected);//踢人
}

void ClientConnection::send(Proto::Type t, const QByteArray &body) {
    if (m_sock->state() == QAbstractSocket::ConnectedState)
        m_sock->write(Proto::pack(t, body));
}

void ClientConnection::sendJson(Proto::Type t, const QJsonObject &o) {
    send(t, QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void ClientConnection::onReadyRead() {
    m_buf.append(m_sock->readAll());
    Proto::Type t;
    QByteArray body;
    while (Proto::unpack(m_buf, t, body))
        m_srv->onPacket(this, t, body);
}

void ClientConnection::onDisconnected() {
    m_srv->onClosed(this);
    deleteLater();
}

/* ================= Server ================= */

Server::Server(QObject *parent) : QObject(parent) {}

bool Server::start(quint16 port, const QString &dbPath) {
    if (!m_db.open(dbPath)) return false;

    connect(&m_tcp, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *s = m_tcp.nextPendingConnection())
            new ClientConnection(s, this);       //自己管理生命周期
    });

    if (!m_tcp.listen(QHostAddress::Any, port)) {
        qWarning() << "listen failed:" << m_tcp.errorString();
        return false;
    }
    qInfo() << "WeChat server listening on port" << port;
    return true;
}

void Server::sendTo(int uid, Proto::Type t, const QByteArray &body) {
    if (auto *c = m_online.value(uid)) c->send(t, body);
}

void Server::sendJsonTo(int uid, Proto::Type t, const QJsonObject &o) {
    if (auto *c = m_online.value(uid)) c->sendJson(t, o);
}

void Server::onClosed(ClientConnection *c) {
    if (c->userId() >= 0 && m_online.value(c->userId()) == c)
    {
        qDebug() << "[onClosed] conn=" << c << " userId=" << c->userId();
        m_online.remove(c->userId());
    }
}

void Server::onPacket(ClientConnection *c, Proto::Type t, const QByteArray &body) {
    switch (t) {
        //登录
    case Proto::Type::Login:
        handleLogin(c, QJsonDocument::fromJson(body).object());
        break;
        //注册
    case Proto::Type::Register:
        handleRegister(c, QJsonDocument::fromJson(body).object());
        break;
        //发文字消息
    case Proto::Type::SendText:
        handleSendText(c, QJsonDocument::fromJson(body).object());
        break;
        //发图片
    case Proto::Type::SendImage:
        handleSendImage(c, body);
        break;
        //拉取历史消息
    case Proto::Type::FetchHistory:
        handleHistory(c, QJsonDocument::fromJson(body).object());
        break;
        //拉取图片
    case Proto::Type::FetchImage:
        handleFetchImage(c, QJsonDocument::fromJson(body).object());
        break;
        //建群
    case Proto::Type::CreateGroup:
        handleCreateGroup(c, QJsonDocument::fromJson(body).object());
        break;
    default:
        break;
    }
}
//处理客户端发来的注册请求
void Server::handleRegister(ClientConnection *c, const QJsonObject &o) {
    const QString u = o["username"].toString().trimmed();
    const QString p = o["password"].toString();
    QString n = o["nickname"].toString().trimmed();
    if (u.isEmpty() || p.isEmpty()) {
        c->sendJson(Proto::Type::RegisterResp, {{"ok", false}, {"msg", "用户名/密码不能为空"}});
        return;
    }
    if (n.isEmpty()) n = u;

    const int uid = m_db.registerUser(u, p, n);
    if (uid < 0)
        c->sendJson(Proto::Type::RegisterResp, {{"ok", false}, {"msg", "用户名已存在"}});
    else
        c->sendJson(Proto::Type::RegisterResp, {{"ok", true}, {"msg", "注册成功，请登录"}});
}

//处理客户端发来的登录请求
void Server::handleLogin(ClientConnection *c, const QJsonObject &o) {
    const QString u = o["username"].toString();
    const QString p = o["password"].toString();
    const int uid = m_db.login(u, p);
    if (uid < 0) {
        c->sendJson(Proto::Type::LoginResp, {{"ok", false}, {"msg", "用户名或密码错误"}});
        return;
    }


    // qDebug() << "\n==== handleLogin ====";
    // qDebug() << "uid=" << uid << " 当前连接c=" << c;
    // qDebug() << "m_online.contains(uid):" << m_online.contains(uid);
    // if(m_online.contains(uid))
    // {
    //     qDebug() << "m_online存的旧连接地址:" << m_online.value(uid);
    // }



    //同一账号重复登录：踢掉旧连接
    if (m_online.contains(uid) && m_online.value(uid) != c) {
        ClientConnection *old = m_online.take(uid);
        qDebug() << "准备踢uid:" << uid;
        old->sendJson(Proto::Type::ErrorMsg, {{"msg", "账号在其他设备登录"}});
        // connect(old, &ClientConnection::sigDisconnected, old, &ClientConnection::deleteLater);
        old->closeStocket();
        // old->deleteLater();
    }
    m_online[uid] = c;
    c->setUserId(uid);

    const QJsonObject me = m_db.userInfo(uid);
    c->sendJson(Proto::Type::LoginResp, {
                                         {"ok",       true},
                                         {"userId",   uid},
                                         {"nickname", me["nickname"].toString()},
                                         {"users",    m_db.allUsers()},
                                         {"groups",   m_db.groupsOf(uid)},
                                         {"sessions", m_db.sessions(uid)},
                                         });
    qInfo() << "user login:" << u << "id =" << uid;
}
//消息分发
//收到一条消息，找到所有要接收这条消息的用户，把消息推送给他们，同时发送者自己也收到
void Server::deliverMessage(const QJsonObject &msg, int senderId) {
    const int ttype = msg["targetType"].toInt();
    const int tid   = msg["targetId"].toInt();

    QSet<int> receivers;
    if (ttype == 0) {
        receivers.insert(tid);
    } else {
        for (int u : m_db.groupMembers(tid)) receivers.insert(u);
    }
    receivers.insert(senderId);          //回显给自己（多端同步）

    for (int u : receivers) sendJsonTo(u, Proto::Type::NewMessage, msg);
}
//处理客户端发来的发送文本消息请求
void Server::handleSendText(ClientConnection *c, const QJsonObject &o) {
    const int uid = c->userId();
    if (uid < 0) return;

    const int ttype = o["targetType"].toInt();
    const int tid   = o["targetId"].toInt();
    const QString text = o["text"].toString();
    if (text.isEmpty()) return;
    if (ttype == 1 && !m_db.isMember(tid, uid)) return;   // 群成员校验

    const qint64 id = m_db.addMessage(uid, ttype, tid, 0, text, {});
    if (id < 0) return;

    QJsonObject msg{
                    {"id", id}, {"senderId", uid},
                    {"senderName", m_db.userInfo(uid)["nickname"].toString()},
                    {"targetType", ttype}, {"targetId", tid},
                    {"msgType", 0}, {"text", text},
                    {"ts", QDateTime::currentSecsSinceEpoch()},
                    };
    deliverMessage(msg, uid);
}
//处理客户端发来的发送图片消息请求
void Server::handleSendImage(ClientConnection *c, const QByteArray &body) {
    const int uid = c->userId();
    if (uid < 0) return;

    QJsonObject meta;
    QByteArray img;
    if (!Proto::unpackBinary(body, meta, img) || img.isEmpty()) return;
    if (img.size() > 20 * 1024 * 1024) return;            // 单图上限 20MB

    const int ttype = meta["targetType"].toInt();
    const int tid   = meta["targetId"].toInt();
    if (ttype == 1 && !m_db.isMember(tid, uid)) return;

    const qint64 id = m_db.addMessage(uid, ttype, tid, 1, "", img);
    if (id < 0) return;

    //广播时不带图片数据，接收端按需FetchImage拉取
    QJsonObject msg{
                    {"id", id}, {"senderId", uid},
                    {"senderName", m_db.userInfo(uid)["nickname"].toString()},
                    {"targetType", ttype}, {"targetId", tid},
                    {"msgType", 1}, {"text", ""},
                    {"w", meta["w"].toInt()}, {"h", meta["h"].toInt()},
                    {"ts", QDateTime::currentSecsSinceEpoch()},
                    };
    deliverMessage(msg, uid);
}
//处理客户端发来的拉取历史消息请求
void Server::handleHistory(ClientConnection *c, const QJsonObject &o) {
    const int uid = c->userId();
    if (uid < 0) return;

    const int ttype = o["targetType"].toInt();
    const int tid   = o["targetId"].toInt();
    const qint64 before = qint64(o["beforeId"].toDouble());
    int limit = o["limit"].toInt(50);
    if (limit <= 0 || limit > 200) limit = 50;

    c->sendJson(Proto::Type::HistoryResp, {
                                           {"targetType", ttype},
                                           {"targetId",   tid},
                                           {"messages",   m_db.history(uid, ttype, tid, before, limit)},
                                           });
}
//处理客户端发来的拉取图片请求
void Server::handleFetchImage(ClientConnection *c, const QJsonObject &o) {
    const qint64 id = qint64(o["msgId"].toDouble());
    const QByteArray img = m_db.imageOf(id);
    c->send(Proto::Type::ImageResp,
            Proto::packBinary({{"msgId", double(id)}}, img));
}

//处理客户端发来的建群请求
void Server::handleCreateGroup(ClientConnection *c, const QJsonObject &o) {
    const int uid = c->userId();
    if (uid < 0) return;

    QList<int> members;
    for (const auto &v : o["members"].toArray()) members << v.toInt();
    if (members.isEmpty()) return;

    //群聊名称
    QString name = o["name"].toString().trimmed();
    //为空兜底
    if (name.isEmpty()) name = QString("群聊(%1)").arg(members.size() + 1);

    const int gid = m_db.createGroup(name, uid, members);
    if (gid < 0) return;

    QJsonArray ma;
    const QList<int> all = m_db.groupMembers(gid);
    for (int m : all) ma.append(m);

    QJsonObject g{{"id", gid}, {"name", name}, {"owner", uid}, {"members", ma}};
    for (int m : all) sendJsonTo(m, Proto::Type::GroupCreated, g);
}
