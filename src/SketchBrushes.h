#pragma once
// Pincéis do Esboço: os de fábrica (resources/brushes, CC0 do pacote
// mypaint-brushes) e os que a pessoa importa (.myb soltos ou um .zip com
// vários), guardados fora do projeto pra valer em todos.

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

namespace SketchBrushes {

struct Info {
    QString id;          // "lapis", ou o nome do arquivo importado
    QString name;        // o que aparece na lista
    QString sub;         // linha de baixo ("grafite, granulado")
    QString path;        // :/brushes/x.myb ou o arquivo importado
    bool    builtin = false;
    bool    eraser  = false;
    qreal   diameter = 0;   // tamanho com que abre (0 = o do pincel)
};

QList<Info> builtins();
QList<Info> imported();
Info find(const QString& id);       // id vazio/desconhecido = o Lápis
QString importDir();

// Copia pra pasta dos importados. Aceita .myb e .zip (pacote com vários).
// Devolve quantos pincéis entraram; `error` diz o que não deu.
int importFiles(const QStringList& paths, QString* error = nullptr);

QByteArray readMyb(const Info& info);
// Amostra de traço (um S com a pressão subindo e descendo), pra lista.
QImage preview(const Info& info, const QSize& size, const QColor& ink, qreal dpr = 1.0);

} // namespace SketchBrushes
