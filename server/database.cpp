// server/database.cpp
#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <limits>

//密码加密
static QString sha256(const QString &s) {
    return QString::fromLatin1(
        QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha256).toHex());
}

//打开数据库
bool Database::open(const QString &path) {
    m_db = QSqlDatabase::addDatabase("QSQLITE", "wechat");
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "open db failed:" << m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    //设置wal模式
    q.exec("PRAGMA journal_mode=WAL");

    //建表
    const char *ddl[] = {
        "CREATE TABLE IF NOT EXISTS users("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"    //用户
        " username TEXT UNIQUE NOT NULL,"
        " password TEXT NOT NULL,"
        " nickname TEXT NOT NULL)",

        "CREATE TABLE IF NOT EXISTS chat_groups("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"       //群聊
        " name TEXT NOT NULL,"
        " owner_id INTEGER NOT NULL)",

        "CREATE TABLE IF NOT EXISTS group_members("
        " group_id INTEGER NOT NULL,"                 //群成员
        " user_id  INTEGER NOT NULL,"
        " PRIMARY KEY(group_id,user_id))",

        "CREATE TABLE IF NOT EXISTS messages("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"         //消息表
        " sender_id   INTEGER NOT NULL,"
        " target_type INTEGER NOT NULL,"   // 0=用户 1=群
        " target_id   INTEGER NOT NULL,"
        " msg_type    INTEGER NOT NULL,"   // 0=文本 1=图片
        " content     TEXT DEFAULT '',"
        " image       BLOB,"
        " ts          INTEGER NOT NULL)",

        "CREATE INDEX IF NOT EXISTS idx_msg ON messages(target_type,target_id,id)",
    };
    for (const char *s : ddl)
        if (!q.exec(QString::fromUtf8(s)))
            qWarning() << q.lastError().text();

    return true;
}

//注册
int Database::registerUser(const QString &u, const QString &p, const QString &n) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO users(username,password,nickname) VALUES(?,?,?)");
    q.addBindValue(u);
    q.addBindValue(sha256(p));
    q.addBindValue(n);
    if (!q.exec()) return -1;      // 用户名重复
    return q.lastInsertId().toInt();
}

//登录
int Database::login(const QString &u, const QString &p) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM users WHERE username=? AND password=?");
    q.addBindValue(u);
    q.addBindValue(sha256(p));
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

//按id查找用户
QJsonObject Database::userInfo(int uid) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id,username,nickname FROM users WHERE id=?");
    q.addBindValue(uid);
    q.exec();
    if (q.next())
        return {{"id", q.value(0).toInt()},
                {"username", q.value(1).toString()},
                {"nickname", q.value(2).toString()}};
    return {};
}

//查找所有用户
QJsonArray Database::allUsers() {
    QJsonArray a;
    QSqlQuery q(m_db);
    q.exec("SELECT id,username,nickname FROM users ORDER BY id");
    while (q.next())
        a.append(QJsonObject{{"id", q.value(0).toInt()},
                             {"username", q.value(1).toString()},
                             {"nickname", q.value(2).toString()}});
    return a;
}
//查找群聊成员
QList<int> Database::groupMembers(int gid) {
    QList<int> r;
    QSqlQuery q(m_db);
    q.prepare("SELECT user_id FROM group_members WHERE group_id=?");
    q.addBindValue(gid);
    q.exec();
    while (q.next()) r << q.value(0).toInt();
    return r;
}

//查找用户是不是群成员
bool Database::isMember(int gid, int uid) {
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM group_members WHERE group_id=? AND user_id=?");
    q.addBindValue(gid);
    q.addBindValue(uid);
    return q.exec() && q.next();
}

//根据用户uid，查询该用户加入的所有群聊
QJsonArray Database::groupsOf(int uid) {
    QJsonArray a;
    QSqlQuery q(m_db);
    q.prepare("SELECT g.id,g.name,g.owner_id FROM chat_groups g "
              "JOIN group_members m ON m.group_id=g.id WHERE m.user_id=?");
    q.addBindValue(uid);
    q.exec();
    while (q.next()) {
        const int gid = q.value(0).toInt();
        QJsonArray members;
        for (int m : groupMembers(gid)) members.append(m);
        a.append(QJsonObject{{"id", gid},
                             {"name", q.value(1).toString()},
                             {"owner", q.value(2).toInt()},
                             {"members", members}});
    }
    return a;
}
//创建群聊
int Database::createGroup(const QString &name, int owner, const QList<int> &members) {
    QSqlQuery q(m_db);
    //将数据插入群聊表
    q.prepare("INSERT INTO chat_groups(name,owner_id) VALUES(?,?)");
    q.addBindValue(name);
    q.addBindValue(owner);
    if (!q.exec()) return -1;
    const int gid = q.lastInsertId().toInt();

    QList<int> all = members;
    if (!all.contains(owner)) all.append(owner);
    //将数据插入群聊成员表
    q.prepare("INSERT OR IGNORE INTO group_members(group_id,user_id) VALUES(?,?)");
    for (int u : all) {
        q.addBindValue(gid);
        q.addBindValue(u);
        q.exec();
    }
    return gid;
}

//添加消息
qint64 Database::addMessage(int sender, int ttype, int tid, int mtype,
                            const QString &text, const QByteArray &img) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO messages(sender_id,target_type,target_id,msg_type,"
              "content,image,ts) VALUES(?,?,?,?,?,?,?)");
    q.addBindValue(sender);
    q.addBindValue(ttype);
    q.addBindValue(tid);
    q.addBindValue(mtype);
    q.addBindValue(text);
    q.addBindValue(img.isEmpty() ? QVariant() : QVariant(img));
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        qWarning() << "addMessage:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}
//查找图片
QByteArray Database::imageOf(qint64 msgId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT image FROM messages WHERE id=?");
    q.addBindValue(msgId);
    if (q.exec() && q.next()) return q.value(0).toByteArray();
    return {};
}
//历史消息
QJsonArray Database::history(int uid, int ttype, int tid, qint64 beforeId, int limit) {
    if (beforeId <= 0) beforeId = std::numeric_limits<qint64>::max();

    QSqlQuery q(m_db);
    //私聊
    if (ttype == 0) {
        q.prepare("SELECT m.id,m.sender_id,m.msg_type,m.content,m.ts,u.nickname "
                  "FROM messages m LEFT JOIN users u ON u.id=m.sender_id "
                  "WHERE m.target_type=0 "
                  "AND ((m.sender_id=? AND m.target_id=?) OR (m.sender_id=? AND m.target_id=?)) "
                  "AND m.id<? ORDER BY m.id DESC LIMIT ?");
        q.addBindValue(uid);
        q.addBindValue(tid);
        q.addBindValue(tid);
        q.addBindValue(uid);
        q.addBindValue(beforeId);
        q.addBindValue(limit);
    } else {
        //群聊
        q.prepare("SELECT m.id,m.sender_id,m.msg_type,m.content,m.ts,u.nickname "
                  "FROM messages m LEFT JOIN users u ON u.id=m.sender_id "
                  "WHERE m.target_type=1 AND m.target_id=? AND m.id<? "
                  "ORDER BY m.id DESC LIMIT ?");
        q.addBindValue(tid);
        q.addBindValue(beforeId);
        q.addBindValue(limit);
    }
    q.exec();

    QList<QJsonObject> tmp;
    while (q.next()) {
        tmp.append(QJsonObject{
                                {"id",       q.value(0).toLongLong()},
                                {"senderId", q.value(1).toInt()},
                                {"msgType",  q.value(2).toInt()},
                                {"text",     q.value(3).toString()},
                                {"ts",       q.value(4).toLongLong()},          //时间戳
                                {"senderName", q.value(5).toString()},
                                });
    }
    QJsonArray a;
    for (const auto &o : tmp) a.append(o);
    return a;
}

//聊天首页会话列表，每个会话展示最后一条消息
QJsonArray Database::sessions(int uid) {
    QJsonArray out;

    //拿这条最新消息id去查消息内容
    auto appendLatest = [&](int ttype, int tid, qint64 mid) {
        QSqlQuery q2(m_db);
        q2.prepare("SELECT msg_type,content,ts,sender_id FROM messages WHERE id=?");
        q2.addBindValue(mid);
        if (q2.exec() && q2.next())
            out.append(QJsonObject{
                                   {"targetType", ttype},
                                   {"targetId",   tid},
                                   {"msgType",    q2.value(0).toInt()},
                                   {"text",       q2.value(1).toString()},
                                   {"ts",         q2.value(2).toLongLong()},
                                   {"senderId",   q2.value(3).toInt()},
                                   });
    };

    // ---- 私聊 ----
    QSqlQuery q(m_db);
    q.prepare("SELECT peer, MAX(mid) FROM ("
              " SELECT target_id AS peer, MAX(id) AS mid FROM messages "
              "   WHERE target_type=0 AND sender_id=? GROUP BY target_id"
              " UNION ALL"
              " SELECT sender_id AS peer, MAX(id) AS mid FROM messages "
              "   WHERE target_type=0 AND target_id=? GROUP BY sender_id"
              ") GROUP BY peer");
    q.addBindValue(uid);
    q.addBindValue(uid);
    q.exec();
    //群id+该群最新消息id
    QList<QPair<int, qint64>> peers;
    while (q.next()) peers.append({q.value(0).toInt(), q.value(1).toLongLong()});
    for (auto &p : peers) appendLatest(0, p.first, p.second);

    // ---- 群聊 ----
    QSqlQuery g(m_db);
    g.prepare("SELECT target_id, MAX(id) FROM messages "
              "WHERE target_type=1 AND target_id IN "
              "(SELECT group_id FROM group_members WHERE user_id=?) "
              "GROUP BY target_id");
    g.addBindValue(uid);
    g.exec();
    QList<QPair<int, qint64>> grps;
    while (g.next()) grps.append({g.value(0).toInt(), g.value(1).toLongLong()});
    for (auto &p : grps) appendLatest(1, p.first, p.second);

    return out;
}
