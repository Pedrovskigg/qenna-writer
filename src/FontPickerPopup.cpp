#include "FontPickerPopup.h"

#include "AnchorUtils.h"

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

void FontPickerPopup::showNear(const QRect &anchorGlobal, Qt::Edge barSide)
{
    QPoint pos = anchorGlobal.bottomLeft();
    const QScreen *screen = QGuiApplication::screenAt(anchorGlobal.center());
    if (screen) {
        pos = AnchorUtils::positionNear(anchorGlobal, size(), barSide, screen->availableGeometry());
    }
    move(pos);
    show();
    list->setFocus();
}
