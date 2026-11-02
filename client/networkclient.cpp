// client/networkclient.cpp
#include "networkclient.h"
#include <QJsonDocument>

NetworkClient &NetworkClient::inst() {
    static NetworkClient c;
    return c;
}

NetworkClient::NetworkClient() {
    connect(&m_sock, &QTcpSocket::readyRead,    this, &NetworkClient::onReadyRead);
    connect(&m_sock, &QTcpSocket::connected,    this, &NetworkClient::connected);
    connect(&m_sock, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
}

void NetworkClient::connectTo(const QString &host, quint16 port) {
    m_sock.abort();
    m_buf.clear();
    m_sock.connectToHost(host, port);
}

void NetworkClient::send(Proto::Type t, const QByteArray &body) {
    if (m_sock.state() == QAbstractSocket::ConnectedState)
        m_sock.write(Proto::pack(t, body));
}

void NetworkClient::sendJson(Proto::Type t, const QJsonObject &o) {
    send(t, QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void NetworkClient::onReadyRead() {
    m_buf.append(m_sock.readAll());
    Proto::Type t;
    QByteArray body;
    while (Proto::unpack(m_buf, t, body))
        emit packet(t, body);
}

void NetworkClient::onDisconnected() {
    emit disconnected();
}
