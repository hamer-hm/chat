// client/networkclient.h
#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include "protocol.h"

class NetworkClient : public QObject {
    Q_OBJECT
public:
    static NetworkClient &inst();

    void connectTo(const QString &host, quint16 port);
    bool isConnected() const {
        return m_sock.state() == QAbstractSocket::ConnectedState;
    }
    void send(Proto::Type t, const QByteArray &body);
    void sendJson(Proto::Type t, const QJsonObject &o);

    // 登录后填充
    int      myId = -1;
    QString  myName;
    QJsonArray users;      // [{id,username,nickname}]
    QJsonArray groups;     // [{id,name,owner,members}]
    QJsonArray sessions;   // [{targetType,targetId,msgType,text,ts,senderId}]

    QHash<int, QString> userNames;    //id->昵称（用于气泡显示）
    QHash<int, QString> groupNames;

signals:
    void packet(Proto::Type t, const QByteArray &body);
    void connected();
    void disconnected();

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    NetworkClient();
    QTcpSocket m_sock;
    QByteArray m_buf;
};
