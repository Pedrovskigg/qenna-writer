#pragma once
// Vinhetas generativas da gaveta de Manuscritos (Índice ilustrado, Temporadas).
//
// Duas fontes com papéis diferentes: a IDENTIDADE vem do id do capítulo (que
// nunca muda, nem renomeando) e escolhe a família e o desenho; o CRESCIMENTO
// vem das palavras — a árvore do capítulo 3 é sempre a mesma árvore, só ganha
// galhos conforme o capítulo cresce. Trocar a família (menu "Trocar desenho")
// muda só a fundação: semente e crescimento continuam os mesmos.

#include <QColor>
#include <QList>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>

namespace MsVignette {

enum Family { Branches, Roots, Flames, City, Stars, Mountains, Coral, Cracks,
              Mandala, Lightning, Waves, Circles, Rays, FamilyCount };

// Ids gravados no projeto ("branches", "city"…) e nomes na tela.
QString familyId(int family);
int familyFromId(const QString& id);           // -1 = vazio/desconhecido
QString familyName(int family);

// Família automática de um capítulo (só pelo id).
int autoFamily(const QString& chapterId);

// Famílias de um livro, na ordem de leitura. Automático: vizinhos nunca caem
// na mesma família. A família do livro (bookFamily) vale pra todos; a do
// capítulo (chapterFamilies[i], "" = sem) vale mais que a do livro.
QList<int> familiesForBook(const QStringList& chapterIds, const QStringList& chapterFamilies,
                           const QString& bookFamily);
// Só o automático (sem as escolhas), pro item "Automático" do menu.
QList<int> autoFamilies(const QStringList& chapterIds);

// Desenha a vinheta. circle = medalhão redondo (Índice ilustrado); senão
// retângulo arredondado que ocupa o quadro inteiro (Temporadas). Um quadro
// mais largo mostra mais cena, não mais zoom.
QPixmap render(const QString& seedId, int words, const QColor& base, int family,
               QSize size, qreal dpr, bool circle);

// Imagem própria do capítulo, recortada no mesmo formato.
QPixmap renderImage(const QPixmap& image, const QColor& base, QSize size, qreal dpr, bool circle);

}
