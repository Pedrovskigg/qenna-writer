#pragma once

#include <QColor>
#include <QFrame>
#include <QString>
#include <QVector>

class QHBoxLayout;

// Barra que aparece em cima do que está selecionado na Lousa (card, vários
// cards, linha). Substitui os ícones minúsculos que ficavam em todo post-it.
// Quem decide os botões é o LousaPanel; aqui só se desenha e avisa o clique.
class LousaActionBar : public QFrame
{
    Q_OBJECT
public:
    struct Btn {
        QString id;
        QString icon;        // nome em :/icons/lousa/ ("" = só texto)
        QString text;        // "" = só ícone (o tip vira tooltip)
        QString tip;
        bool    checkable = false;
        bool    checked   = false;
        QColor  swatch;      // válido = botão de cor (bolinha)
        bool    separatorBefore = false;
        bool    danger = false;
    };

    explicit LousaActionBar(QWidget* parent = nullptr);
    void setButtons(const QVector<Btn>& buttons);
    void applyTheme();

signals:
    void triggered(const QString& id, const QPoint& globalPos);

private:
    QHBoxLayout*  m_lay = nullptr;
    QVector<Btn>  m_buttons;
};
