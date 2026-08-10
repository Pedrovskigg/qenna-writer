#pragma once

#include <QPixmap>
#include <QString>

// Utilitários de capa retrato (projeto/manuscrito): leitura de arquivo →
// data URL JPEG sem crop forçado (mantém proporção), e decodificação de
// volta pra preview. Compartilhado por ProjectInfoPanel e o diálogo de
// criação/edição de manuscrito (MainWindow.cpp). Não usar pra avatares —
// esses seguem o padrão quadrado do ImageCropDialog.
namespace CoverUtils {

constexpr int kMaxCoverSide = 1200;
constexpr int kJpegQuality = 88;

// Carrega imagem do disco, redimensiona pra caber em kMaxCoverSide mantendo
// proporção e devolve data URL JPEG. String vazia se falhar a leitura.
QString loadCoverAsDataUrl(const QString& path);

// Decodifica um data URL de volta pra QPixmap. QPixmap nula se vazio/inválido.
QPixmap pixmapFromDataUrl(const QString& dataUrl);

} // namespace CoverUtils
