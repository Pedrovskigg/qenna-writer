#ifndef FONTPICKERPOPUP_H
#define FONTPICKERPOPUP_H

#include <QFrame>
#include <QStringList>

class QListWidget;

class FontPickerPopup : public QFrame
{
    Q_OBJECT

public:
    explicit FontPickerPopup(QWidget *parent = nullptr);

    void setFontFamilies(const QStringList &families, const QString &current);
    void showAtBelow(const QPoint &globalAnchor);

signals:
    void fontSelected(const QString &family);

private:
    QListWidget *list;
    QString currentFamily;
};

#endif
