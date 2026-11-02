// client/mainwindow.h
#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QHash>
#include "protocol.h"
#include "messagebubble.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onPacket(Proto::Type t, const QByteArray &body);
    void onSessionClicked(QListWidgetItem *item);
    void onSendText();
    void onPickImage();
    void onCreateGroup();

private:
    void setupUi();
    void buildSessionList();
    void addSessionItem(int ttype, int tid, const QString &name);
    void updateSessionPreview(int ttype, int tid, const QString &text);
    void switchToSession(int ttype, int tid, const QString &title);
    void appendMessage(const QJsonObject &msg,bool prepend);
    void handleNewMessage(const QJsonObject &msg);
    void clearMessages();
    void scrollToBottom();
    void preserveScroll(qint64 oldValue, qint64 oldMax);
    static QString skey(int ttype, int tid) {
        return QString("%1_%2").arg(ttype).arg(tid);
    }

    QListWidget    *m_sessionList = nullptr;
    QScrollArea    *m_scroll = nullptr;
    QWidget        *m_msgContainer = nullptr;
    QVBoxLayout    *m_msgLayout = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QLabel         *m_title = nullptr;

    int m_curType = -1;
    int m_curId   = -1;

    QHash<QString, QListWidgetItem *> m_sessionItems;
    QHash<qint64, MessageBubble *>    m_bubbles;

    quint64 m_beforeId=0;
    bool m_isLoadingHistory = false;
    int m_oldBarValue=0;
    int m_oldBarMax=0;
    int m_newBarMax=0;
    int first=0;
    bool m_sendMsg=false;
};
