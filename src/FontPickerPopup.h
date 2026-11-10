#ifndef FONTPICKERPOPUP_H
#define FONTPICKERPOPUP_H

#include <QFrame>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // Uma família e os pesos dela (Alegreya › Black, Medium...), que o sistema
    // entrega como se fossem fontes separadas.
    struct FontEntry {
        QString family;
        QStringList variants;
    };

    QString sectionTitle(const QString &section) const;
    void rebuildCatalog();
    void populate();
    void applyVisibility();
    void selectCurrentFont();
    void selectFirstMatch();
    void moveCursor(int step);
    void activate(QListWidgetItem *item);
    void toggleSection(QListWidgetItem *header);
    void toggleFamily(QListWidgetItem *row);
    void choose(const QString &family);
    void rememberRecent(const QString &family);
    void saveOpenState() const;

    QLineEdit *search;
    QListWidget *list;
    QString currentFamily;
    QStringList m_families;
    QStringList m_recents;
    QHash<QString, QVector<FontEntry>> m_sections;
    QSet<QString> m_openSections;
    QSet<QString> m_openFamilies;
};

#endif
