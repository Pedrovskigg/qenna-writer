#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>

#include <memory>

namespace AnimBg { class Stack; }
class QTimer;

// Widget transparente que pinta uma imagem de fundo cobrindo toda a sua
// área, com modos centrar / repetir / esticar / ajustar / preencher.
// É posto como filho do editorContainer e mantido em lower() pra ficar
// atrás de todos os outros painéis. Por cima da imagem (ou só da cor, sem
// imagem) pinta o overlay do tema — degradê e grão (BackgroundOverlay). Sem
// imagem nem overlay, o MainWindow o esconde e o container pinta a cor via QSS.
//
// Nos temas Animados (AnimBackground) a mesa ganha vida: um timer de 30
// quadros por segundo avança o clima e repinta só a mesa à vista — nunca a
// folha nem os painéis por cima. Pausa sozinho quando o Qenna perde o foco,
// é minimizado, ou quando a animação está desligada nas Configurações.
class BackgroundWidget : public QWidget {
    Q_OBJECT
public:
    explicit BackgroundWidget(QWidget* parent = nullptr);
    ~BackgroundWidget() override;

    // Define o path da imagem (vazio = sem imagem) e o modo de exibição
    // (Theme::BackgroundMode). Releitura ocorre só quando o path muda.
    void setBackground(const QString& imagePath, int mode);

    // Cor sólida pintada antes da imagem. Útil quando o modo deixa áreas
    // sem cobertura (centralizado, ajustar).
    void setFillColor(const QColor& color);

    bool hasImage() const { return !m_source.isNull(); }

    // Liga, troca ou desliga a animação conforme o tema atual e o Personalizar.
    void refreshAnimation();
    bool animated() const { return bool(m_anim); }

    // Onde está a folha, nas coordenadas deste widget. A animação nunca
    // repinta em cima dela; nos temas Animados a sombra da folha é pintada
    // aqui (o QGraphicsDropShadowEffect refaria o desfoque a cada quadro).
    void setSheetRect(const QRect& r);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void rebuildScaled();
    void paintImage(QPainter& p);
    void paintSheetShadow(QPainter& p);
    void tick();
    void updateTimer();
    QRegion animRegion() const;
    double sceneScale() const;

    QString m_path;
    int     m_mode = 3; // BgFit
    QPixmap m_source;   // imagem decodificada em tamanho nativo
    QPixmap m_scaled;   // cache do resultado pra tamanho atual da janela
    QSize   m_scaledFor; // qual size o cache foi gerado
    QColor  m_fill;

    std::unique_ptr<AnimBg::Stack> m_anim;
    QString m_animKey;
    QTimer* m_timer = nullptr;
    QElapsedTimer m_clock;
    QRect m_sheet;
    QImage m_shadow;
    QString m_shadowKey;
};
