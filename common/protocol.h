// #common/protocol.h —— 客户端/服务端共用协议
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QtEndian>

namespace Proto {

enum class Type : quint8 {
    // ---- 客户端 -> 服务端 ----
    Login        = 1,
    Register     = 2,
    SendText     = 10,
    SendImage    = 11,
    FetchHistory = 12,
    FetchImage   = 13,
    FetchUsers   = 14,
    CreateGroup  = 15,

    // ---- 服务端 -> 客户端 ----
    LoginResp    = 100,
    RegisterResp = 101,
    NewMessage   = 110,
    HistoryResp  = 112,
    ImageResp    = 113,
    UsersResp    = 114,
    GroupCreated = 115,
    ErrorMsg     = 199,
};

//帧格式:[quint32 totalLen][quint8 type][body]
//totalLen=1+body.size()
inline QByteArray pack(Type t, const QByteArray &body) {
    QByteArray out;
    const quint32 total = quint32(1 + body.size());
    out.resize(4);
    qToBigEndian<quint32>(total, out.data());
    out.append(char(quint8(t)));
    out.append(body);
    return out;
}

inline QByteArray packJson(Type t, const QJsonObject &o) {
    return pack(t, QJsonDocument(o).toJson(QJsonDocument::Compact));
}

//从缓冲区取出一个完整帧；返回false表示数据不足
inline bool unpack(QByteArray &buf, Type &t, QByteArray &body) {
    if (buf.size() < 5) return false;
    const quint32 total =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(buf.constData()));
    if (total < 1 || total > 128u * 1024 * 1024) {   // 防脏包
        buf.clear();
        return false;
    }
    if (quint32(buf.size()) < 4 + total) return false;
    t = Type(quint8(buf.at(4)));
    body = buf.mid(5, int(total) - 1);
    buf.remove(0, int(4 + total));
    return true;
}

//二进制帧体:[quint32 metaLen][metaJson][raw bytes]
//用于图片收发
inline QByteArray packBinary(const QJsonObject &meta, const QByteArray &bin) {
    const QByteArray m = QJsonDocument(meta).toJson(QJsonDocument::Compact);
    QByteArray out;
    out.resize(4);
    qToBigEndian<quint32>(quint32(m.size()), out.data());
    out.append(m);
    out.append(bin);
    return out;
}

inline bool unpackBinary(const QByteArray &body, QJsonObject &meta, QByteArray &bin) {
    if (body.size() < 4) return false;
    const quint32 n =
        qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(body.constData()));
    if (quint32(body.size()) < 4 + n) return false;
    meta = QJsonDocument::fromJson(body.mid(4, int(n))).object();
    bin  = body.mid(4 + int(n));
    return true;
}

} // namespace Proto
