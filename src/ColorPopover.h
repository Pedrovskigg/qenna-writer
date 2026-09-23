#pragma once
// Seletor de cor do Qenna — balão ancorado onde o usuário clicou (não janela),
// com as cores do tema primeiro, paleta curada, recentes e uma área
// "Personalizada" recolhida (saturação/brilho, matiz, hex e, quando pedido,
// opacidade). Substitui o QColorDialog no app inteiro.
//
// Dois jeitos de usar:
//  - ColorPopover::getColor(...) — síncrono, mesmo contrato do
//    QColorDialog::getColor (cor inválida = cancelou). Troca direta.
//  - instância + sinais previewed()/finished() — prévia ao vivo, cor que
//    acompanha o tema e "Restaurar padrão" (ex.: linhas da Timeline).

#include <QColor>
#include <QColorDialog>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;
class QVBoxLayout;

namespace ColorPick {
// Escolha: cor concreta, ou uma referência a uma cor do tema ativo
// ("accent", "warning", "info", "success", "danger") que acompanha a troca
// de tema.
struct Choice {
    QColor  color;
    QString themeKey;
    bool isTheme() const { return !themeKey.isEmpty(); }
    bool operator==(const Choice& o) const
    { return themeKey == o.themeKey && (isTheme() || color == o.color); }
    bool operator!=(const Choice& o) const { return !(*this == o); }
};
QColor themeColor(const QString& key);   // cor atual do tema pra uma chave
QStringList themeKeys();                 // ordem de exibição
}

class ColorPopover : public QWidget {
    Q_OBJECT
public:
    enum Result { Accepted, Cancelled, Reset };
    struct Options {
        QString title;
        bool themeBinding = false; // escolha "do tema" guarda a referência
        bool alpha = false;        // mostra o controle de opacidade
        bool resetAvailable = false;
    };

    ColorPopover(const ColorPick::Choice& current, const Options& opt, QWidget* parent = nullptr);

    // Mostra ancorado num ponto da tela (normalmente onde foi o clique),
    // mantendo o balão inteiro dentro da tela.
    void popupAt(const QPoint& globalAnchor);

    // Troca direta do QColorDialog::getColor. Abre junto do cursor.
    static QColor getColor(const QColor& initial, QWidget* parent = nullptr,
                           const QString& title = QString(),
                           QColorDialog::ColorDialogOptions options = {});

signals:
    void previewed(ColorPick::Choice choice); // hover/arraste; volta pra atual ao sair
    void finished(ColorPick::Choice choice, ColorPopover::Result result);

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void build();
    void rebuildSwatches();
    void updateSelection();
    bool isSelected(const ColorPick::Choice& item) const;
    void setCustomOpen(bool open);
    void syncCustomFromCurrent();
    void preview(const ColorPick::Choice& c);
    void finish(Result r);
    void refreshHeader(const ColorPick::Choice& shown);
    void keepOnScreen();
    static void rememberRecent(const QColor& c);

    ColorPick::Choice m_initial;
    ColorPick::Choice m_current;
    Options m_opt;
    bool m_finished = false;
    QPoint m_anchor;

    QWidget*     m_panel = nullptr;
    QWidget*     m_curSwatch = nullptr;
    QLabel*      m_title = nullptr;
    QLabel*      m_sub = nullptr;
    QLabel*      m_themeNote = nullptr;
    QWidget*     m_themeRow = nullptr;
    QWidget*     m_paletteGrid = nullptr;
    QWidget*     m_recentSec = nullptr;
    QWidget*     m_recentRow = nullptr;
    QToolButton* m_disc = nullptr;
    QWidget*     m_custom = nullptr;
    class SVSquare*    m_sv = nullptr;
    class HueSlider*   m_hue = nullptr;
    class AlphaSlider* m_alpha = nullptr;
    QLineEdit*   m_hex = nullptr;
};
