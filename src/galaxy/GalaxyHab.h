#pragma once

// As classes de planeta do lado de quem vê: nome, cor, o porquê em palavras, o selo e a barra da mistura.

#include "GalaxyViews.h"

#include <QWidget>

class QLabel;

namespace galaxy {

Rgb habRgb(int n);
QColor habColor(int n, double a = 1);
const char* habRoman(int n);
QString habName(int n);
QString habDesc(int n);
QString habLabel(int n);     // "I · Éden"
QString habWhy(const HabInfo& i, const QString& sep);
QString atmText(const Body& b);     // a atmosfera traduzida, ou o que o autor decidiu
QLabel* habPill(int n, QWidget* parent);

// A mistura de um lugar: uma barra só, com a largura de cada classe.
class HabBar : public QWidget {
public:
    explicit HabBar(QWidget* parent = nullptr);
    void setCounts(const std::array<int, 7>& c);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    std::array<int, 7> m_c {};
};

} // namespace galaxy
