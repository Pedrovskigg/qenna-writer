#pragma once
// Peças da capa rápida (ver QuickCover.h): a capa em si, que se edita direto
// no preview (hover mostra o pincel; editando, clica num texto pra escolher e
// arrasta pra mover), a bolinha de cor e o painel das quatro abas (Foto,
// Degradê, Grão, Texto) que entra no lugar dos campos da folha.

#include "QuickCover.h"

#include <QAbstractButton>
#include <QWidget>

#include <functional>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSlider;
class QStackedWidget;
class QTimer;
class QToolButton;

// Bolinha arco-íris com a cor atual no meio. Clique abre o seletor de cor do
// Qenna (ColorPopover).
class ColorWheelButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit ColorWheelButton(QWidget* parent = nullptr);
    void setColor(const QColor& c);
    QColor color() const { return m_color; }
    void setMarked(bool on);   // anel em volta (a cor está em uso)
    void setPickerTitle(const QString& t) { m_title = t; }
    QSize sizeHint() const override { return QSize(28, 28); }
signals:
    void colorPicked(const QColor& c);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QColor m_color;
    QString m_title;
    bool m_marked = false;
};

class QuickCoverCanvas : public QWidget {
    Q_OBJECT
public:
    explicit QuickCoverCanvas(QuickCover::Spec* spec, QWidget* parent = nullptr);
    void setTexts(const QString& title, const QString& author);
    void setEditing(bool on);
    bool editing() const { return m_editing; }
    int selected() const { return m_sel; }
    void setSelected(int i);
    void refresh() { update(); }
    // Uma capa pronta que veio de fora (Cover Creator, imagem): mostrada como
    // está até o autor clicar nela. Nula = desenha a capa rápida.
    void setStaticCover(const QPixmap& pm) { m_static = pm; update(); }
signals:
    void editRequested();
    void selectedChanged(int index);
    void moved();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
private:
    int hitAt(const QPointF& p) const;
    QuickCover::Spec* m_spec;
    QPixmap m_static;
    QString m_title, m_author;
    QVector<QRectF> m_hits;
    bool m_editing = false;
    bool m_hover = false;
    bool m_snap = false;
    int m_sel = 0;
    int m_drag = -1;
    QPointF m_grab;
};

class QuickCoverPanel : public QWidget {
    Q_OBJECT
public:
    // title/author: os campos da folha (o painel lê e escreve neles)
    QuickCoverPanel(QuickCover::Spec* spec, QuickCoverCanvas* canvas,
                    std::function<QString()> title, std::function<void(const QString&)> setTitle,
                    std::function<QString()> author, std::function<void(const QString&)> setAuthor,
                    const QStringList& fontFamilies, QWidget* parent = nullptr);
    void showText(int index);   // pula pra aba Texto com esse texto escolhido
    void syncFromSpec();
    void imagePicked() { refreshThumbMarks(); }
signals:
    void changed();
    void doneRequested();
    void exportRequested();
    void pickImageRequested();
private:
    QWidget* buildFoto();
    QWidget* buildFade();
    QWidget* buildGrain();
    QWidget* buildText();
    void loadMoreThumbs();
    void refreshThumbMarks();
    void refreshTextPane();
    void rebuildFontList();
    void applyTheme();
    QuickCover::Text* current();

    QuickCover::Spec* m_spec;
    QuickCoverCanvas* m_canvas;
    std::function<QString()> m_title, m_author;
    std::function<void(const QString&)> m_setTitle, m_setAuthor;
    QStringList m_families;

    QButtonGroup* m_tabs = nullptr;
    QStackedWidget* m_pages = nullptr;
    // foto
    ColorWheelButton* m_bgWheel = nullptr;
    QGridLayout* m_thumbGrid = nullptr;
    QList<QToolButton*> m_thumbs;
    QStringList m_imageNames;
    int m_thumbsLoaded = 0;
    QTimer* m_thumbTimer = nullptr;
    // degradê / grão
    QButtonGroup* m_fadeGroup = nullptr;
    QToolButton* m_fadeDark = nullptr;
    QToolButton* m_fadeLight = nullptr;
    QSlider* m_fadeOp = nullptr;
    QSlider* m_fadeSize = nullptr;
    QSlider* m_grain = nullptr;
    QSlider* m_grainSize = nullptr;
    // texto
    QLabel* m_editingLbl = nullptr;
    QLineEdit* m_textEdit = nullptr;
    QToolButton* m_removeBtn = nullptr;
    QToolButton* m_boldBtn = nullptr;
    QToolButton* m_italicBtn = nullptr;
    QToolButton* m_underBtn = nullptr;
    ColorWheelButton* m_textWheel = nullptr;
    QLineEdit* m_fontSearch = nullptr;
    QWidget* m_fontBox = nullptr;
    QGridLayout* m_fontGrid = nullptr;
    QList<QPushButton*> m_fontBtns;
};
