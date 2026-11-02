// client/mainwindow.cpp
#include "mainwindow.h"
#include "networkclient.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QFileDialog>
#include <QImage>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QScrollBar>
#include <QDialog>
#include <QLineEdit>
#include <QDateTime>
#include <QTimer>
#include <QSharedPointer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("微信 · %1").arg(NetworkClient::inst().myName));
    resize(1020, 700);
    setupUi();
    buildSessionList();

    connect(&NetworkClient::inst(), &NetworkClient::packet,
            this, &MainWindow::onPacket);
}

void MainWindow::setupUi() {
    auto *central = new QWidget;
    setCentralWidget(central);
    auto *h = new QHBoxLayout(central);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    /* ---------------- 左侧会话栏 ---------------- */
    auto *left = new QWidget;
    left->setFixedWidth(262);
    left->setStyleSheet("background:#2e2e2e;");
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);

    auto *topBar = new QWidget;
    topBar->setFixedHeight(58);
    topBar->setStyleSheet("background:#252525;");
    auto *tb = new QHBoxLayout(topBar);
    tb->setContentsMargins(12, 0, 12, 0);
    tb->setSpacing(10);

    const QString me = NetworkClient::inst().myName;
    auto *avatar = new QLabel(me.isEmpty() ? "?" : me.left(1).toUpper());
    avatar->setFixedSize(34, 34);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet("background:#07c160;color:white;border-radius:17px;"
                          "font-weight:bold;font-size:15px;");
    auto *meLabel = new QLabel(me);
    meLabel->setStyleSheet("color:#e0e0e0;font-size:14px;");
    tb->addWidget(avatar);
    tb->addWidget(meLabel);
    tb->addStretch();

    auto *addBtn = new QPushButton("＋");
    addBtn->setFixedSize(26, 26);
    addBtn->setToolTip(QStringLiteral("发起群聊"));
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet("QPushButton{background:#3a3a3a;color:#ddd;border:none;"
                          "border-radius:13px;font-size:16px;}"
                          "QPushButton:hover{background:#4a4a4a;}");
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onCreateGroup);
    tb->addWidget(addBtn);
    lv->addWidget(topBar);

    m_sessionList = new QListWidget;
    m_sessionList->setWordWrap(true);
    m_sessionList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_sessionList->setStyleSheet(R"(
        QListWidget { background:#2e2e2e; border:none; outline:none; }
        QListWidget::item {
            height:62px; color:#cfcfcf; padding:8px 12px;
            border-bottom:1px solid #383838; font-size:13px;
        }
        QListWidget::item:selected { background:#3d3d3d; }
        QListWidget::item:hover    { background:#373737; }
        QScrollBar:vertical { width:6px; background:transparent; }
        QScrollBar::handle:vertical { background:#555; border-radius:3px; }
    )");
    connect(m_sessionList, &QListWidget::itemClicked,
            this, &MainWindow::onSessionClicked);
    lv->addWidget(m_sessionList, 1);

    /* ---------------- 右侧聊天区 ---------------- */
    auto *right = new QWidget;
    right->setStyleSheet("background:#f2f2f2;");
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(0);

    m_title = new QLabel(QStringLiteral("请选择一个会话"));
    m_title->setFixedHeight(58);
    m_title->setStyleSheet("background:#f2f2f2;color:#222;font-size:16px;"
                           "font-weight:bold;padding-left:24px;"
                           "border-bottom:1px solid #e0e0e0;");
    rv->addWidget(m_title);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setStyleSheet("QScrollArea{border:none;background:#f2f2f2;}");
    m_msgContainer = new QWidget;
    m_msgContainer->setStyleSheet("background:#f2f2f2;");
    m_msgLayout = new QVBoxLayout(m_msgContainer);
    m_msgLayout->setContentsMargins(0, 12, 0, 12);
    m_msgLayout->setSpacing(0);
    m_msgLayout->addStretch(1);
    m_scroll->setWidget(m_msgContainer);

    //滚动条加载历史消息
    auto *vBar = m_scroll->verticalScrollBar();
    connect(vBar, &QScrollBar::valueChanged, this, [this, vBar](int) {
        if (m_isLoadingHistory)   return;
        if (m_beforeId <= 0)      return;          // 没有更早的消息了
        if (vBar->value() <= vBar->minimum() + 5) {
            m_oldBarValue = vBar->value();
            m_oldBarMax   = vBar->maximum();       // 在触发时刻记录
            m_isLoadingHistory = true;
            NetworkClient::inst().sendJson(Proto::Type::FetchHistory, {
                                                                          {"beforeId",   double(m_beforeId)},
                                                                          {"targetType", m_curType},
                                                                          {"targetId",   m_curId},
                                                                          {"limit",      15}
                                                                      });
        }
    });
    rv->addWidget(m_scroll, 1);

    auto *bottom = new QWidget;
    bottom->setFixedHeight(170);
    bottom->setStyleSheet("background:#ffffff;border-top:1px solid #e0e0e0;");
    auto *bv = new QVBoxLayout(bottom);
    bv->setContentsMargins(14, 8, 14, 10);
    bv->setSpacing(6);

    auto *toolRow = new QHBoxLayout;
    auto *imgBtn = new QPushButton(QStringLiteral("🖼  图片"));
    imgBtn->setCursor(Qt::PointingHandCursor);
    imgBtn->setStyleSheet("QPushButton{border:none;color:#5a5a5a;font-size:13px;"
                          "padding:3px 6px;}"
                          "QPushButton:hover{color:#07c160;}");
    connect(imgBtn, &QPushButton::clicked, this, &MainWindow::onPickImage);
    toolRow->addWidget(imgBtn);
    toolRow->addStretch();
    bv->addLayout(toolRow);

    m_input = new QPlainTextEdit;
    m_input->setPlaceholderText(QStringLiteral("输入消息，Ctrl + Enter 发送"));
    m_input->setStyleSheet("QPlainTextEdit{border:none;font-size:14px;color:#222;}");
    bv->addWidget(m_input, 1);

    auto *sendRow = new QHBoxLayout;
    sendRow->addStretch();
    auto *sendBtn = new QPushButton(QStringLiteral("发 送"));
    sendBtn->setFixedSize(86, 30);
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setStyleSheet("QPushButton{background:#07c160;color:white;border:none;"
                           "border-radius:4px;font-size:13px;}"
                           "QPushButton:hover{background:#06ad56;}");
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSendText);
    sendRow->addWidget(sendBtn);
    bv->addLayout(sendRow);

    rv->addWidget(bottom);

    h->addWidget(left);
    h->addWidget(right, 1);

    auto *sc1 = new QShortcut(QKeySequence("Ctrl+Return"), m_input);
    connect(sc1, &QShortcut::activated, this, &MainWindow::onSendText);
    auto *sc2 = new QShortcut(QKeySequence("Ctrl+Enter"), m_input);
    connect(sc2, &QShortcut::activated, this, &MainWindow::onSendText);
}

/* ---------------- 会话列表 ---------------- */

void MainWindow::buildSessionList() {
    auto &nc = NetworkClient::inst();
    m_sessionList->clear();
    m_sessionItems.clear();

    for (const auto &v : nc.users) {
        const QJsonObject u = v.toObject();
        const int id = u["id"].toInt();
        if (id == nc.myId) continue;
        addSessionItem(0, id, u["nickname"].toString());
    }
    for (const auto &v : nc.groups) {
        const QJsonObject g = v.toObject();
        addSessionItem(1, g["id"].toInt(), g["name"].toString());
    }
    for (const auto &v : nc.sessions) {
        const QJsonObject s = v.toObject();
        const int ttype = s["targetType"].toInt();
        const int tid   = s["targetId"].toInt();
        auto *it = m_sessionItems.value(skey(ttype, tid));
        if (!it) continue;
        const QString preview = s["msgType"].toInt() == 1
                                    ? QStringLiteral("[图片]")
                                    : s["text"].toString();
        it->setText(it->data(Qt::UserRole + 2).toString() + "\n" + preview);
    }
}

void MainWindow::addSessionItem(int ttype, int tid, const QString &name) {
    auto *it = new QListWidgetItem(name);
    it->setData(Qt::UserRole,     ttype);
    it->setData(Qt::UserRole + 1, tid);
    it->setData(Qt::UserRole + 2, name);
    it->setSizeHint(QSize(0, 62));
    m_sessionList->addItem(it);
    m_sessionItems.insert(skey(ttype, tid), it);
}

void MainWindow::updateSessionPreview(int ttype, int tid, const QString &text) {
    auto *it = m_sessionItems.value(skey(ttype, tid));
    if (!it) return;
    it->setText(it->data(Qt::UserRole + 2).toString() + "\n" + text);
}

void MainWindow::onSessionClicked(QListWidgetItem *item) {
    if (!item) return;
    switchToSession(item->data(Qt::UserRole).toInt(),
                    item->data(Qt::UserRole + 1).toInt(),
                    item->data(Qt::UserRole + 2).toString());
}
//拉取历史消息
void MainWindow::switchToSession(int ttype, int tid, const QString &title) {
    m_curType = ttype;
    m_curId   = tid;
    m_title->setText(title);
    clearMessages();

    NetworkClient::inst().sendJson(Proto::Type::FetchHistory, {
                                                               {"targetType", ttype},
                                                               {"targetId",   tid},
                                                               {"beforeId",   0},
                                                               {"limit",      15},
                                                               });
}
//清理消息--切换聊天框时调用
void MainWindow::clearMessages() {
    m_bubbles.clear();
    while (m_msgLayout->count() > 1) {
        QLayoutItem *child = m_msgLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    m_beforeId         = 0;
    first              = 0;
    m_isLoadingHistory = false;
    m_oldBarMax        = 0;
    m_oldBarValue      = 0;
}

/* ---------------- 消息渲染 ---------------- */

//prepend = true：插到 stretch 之前（保持视觉“旧→新”）
//prepend = false：追加到 stretch 之前（最新消息）
//本函数所有插入都放在最后 stretch 之前/顶部，因为布局最后有一个stretch
void MainWindow::appendMessage(const QJsonObject &msg, bool prepend) {
    auto &nc = NetworkClient::inst();
    const int senderId = msg["senderId"].toInt();
    const bool self = (senderId == nc.myId);

    QString name = self ? QStringLiteral("我")
                        : nc.userNames.value(senderId,
                                             msg["senderName"].toString(QStringLiteral("用户")));

    auto *bubble = new MessageBubble(msg, name, self, m_msgContainer);

    if (prepend)
        m_msgLayout->insertWidget(0, bubble);                 // 插到顶部
    else
        m_msgLayout->insertWidget(m_msgLayout->count() - 1, bubble);  // 追加到底部

    m_bubbles.insert(qint64(msg["id"].toDouble()), bubble);

    if (msg["msgType"].toInt() == 1)
        nc.sendJson(Proto::Type::FetchImage, {{"msgId", msg["id"]}});
    //当点击图片时查看大图
    connect(bubble, &MessageBubble::imageClicked, this, [this](const QPixmap &pm) {
        auto *dlg = new QDialog(this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(QStringLiteral("查看图片"));
        dlg->setStyleSheet("background:#1e1e1e;");
        auto *l = new QVBoxLayout(dlg);
        auto *lab = new QLabel;
        lab->setAlignment(Qt::AlignCenter);
        lab->setPixmap(pm.scaled(900, 680, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        l->addWidget(lab);
        dlg->resize(lab->pixmap().size() + QSize(20, 20));
        dlg->show();
    });
}

//用rangeChanged确保布局已完成后再滚到底
void MainWindow::scrollToBottom() {
    auto *bar = m_scroll->verticalScrollBar();
    auto conn = QSharedPointer<QMetaObject::Connection>::create();
    *conn = connect(bar, &QScrollBar::rangeChanged, this,
                    [bar, conn](int, int newMax) {
                        QObject::disconnect(*conn);
                        bar->setValue(newMax);
                    });
    //兜底：若当前已经能读到正确max（比如空会话、消息很少）
    bar->setValue(bar->maximum());
}

//用rangeChanged确保布局已完成后再恢复位置
void MainWindow::preserveScroll(qint64 oldValue, qint64 oldMax) {
    auto *bar = m_scroll->verticalScrollBar();
    auto conn = QSharedPointer<QMetaObject::Connection>::create();
    *conn = connect(bar, &QScrollBar::rangeChanged, this,
                    [bar, conn, oldValue, oldMax](int, int newMax) {
                        QObject::disconnect(*conn);
                        bar->setValue(int(oldValue + (newMax - oldMax)));
                    });
}

/* ---------------- 收发 ---------------- */

//发送文字消息
void MainWindow::onSendText() {
    if (m_curType < 0) return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;

    NetworkClient::inst().sendJson(Proto::Type::SendText, {
                                                           {"targetType", m_curType},
                                                           {"targetId",   m_curId},
                                                           {"text",       text},
                                                           });
    m_input->clear();
}

//发送图片消息
void MainWindow::onPickImage() {
    if (m_curType < 0) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) return;
    //禁止图片过大，如过大压缩
    if (img.width() > 1280 || img.height() > 1280)
        img = img.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    if (!img.save(&buf, "JPG", 82)) return;
    buf.close();

    NetworkClient::inst().send(Proto::Type::SendImage, Proto::packBinary({
                                                                          {"targetType", m_curType},
                                                                          {"targetId",   m_curId},
                                                                          {"w", img.width()},
                                                                          {"h", img.height()},
                                                                          }, data));
}

//新建群聊
void MainWindow::onCreateGroup() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("发起群聊"));
    dlg.resize(320, 420);
    auto *l = new QVBoxLayout(&dlg);

    auto *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QStringLiteral("群名称（留空自动生成）"));
    nameEdit->setStyleSheet("padding:6px;border:1px solid #ddd;border-radius:4px;");
    l->addWidget(nameEdit);

    auto *list = new QListWidget;
    auto &nc = NetworkClient::inst();
    for (const auto &v : nc.users) {
        const QJsonObject u = v.toObject();
        if (u["id"].toInt() == nc.myId) continue;
        auto *it = new QListWidgetItem(u["nickname"].toString());
        it->setData(Qt::UserRole, u["id"].toInt());
        it->setCheckState(Qt::Unchecked);
        list->addItem(it);
    }
    l->addWidget(list);

    auto *ok = new QPushButton(QStringLiteral("创建"));
    ok->setStyleSheet("background:#07c160;color:white;padding:8px;"
                      "border:none;border-radius:4px;");
    l->addWidget(ok);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    QJsonArray members;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->checkState() == Qt::Checked)
            members.append(list->item(i)->data(Qt::UserRole).toInt());
    if (members.isEmpty()) return;

    nc.sendJson(Proto::Type::CreateGroup, {
                                           {"name",    nameEdit->text().trimmed()},
                                           {"members", members},
                                           });
}

/* ---------------- 网络包分发 ---------------- */

//新消息
void MainWindow::handleNewMessage(const QJsonObject &msg) {
    const int ttype  = msg["targetType"].toInt();
    const int tid    = msg["targetId"].toInt();
    const int sender = msg["senderId"].toInt();
    const int myId   = NetworkClient::inst().myId;

    const QString preview = msg["msgType"].toInt() == 1
                                ? QStringLiteral("[图片]")
                                : msg["text"].toString();

    if (ttype == 0) {
        const int peer = (sender == myId) ? tid : sender;
        updateSessionPreview(0, peer, preview);
    } else {
        updateSessionPreview(1, tid, preview);
    }

    bool current = false;
    if (ttype == 0 && m_curType == 0)
        current = (sender == m_curId || tid == m_curId);
    else if (ttype == 1 && m_curType == 1)
        current = (tid == m_curId);

    if (current) {
        appendMessage(msg, /*prepend=*/false);   // 新消息追加到底部
        scrollToBottom();
    }
}

void MainWindow::onPacket(Proto::Type t, const QByteArray &body) {
    switch (t) {
        //拉取历史消息
    case Proto::Type::HistoryResp: {
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        //先校验target，再动first/m_isLoadingHistory
        if (o["targetType"].toInt() != m_curType ||
            o["targetId"].toInt()   != m_curId) break;

        m_isLoadingHistory = false;

        const QJsonArray arr = o["messages"].toArray();
        if (arr.isEmpty()) {                       // 防空数组崩溃
            m_beforeId = 0;
            break;
        }

        const bool isInitial = (first == 0);       // 判断首屏还是分页
        first++;

        // 分页锚点：服务端降序返回，最后一条是“最旧”，用它作为 beforeId
        m_beforeId = qint64(arr.last().toObject()["id"].toDouble());

        //服务端降序返回，正序遍历+prepend
        for (const auto &v : arr)
            appendMessage(v.toObject(), /*prepend=*/true);

        if (isInitial)
            scrollToBottom();
        else
            preserveScroll(m_oldBarValue, m_oldBarMax);
        break;
    }
        //新消息
    case Proto::Type::NewMessage: {
        handleNewMessage(QJsonDocument::fromJson(body).object());
        break;
    }
        //加载图片
    case Proto::Type::ImageResp: {
        QJsonObject meta;
        QByteArray img;
        if (!Proto::unpackBinary(body, meta, img)) break;
        const qint64 mid = qint64(meta["msgId"].toDouble());
        if (auto *b = m_bubbles.value(mid)) {
            auto *bar = m_scroll->verticalScrollBar();
            //只有原本就在底部时才跟着滚到底，避免打断用户看历史
            const bool wasAtBottom = bar->value() >= bar->maximum() - 50;
            b->setImage(img);
            if (wasAtBottom) scrollToBottom();
        }
        break;
    }
        //创建群聊
    case Proto::Type::GroupCreated: {
        const QJsonObject g = QJsonDocument::fromJson(body).object();
        const int gid = g["id"].toInt();
        auto &nc = NetworkClient::inst();
        if (nc.groupNames.contains(gid)) break;
        nc.groupNames.insert(gid, g["name"].toString());
        nc.groups.append(g);
        addSessionItem(1, gid, g["name"].toString());
        break;
    }
        //多端登录顶号
    case Proto::Type::ErrorMsg: {
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        QDialog dlg(this);
        dlg.setFixedSize(200, 100);
        dlg.setWindowTitle("提示");
        dlg.setStyleSheet(R"(
        QDialog { background-color:#ffffff; border-radius:10px; }
        QLabel { font-size:14px; color:#333333; }
        QPushButton {
            background-color:#07c160; color:white; border:none; border-radius:6px;
            font-size:14px; padding:8px 24px; min-width:90px;
        }
        QPushButton:hover { background-color:#06ad56; }
        QPushButton:pressed { background-color:#059c4d; }
        )");
        QVBoxLayout lay(&dlg);
        auto *label = new QLabel(o["msg"].toString());
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("font-size:14px;");
        lay.addWidget(label);
        QPushButton *okBtn = new QPushButton("确定");
        okBtn->setFixedWidth(80);
        connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        lay.addWidget(okBtn);
        lay.setAlignment(okBtn, Qt::AlignCenter);
        dlg.move(this->frameGeometry().center() - dlg.rect().center());
        dlg.exec();
        this->close();
        break;
    }
    default:
        break;
    }
}
