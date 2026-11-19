#pragma once
// E-book como leitor (E-reader Lab, 18/11/2026; aprovado: "quero os dois").
//
// A prévia do EPUB virou um leitor de verdade: o livro inteiro reflui na tela
// de um e-reader de tinta, de um celular ou de um tablet, com a cor de leitura,
// o tamanho e a fonte que quem lê escolheria. Mora em dois lugares:
//  - no painel de Exportar, quando o formato é EPUB (a prévia de cara nova);
//  - na sala de leitura (EbookRoom), em tela cheia: "Ler como e-book" no menu
//    do Q, F8, o botão de e-reader do Manuscrito e "Abrir a prévia grande".
// O conteúdo vem do Exporter::buildEbookPreview, que usa o mesmo corpo XHTML,
// a mesma abertura de capítulo e a mesma capitular que vão dentro do EPUB.

#include "Exporter.h"

#include <QDialog>
#include <QImage>
#include <QMarginsF>
#include <QWidget>

#include <functional>
#include <memory>

class QLabel;
class QPushButton;
class QToolButton;
class QTimer;
class QVariantAnimation;
class QVBoxLayout;
class QScrollArea;

namespace Ebook {

enum Device { Eink = 0, Phone = 1, Tablet = 2 };
enum Paper { White = 0, Sepia = 1, Black = 2 };
constexpr int kSizeSteps = 5;
constexpr int kFontCount = 3;
constexpr int kSpacingCount = 3;

// O jeito de ler (vale pro painel e pra sala, lembrado entre as aberturas).
struct Look {
    int device = Eink;
    int paper = White;
    int size = 2;       // 0..kSizeSteps-1
    int font = 0;       // 0 serifa, 1 Alegreya, 2 sem serifa
    int spacing = 1;    // 0 justo, 1 normal, 2 largo
    static Look load();
    void save() const;
    bool operator==(const Look& o) const {
        return device == o.device && paper == o.paper && size == o.size && font == o.font && spacing == o.spacing;
    }
    bool operator!=(const Look& o) const { return !(*this == o); }
};

// O aparelho, em px lógicos (o desenho do lab).
struct Geo {
    QSizeF outer;          // o aparelho inteiro
    QMarginsF bezel;       // moldura até a tela
    qreal radius = 0;      // canto do aparelho
    qreal screenRadius = 0;
    QMarginsF text;        // margem do texto dentro da tela
    qreal fontPx = 15;     // letra no tamanho do meio
    QColor body;
};
Geo geometry(int device);
QSizeF screenSize(int device);
QSizeF contentSize(int device);   // a área do texto (sem o rodapé)
constexpr qreal kFooterH = 14;

struct Colors { QColor bg, ink, muted; };
Colors colors(const Look& look);
QString fontFamily(int font);
Exporter::EbookLook exporterLook(const Look& look);

QString deviceName(int device);
QString fontName(int font);
QString spacingName(int spacing);

} // namespace Ebook

// ------------------------------------------------------------------ a tela
// O aparelho desenhado com a página do livro dentro e o rodapé do leitor.
// Clique na metade direita (ou esquerda) folheia; a roda e as setas também.
// No sumário, o nome da parte leva até ela.
class EbookView : public QWidget {
    Q_OBJECT
public:
    explicit EbookView(QWidget* parent = nullptr);
    void setBook(std::shared_ptr<const Exporter::EbookPreview> book, const Ebook::Look& look);
    const Exporter::EbookPreview* book() const { return m_book.get(); }
    void setScreen(int screen, bool animate = true);
    void step(int delta) { setScreen(m_screen + delta); }
    int screen() const { return m_screen; }
    int screens() const { return m_book ? m_book->pages() : 0; }
    void setMaxScale(qreal k) { m_maxScale = k; update(); }
    void setEmptyText(const QString& t) { m_emptyText = t; update(); }
    // texto do rodapé do leitor naquela tela (esquerda, direita)
    QPair<QString, QString> footer(int screen) const;

signals:
    void screenChanged(int screen);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void resizeEvent(QResizeEvent*) override { m_cache.clear(); }

private:
    struct Frame { QRectF device, screen; qreal k = 1; };
    Frame frame() const;
    QImage screenImage(int screen, const QSize& px, qreal k, qreal dpr);
    int linkUnder(const QPointF& pos) const;

    std::shared_ptr<const Exporter::EbookPreview> m_book;
    Ebook::Look m_look;
    int m_screen = 0;
    int m_fromScreen = -1;
    int m_dir = 1;
    qreal m_maxScale = 1.0;
    QString m_emptyText;
    QVariantAnimation* m_slide = nullptr;
    QVariantAnimation* m_flash = nullptr;
    struct Cached { int screen; QImage img; };
    QList<Cached> m_cache;
    int m_wheelAcc = 0;
};

// ------------------------------------------------------------------ os controles
// Peças da barra de leitura: aparelho, cor do papel, tamanho, letra e
// entrelinha. Cada instância mostra só as peças pedidas; quem as usa junta
// várias e mantém o mesmo Look em todas.
class EbookLookBar : public QWidget {
    Q_OBJECT
public:
    enum Part { DevicePart = 1, PaperPart = 2, SizePart = 4, FontPart = 8, SpacingPart = 16 };
    EbookLookBar(int parts, QWidget* parent = nullptr);
    void setLook(const Ebook::Look& look);
    const Ebook::Look& look() const { return m_look; }
    void applyTheme();

signals:
    void lookChanged(const Ebook::Look& look);

private:
    void sync();
    void change(const std::function<void(Ebook::Look&)>& f);
    Ebook::Look m_look;
    QList<QPushButton*> m_device, m_font, m_spacing;
    QList<QToolButton*> m_paper;
    QToolButton* m_smaller = nullptr;
    QToolButton* m_bigger = nullptr;
};

// ------------------------------------------------------------------ a régua
// O progresso no livro: uma marquinha onde cada parte começa; clique ou
// arraste pra ir.
class EbookScrubber : public QWidget {
    Q_OBJECT
public:
    explicit EbookScrubber(QWidget* parent = nullptr);
    void setState(int screen, int screens, const QList<int>& ticks, const QStringList& tickNames);
signals:
    void seek(int screen);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
private:
    int screenAt(qreal x) const;
    int m_screen = 0, m_screens = 1;
    QList<int> m_ticks;
    QStringList m_tickNames;
};

// ------------------------------------------------------------------ a sala de leitura
// Tela cheia sobre a janela: sumário do lado com a tela onde cada parte
// começa, o aparelho grande no meio, régua de progresso embaixo e o Aa com
// tamanho, letra e entrelinha. "Exportar EPUB" sai dali mesmo.
class EbookRoom : public QDialog {
    Q_OBJECT
public:
    // positionKey: onde lembrar a última posição (vazio = não lembra).
    EbookRoom(std::shared_ptr<Exporter> exporter, const Exporter::Selection& selection,
              const QString& subtitle, const QString& positionKey, QWidget* parent);
    ~EbookRoom() override;

signals:
    void exportRequested(const Exporter::Selection& selection);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void rebuild();
    void refresh();
    void applyTheme();
    void toggleAaPopup();
    void savePosition();

    std::shared_ptr<Exporter> m_exporter;
    Exporter::Selection m_selection;
    QString m_positionKey;
    Ebook::Look m_look;
    std::shared_ptr<Exporter::EbookPreview> m_book;
    bool m_restored = false;

    QWidget* m_top = nullptr;
    QWidget* m_rail = nullptr;
    QWidget* m_bottom = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    EbookLookBar* m_bar = nullptr;
    QToolButton* m_aaBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    QVBoxLayout* m_railList = nullptr;
    QList<QPushButton*> m_railBtns;
    QLabel* m_railNote = nullptr;
    EbookView* m_view = nullptr;
    QToolButton* m_prevBtn = nullptr;
    QToolButton* m_nextBtn = nullptr;
    EbookScrubber* m_scrub = nullptr;
    QLabel* m_where = nullptr;
};
