// server/database.h
#pragma once
#include <QSqlDatabase>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QByteArray>

class Database {
public:
    bool open(const QString &path);

    int  registerUser(const QString &user, const QString &pwd, const QString &nick);
    int  login(const QString &user, const QString &pwd);
    QJsonObject userInfo(int uid);
    QJsonArray  allUsers();

    QJsonArray  groupsOf(int uid);
    int         createGroup(const QString &name, int owner, const QList<int> &members);
    QList<int>  groupMembers(int gid);
    bool        isMember(int gid, int uid);

    qint64      addMessage(int sender, int ttype, int tid, int mtype,
                      const QString &text, const QByteArray &img);
    QByteArray  imageOf(qint64 msgId);
    QJsonArray  history(int uid, int ttype, int tid, qint64 beforeId, int limit);
    QJsonArray  sessions(int uid);

private:
    QSqlDatabase m_db;
};
