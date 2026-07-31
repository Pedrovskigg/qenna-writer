#pragma once

#include <QImage>
#include <QString>
#include <QVector>

// Persistência simples das imagens geradas por IA num projeto — toda
// imagem gerada (via chat, ficha, menu de contexto ou seleção) vira um
// arquivo PNG em <projeto>/ai_context/imagens_geradas/, com metadata num
// índice JSON ao lado (index.json). Base pra galeria dentro do app
// (ImageGalleryDialog) — garante que a imagem sempre esteja salva em algum
// lugar, mesmo que o autor não clique em "Salvar como" manualmente.
namespace GeneratedImageGallery {

struct Entry {
    QString filePath;      // caminho absoluto do arquivo de imagem
    QString characterName; // vazio = geração livre, sem personagem alvo
    QString prompt;        // prompt final usado (engenhado ou fornecido pelo autor)
    QString createdAt;     // ISO 8601
};

// Salva img em disco (PNG) + adiciona ao índice. Entry.filePath vazio se
// falhar (projectRoot vazio, imagem nula, ou erro de escrita).
Entry save(const QString& projectRoot, const QImage& img,
           const QString& characterName, const QString& prompt);

// Lê o índice inteiro, mais recente primeiro. Lista vazia se não houver
// nada gerado ainda ou o projeto não tiver índice.
QVector<Entry> load(const QString& projectRoot);

} // namespace GeneratedImageGallery
