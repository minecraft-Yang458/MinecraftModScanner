#pragma once

#include <QStyledItemDelegate>

class ActionButtonDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ActionButtonDelegate(QObject *parent = nullptr);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

signals:
    void clicked(const QModelIndex &index, const QPoint &globalPosition);

private:
    bool m_enabled = true;
};
