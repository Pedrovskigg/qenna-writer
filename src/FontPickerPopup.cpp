#include "FontPickerPopup.h"

#include <QFont>
#include <QListWidget>
#include <QListWidgetItem>
#include <QScreen>
#include <QVBoxLayout>
#include <QGuiApplication>

FontPickerPopup::FontPickerPopup(QWidget *parent)
    : QFrame(parent, Qt::Popup)
    , list(new QListWidget(this))
{
    setObjectName(QStringLiteral("fontPickerPopup"));
    setFrameShape(QFrame::NoFrame);

    list->setObjectName(QStringLiteral("fontPickerList"));
    list->setUniformItemSizes(false);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(list);

    setFixedSize(240, 340);

    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        emit fontSelected(item->text());
        hide();
    });
}

void FontPickerPopup::setFontFamilies(const QStringList &families, const QString &current)
{
    currentFamily = current;
    list->clear();
    int currentRow = -1;
    for (int i = 0; i < families.size(); ++i) {
        const QString &f = families.at(i);
        auto *item = new QListWidgetItem(f);
        item->setData(Qt::FontRole, QFont(f, 12));
        if (f == current) {
            currentRow = i;
        }
        list->addItem(item);
    }
    if (currentRow >= 0) {
        list->setCurrentRow(currentRow);
        list->scrollToItem(list->item(currentRow), QAbstractItemView::PositionAtCenter);
    }
}

void FontPickerPopup::showAtBelow(const QPoint &globalAnchor)
{
    QPoint pos = globalAnchor;
    const QScreen *screen = QGuiApplication::screenAt(pos);
    if (screen) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + width() > avail.right()) {
            pos.setX(avail.right() - width());
        }
        if (pos.y() + height() > avail.bottom()) {
            pos.setY(globalAnchor.y() - height());
        }
    }
    move(pos);
    show();
    list->setFocus();
}
