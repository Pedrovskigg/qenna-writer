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
    // barSide = lado da tela onde mora a barra do botão-anchor.
    void showNear(const QRect &anchorGlobal, Qt::Edge barSide = Qt::TopEdge);

signals:
    void fontSelected(const QString &family);

private:
    QListWidget *list;
    QString currentFamily;
};

#endif
