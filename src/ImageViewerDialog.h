#pragma once

#include <QImage>
#include <QString>

class QWidget;

// Visualizador modal simples pra ver uma imagem em tamanho grande e salvá-la
// em disco — usado em qualquer lugar do app que mostra uma miniatura de
// imagem gerada/anexada (bolhas do chat, diálogo de geração, galeria) e
// precisa de um jeito de "ver grande" / "salvar como".
namespace ImageViewerDialog {

// Abre modal, bloqueia até fechar. suggestedFileName sem extensão (ex.
// "Klara_2026-07-31") — usado como nome padrão no "Salvar como".
void show(const QImage& image, QWidget* parent, const QString& suggestedFileName = QString());

} // namespace ImageViewerDialog
