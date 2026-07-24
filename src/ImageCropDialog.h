#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

class QPushButton;
class CropCanvas;

// Diálogo modal de recorte manual de foto — abre o seletor de arquivo, deixa
// o usuário arrastar/redimensionar um quadrado sobre a imagem original (em
// vez do crop automático centralizado de sempre) e devolve o resultado final
// pronto (quadrado 400x400 JPEG base64) pra salvar em Element::image. Único
// ponto de crop manual do app — substitui as duas cópias de crop automático
// que existiam em ElementCreateDialog e CharacterSheetPanel.
class ImageCropDialog : public QDialog {
    Q_OBJECT
public:
    // Abre QFileDialog::getOpenFileName; se o usuário escolher um arquivo,
    // carrega a imagem, abre o diálogo de crop, e devolve a data URL final
    // (quadrado 400x400 JPEG base64). Retorna QString() se o usuário cancelar
    // em qualquer etapa (seleção de arquivo ou crop) ou a imagem não carregar.
    static QString pickAndCropImage(QWidget* parent,
                                     const QString& fileDialogTitle = tr("Selecionar foto"));

private:
    explicit ImageCropDialog(const QImage& original, QWidget* parent);

    // Aplica o retângulo de crop escolhido (convertido de volta pra
    // coordenadas da imagem original) + o pipeline de encode final.
    QString resultDataUrl() const;

    QImage       m_original;  // resolução plena — só ela é usada no copy() final
    CropCanvas*  m_canvas = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};
