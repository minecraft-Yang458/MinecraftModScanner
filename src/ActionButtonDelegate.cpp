#include "ActionButtonDelegate.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>

ActionButtonDelegate::ActionButtonDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ActionButtonDelegate::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void ActionButtonDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    QStyleOptionViewItem itemOption(option);
    initStyleOption(&itemOption, index);
    itemOption.text.clear();
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, widget);

    QStyleOptionButton button;
    button.rect = option.rect.adjusted(7, 4, -7, -4);
    button.text = QStringLiteral("操作");
    button.state = QStyle::State_Raised;
    if (m_enabled)
        button.state |= QStyle::State_Enabled;
    style->drawControl(QStyle::CE_PushButton, &button, painter, widget);
}

QSize ActionButtonDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QSize(84, qMax(30, option.fontMetrics.height() + 12));
}

bool ActionButtonDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index)
{
    Q_UNUSED(model);
    if (event->type() == QEvent::MouseButtonRelease && m_enabled) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            const QRect buttonRect = option.rect.adjusted(7, 4, -7, -4);
            if (buttonRect.contains(mouseEvent->position().toPoint())) {
                emit clicked(index, mouseEvent->globalPosition().toPoint());
                return true;
            }
        }
    }
    return false;
}
