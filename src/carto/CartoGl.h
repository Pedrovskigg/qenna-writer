#pragma once
// Giro do Mapa em OpenGL (experimental, 26/11/2026, pedido do Pe).
//
// Enquanto a mão gira ou inclina o mapa, a placa de vídeo desenha o terreno: uma malha do mundo com a
// pintura vista de cima como textura, projetada com a mesma conta do pintor (CartoPaint). O chão dos
// sprites (o mapa de alturas da vista) sai de uma conta rápida no processador, sem cor nem luz, então
// árvores, casas, estradas e nomes continuam no caminho de sempre. Soltou, a pintura nítida de sempre.
//
// Tudo fora da tela: contexto num QOffscreenSurface, desenho num framebuffer próprio e leitura de volta
// pra uma imagem. A janela do app nunca vira OpenGL, e desligar a opção volta ao giro pelo processador.
//
// Travas (todas pedidas pelo Pe): desligado por padrão, só liga pelas Configurações › Avançado, com aviso
// de experimental; fechar o app segurando Shift desliga; e se o app fechar sem passar pela saída normal
// numa sessão em que o OpenGL foi usado (travou, caiu), a próxima abertura já vem com ele desligado.

#include "CartoPaint.h"

#include <QImage>
#include <atomic>
#include <memory>
#include <vector>

class QOpenGLContext;
class QOffscreenSurface;

namespace carto {

// O que a placa precisa, preparado fora da linha da tela (no pool do Canvas).
struct GlTerrain {
    quint64 key = 0;
    QImage tex;                       // a pintura vista de cima, com a luz da hora (RGBA8888)
    double s0 = 1, cu0 = 0, cv0 = 0;  // px do mundo por px da textura e a origem dela
    double ccx = 0, ccy = 0;          // centro do mundo
    std::vector<float> verts;         // por vértice: (u0, v0, A) na textura; A = erguida com liftK = 1
    std::vector<quint32> idx;
    // erguida por px do mundo (unidades do plano com liftK = 1; −1 = água), pro chão dos sprites
    int W = 0, H = 0;
    std::vector<float> A;
};

std::shared_ptr<GlTerrain> prepareGlTerrain(World& w, Style style, bool info, const RealmPaint* realms,
                                            const std::vector<float>& lut, quint64 key, int maxTex,
                                            const std::atomic<bool>* cancel);

// O mapa de alturas da vista g (já recortada no pedaço da tela): erguida de cada linha de chão e,
// por linha da tela, qual linha de chão aparece ali (−1 = água). O mesmo formato do pintor.
void fastAux(const GlTerrain& t, const Geom& g, double liftK, Aux& aux);

class GlSpin {
public:
    static bool enabled();                    // a opção (padrão desligada)
    static void setEnabled(bool on);
    static void startupGuard();               // chamar logo depois do QApplication nascer
    static GlSpin* get();                     // cria o contexto na primeira vez; nullptr sem OpenGL 3.3
    static QString unavailableReason();       // por que o get() devolveu nullptr (vazio se não tentou)

    int maxTexture() const { return m_maxTex; }
    quint64 uploadedKey() const { return m_key; }
    bool upload(const GlTerrain& t);          // malha e textura pra placa
    bool render(const Geom& g, double liftK, double s0, double cu0, double cv0, double ccx, double ccy,
                QImage& out);                 // a vista do pedaço, RGBA8888 do tamanho g.W × g.H

private:
    GlSpin() = default;
    bool init();
    bool ensureFbo(int w, int h);
    void fail(const QString& why);

    QOpenGLContext* m_ctx = nullptr;
    QOffscreenSurface* m_surface = nullptr;
    unsigned m_prog = 0, m_vao = 0, m_vbo = 0, m_ebo = 0, m_tex = 0, m_fbo = 0, m_color = 0, m_depth = 0;
    int m_fboW = 0, m_fboH = 0, m_texW = 1, m_texH = 1, m_maxTex = 4096;
    qint64 m_count = 0;
    quint64 m_key = 0;
    bool m_broken = false;
};

} // namespace carto
