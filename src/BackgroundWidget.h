#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>

// Widget transparente que pinta uma imagem de fundo cobrindo toda a sua
// área, com modos centrar / repetir / esticar / ajustar / preencher.
// É posto como filho do editorContainer e mantido em lower() pra ficar
// atrás de todos os outros painéis. Por cima da imagem (ou só da cor, sem
// imagem) pinta o overlay do tema — degradê e grão (BackgroundOverlay). Sem
// imagem nem overlay, o MainWindow o esconde e o container pinta a cor via QSS.
class BackgroundWidget : public QWidget {
    Q_OBJECT
public:
    explicit BackgroundWidget(QWidget* parent = nullptr);

    // Define o path da imagem (vazio = sem imagem) e o modo de exibição
    // (Theme::BackgroundMode). Releitura ocorre só quando o path muda.
    void setBackground(const QString& imagePath, int mode);

    // Cor sólida pintada antes da imagem. Útil quando o modo deixa áreas
    // sem cobertura (centralizado, ajustar).
    void setFillColor(const QColor& color);

    bool hasImage() const { return !m_source.isNull(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void rebuildScaled();
    void paintImage(QPainter& p);

    QString m_path;
    int     m_mode = 3; // BgFit
    QPixmap m_source;   // imagem decodificada em tamanho nativo
    QPixmap m_scaled;   // cache do resultado pra tamanho atual da janela
    QSize   m_scaledFor; // qual size o cache foi gerado
    QColor  m_fill;
};
