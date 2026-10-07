#pragma once

#include <QFrame>
#include <QList>
#include <QPair>
#include <QString>

class QToolButton;
class QHBoxLayout;

// Doca da Lousa (opção 1 do rework, escolhida em 2026-10-06): a barra de
// criação flutuando embaixo do quadro, com o nome de cada ferramenta. Fica
// só com os ícones quando a janela é estreita.
class LousaDock : public QFrame
{
    Q_OBJECT
public:
    explicit LousaDock(QWidget* parent = nullptr);

    void setCompact(bool compact);
    bool isCompact() const { return m_compact; }
    int  fullWidth() const;          // largura com os nomes
    // Botões que ficam "acesos" enquanto o modo está ligado (Área, Ligar).
    void setToolChecked(const QString& kind, bool on);
    void applyTheme();

signals:
    // postit, comment, text, symbol, image, character, doc, area, connect
    void createRequested(const QString& kind);

private:
    void layoutButtons();
    QHBoxLayout* m_lay = nullptr;
    QList<QPair<QToolButton*, QString>> m_tools;
    QList<QFrame*> m_seps;
    bool m_compact = false;
};
