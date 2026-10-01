#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "ScreenplayFormat.h"

// Fountain (fountain.io): roteiro em texto puro, lido por vários programas de
// roteiro. Lê e escreve o essencial da especificação: cabeçalho de cena, ação,
// personagem, parênteses, diálogo, transição, página de rosto e seções (#),
// que no Qenna viram os capítulos/sequências. Ênfase (*itálico*, **negrito**)
// sai do texto na leitura; notas [[ ]] e /* comentários */ são ignorados.
namespace Fountain {

struct Line {
    ScreenplayElement element = ScreenplayElement::Action;
    QString text;
};

struct Chapter {
    QString title;          // da seção "# ..." (vazio = sem seção)
    QList<Line> lines;
};

struct Document {
    QString title, author, contact;   // da página de rosto, se houver
    QList<Chapter> chapters;
    int sceneCount() const;
    // Deixa de cada personagem (sem extensões) → quantas falas.
    QList<QPair<QString, int>> characters() const;
};

Document parse(const QString& text);

// HTML de um capítulo no formato do editor (geometria de cada elemento e um
// <hr> antes de cada cena que não abre o capítulo — cena do Qenna).
QString chapterHtml(const QList<Line>& lines);

// Texto Fountain de um roteiro: página de rosto, "# título" por capítulo e o
// corpo. sceneNumbers põe "#1#" no fim de cada cabeçalho.
struct ChapterSource { QString title; QString html; };
QString write(const QList<ChapterSource>& chapters, const QString& title, const QString& author,
              const QString& contact, bool sceneNumbers);

}
