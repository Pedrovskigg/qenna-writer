#pragma once

#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

class QPainter;
namespace Theme { struct MiraTheme; }

// Temas Animados (2026-10): o clima da mesa — chuva, neve, cidade, floresta,
// vela... — pintado atrás de todos os painéis. Portado do Weather Lab
// (C:/mira-writing/anim-lab, fx.js + city_v.js), onde cada cena foi aprovada.
//
// Tudo é desenhado numa cena lógica de 758 de altura (a mesma da prévia de
// temas) e largura proporcional à janela; quem pinta escala o QPainter.
// A mesa no app é o BackgroundWidget; a prévia e as miniaturas do painel de
// Temas chamam paintPreview().
namespace AnimBg {

constexpr double kSceneH = 758.0;

// ---- Preferências globais (Configurações > Aparência) ----
bool enabled();                 // false = a mesa fica parada no último quadro
void setEnabled(bool on);
int intensity();                // 0 leve, 1 média, 2 forte
void setIntensity(int level);

// ---- Personalizar de cada tema ----
// tod: "theme" | "day" | "dusk" | "night"; rain: "none" | "drizzle" | "rain" |
// "storm"; extras: "stars", "fog", "leaves", "petals", "fireflies", "snow".
struct Config {
    QString tod = QStringLiteral("theme");
    QString rain = QStringLiteral("none");
    QStringList extras;
    bool operator==(const Config& o) const;
    bool operator!=(const Config& o) const { return !(*this == o); }
};
Config factoryConfig(const Theme::MiraTheme& t);   // o que o tema traz de fábrica
Config config(const Theme::MiraTheme& t);          // o salvo pela pessoa, ou o de fábrica
void setConfig(const QString& themeId, const Config& c);
void resetConfig(const QString& themeId);
// Cenas com luz própria (vela, aurora, neon…) não trocam a hora do dia.
bool todAllowed(const QString& scene);
// Ordem das camadas no Personalizar.
QStringList extraIds();

// Avisa quem pinta (mesa, prévia) que algo mudou: preferência ou Personalizar.
class Notifier : public QObject {
    Q_OBJECT
public:
    static Notifier* instance();
signals:
    void changed();
private:
    Notifier() = default;
};

// Pedaços da mesa que ficam à vista dos dois lados da folha, em coordenadas
// da cena. É onde moram a vela, o farol, o sol, a placa de neon e as carpas.
struct Zones {
    QRectF left;
    QRectF right;
};

class Layer;

// Uma pilha de camadas: o cenário do tema, as camadas por cima e a chuva.
class Stack {
public:
    Stack();
    ~Stack();

    // Monta tudo de novo pra este tema e este Personalizar.
    void setup(const Theme::MiraTheme& t, const Config& c, double sceneW, const Zones& zones);
    // Janela mudou de tamanho ou a folha andou: só recalcula o necessário.
    void resize(double sceneW, const Zones& zones);
    bool isEmpty() const;
    const QString& themeId() const { return m_themeId; }

    void step(double dt);
    // O QPainter já está nas coordenadas da cena. pxPerUnit = pixels físicos
    // por unidade da cena (escala × devicePixelRatio): os caches internos são
    // feitos nessa resolução pra não borrar.
    void paint(QPainter& p, double pxPerUnit);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    QString m_themeId;
};

// Prévia do painel de Temas. `live` avança a animação pelo relógio a cada
// pintura (a prévia grande, redesenhada por timer); sem `live` é um quadro
// parado guardado em cache (as miniaturas da grade).
void paintPreview(QPainter& p, const QRectF& r, const Theme::MiraTheme& t, bool live);

} // namespace AnimBg
