// server/server.h
#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QSet>
#include "database.h"
#include "protocol.h"

class Server;

class ClientConnection : public QObject {
    Q_OBJECT
public:
    ClientConnection(QTcpSocket *sock, Server *srv);
    int  userId() const { return m_uid; }
    void setUserId(int id) { m_uid = id; }

    void send(Proto::Type t, const QByteArray &body);
    void sendJson(Proto::Type t, const QJsonObject &o);

    void closeStocket(){m_sock->close();}
private slots:
    void onReadyRead();
    void onDisconnected();
signals:
    void sigDisconnected();//踢人用

private:
    QTcpSocket *m_sock;
    Server     *m_srv;
    QByteArray  m_buf;
    int         m_uid = -1;
};

class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    bool start(quint16 port, const QString &dbPath);

    void onPacket(ClientConnection *c, Proto::Type t, const QByteArray &body);
    void onClosed(ClientConnection *c);

    void sendTo(int uid, Proto::Type t, const QByteArray &body);
    void sendJsonTo(int uid, Proto::Type t, const QJsonObject &o);
    Database &db() { return m_db; }

private:
    void handleLogin(ClientConnection *c, const QJsonObject &o);
    void handleRegister(ClientConnection *c, const QJsonObject &o);
    void handleSendText(ClientConnection *c, const QJsonObject &o);
    void handleSendImage(ClientConnection *c, const QByteArray &body);
    void handleHistory(ClientConnection *c, const QJsonObject &o);
    void handleFetchImage(ClientConnection *c, const QJsonObject &o);
    void handleCreateGroup(ClientConnection *c, const QJsonObject &o);
    void deliverMessage(const QJsonObject &msg, int senderId);

    QTcpServer  m_tcp;
    Database    m_db;
    QHash<int, ClientConnection *> m_online;   // userId -> conn
};
