#include "ToolbarGroupWidget.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QHelpEvent>
#include <QToolButton>
#include <QToolTip>

namespace {
const char kGroupMimeType[]  = "application/x-qenna-toolbar-group";
const char kButtonMimeType[] = "application/x-qenna-toolbar-button";
constexpr int kHandleThickness = 14; // px reservados pra alça quando draggable
// Espessura do traço que marca onde o botão arrastado vai cair.
constexpr int kMarkerThickness = 3;
// Um grupo vazio precisa continuar sendo alvo de drop em modo de edição —
// senão, esvaziou uma vez, nunca mais dá pra devolver um botão pra ele.
constexpr int kEmptyDropExtent = 26;
// Quanto tempo segurando um ícone liga o modo de edição. Folgado de propósito:
// precisa ser bem mais longo que qualquer clique normal, senão um clique
// hesitante no negrito viraria "reorganizar barra".
constexpr int kLongPressMs = 600;
}

ToolbarGroupWidget::ToolbarGroupWidget(const QString& groupId, Qt::Orientation orientation, QWidget* parent)
    : QWidget(parent)
    , m_groupId(groupId)
    , m_orientation(orientation)
{
    // QWidget puro é Preferred/Preferred por padrão — sob pressão de espaço
    // (barra vertical com ~29 botões pode passar de 1000px, mais alto que
    // muita janela) o Qt comprimiria o grupo abaixo do que os botões fixos
    // internos precisam, gerando sobreposição visual. Fixed recusa encolher:
    // o pior caso vira "corta embaixo", não "esmaga por cima".
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_layout = new QBoxLayout(
        orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);

    // Alça: reservada no layout desde o início (senão inserir depois
    // deslocaria todo o resto), só fica visível/com espaço real quando
    // draggable — QLayout ignora widgets escondidos.
    m_handle = new QLabel(this);
    m_handle->setFixedSize(orientation == Qt::Horizontal ? QSize(kHandleThickness, 20)
                                                          : QSize(20, kHandleThickness));
    m_handle->setAttribute(Qt::WA_TransparentForMouseEvents); // clique cai no container, não na alça
    m_handle->setVisible(false);
    m_layout->addWidget(m_handle, 0,
                         orientation == Qt::Horizontal ? Qt::AlignVCenter : Qt::AlignHCenter);

    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(kLongPressMs);
    connect(m_longPressTimer, &QTimer::timeout, this, &ToolbarGroupWidget::onLongPressElapsed);
}

void ToolbarGroupWidget::addButton(QWidget* button, const QString& buttonId)
{
    if (!button || buttonId.isEmpty()) return;
    m_buttons.append({ buttonId, button });
    if (button->parentWidget() != this) {
        // setParent() esconde o widget; preservar o estado de visibilidade é o
        // que impede um botão colapsado no menu "⋯" de reaparecer sozinho
        // quando a barra é reconstruída (ver TopToolbar::collapseToOverflow).
        const bool wasVisible = !button->isHidden();
        button->setParent(this);
        if (wasVisible) button->show();
    }
    m_layout->addWidget(button, 0,
                         m_orientation == Qt::Horizontal ? Qt::AlignVCenter : Qt::AlignHCenter);
    // Sempre, não só em modo de edição: fora dele o filtro é transparente e
    // serve pra cronometrar o clique-e-segurar que LIGA o modo.
    button->installEventFilter(this);
}

void ToolbarGroupWidget::clearButtons()
{
    for (const auto& entry : std::as_const(m_buttons)) {
        if (!entry.second) continue;
        entry.second->removeEventFilter(this);
        m_layout->removeWidget(entry.second);
    }
    m_buttons.clear();
}

void ToolbarGroupWidget::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) return;
    m_orientation = orientation;
    const bool horizontal = (orientation == Qt::Horizontal);
    m_layout->setDirection(horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    m_handle->setFixedSize(horizontal ? QSize(kHandleThickness, 20)
                                      : QSize(20, kHandleThickness));
    m_layout->setAlignment(m_handle, horizontal ? Qt::AlignVCenter : Qt::AlignHCenter);
    refreshEmptyPlaceholder();
    update();
}

void ToolbarGroupWidget::setDraggable(bool on)
{
    if (m_draggable == on) return;
    m_draggable = on;
    m_handle->setVisible(on);
    setAcceptDrops(on);
    refreshEmptyPlaceholder();
    resetPress();
    update();
}

void ToolbarGroupWidget::refreshEmptyPlaceholder()
{
    const bool reserve = m_draggable && m_buttons.isEmpty();
    // Zera SEMPRE os dois eixos antes: trocar de orientacao ao vivo deixaria o
    // eixo antigo preso na reserva anterior.
    setMinimumWidth(0);
    setMinimumHeight(0);
    if (!reserve) return;
    if (m_orientation == Qt::Horizontal) setMinimumWidth(kEmptyDropExtent);
    else                                 setMinimumHeight(kEmptyDropExtent);
}

void ToolbarGroupWidget::applyTheme(const QColor& handleColor, const QColor& dropHighlight)
{
    m_handleColor = handleColor;
    m_dropHighlightColor = dropHighlight;
    update();
}

QString ToolbarGroupWidget::buttonIdOf(const QWidget* w) const
{
    for (const auto& entry : m_buttons)
        if (entry.second == w) return entry.first;
    return QString();
}

QWidget* ToolbarGroupWidget::buttonWidget(const QString& id) const
{
    for (const auto& entry : m_buttons)
        if (entry.first == id) return entry.second;
    return nullptr;
}

int ToolbarGroupWidget::alongAxis(const QPoint& p) const
{
    return m_orientation == Qt::Horizontal ? p.x() : p.y();
}

void ToolbarGroupWidget::resetPress()
{
    m_pressActive = false;
    m_dragStartPos = QPoint();
    m_pressedButtonId.clear();
    cancelLongPress();
}

// Montada na exibição, não gravada em cada botão: vários tooltips da barra são
// dinâmicos (tamanho da fonte, nome da fonte, espaçamento) e seriam
// sobrescritos por quem os atualiza. Assim a dica acompanha qualquer texto,
// presente ou futuro, sem tocar em nenhuma das ~30 chamadas de setToolTip().
QString ToolbarGroupWidget::composedToolTip(const QWidget* button) const
{
    const QString base = button->toolTip().trimmed();

    QString hint;
    if (m_draggable) {
        hint = tr("Arraste para mover");
    } else {
        // Nos botões de menu InstantPopup o gesto não funciona (o menu abre no
        // press), então prometer a dica neles seria mentira — ver startLongPress.
        const auto* tb = qobject_cast<const QToolButton*>(button);
        const bool instantMenu = tb && tb->menu() && tb->popupMode() == QToolButton::InstantPopup;
        if (!instantMenu) hint = tr("Clique e segure para reorganizar");
    }
    if (hint.isEmpty()) return base;

    const QString hintHtml = QStringLiteral("<div style=\"color:%1;\">%2</div>")
                                 .arg(m_handleColor.name(), hint.toHtmlEscaped());
    if (base.isEmpty()) return hintHtml;
    return QStringLiteral("<div>%1</div>%2").arg(base.toHtmlEscaped(), hintHtml);
}

void ToolbarGroupWidget::startLongPress(QWidget* button)
{
    // Botão de menu InstantPopup abre o menu já no press, e o menu roda um event
    // loop próprio por cima — o gesto terminaria em cima de um menu aberto. Fora
    // do modo de edição esses ficam sem o gesto (os outros botões cobrem bem).
    if (!m_draggable) {
        auto* tb = qobject_cast<QToolButton*>(button);
        if (tb && tb->menu() && tb->popupMode() == QToolButton::InstantPopup) return;
    }
    m_longPressButton = button;
    m_longPressFired = false;
    m_longPressTimer->start();
}

void ToolbarGroupWidget::cancelLongPress()
{
    if (m_longPressTimer) m_longPressTimer->stop();
    m_longPressButton = nullptr;
}

void ToolbarGroupWidget::onLongPressElapsed()
{
    QWidget* button = m_longPressButton;
    m_longPressButton = nullptr;
    if (!button) return;
    m_longPressFired = true;
    // Fora do modo de edição o press NÃO foi engolido, então o botão está
    // visualmente afundado esperando um release que vamos engolir.
    if (auto* ab = qobject_cast<QAbstractButton*>(button)) ab->setDown(false);
    m_pressActive = false;
    emit toggleEditModeRequested();
}

// ---------------------------------------------------------------- arrastar --

bool ToolbarGroupWidget::eventFilter(QObject* watched, QEvent* event)
{
    auto* w = qobject_cast<QWidget*>(watched);
    if (!w || w->parentWidget() != this) return QWidget::eventFilter(watched, event);

    // `swallow` é a diferença entre os dois modos: editando, o filtro consome
    // tudo (o botão vira algo pra arrastar, não pra clicar); fora da edição ele
    // deixa passar e só observa, pra não mudar em nada o uso normal da barra.
    const bool swallow = m_draggable;

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_pressActive = true;
            m_dragStartPos = w->mapTo(this, me->position().toPoint());
            m_pressedButtonId = buttonIdOf(w);
            m_longPressFired = false;
            startLongPress(w);
        }
        return swallow;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (m_pressActive && (me->buttons() & Qt::LeftButton)) {
            const QPoint p = w->mapTo(this, me->position().toPoint());
            if ((p - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                // Saiu do lugar: é arrasto, não "segurar parado".
                cancelLongPress();
                if (m_draggable) beginDrag();
            }
        }
        return swallow;
    }
    case QEvent::MouseButtonRelease: {
        // Se o gesto de segurar já disparou, este release é o fim dele — engolir
        // pra que soltar o botão não dispare também a ação dele.
        const bool afterLongPress = m_longPressFired;
        m_longPressFired = false;
        resetPress();
        return swallow || afterLongPress;
    }
    case QEvent::MouseButtonDblClick:
        // Editando, é só mais um clique em cima de um botão: engolido como os
        // outros. Fora da edição passa direto e vira dois cliques normais.
        resetPress();
        return swallow;
    case QEvent::ToolTip: {
        const QString text = composedToolTip(w);
        if (text.isEmpty()) return false;
        QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), text, w);
        return true;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void ToolbarGroupWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_draggable && event->button() == Qt::LeftButton
        && m_handle->geometry().contains(event->pos())) {
        m_pressActive = true;
        m_dragStartPos = event->pos();
        m_pressedButtonId.clear(); // alça = arrastar o grupo inteiro
        // accept() é obrigatório: um press ignorado não vira grab, e sem grab
        // este widget nunca receberia os mouseMoveEvent seguintes — era por
        // isso que o arrasto de grupo simplesmente não acontecia.
        event->accept();
        return;
    }
    resetPress();
    QWidget::mousePressEvent(event);
}

void ToolbarGroupWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_pressActive && (event->buttons() & Qt::LeftButton)
        && (event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
        beginDrag();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ToolbarGroupWidget::mouseReleaseEvent(QMouseEvent* event)
{
    resetPress();
    QWidget::mouseReleaseEvent(event);
}

void ToolbarGroupWidget::beginDrag()
{
    const bool draggingGroup = m_pressedButtonId.isEmpty();
    QWidget* source = draggingGroup ? this : buttonWidget(m_pressedButtonId);
    if (!source) { resetPress(); return; }

    auto* mime = new QMimeData();
    if (draggingGroup) mime->setData(QString::fromLatin1(kGroupMimeType), m_groupId.toUtf8());
    else               mime->setData(QString::fromLatin1(kButtonMimeType), m_pressedButtonId.toUtf8());

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    // Miniatura do que está sendo arrastado, ancorada no cursor — sem isso o
    // arrasto fica sem feedback nenhum e parece que não pegou.
    const QPixmap preview = source->grab();
    if (!preview.isNull()) {
        drag->setPixmap(preview);
        drag->setHotSpot(QPoint(preview.width() / 2, preview.height() / 2));
    }

    // Zerar ANTES do exec(): ele roda um event loop aninhado e só retorna no
    // fim do arrasto.
    resetPress();
    drag->exec(Qt::MoveAction);
}

// ------------------------------------------------------------------ soltar --

void ToolbarGroupWidget::dragEnterEvent(QDragEnterEvent* event)
{
    // QDragEnterEvent É um QDragMoveEvent — mesma decisão, um lugar só.
    dragMoveEvent(event);
}

void ToolbarGroupWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (!m_draggable) return;
    const QMimeData* mime = event->mimeData();

    if (mime->hasFormat(QString::fromLatin1(kButtonMimeType))) {
        // Botão pode ser reposicionado dentro do próprio grupo, então aqui não
        // existe o "soltar em si mesmo" que o grupo precisa recusar.
        const int idx = insertionIndexAt(event->position().toPoint());
        if (idx != m_dropIndex) { m_dropIndex = idx; update(); }
        event->acceptProposedAction();
        return;
    }

    if (mime->hasFormat(QString::fromLatin1(kGroupMimeType))) {
        const QString draggedId = QString::fromUtf8(mime->data(QString::fromLatin1(kGroupMimeType)));
        if (draggedId == m_groupId) return; // não destaca soltar em si mesmo
        if (!m_groupDropHighlight) { m_groupDropHighlight = true; update(); }
        event->acceptProposedAction();
    }
}

void ToolbarGroupWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    m_groupDropHighlight = false;
    m_dropIndex = -1;
    update();
}

void ToolbarGroupWidget::dropEvent(QDropEvent* event)
{
    const int dropIndex = m_dropIndex;
    m_groupDropHighlight = false;
    m_dropIndex = -1;
    update();

    const QMimeData* mime = event->mimeData();

    if (mime->hasFormat(QString::fromLatin1(kButtonMimeType))) {
        const QString buttonId = QString::fromUtf8(mime->data(QString::fromLatin1(kButtonMimeType)));
        if (buttonId.isEmpty()) return;
        event->acceptProposedAction();
        const int idx = dropIndex >= 0 ? dropIndex : insertionIndexAt(event->position().toPoint());
        emit buttonDroppedOn(buttonId, m_groupId, idx);
        return;
    }

    if (mime->hasFormat(QString::fromLatin1(kGroupMimeType))) {
        const QString draggedId = QString::fromUtf8(mime->data(QString::fromLatin1(kGroupMimeType)));
        if (draggedId.isEmpty() || draggedId == m_groupId) return;
        event->acceptProposedAction();
        emit groupDroppedOn(draggedId, m_groupId);
    }
}

int ToolbarGroupWidget::insertionIndexAt(const QPoint& pos) const
{
    const int p = alongAxis(pos);
    int idx = 0;
    for (const auto& entry : m_buttons) {
        QWidget* w = entry.second;
        if (!w || w->isHidden()) { ++idx; continue; }
        if (p < alongAxis(w->geometry().center())) return idx;
        ++idx;
    }
    return m_buttons.size();
}

QRect ToolbarGroupWidget::insertionMarkerRect(int index) const
{
    // Antes do botão `index`; se index == n, depois do último.
    const bool horizontal = (m_orientation == Qt::Horizontal);
    int pos = 0;
    if (index < m_buttons.size() && m_buttons.at(index).second) {
        const QRect g = m_buttons.at(index).second->geometry();
        pos = (horizontal ? g.left() : g.top()) - m_layout->spacing() / 2;
    } else if (!m_buttons.isEmpty() && m_buttons.last().second) {
        const QRect g = m_buttons.last().second->geometry();
        pos = (horizontal ? g.right() : g.bottom()) + m_layout->spacing() / 2;
    } else {
        pos = horizontal ? width() / 2 : height() / 2;
    }
    return horizontal ? QRect(pos - kMarkerThickness / 2, 0, kMarkerThickness, height())
                      : QRect(0, pos - kMarkerThickness / 2, width(), kMarkerThickness);
}

void ToolbarGroupWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_draggable) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(m_groupDropHighlight ? m_dropHighlightColor : m_handleColor);
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(1.2);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    // Alça: 3 pontinhos na direção do layout, dentro da área reservada.
    p.setPen(Qt::NoPen);
    p.setBrush(m_handleColor);
    const QRect hr = m_handle->geometry();
    const QPoint c = hr.center();
    if (m_orientation == Qt::Horizontal) {
        for (int dy = -5; dy <= 5; dy += 5) p.drawEllipse(QPoint(c.x(), c.y() + dy), 1, 1);
    } else {
        for (int dx = -5; dx <= 5; dx += 5) p.drawEllipse(QPoint(c.x() + dx, c.y()), 1, 1);
    }

    // Onde o botão arrastado vai cair.
    if (m_dropIndex >= 0) {
        p.setBrush(m_dropHighlightColor);
        p.drawRoundedRect(insertionMarkerRect(m_dropIndex), 1.5, 1.5);
    }
}
