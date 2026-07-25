#pragma once

#include <QPixmap>
#include <QString>

// Decodificação de foto de personagem (Element::image, data-URL base64) e
// montagem do avatar circular padrão do app. Extraído de três duplicações
// independentes (CharacterSheetPanel, PensarioPanel, e agora StatsPanel).
namespace AvatarUtils {

// Decodifica um data-URL base64 (ex. "data:image/png;base64,....") pra
// QPixmap. Retorna pixmap nulo se vazio/inválido.
QPixmap decodeDataUrl(const QString& dataUrl);

// Avatar circular pronto pra uso em listas/cards: usa a foto real se houver
// (crop central, preenchendo o círculo); senão, círculo colorido (cor
// estável derivada de `idForColor`) com a inicial de `name`.
QPixmap circularAvatar(const QString& dataUrl, const QString& name,
                       const QString& idForColor, int size);

} // namespace AvatarUtils
