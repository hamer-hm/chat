// client/messagebubble.h
#pragma once
#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QJsonObject>

class MessageBubble : public QWidget {
    Q_OBJECT
public:
    MessageBubble(const QJsonObject &msg, const QString &senderName,
                  bool self, QWidget *parent = nullptr);

    qint64 msgId() const { return m_id; }
    void   setImage(const QByteArray &data);

signals:
    void imageClicked(const QPixmap &pm);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    qint64   m_id = 0;
    QLabel  *m_imgLabel = nullptr;
    QPixmap  m_pm;
};
