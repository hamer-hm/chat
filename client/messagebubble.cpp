// client/messagebubble.cpp
#include "messagebubble.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
//渲染气泡
MessageBubble::MessageBubble(const QJsonObject &msg, const QString &senderName,
                             bool self, QWidget *parent)
    : QWidget(parent) {
    m_id = qint64(msg["id"].toDouble());
    const int mtype = msg["msgType"].toInt();

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(12, 6, 12, 6);
    outer->setSpacing(0);

    auto *box = new QVBoxLayout;
    box->setSpacing(3);

    auto *nameLabel = new QLabel(self ? QStringLiteral("我") : senderName);
    nameLabel->setStyleSheet("color:#9a9a9a;font-size:11px;");
    nameLabel->setAlignment(self ? Qt::AlignRight : Qt::AlignLeft);
    box->addWidget(nameLabel);

    if (mtype == 0) {
        auto *bubble = new QLabel(msg["text"].toString());
        bubble->setWordWrap(true);
        bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);

        bubble->setMaximumWidth(420);
        bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        bubble->setStyleSheet(self
                                  ? "background:#95ec69;border-radius:8px;padding:9px 11px;font-size:14px;color:#111;"
                                  : "background:#ffffff;border-radius:8px;padding:9px 11px;font-size:14px;color:#111;");
        box->addWidget(bubble, 0, self ? Qt::AlignRight : Qt::AlignLeft);
    } else {
        m_imgLabel = new QLabel(QStringLiteral("图片加载中…"));
        m_imgLabel->setFixedSize(200, 150);
        m_imgLabel->setAlignment(Qt::AlignCenter);
        m_imgLabel->setCursor(Qt::PointingHandCursor);
        m_imgLabel->setStyleSheet("background:#e3e3e3;border-radius:8px;color:#8a8a8a;font-size:12px;");
        m_imgLabel->installEventFilter(this);
        box->addWidget(m_imgLabel, 0, self ? Qt::AlignRight : Qt::AlignLeft);
    }

    if (self) {
        outer->addStretch(1);
        outer->addLayout(box);
    } else {
        outer->addLayout(box);
        outer->addStretch(1);
    }
}
//加载图片
void MessageBubble::setImage(const QByteArray &data) {
    if (!m_imgLabel) return;
    QPixmap pm;
    if (!pm.loadFromData(data) || pm.isNull()) {
        m_imgLabel->setText(QStringLiteral("图片加载失败"));
        return;
    }
    m_pm = pm;
    const QPixmap scaled =
        pm.scaled(240, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imgLabel->setFixedSize(scaled.size());
    m_imgLabel->setPixmap(scaled);
    m_imgLabel->setStyleSheet("border-radius:8px;");
}
//查看大图
bool MessageBubble::eventFilter(QObject *o, QEvent *e) {
    if (o == m_imgLabel && e->type() == QEvent::MouseButtonPress) {
        if (!m_pm.isNull()) emit imageClicked(m_pm);
        return true;
    }
    return QWidget::eventFilter(o, e);
}
