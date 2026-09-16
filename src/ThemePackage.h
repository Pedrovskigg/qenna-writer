#pragma once

#include <QString>
#include <QStringList>

#include "Theme.h"

// Leitura e escrita de arquivos .qtheme — um tema empacotado pra circular
// entre pessoas.
//
// Um .qtheme é um ZIP com no máximo dois itens:
//
//     tema.qtheme
//     ├── theme.json        envelope + o tema serializado
//     └── background.<ext>  a imagem de fundo, quando o tema tem uma
//
// A imagem viaja DENTRO do pacote de propósito: MiraTheme::backgroundImage
// guarda um caminho absoluto da máquina de quem criou o tema, que não existe
// no computador de quem recebe (e que, de quebra, revelaria o nome de usuário
// do autor). No pacote esse campo vira só o nome do item no zip; na importação
// a imagem é extraída pra pasta de dados do app e o campo volta a ser um
// caminho local válido.
//
// Sobre compatibilidade: `format` versiona o envelope. Um pacote de formato
// mais novo ainda é importado — o que esta versão não entender é preservado em
// MiraTheme::extras e devolvido intacto num export futuro, junto de um aviso
// pro usuário. A ideia é que um tema feito hoje continue válido daqui a anos,
// e que um tema feito daqui a anos não seja destruído por um app antigo.
namespace ThemePackage {

// Versão do envelope. Suba só em mudança que quebre a leitura; campo novo
// dentro do tema não precisa disso (extras cuida).
constexpr int kFormatVersion = 1;

// Marcador de tipo dentro do theme.json — impede tratar um zip qualquer
// (ou um pacote de outro tipo, quando existirem) como tema.
inline QString kindTag() { return QStringLiteral("qenna-theme"); }

// "qtheme", sem ponto.
QString extension();

// Filtro pronto pro QFileDialog, já traduzido.
QString fileFilter();

// Nome de arquivo sugerido a partir do nome do tema, sem caracteres que o
// Windows recusa.
QString suggestedFileName(const Theme::MiraTheme& theme);

// Licenças sugeridas. No arquivo gravamos um CÓDIGO estável ("free-with-credit"),
// nunca o texto traduzido: um tema exportado em português precisa dizer a mesma
// coisa quando aberto num Qenna em francês, e uma loja futura precisa conseguir
// filtrar por licença sem adivinhar idioma. O autor também pode escrever a sua
// própria — aí o texto dele vai cru, e é exibido como veio.
QStringList licenseCodes();

// Texto legível de um código conhecido, no idioma atual. Código desconhecido
// (licença escrita à mão pelo autor) volta como veio.
QString licenseDisplayName(const QString& code);

struct ImportResult {
    Theme::MiraTheme theme;
    // Preenchido quando o pacote veio de um Qenna mais novo: o tema foi
    // importado assim mesmo, mas pode ter detalhe que esta versão não aplica.
    // Vazio = importação limpa.
    QString warning;
};

// Grava `theme` como .qtheme em `path`. Gera um uuid se o tema ainda não tiver.
bool exportToFile(const Theme::MiraTheme& theme, const QString& path, QString* error = nullptr);

// Lê um .qtheme. O tema devolvido vem com bundled=false e `id` vazio — quem
// chama deve atribuir um id local (Theme::Manager::uniqueCustomId) antes de
// salvar, porque o id é local à máquina; o uuid é que atravessa.
bool importFromFile(const QString& path, ImportResult* out, QString* error = nullptr);

} // namespace ThemePackage
