#include "SketchBrushes.h"

#include "SketchEngine.h"
#include "ZipReader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QStandardPaths>
#include <cmath>

namespace SketchBrushes {

namespace {
struct Def { const char* id; const char* name; const char* sub; qreal diameter; bool eraser; };
const Def kDefs[] = {
    { "lapis",    QT_TRANSLATE_NOOP("SketchBrushes", "Lápis"),        QT_TRANSLATE_NOOP("SketchBrushes", "grafite, granulado"), 5, false },
    { "carvao",   QT_TRANSLATE_NOOP("SketchBrushes", "Carvão"),       QT_TRANSLATE_NOOP("SketchBrushes", "grosso, áspero"), 12, false },
    { "nanquim",  QT_TRANSLATE_NOOP("SketchBrushes", "Nanquim"),      QT_TRANSLATE_NOOP("SketchBrushes", "ponta fina, firme"), 6, false },
    { "aquarela", QT_TRANSLATE_NOOP("SketchBrushes", "Aquarela"),     QT_TRANSLATE_NOOP("SketchBrushes", "transparente, borda molhada"), 28, false },
    { "macio",    QT_TRANSLATE_NOOP("SketchBrushes", "Pincel macio"), QT_TRANSLATE_NOOP("SketchBrushes", "sombra, volume"), 48, false },
    { "marcador", QT_TRANSLATE_NOOP("SketchBrushes", "Marcador"),     QT_TRANSLATE_NOOP("SketchBrushes", "cor chapada"), 18, false },
    { "borracha", QT_TRANSLATE_NOOP("SketchBrushes", "Borracha"),     QT_TRANSLATE_NOOP("SketchBrushes", "apaga onde passa"), 18, true },
};
} // namespace

QList<Info> builtins()
{
    QList<Info> out;
    for (const Def& d : kDefs) {
        Info i;
        i.id = QString::fromLatin1(d.id);
        i.name = QCoreApplication::translate("SketchBrushes", d.name);
        i.sub = QCoreApplication::translate("SketchBrushes", d.sub);
        i.path = QStringLiteral(":/brushes/%1.myb").arg(i.id);
        i.builtin = true;
        i.eraser = d.eraser;
        i.diameter = d.diameter;
        out << i;
    }
    return out;
}

QString importDir()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/brushes");
    QDir().mkpath(d);
    return d;
}

QList<Info> imported()
{
    QList<Info> out;
    const QFileInfoList files = QDir(importDir()).entryInfoList({ QStringLiteral("*.myb") }, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& fi : files) {
        Info i;
        i.id = QStringLiteral("user:") + fi.fileName();
        QString n = fi.completeBaseName();
        n.replace(QLatin1Char('_'), QLatin1Char(' ')).replace(QLatin1Char('-'), QLatin1Char(' '));
        i.name = n;
        i.path = fi.absoluteFilePath();
        i.eraser = n.contains(QStringLiteral("eraser"), Qt::CaseInsensitive)
                || n.contains(QStringLiteral("borracha"), Qt::CaseInsensitive);
        out << i;
    }
    return out;
}

Info find(const QString& id)
{
    for (const Info& i : builtins()) if (i.id == id) return i;
    for (const Info& i : imported()) if (i.id == id) return i;
    return builtins().first();
}

QByteArray readMyb(const Info& info)
{
    QFile f(info.path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

namespace {
bool looksLikeMyb(const QByteArray& data)
{
    SketchBrush b;
    return b.loadMyb(data);
}

QString freeName(const QString& base)
{
    const QDir dir(importDir());
    QString name = base;
    int n = 2;
    while (dir.exists(name)) name = QStringLiteral("%1 (%2).myb").arg(QFileInfo(base).completeBaseName()).arg(n++);
    return name;
}
} // namespace

int importFiles(const QStringList& paths, QString* error)
{
    int count = 0;
    QStringList bad;
    const QDir dir(importDir());
    auto save = [&](const QString& fileName, const QByteArray& data) {
        if (!looksLikeMyb(data)) { bad << fileName; return; }
        QFile out(dir.filePath(freeName(QFileInfo(fileName).fileName())));
        if (out.open(QIODevice::WriteOnly)) { out.write(data); ++count; }
    };
    for (const QString& p : paths) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) { bad << QFileInfo(p).fileName(); continue; }
        const QByteArray data = f.readAll();
        if (p.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
            ZipReader zip;
            if (!zip.open(data)) { bad << QFileInfo(p).fileName(); continue; }
            for (const QString& entry : zip.fileNames())
                if (entry.endsWith(QStringLiteral(".myb"), Qt::CaseInsensitive))
                    save(entry, zip.fileData(entry));
        } else {
            save(p, data);
        }
    }
    if (error && !bad.isEmpty())
        *error = QCoreApplication::translate("SketchBrushes", "Não são pincéis do MyPaint: %1").arg(bad.join(QStringLiteral(", ")));
    return count;
}

QImage preview(const Info& info, const QSize& size, const QColor& ink, qreal dpr)
{
    const int w = qMax(8, int(size.width() * dpr)), h = qMax(8, int(size.height() * dpr));
    // A borracha apaga: risca um lápis e passa ela no meio, pra dar pra ver.
    SketchSurface s(w, h);
    SketchBrush b;
    auto curve = [&](SketchBrush& br, qreal from, qreal to) {
        br.newStroke();
        s.beginAtomic();
        const int N = 90;
        for (int i = 0; i <= N; ++i) {
            const qreal u = from + (to - from) * i / qreal(N);
            const qreal x = w * (0.06 + 0.88 * u);
            const qreal y = h / 2.0 + std::sin(u * M_PI * 1.6) * h * 0.24;
            const qreal pr = i == 0 ? 0.0 : std::sin(u * M_PI) * 0.85 + 0.1;
            br.strokeTo(s, x, y, pr, 0, 0, 0.004);
        }
        s.endAtomic();
    };
    if (info.eraser) {
        SketchBrush base;
        base.loadMyb(readMyb(builtins().first()));
        base.setColor(ink);
        base.setDiameter(h * 0.22);
        curve(base, 0.0, 1.0);
    }
    if (!b.loadMyb(readMyb(info))) return {};
    b.setColor(ink);
    // Na amostra, o tamanho segue a altura (um aerógrafo de 200 px vira mancha).
    const qreal d = info.diameter > 0 ? info.diameter : b.baseDiameter();
    b.setDiameter(qBound(1.5 * dpr, d * dpr * 0.35, h * 0.55));
    if (info.eraser) curve(b, 0.35, 0.65);
    else             curve(b, 0.0, 1.0);
    QImage img = s.toImage();
    img.setDevicePixelRatio(dpr);
    return img;
}

} // namespace SketchBrushes
