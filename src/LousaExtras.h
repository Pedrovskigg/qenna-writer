#pragma once

#include "LousaTypes.h"
#include "SheetDialog.h"

#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>

class LousaScene;
class LousaView;
class QLabel;
class QLineEdit;
class QCheckBox;
class QTimer;

// Desenho simplificado de uma lousa (minimapa e miniaturas do seletor).
namespace LousaDraw {
struct Snapshot {
    QList<CanvasCard>       cards;
    QList<CanvasZone>       zones;
    QList<CanvasConnection> conns;
    QColor                  bg;
};
Snapshot fromScene(const LousaScene* scene);
Snapshot fromFile(const QString& path, const QColor& themeBg);
QRectF   bounds(const Snapshot& s);
// Desenha `area` (cena) dentro de `target`, mantendo a proporção. Devolve a
// transformação usada (cena → widget), pra quem precisa do caminho de volta.
QTransform draw(QPainter* p, const QRectF& target, const Snapshot& s, const QRectF& area);
} // namespace LousaDraw

// B · Minimapa: o quadro inteiro e onde a tela está. Clica e vai.
class LousaMinimap : public QWidget
{
    Q_OBJECT
public:
    LousaMinimap(LousaScene* scene, LousaView* view, QWidget* parent);
    void scheduleRefresh();
signals:
    void centerRequested(const QPointF& scenePos);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    void jumpTo(const QPoint& p);
    LousaScene* m_scene;
    LousaView*  m_view;
    QTimer*     m_timer = nullptr;
    LousaDraw::Snapshot m_snap;
    QRectF      m_area;
    QTransform  m_xf;
};

// G · Ajuda numa tela só: as teclas desenhadas, por assunto.
class LousaCheatSheet : public QWidget
{
    Q_OBJECT
public:
    explicit LousaCheatSheet(QWidget* parent);
    void showOver();     // cobre o pai inteiro
    void applyTheme();
protected:
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void build();
    QWidget* m_card = nullptr;
};

// J · Buscar na Lousa (Ctrl+F)
class LousaSearchBar : public QFrame
{
    Q_OBJECT
public:
    explicit LousaSearchBar(QWidget* parent);
    void open();
    void setResult(int current, int total);   // total < 0 = sem busca
    QString text() const;
    void applyTheme();
signals:
    void queryChanged(const QString& text);
    void next();
    void previous();
    void closed();
protected:
    bool eventFilter(QObject* o, QEvent* e) override;
private:
    QLineEdit* m_edit = nullptr;
    QLabel*    m_count = nullptr;
};

// A · Fundo e cards: desenho do fundo, cor (do tema ou própria), inclinação, minimapa.
namespace LousaBoardLook {
struct Choice {
    QString style;      // dots, grid, lines, plain, cork, whiteboard
    QColor  color;      // inválida = do tema
    bool    tilt = true;
    bool    minimap = true;
};
// Abre o balão junto do ponto; chama onChange a cada escolha (pré-visualiza no quadro).
void popup(QWidget* parent, const QPoint& globalPos, const Choice& current,
           const std::function<void(const Choice&)>& onChange);
} // namespace LousaBoardLook

// H · Exportar como imagem, escolhendo o pedaço e vendo antes.
class LousaExportSheet : public SheetDialog
{
    Q_OBJECT
public:
    LousaExportSheet(LousaScene* scene, LousaView* view, QWidget* parent);
    QImage render() const;
private:
    void refreshPreview();
    QRectF chosenRect() const;
    LousaScene* m_scene;
    LousaView*  m_view;
    int         m_mode = 0;   // 0 lousa toda, 1 uma área, 2 o que está na tela
    int         m_zone = 0;
    QList<CanvasZone> m_zones;
    QCheckBox*  m_transparent = nullptr;
    QLabel*     m_preview = nullptr;
    QLabel*     m_size = nullptr;
    QWidget*    m_zoneRow = nullptr;
};

// I · Escolher a lousa pela miniatura (quando o projeto tem várias).
namespace LousaBoardPicker {
struct Entry { QString id; QString name; QString file; };
// Devolve o id escolhido ("" = cancelou). activeId fica destacado.
// manage = também oferece "+ Nova lousa" ("__new__") e, no botão direito de
// cada uma, Renomear ("__rename__:<id>") e Excluir ("__delete__:<id>").
QString pick(QWidget* parent, const QPoint& globalPos, const QString& projectRoot,
             const QList<Entry>& boards, const QString& activeId, bool manage = false);
} // namespace LousaBoardPicker
