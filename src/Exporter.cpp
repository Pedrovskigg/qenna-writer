#include "Exporter.h"

#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"
#include "ZipWriter.h"
#include "WordCounter.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QSettings>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QProgressDialog>
#include <QFont>
#include <QImage>
#include <QMarginsF>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QUuid>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentWriter>
#include <QTextFrame>
#include <algorithm>
#include <functional>

namespace {
// O HTML salvo carrega a cor de texto do tema da tela (clara, pra fundo escuro),
// que sobre papel claro (ODT/EPUB) ou fundo trocado (preview) fica
// ilegível/errada. Força o foreground de todo o documento pra uma cor fixa,
// preservando negrito/itálico/sublinhado e os marca-textos (que são cor de
// fundo). Exportação sempre chama com preto; o preview de e-reader passa a
// cor de tinta da paleta ativa (clara ou escura).
void forceTextColor(QTextDocument& doc, const QColor& color) {
    QTextCursor c(&doc);
    c.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setForeground(color);
    c.mergeCharFormat(fmt);
}

// Remove os marca-textos (cor de fundo do texto). Itera fragmento a fragmento,
// coletando os ranges com fundo (e o formato já SEM background) antes de alterar,
// pra não invalidar a iteração. Usa setCharFormat com clearBackground em vez de
// mesclar um brush "vazio" — QBrush(Qt::NoBrush) tem cor preta por padrão e o
// gravador ODF a escreveria como fundo preto. O único background no texto do
// app é o marcador, então isso só apaga marca-textos.
void stripMarkers(QTextDocument& doc) {
    struct Range { int from; int to; QTextCharFormat fmt; };
    QList<Range> ranges;
    for (QTextBlock blk = doc.begin(); blk.isValid(); blk = blk.next()) {
        for (auto it = blk.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            QTextCharFormat fmt = frag.charFormat();
            if (fmt.background().style() == Qt::NoBrush) continue;
            fmt.clearBackground();
            ranges.append({ frag.position(), frag.position() + frag.length(), fmt });
        }
    }
    for (const Range& r : ranges) {
        QTextCursor c(&doc);
        c.setPosition(r.from);
        c.setPosition(r.to, QTextCursor::KeepAnchor);
        c.setCharFormat(r.fmt);
    }
}

// Preview em preto-e-branco: em vez de apagar o marca-texto (como
// stripMarkers), converte o fundo colorido pra um cinza de luminância
// equivalente — o usuário quer VER que ali tinha um marcador, só sem cor.
void desaturateMarkerBackgrounds(QTextDocument& doc) {
    struct Range { int from; int to; QTextCharFormat fmt; };
    QList<Range> ranges;
    for (QTextBlock blk = doc.begin(); blk.isValid(); blk = blk.next()) {
        for (auto it = blk.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            QTextCharFormat fmt = frag.charFormat();
            if (fmt.background().style() == Qt::NoBrush) continue;
            const int gray = qGray(fmt.background().color().rgb());
            fmt.setBackground(QColor(gray, gray, gray));
            ranges.append({ frag.position(), frag.position() + frag.length(), fmt });
        }
    }
    for (const Range& r : ranges) {
        QTextCursor c(&doc);
        c.setPosition(r.from);
        c.setPosition(r.to, QTextCursor::KeepAnchor);
        c.setCharFormat(r.fmt);
    }
}

// Preview em preto-e-branco: dessatura toda imagem embutida no documento
// (capa e imagens de corpo). Funciona tanto pra imagem inserida via
// QTextCursor::insertImage (cover, resource pré-registrado) quanto pra <img
// data:...> vindo de insertHtml (o próprio Qt decodifica e cacheia o recurso
// sob o nome/URL da tag na primeira vez que doc.resource() é chamado).
void desaturateImages(QTextDocument& doc) {
    QSet<QString> names;
    for (QTextBlock blk = doc.begin(); blk.isValid(); blk = blk.next()) {
        for (auto it = blk.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat fmt = frag.charFormat();
            if (fmt.isImageFormat()) names.insert(fmt.toImageFormat().name());
        }
    }
    for (const QString& name : names) {
        const QUrl url(name);
        const QImage img = doc.resource(QTextDocument::ImageResource, url).value<QImage>();
        if (img.isNull()) continue;
        const QImage gray = img.convertToFormat(QImage::Format_Grayscale8)
                                .convertToFormat(QImage::Format_RGB32);
        doc.addResource(QTextDocument::ImageResource, url, gray);
    }
}

// ── Helpers do EPUB ──
QString escXml(const QString& s) {
    QString o = s;
    o.replace(QChar('&'), QStringLiteral("&amp;"));
    o.replace(QChar('<'), QStringLiteral("&lt;"));
    o.replace(QChar('>'), QStringLiteral("&gt;"));
    o.replace(QChar('"'), QStringLiteral("&quot;"));
    return o;
}

QString mimeToExt(const QString& mime) {
    if (mime == QLatin1String("image/png")) return QStringLiteral("png");
    if (mime == QLatin1String("image/gif")) return QStringLiteral("gif");
    if (mime == QLatin1String("image/webp")) return QStringLiteral("webp");
    if (mime == QLatin1String("image/svg+xml")) return QStringLiteral("svg");
    return QStringLiteral("jpg");
}

// "data:<mime>;base64,<b64>" → (mime, bytes). false se não for data-url base64.
bool parseDataUrl(const QString& url, QString& mimeOut, QByteArray& bytesOut) {
    if (!url.startsWith(QLatin1String("data:"))) return false;
    const int comma = url.indexOf(QChar(','));
    if (comma < 0) return false;
    const QString header = url.mid(5, comma - 5); // ex: "image/jpeg;base64"
    if (!header.contains(QLatin1String("base64"))) return false;
    mimeOut = header.section(QChar(';'), 0, 0);
    bytesOut = QByteArray::fromBase64(url.mid(comma + 1).toLatin1());
    return !bytesOut.isEmpty();
}
}

Exporter::Exporter(ProjectModel* model, const QString& projectRoot, const DocStyle& style)
    : m_model(model), m_root(projectRoot), m_style(style) {}

namespace {
// Corpo do texto na exportação para página (ODT/PDF/DOCX). O preview de
// e-reader não usa isto — lá vale o tamanho do editor.
constexpr double kExportBodyPt = 12.0;
// userState que marca bloco de título de capítulo, pra ele escapar do reescalo.
constexpr int kExportTitleBlock = 101;

} // namespace

void Exporter::applyParagraphStyle(QTextDocument& doc, double bodyPointSize) const {
    // O editor trabalha em tamanho de LEITURA EM TELA (16pt é comum). Numa
    // folha A4 isso rende poucas palavras por linha e faz a página parecer
    // toda margem — foi exatamente o que apareceu num manuscrito de 468
    // páginas. Pra papel, o corpo vai pra kExportBodyPt.
    const double target = bodyPointSize > 0 ? bodyPointSize : m_style.fontSize;
    if (!m_style.fontFamily.isEmpty()) {
        QFont f(m_style.fontFamily);
        f.setPointSizeF(target);
        doc.setDefaultFont(f);
    }

    if (bodyPointSize > 0 && m_style.fontSize > 0) {
        // Reescala PROPORCIONAL, não tamanho fixo: quem deixou um trecho maior
        // de propósito (epígrafe, grito) continua maior depois da conversão.
        // Título de capítulo fica de fora — ele é marcado na inserção e já
        // nasce dimensionado.
        const double factor = bodyPointSize / m_style.fontSize;
        struct Resize { int pos; int len; double pt; };
        QVector<Resize> plan;
        for (QTextBlock blk = doc.begin(); blk.isValid(); blk = blk.next()) {
            if (blk.userState() == kExportTitleBlock) continue;
            for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
                const QTextFragment frag = it.fragment();
                if (!frag.isValid() || frag.length() == 0) continue;
                double pt = frag.charFormat().fontPointSize();
                if (pt <= 0) pt = m_style.fontSize;   // herdou do documento
                plan.append({ frag.position(), frag.length(), pt * factor });
            }
        }
        // Aplica depois de mapear: mexer no documento durante a varredura
        // invalida os fragmentos que ainda não foram lidos.
        for (const Resize& r : plan) {
            QTextCursor fc(&doc);
            fc.setPosition(r.pos);
            fc.setPosition(r.pos + r.len, QTextCursor::KeepAnchor);
            QTextCharFormat cf;
            cf.setFontPointSize(r.pt);
            fc.mergeCharFormat(cf);
        }
    }

    QTextCursor c(&doc);
    c.select(QTextCursor::Document);
    QTextBlockFormat bf;
    bf.setLineHeight(m_style.lineHeightPercent, QTextBlockFormat::ProportionalHeight);
    bf.setTextIndent(m_style.firstLineIndent ? 30 : 0);
    bf.setTopMargin(m_style.spacingBefore);
    bf.setBottomMargin(m_style.spacingAfter);
    c.mergeBlockFormat(bf);
}

QString Exporter::previewCss(const QColor& fg, const QColor& bg) const {
    const QString lh = QString::number(m_style.lineHeightPercent / 100.0, 'f', 2);
    const QString indent = m_style.firstLineIndent ? QStringLiteral("1.5em") : QStringLiteral("0");
    QString bodyExtra = QStringLiteral("color: %1;").arg(fg.name());
    if (bg.isValid()) bodyExtra += QStringLiteral(" background-color: %1;").arg(bg.name());
    return QStringLiteral("body { font-family: Georgia, 'Times New Roman', serif; line-height: ")
        + lh + QStringLiteral("; margin: 0 5%; ") + bodyExtra + QStringLiteral(" }\n")
        + QStringLiteral("p { margin: 0 0 0.4em; text-indent: ") + indent + QStringLiteral("; }\n")
        + QStringLiteral("h1.chapter-title { text-align: center; font-weight: bold; margin: 1em 0 1.5em; text-indent: 0; }\n")
        + QStringLiteral("strong, b { font-weight: bold; } em, i { font-style: italic; }\n")
        + QStringLiteral("u { text-decoration: underline; } s, del { text-decoration: line-through; }\n")
        + QStringLiteral("img { max-width: 100%; height: auto; display: block; margin: 1em auto; }\n");
}

namespace {
// Exporter não é QObject, então não existe tr() aqui. Sem um helper, o caminho
// de menor esforço vira QStringLiteral — foi assim que o aviso de exportação e
// os diálogos de salvar ficaram FORA da tradução desde sempre, sem ninguém
// notar. Toda string visível ao usuário neste arquivo passa por aqui.
QString subTr(const char* text) {
    return QCoreApplication::translate("Exporter", text);
}
} // namespace

QString Exporter::safeName(const QString& s) {
    QString out = s;
    out.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    out = out.trimmed();
    return out.isEmpty() ? subTr(QT_TRANSLATE_NOOP("Exporter", "Documento")) : out;
}

QString Exporter::chapterHtmlPrimary(const Chapter& ch) const {
    if (ch.file.isEmpty()) return QString();
    bool ok = false;
    const QString full = ProjectStorage::readChapter(m_root, ch.file, &ok);
    if (!ok) return QString();

    QStringList segs = SceneUtils::splitHtmlIntoScenes(full);
    const int n = qMin(ch.scenes.size(), segs.size());
    for (int i = 0; i < n; ++i) {
        const Scene& sc = ch.scenes.at(i);
        QString primaryId;
        for (const Variation& v : sc.variations)
            if (v.isPrimary) { primaryId = v.id; break; }
        // A variação ativa já está refletida no segmento do arquivo. Só trocamos
        // quando a primária é OUTRA — aí lemos o arquivo dela.
        if (!primaryId.isEmpty() && primaryId != sc.activeVariationId) {
            const QString path =
                ProjectStorage::variationPath(m_root, ch.manuscriptId, sc.id, primaryId);
            bool vok = false;
            const QString varHtml = ProjectStorage::readText(path, &vok);
            if (vok && !varHtml.trimmed().isEmpty()) segs[i] = varHtml;
        }
    }
    return SceneUtils::joinScenesHtml(segs);
}

QString Exporter::itemHtml(const DrawerItem& it) const {
    if (it.isSheet)
        return ProjectModel::characterSheetToHtml(it.sheet, it.title, QString(), QString());
    if (it.hasInlineHtml) return it.html;
    if (!it.file.isEmpty()) {
        bool ok = false;
        const QString txt = ProjectStorage::readText(
            ProjectStorage::joinPath(m_root, it.file), &ok);
        if (ok) return txt;
    }
    return it.html; // pode estar vazio
}

QString Exporter::formatExt(Format fmt) {
    switch (fmt) {
        case Format::Pdf:  return QStringLiteral("pdf");
        case Format::Epub: return QStringLiteral("epub");
        case Format::Docx: return QStringLiteral("docx");
        default:           return QStringLiteral("odt");
    }
}

QByteArray Exporter::writeDoc(QTextDocument& doc, Format fmt, const QString& docTitle) const {
    if (fmt == Format::Docx) return docxFromDocument(doc);

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    if (fmt == Format::Pdf) {
        QPdfWriter writer(&buf);
        // 300 dpi (qualidade de impressão) em vez do default 1200 — corta o custo
        // de rasterização em ~16x, sem perda perceptível: texto é vetorial.
        writer.setResolution(300);
        writer.setPageSize(QPageSize(QPageSize::A4));
        // 1 polegada = 25,4 mm nos quatro lados, igual ao DOCX e ao preset de
        // submissão — exportar o mesmo capítulo em formatos diferentes tem
        // que dar a mesma página.
        writer.setPageMargins(QMarginsF(25.4, 25.4, 25.4, 25.4), QPageLayout::Millimeter);
        writer.setTitle(docTitle.trimmed().isEmpty()
            ? (m_model ? m_model->projectName() : QString()) : docTitle);

        // ARMADILHA DO QT: QTextDocument::print() aplica 2 cm de margem POR
        // CONTA PRÓPRIA quando o documento não tem pageSize definido — ele
        // clona o doc e sobrescreve o formato do frame raiz, e é por isso que
        // zerar documentMargin/frameFormat aqui não muda nada. Essa margem
        // SOMA com a do writer: 25,4 mm viravam 45 mm na página.
        //
        // Definir o pageSize faz o documento contar como "paginado", e aí o
        // print() respeita só a margem do writer. Medido: 45,2 mm -> 28,3 mm
        // (os 2,9 mm que sobram são o recuo de primeira linha do autor).
        // O pageSize tem que vir na régua de 96 dpi (a que o Qt assume para
        // documento), NÃO em pixels do device: passar device pixels dá uma
        // página 3x maior que a real e o texto sai microscópico.
        const QRect paint = writer.pageLayout().paintRectPixels(writer.resolution());
        const double escala = writer.resolution() / 96.0;
        doc.setPageSize(QSizeF(paint.width() / escala, paint.height() / escala));
        doc.print(&writer);
    } else {
        QTextDocumentWriter writer(&buf, "ODF");
        writer.write(&doc);
    }
    buf.close();
    return bytes;
}

QByteArray Exporter::docxFromDocument(QTextDocument& doc, const QString& runningHeader) const {
    // EMU (English Metric Units): unidade de tamanho do DrawingML. 1 px (96dpi) =
    // 9525 EMU. Twips (1/20 pt): unidade de medida do WordprocessingML; 1 px = 15.
    constexpr qint64 kEmuPerPx  = 9525;
    constexpr int    kTwipsPerPx = 15;
    // Largura útil da página A4 (11906 twips) com margem de 1 polegada dos
    // dois lados: 11906 - 2×1440 = 9026 twips → EMU. Tem que andar junto com
    // o pgMar lá embaixo: imagem calibrada pra margem antiga vaza pra fora.
    constexpr qint64 kMaxImgCx  = 9026LL * 635;

    struct Img { QString path; QByteArray bytes; QString rId; };
    QList<Img> images;
    int drawingId = 0;

    auto colorHex = [](const QColor& c) {
        return QString::asprintf("%02X%02X%02X", c.red(), c.green(), c.blue());
    };

    // <w:rPr> a partir do formato do fragmento. Foreground é ignorado: forceTextColor
    // já uniformizou pra preto (default do Word). Marca-texto vira sombreamento (w:shd).
    // A ordem dos filhos de <w:rPr> é fixada pelo schema OOXML (CT_RPr):
    // rFonts → b → i → strike → sz/szCs → u → shd. Fora de ordem, Word tolera
    // mas LibreOffice/Google Docs recusam o arquivo.
    auto runPropsXml = [&](const QTextCharFormat& f) -> QString {
        QString p;
        const QFont fnt = f.font();
        if (f.hasProperty(QTextFormat::FontFamilies)) {
            const QStringList fams = f.fontFamilies().toStringList();
            if (!fams.isEmpty())
                p += QStringLiteral("<w:rFonts w:ascii=\"%1\" w:hAnsi=\"%1\" w:cs=\"%1\"/>")
                         .arg(escXml(fams.first()));
        }
        if (fnt.bold())      p += QStringLiteral("<w:b/>");
        if (fnt.italic())    p += QStringLiteral("<w:i/>");
        if (fnt.strikeOut()) p += QStringLiteral("<w:strike/>");
        if (f.hasProperty(QTextFormat::FontPointSize) && f.fontPointSize() > 0) {
            const int halfPt = qRound(f.fontPointSize() * 2);
            p += QStringLiteral("<w:sz w:val=\"%1\"/><w:szCs w:val=\"%1\"/>").arg(halfPt);
        }
        if (fnt.underline()) p += QStringLiteral("<w:u w:val=\"single\"/>");
        if (f.background().style() != Qt::NoBrush) {
            p += QStringLiteral("<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"%1\"/>")
                     .arg(colorHex(f.background().color()));
        }
        return p.isEmpty() ? QString() : QStringLiteral("<w:rPr>") + p + QStringLiteral("</w:rPr>");
    };

    auto drawingRunXml = [&](const QString& rId, int id, qint64 cx, qint64 cy) {
        return QStringLiteral(
            "<w:r><w:drawing><wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
            "<wp:extent cx=\"%1\" cy=\"%2\"/><wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"0\"/>"
            "<wp:docPr id=\"%3\" name=\"Imagem %3\"/>"
            "<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
            "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
            "<pic:pic><pic:nvPicPr><pic:cNvPr id=\"%3\" name=\"Imagem %3\"/><pic:cNvPicPr/></pic:nvPicPr>"
            "<pic:blipFill><a:blip r:embed=\"%4\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
            "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"%1\" cy=\"%2\"/></a:xfrm>"
            "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic>"
            "</a:graphicData></a:graphic></wp:inline></w:drawing></w:r>")
            .arg(QString::number(cx), QString::number(cy), QString::number(id), rId);
    };

    // ── Corpo: um <w:p> por bloco, um <w:r> por fragmento ──
    QString body;
    for (QTextBlock blk = doc.begin(); blk.isValid(); blk = blk.next()) {
        const QTextBlockFormat bf = blk.blockFormat();

        // Ordem fixa do schema OOXML (CT_PPr): pageBreakBefore → spacing → ind → jc.
        QString pPr;
        if (bf.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore)
            pPr += QStringLiteral("<w:pageBreakBefore/>");

        QString spacing;
        if (bf.topMargin() > 0)
            spacing += QStringLiteral(" w:before=\"%1\"").arg(qRound(bf.topMargin() * kTwipsPerPx));
        if (bf.bottomMargin() > 0)
            spacing += QStringLiteral(" w:after=\"%1\"").arg(qRound(bf.bottomMargin() * kTwipsPerPx));
        if (bf.lineHeightType() == QTextBlockFormat::ProportionalHeight && bf.lineHeight() > 0)
            spacing += QStringLiteral(" w:line=\"%1\" w:lineRule=\"auto\"")
                           .arg(qRound(bf.lineHeight() * 240.0 / 100.0));
        if (!spacing.isEmpty())
            pPr += QStringLiteral("<w:spacing%1/>").arg(spacing);
        if (bf.textIndent() > 0)
            pPr += QStringLiteral("<w:ind w:firstLine=\"%1\"/>").arg(qRound(bf.textIndent() * kTwipsPerPx));

        const Qt::Alignment al = bf.alignment();
        if (al & Qt::AlignRight)        pPr += QStringLiteral("<w:jc w:val=\"right\"/>");
        else if (al & Qt::AlignHCenter) pPr += QStringLiteral("<w:jc w:val=\"center\"/>");
        else if (al & Qt::AlignJustify) pPr += QStringLiteral("<w:jc w:val=\"both\"/>");

        if (!pPr.isEmpty())
            pPr = QStringLiteral("<w:pPr>") + pPr + QStringLiteral("</w:pPr>");

        QString runs;
        for (auto it = blk.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid()) continue;
            const QTextCharFormat cf = frag.charFormat();

            if (cf.isImageFormat()) {
                const QTextImageFormat imf = cf.toImageFormat();
                QString mime;
                QByteArray bytes;
                if (!parseDataUrl(imf.name(), mime, bytes)) continue;

                // Word só lê com folga PNG/JPEG/GIF; o resto (webp/svg) re-encoda em PNG.
                QString ext = mimeToExt(mime);
                if (ext != QLatin1String("png") && ext != QLatin1String("jpg")
                    && ext != QLatin1String("gif")) {
                    const QImage im = QImage::fromData(bytes);
                    if (!im.isNull()) {
                        QByteArray out;
                        QBuffer ob(&out);
                        ob.open(QIODevice::WriteOnly);
                        if (im.save(&ob, "PNG")) { bytes = out; ext = QStringLiteral("png"); }
                    } else {
                        continue;
                    }
                }

                int wpx = qRound(imf.width());
                int hpx = qRound(imf.height());
                if (wpx <= 0 || hpx <= 0) {
                    const QImage im = QImage::fromData(bytes);
                    wpx = im.width();
                    hpx = im.height();
                }
                if (wpx <= 0) wpx = 300;
                if (hpx <= 0) hpx = 200;
                qint64 cx = qint64(wpx) * kEmuPerPx;
                qint64 cy = qint64(hpx) * kEmuPerPx;
                if (cx > kMaxImgCx) { cy = cy * kMaxImgCx / cx; cx = kMaxImgCx; }

                const QString rId = QStringLiteral("rId%1").arg(images.size() + 2);
                images.append({ QStringLiteral("media/image%1.%2").arg(images.size() + 1).arg(ext),
                                bytes, rId });
                runs += drawingRunXml(rId, ++drawingId, cx, cy);
            } else {
                const QString rp = runPropsXml(cf);
                // Quebras de linha leves (Shift+Enter) chegam como U+2028; viram <w:br/>.
                const QStringList lines = frag.text().split(QChar(0x2028));
                for (int li = 0; li < lines.size(); ++li) {
                    runs += QStringLiteral("<w:r>") + rp;
                    if (li > 0) runs += QStringLiteral("<w:br/>");
                    runs += QStringLiteral("<w:t xml:space=\"preserve\">")
                            + escXml(lines.at(li)) + QStringLiteral("</w:t></w:r>");
                }
            }
        }
        body += QStringLiteral("<w:p>") + pPr + runs + QStringLiteral("</w:p>");
    }

    // ── styles.xml: fonte/tamanho padrão do projeto como docDefaults ──
    const QString defFamily = m_style.fontFamily.isEmpty()
        ? QStringLiteral("Calibri") : m_style.fontFamily;
    const QString styles =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:docDefaults><w:rPrDefault><w:rPr>"
        "<w:rFonts w:ascii=\"") + escXml(defFamily) + QStringLiteral("\" w:hAnsi=\"")
        + escXml(defFamily) + QStringLiteral("\" w:cs=\"") + escXml(defFamily) + QStringLiteral("\"/>"
        "<w:sz w:val=\"") + QString::number(qRound(m_style.fontSize * 2)) + QStringLiteral("\"/>"
        "<w:szCs w:val=\"") + QString::number(qRound(m_style.fontSize * 2)) + QStringLiteral("\"/>"
        "</w:rPr></w:rPrDefault></w:docDefaults></w:styles>\n");

    // ── document.xml ──
    const QString document =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
        "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
        "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        "<w:body>") + body + QStringLiteral(
        "<w:sectPr>") + (runningHeader.isEmpty() ? QString() : QStringLiteral(
        // titlePg impede o Word de repetir o cabeçalho na primeira página,
        // que é a capa e já traz nome e título.
        "<w:headerReference w:type=\"default\" r:id=\"rIdHdr\"/>"))
        + QStringLiteral("<w:pgSz w:w=\"11906\" w:h=\"16838\"/>")
        // Margem de 1 polegada (1440 twips) nos quatro lados, cabeçalho a meia
        // polegada (720) do topo. Nasceu como exigência do formato de
        // submissão e virou o padrão de toda exportação: é a margem que o
        // olho espera num documento de texto, e dá espaço pra quem imprime
        // anotar na lateral.
        + QStringLiteral("<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" "
                         "w:left=\"1440\" w:header=\"720\" w:footer=\"720\" w:gutter=\"0\"/>")
        // titlePg (não repetir o cabeçalho na primeira página, que é a capa)
        // vem DEPOIS de pgMar: a ordem dos filhos de CT_SectPr é fixada pelo
        // schema, e fora de ordem o Word tolera mas LibreOffice recusa o
        // arquivo inteiro — mesma armadilha já anotada no <w:rPr> acima.
        + (runningHeader.isEmpty() ? QString() : QStringLiteral("<w:titlePg/>"))
        + QStringLiteral("</w:sectPr>"
        "</w:body></w:document>\n");

    // ── document.xml.rels: estilos (rId1) + imagens ──
    QString rels =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
        "Target=\"styles.xml\"/>");
    for (const Img& im : images)
        rels += QStringLiteral("<Relationship Id=\"%1\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
            "Target=\"%2\"/>").arg(im.rId, im.path);
    if (!runningHeader.isEmpty())
        rels += QStringLiteral("<Relationship Id=\"rIdHdr\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/header\" "
            "Target=\"header1.xml\"/>");
    rels += QStringLiteral("</Relationships>\n");

    // ── Empacota ──
    ZipWriter zip;
    zip.addFile(QStringLiteral("[Content_Types].xml"), (QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Default Extension=\"png\" ContentType=\"image/png\"/>"
        "<Default Extension=\"jpg\" ContentType=\"image/jpeg\"/>"
        "<Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>"
        "<Default Extension=\"gif\" ContentType=\"image/gif\"/>"
        "<Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/styles.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>")
        // Parte não declarada aqui faz o Word recusar o arquivo inteiro,
        // não só ignorar o cabeçalho.
        + (runningHeader.isEmpty() ? QString() : QStringLiteral(
            "<Override PartName=\"/word/header1.xml\" "
            "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml\"/>"))
        + QStringLiteral("</Types>\n")).toUtf8());
    zip.addFile(QStringLiteral("_rels/.rels"), QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"word/document.xml\"/></Relationships>\n"));
    if (!runningHeader.isEmpty()) {
        // "Sobrenome / Título / 3": o número é um campo PAGE, então o Word
        // renumera sozinho quando o texto cresce. instrText leva
        // xml:space=preserve porque os espaços em volta de PAGE fazem parte
        // do campo — sem eles o Word lê o nome do campo errado.
        const QString hdr = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:hdr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
            "<w:p><w:pPr><w:jc w:val=\"right\"/>"
            "<w:rPr><w:rFonts w:ascii=\"Courier New\" w:hAnsi=\"Courier New\"/>"
            "<w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/></w:rPr></w:pPr>"
            "<w:r><w:rPr><w:rFonts w:ascii=\"Courier New\" w:hAnsi=\"Courier New\"/>"
            "<w:sz w:val=\"24\"/><w:szCs w:val=\"24\"/></w:rPr>"
            "<w:t xml:space=\"preserve\">%1 / </w:t></w:r>"
            "<w:r><w:fldChar w:fldCharType=\"begin\"/></w:r>"
            "<w:r><w:instrText xml:space=\"preserve\"> PAGE </w:instrText></w:r>"
            "<w:r><w:fldChar w:fldCharType=\"end\"/></w:r>"
            "</w:p></w:hdr>\n").arg(runningHeader.toHtmlEscaped());
        zip.addFile(QStringLiteral("word/header1.xml"), hdr.toUtf8());
    }
    zip.addFile(QStringLiteral("word/document.xml"), document.toUtf8());
    zip.addFile(QStringLiteral("word/styles.xml"), styles.toUtf8());
    zip.addFile(QStringLiteral("word/_rels/document.xml.rels"), rels.toUtf8());
    for (const Img& im : images)
        zip.addFile(QStringLiteral("word/") + im.path, im.bytes, /*compress=*/false);
    return zip.finish();
}

QByteArray Exporter::exportItem(const QString& html, bool includeMarkers, Format fmt,
                                const QString& docTitle) const {
    QTextDocument doc;
    doc.setHtml(html.isEmpty() ? QStringLiteral("<p></p>") : html);
    applyParagraphStyle(doc, kExportBodyPt);
    forceTextColor(doc, Qt::black);
    if (!includeMarkers) stripMarkers(doc);
    return writeDoc(doc, fmt, docTitle);
}

QByteArray Exporter::exportChapters(const QList<const Chapter*>& chapters, bool includeMarkers, Format fmt,
                                    const QString& docTitle) const {
    QTextDocument doc;
    QTextCursor cur(&doc);
    bool first = true;
    for (const Chapter* ch : chapters) {
        // Bloco do título: quebra de página antes (menos no primeiro capítulo).
        QTextBlockFormat titleBlock;
        if (!first) titleBlock.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        if (first) cur.setBlockFormat(titleBlock);
        else       cur.insertBlock(titleBlock);
        first = false;

        QTextCharFormat titleChar;
        titleChar.setFontWeight(QFont::Bold);
        titleChar.setFontPointSize(16);
        const QString title = ch->title.trimmed().isEmpty()
            ? subTr(QT_TRANSLATE_NOOP("Exporter", "Capítulo")) : ch->title;
        cur.block().setUserState(kExportTitleBlock);
        cur.insertText(title, titleChar);

        // Corpo: novo bloco, formatação limpa, conteúdo do capítulo.
        QTextBlockFormat bodyBlock;
        cur.insertBlock(bodyBlock, QTextCharFormat());
        cur.insertHtml(chapterHtmlPrimary(*ch));
    }

    applyParagraphStyle(doc, kExportBodyPt);
    forceTextColor(doc, Qt::black);
    if (!includeMarkers) stripMarkers(doc);
    return writeDoc(doc, fmt, docTitle);
}

// ─────────────────────────── Formato de submissão ───────────────────────────

// Padrão Shunn ("Modern Manuscript Format"), o que editora e revista esperam
// receber: monoespaçada 12, entrelinha dupla, margens de 1 polegada, recuo de
// meia polegada, alinhado à esquerda (nunca justificado), cabeçalho corrido e
// "#" como quebra de cena. É deliberadamente feio — a função dele é ser fácil
// de marcar e de estimar, não bonito.

namespace {

// RÉGUA DO DOCUMENTO: o writer DOCX daqui assume pixels a 96 dpi
// (kTwipsPerPx = 15 → 1 px = 0,75 pt), então o documento é montado nessa mesma
// régua e o PDF escala por resolução/96. Sem isso, a mesma medida sairia com
// tamanhos diferentes em cada formato.
constexpr int kSubMarginPx    = 96;   // 1 polegada
constexpr int kSubIndentPx    = 48;   // recuo de primeira linha: 0,5 polegada
constexpr int kSubHeaderTopPx = 48;   // cabeçalho corrido a 0,5 pol do topo
constexpr int kSubTitleDropPx = 288;  // título a ~3 pol do topo da capa
constexpr double kSubFontPt   = 12.0;
// A4 em px a 96 dpi (210 × 297 mm). O app usa A4 no resto das exportações.
constexpr int kSubPageWPx = 794;
constexpr int kSubPageHPx = 1123;

// Estado de bloco, pra reconhecer o que é o quê depois de o HTML já estar no
// documento: o corpo vem de insertHtml e não dá pra marcar na inserção.
enum SubBlockKind { SubBody = 0, SubTitlePage = 1, SubChapterTitle = 2, SubSceneBreak = 3 };

// Sobrenome pro cabeçalho corrido. Shunn pede o sobrenome do autor; usa o nome
// de publicação quando existe, porque é ele que o editor vê na capa.
QString surnameFor(const Exporter::SubmissionInfo& info)
{
    const QString src = info.byline.trimmed().isEmpty() ? info.legalName.trimmed()
                                                        : info.byline.trimmed();
    const QStringList parts = src.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return parts.isEmpty() ? QString() : parts.last();
}

} // namespace

Exporter::SubmissionInfo Exporter::SubmissionInfo::load()
{
    QSettings s;
    SubmissionInfo info;
    info.legalName  = s.value(QStringLiteral("submission/legalName")).toString();
    info.address    = s.value(QStringLiteral("submission/address")).toString();
    info.contact    = s.value(QStringLiteral("submission/contact")).toString();
    info.byline     = s.value(QStringLiteral("submission/byline")).toString();
    info.titleShort = s.value(QStringLiteral("submission/titleShort")).toString();
    return info;
}

void Exporter::SubmissionInfo::save() const
{
    QSettings s;
    s.setValue(QStringLiteral("submission/legalName"), legalName);
    s.setValue(QStringLiteral("submission/address"), address);
    s.setValue(QStringLiteral("submission/contact"), contact);
    s.setValue(QStringLiteral("submission/byline"), byline);
    s.setValue(QStringLiteral("submission/titleShort"), titleShort);
}

void Exporter::insertSubmissionTitlePage(QTextCursor& cur, const QString& manuscriptTitle,
                                        const SubmissionInfo& info, int wordCount) const
{
    QTextCharFormat plain;
    plain.setFontFamilies({ QStringLiteral("Courier New") });
    plain.setFontPointSize(kSubFontPt);

    // Entrelinha simples no bloco de contato — só o CORPO do manuscrito é que
    // vai em entrelinha dupla.
    QTextBlockFormat single;
    single.setLineHeight(100, QTextBlockFormat::ProportionalHeight);
    single.setTextIndent(0);
    single.setTopMargin(0);
    single.setBottomMargin(0);

    auto line = [&](const QString& text, Qt::Alignment align, bool firstBlock) {
        QTextBlockFormat bf = single;
        bf.setAlignment(align);
        if (firstBlock) cur.setBlockFormat(bf);
        else            cur.insertBlock(bf, plain);
        cur.block().setUserState(SubTitlePage);
        if (!text.isEmpty()) cur.insertText(text, plain);
    };

    // A contagem vai numa linha própria à direita, em vez de lado a lado com o
    // nome como no layout clássico: alinhar dois blocos na mesma linha exigiria
    // tabela ou tab stop, e nenhum dos dois sobrevive igual nos três formatos
    // que exportamos. A informação que o editor procura continua no lugar que
    // ele olha — topo da primeira página.
    bool first = true;
    if (wordCount > 0) {
        line(subTr(QT_TRANSLATE_NOOP("Exporter", "Aproximadamente %1 palavras")).arg(QLocale().toString(wordCount)),
             Qt::AlignRight, first);
        first = false;
    }

    QStringList contactLines;
    if (!info.legalName.trimmed().isEmpty()) contactLines << info.legalName.trimmed();
    const QStringList addr = info.address.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& a : addr) contactLines << a.trimmed();
    const QStringList contact = info.contact.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& c : contact) contactLines << c.trimmed();

    for (const QString& l : contactLines) { line(l, Qt::AlignLeft, first); first = false; }
    if (first) line(QString(), Qt::AlignLeft, true); // capa sem dado nenhum

    // Título e byline no meio da página, empurrados por margem (não por linhas
    // vazias — linha vazia depende da entrelinha e some no DOCX).
    QTextBlockFormat titleBf = single;
    titleBf.setAlignment(Qt::AlignHCenter);
    titleBf.setTopMargin(kSubTitleDropPx);
    cur.insertBlock(titleBf, plain);
    cur.block().setUserState(SubTitlePage);
    cur.insertText(manuscriptTitle.toUpper(), plain);

    QTextBlockFormat bylineBf = single;
    bylineBf.setAlignment(Qt::AlignHCenter);
    bylineBf.setTopMargin(24);
    const QString byline = info.byline.trimmed().isEmpty() ? info.legalName.trimmed()
                                                           : info.byline.trimmed();
    if (!byline.isEmpty()) {
        cur.insertBlock(bylineBf, plain);
        cur.block().setUserState(SubTitlePage);
        cur.insertText(subTr(QT_TRANSLATE_NOOP("Exporter", "por %1")).arg(byline), plain);
    }
}

void Exporter::buildSubmissionBody(QTextDocument& doc, const QList<const Chapter*>& chapters,
                                   bool includeMarkers) const
{
    QTextCursor cur(&doc);
    cur.movePosition(QTextCursor::End);

    QTextCharFormat plain;
    plain.setFontFamilies({ QStringLiteral("Courier New") });
    plain.setFontPointSize(kSubFontPt);

    for (const Chapter* ch : chapters) {
        // Todo capítulo abre em página nova — inclusive o primeiro, que vem
        // depois da capa.
        QTextBlockFormat titleBf;
        titleBf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        titleBf.setAlignment(Qt::AlignHCenter);
        titleBf.setTextIndent(0);
        titleBf.setTopMargin(0);
        titleBf.setBottomMargin(0);
        cur.insertBlock(titleBf, plain);
        cur.block().setUserState(SubChapterTitle);

        const QString title = ch->title.trimmed().isEmpty()
            ? subTr(QT_TRANSLATE_NOOP("Exporter", "Capítulo")) : ch->title.trimmed();
        // Sem negrito: manuscrito de submissão não usa peso de fonte pra
        // hierarquia, o editor marca isso na diagramação.
        cur.insertText(title.toUpper(), plain);

        QTextBlockFormat bodyBf;
        bodyBf.setTextIndent(kSubIndentPx);
        cur.insertBlock(bodyBf, plain);
        cur.insertHtml(chapterHtmlPrimary(*ch));
    }

    // Uniformiza o corpo inteiro DEPOIS da inserção: o HTML do capítulo traz
    // formatação do editor (fonte serif, justificado, espaçamento) que não tem
    // lugar num manuscrito de submissão.
    for (QTextBlock blk = doc.begin(); blk.isValid(); blk = blk.next()) {
        if (blk.userState() == SubTitlePage) continue;   // capa tem layout próprio

        // Normaliza SÓ família e tamanho. Itálico, negrito e sublinhado do
        // autor são informação (ênfase, título de obra citada) e passam
        // intactos — o formato dita a régua da página, não o que o texto diz.
        QTextCursor bc(blk);
        bc.select(QTextCursor::BlockUnderCursor);
        QTextCharFormat cf;
        cf.setFontFamilies({ QStringLiteral("Courier New") });
        cf.setFontPointSize(kSubFontPt);
        bc.mergeCharFormat(cf);

        QTextBlockFormat bf = blk.blockFormat();
        bf.setLineHeight(200, QTextBlockFormat::ProportionalHeight);  // entrelinha dupla
        bf.setTopMargin(0);
        bf.setBottomMargin(0);
        if (blk.userState() == SubChapterTitle) {
            bf.setAlignment(Qt::AlignHCenter);
            bf.setTextIndent(0);
        } else {
            bf.setAlignment(Qt::AlignLeft);      // nunca justificado
            bf.setTextIndent(kSubIndentPx);
        }
        // Régua de cena: o manuscrito guarda quebra de cena como <hr>, que no
        // formato de submissão é um "#" centralizado — linha em branco sozinha
        // se perde na virada de página e o editor não a vê.
        if (bf.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            bf.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
            bf.setAlignment(Qt::AlignHCenter);
            bf.setTextIndent(0);
            QTextCursor hc(blk);
            hc.select(QTextCursor::BlockUnderCursor);
            hc.removeSelectedText();
            QTextCursor ins(blk);
            ins.insertText(QStringLiteral("#"), plain);
        }
        QTextCursor bfc(blk);
        bfc.setBlockFormat(bf);
    }

    forceTextColor(doc, Qt::black);
    if (!includeMarkers) stripMarkers(doc);
}

QByteArray Exporter::submissionPdf(QTextDocument& doc, const QString& runningHeader,
                                  const QString& docTitle) const
{
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    {
        QPdfWriter writer(&buf);
        writer.setResolution(300);
        writer.setPageSize(QPageSize(QPageSize::A4));
        // Margem zero no writer: a margem de 1 polegada é desenhada por nós,
        // porque o cabeçalho corrido mora DENTRO dela.
        writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
        writer.setTitle(docTitle.trimmed().isEmpty()
            ? (m_model ? m_model->projectName() : QString()) : docTitle);

        const int contentW = kSubPageWPx - 2 * kSubMarginPx;
        const int contentH = kSubPageHPx - 2 * kSubMarginPx;
        doc.setPageSize(QSizeF(contentW, contentH));

        QPainter painter(&writer);
        // QTextDocument::print() não desenha cabeçalho com número de página,
        // então a paginação é feita aqui: escala px(96dpi) → device e desenha
        // uma página por vez.
        const double scale = writer.resolution() / 96.0;
        painter.scale(scale, scale);

        QFont headerFont(QStringLiteral("Courier New"));
        headerFont.setPointSizeF(kSubFontPt);

        const int pages = doc.pageCount();
        for (int i = 0; i < pages; ++i) {
            if (i > 0) writer.newPage();

            // Cabeçalho corrido a partir da segunda página — a primeira é a
            // capa, que já traz nome e título.
            if (i > 0 && !runningHeader.isEmpty()) {
                painter.save();
                painter.setFont(headerFont);
                painter.setPen(Qt::black);
                const QRect hr(kSubMarginPx, kSubHeaderTopPx, contentW, 24);
                painter.drawText(hr, Qt::AlignRight | Qt::AlignVCenter,
                                 QStringLiteral("%1 / %2").arg(runningHeader).arg(i + 1));
                painter.restore();
            }

            painter.save();
            painter.translate(kSubMarginPx, kSubMarginPx);
            // Recorta na altura da página pra linha partida não vazar na
            // margem de baixo, e translada pra fatia certa do documento.
            painter.setClipRect(QRectF(0, 0, contentW, contentH));
            painter.translate(0, -double(i) * contentH);
            doc.drawContents(&painter,
                             QRectF(0, double(i) * contentH, contentW, contentH));
            painter.restore();
        }
    }
    buf.close();
    return bytes;
}

QByteArray Exporter::exportSubmission(const QList<const Chapter*>& chapters,
                                      const QString& manuscriptTitle,
                                      const SubmissionInfo& info,
                                      bool includeMarkers, Format fmt) const
{
    // Contagem ARREDONDADA, como o formato pede: o editor quer ordem de
    // grandeza pra estimar páginas, não o número exato — e número exato
    // envelhece a cada save.
    int words = 0;
    for (const Chapter* ch : chapters)
        words += WordCounter::countWordsInHtml(chapterHtmlPrimary(*ch));
    const int rounded = words >= 10000 ? (words + 500) / 1000 * 1000
                      : words >= 1000  ? (words + 50) / 100 * 100
                                       : words;

    QTextDocument doc;
    QFont base(QStringLiteral("Courier New"));
    base.setPointSizeF(kSubFontPt);
    doc.setDefaultFont(base);
    doc.setDocumentMargin(0);   // a margem da página é nossa, não do documento

    QTextCursor cur(&doc);
    insertSubmissionTitlePage(cur, manuscriptTitle, info, rounded);
    buildSubmissionBody(doc, chapters, includeMarkers);

    const QString shortTitle = info.titleShort.trimmed().isEmpty()
        ? manuscriptTitle.trimmed() : info.titleShort.trimmed();
    const QString surname = surnameFor(info);
    QString header = surname.isEmpty() ? shortTitle
                   : shortTitle.isEmpty() ? surname
                   : QStringLiteral("%1 / %2").arg(surname, shortTitle);

    if (fmt == Format::Pdf)  return submissionPdf(doc, header, manuscriptTitle);
    if (fmt == Format::Docx) return docxFromDocument(doc, header);
    // ODT: o writer é o do Qt e não aceita cabeçalho corrido injetado. Todo o
    // resto do formato vale; o painel avisa que a numeração de página fica de
    // fora nesse caso.
    return writeDoc(doc, fmt, manuscriptTitle);
}

QList<Exporter::OutFile> Exporter::buildFiles(const Selection& sel) const {
    QList<OutFile> files;
    if (!m_model) return files;

    // EPUB: um único arquivo com tudo dentro (ignora documento único/separado).
    // Nome do arquivo usa o manuscrito quando a seleção é de 1 só; senão o
    // projeto (omnibus com 2+ manuscritos, ou sem manuscrito nenhum).
    if (sel.format == Format::Epub) {
        const QByteArray epub = buildEpub(sel);
        if (!epub.isEmpty()) {
            const Manuscript* soloMs = singleManuscriptInSelection(sel);
            const QString baseName = safeName(soloMs
                ? m_model->manuscriptEffectiveTitle(soloMs->id) : m_model->projectName());
            files.append({ baseName + QStringLiteral(".epub"), epub });
        }
        return files;
    }

    // ── Manuscritos ──
    for (const Manuscript& ms : m_model->manuscripts()) {
        QList<const Chapter*> selected;
        for (const Chapter* ch : m_model->orderedChaptersForManuscript(ms.id)) {
            if (sel.chapterIds.contains(ch->id)) selected.append(ch);
        }
        if (selected.isEmpty()) continue;

        const QString effectiveTitle = m_model->manuscriptEffectiveTitle(ms.id);
        const QString msTitle = safeName(effectiveTitle.isEmpty() ? subTr(QT_TRANSLATE_NOOP("Exporter", "Manuscrito")) : effectiveTitle);
        const QString ext = formatExt(sel.format);

        if (sel.manuscriptMode == ManuscriptMode::SingleDocument) {
            const QByteArray bytes = submissionApplies(sel)
                ? exportSubmission(selected, effectiveTitle, sel.submission,
                                   sel.includeMarkers, sel.format)
                : exportChapters(selected, sel.includeMarkers, sel.format, effectiveTitle);
            files.append({ QStringLiteral("Manuscritos/%1.%2").arg(msTitle, ext), bytes });
        } else {
            for (int i = 0; i < selected.size(); ++i) {
                const Chapter* ch = selected.at(i);
                const QString chTitle = safeName(ch->title.isEmpty()
                    ? m_model->chapterDisplayLabel(*ch) : ch->title);
                const QString path = QStringLiteral("Manuscritos/%1/%2 - %3.%4")
                    .arg(msTitle, QString::number(i + 1).rightJustified(2, QLatin1Char('0')), chTitle, ext);
                files.append({ path, exportItem(chapterHtmlPrimary(*ch), sel.includeMarkers, sel.format) });
            }
        }
    }

    // ── Gavetas (sempre arquivos separados, preservando pastas) ──
    for (const Drawer& d : m_model->drawers()) {
        const QString drawerTitle = safeName(d.title);

        std::function<void(const QString&, const QString&)> walk =
            [&](const QString& folderId, const QString& prefix) {
                for (const DrawerItem& it : d.items) {
                    if ((it.folderId.isEmpty() ? QString() : it.folderId) != folderId) continue;
                    if (!sel.itemIds.contains(it.id)) continue;
                    const QString itTitle = safeName(it.title);
                    files.append({ QStringLiteral("%1%2.%3").arg(prefix, itTitle, formatExt(sel.format)),
                                   exportItem(itemHtml(it), sel.includeMarkers, sel.format) });
                }
                for (const Folder& f : d.folders) {
                    if ((f.parentId.isEmpty() ? QString() : f.parentId) != folderId) continue;
                    walk(f.id, prefix + safeName(f.title) + QStringLiteral("/"));
                }
            };
        walk(QString(), drawerTitle + QStringLiteral("/"));
    }

    return files;
}

QString Exporter::itemBodyXhtml(const QString& rawHtml, bool includeMarkers,
                                QList<QPair<QString, QByteArray>>& imagesOut,
                                QStringList& imageMimesOut, int& imgCounter) const {
    QTextDocument doc;
    doc.setHtml(rawHtml.isEmpty() ? QStringLiteral("<p></p>") : rawHtml);
    forceTextColor(doc, Qt::black);
    if (!includeMarkers) stripMarkers(doc);
    QString html = doc.toHtml();

    // Extrai só o conteúdo de dentro do <body>.
    QString body;
    const int bs = html.indexOf(QLatin1String("<body"), 0, Qt::CaseInsensitive);
    const int be = html.lastIndexOf(QLatin1String("</body>"), -1, Qt::CaseInsensitive);
    if (bs >= 0 && be > bs) {
        const int gt = html.indexOf(QChar('>'), bs);
        body = html.mid(gt + 1, be - gt - 1);
    } else {
        body = html;
    }

    // Extrai imagens embutidas (data: URL) para arquivos e reescreve o src.
    static const QRegularExpression imgRe(
        QStringLiteral("<img[^>]*\\bsrc=\"(data:[^\"]+)\"[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    QString rebuilt;
    int last = 0;
    auto it = imgRe.globalMatch(body);
    while (it.hasNext()) {
        const auto m = it.next();
        rebuilt += body.mid(last, m.capturedStart() - last);
        const QString dataUrl = m.captured(1);
        QString mime;
        QByteArray bytes;
        if (parseDataUrl(dataUrl, mime, bytes)) {
            const QString fn = QStringLiteral("images/img%1.%2")
                .arg(++imgCounter).arg(mimeToExt(mime));
            imagesOut.append({ fn, bytes });
            imageMimesOut.append(mime);
            QString tag = m.captured(0);
            tag.replace(dataUrl, fn);
            if (!tag.endsWith(QLatin1String("/>")))
                tag.chop(1), tag += QStringLiteral("/>");
            rebuilt += tag;
        } else {
            rebuilt += m.captured(0);
        }
        last = m.capturedEnd();
    }
    rebuilt += body.mid(last);
    body = rebuilt;

    // Normalizações para XHTML bem-formado.
    body.replace(QLatin1String("&nbsp;"), QLatin1String("&#160;"));
    body.replace(QRegularExpression(QStringLiteral("<br>"), QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("<br/>"));
    body.replace(QRegularExpression(QStringLiteral("<hr>"), QRegularExpression::CaseInsensitiveOption),
                 QStringLiteral("<hr/>"));
    return body;
}

const Manuscript* Exporter::singleManuscriptInSelection(const Selection& sel) const {
    if (!m_model) return nullptr;
    const Manuscript* found = nullptr;
    for (const Manuscript& ms : m_model->manuscripts()) {
        bool hasSelected = false;
        for (const Chapter* ch : m_model->orderedChaptersForManuscript(ms.id)) {
            if (sel.chapterIds.contains(ch->id)) { hasSelected = true; break; }
        }
        if (!hasSelected) continue;
        if (found) return nullptr; // 2º manuscrito com capítulos selecionados → ambíguo
        found = &ms;
    }
    return found;
}

QByteArray Exporter::buildEpub(const Selection& sel) const {
    struct Item { QString id; QString title; QString filename; QString body; };
    QList<Item> items;
    QList<QPair<QString, QByteArray>> images; // path relativo (OEBPS/...) → bytes
    QStringList imageMimes;
    int imgCounter = 0;
    int counter = 0;

    // Capítulos por manuscrito, em ordem.
    for (const Manuscript& ms : m_model->manuscripts()) {
        QList<const Chapter*> chaps;
        for (const Chapter* ch : m_model->orderedChaptersForManuscript(ms.id)) {
            if (sel.chapterIds.contains(ch->id)) chaps.append(ch);
        }
        for (const Chapter* ch : chaps) {
            ++counter;
            Item it;
            it.id = QStringLiteral("ch_%1").arg(counter);
            it.title = ch->title.trimmed().isEmpty() ? subTr(QT_TRANSLATE_NOOP("Exporter", "Capítulo")) : ch->title;
            it.filename = it.id + QStringLiteral(".xhtml");
            it.body = itemBodyXhtml(chapterHtmlPrimary(*ch), sel.includeMarkers,
                                    images, imageMimes, imgCounter);
            items.append(it);
        }
    }

    // Documentos de gaveta selecionados (recursivo nas pastas).
    for (const Drawer& d : m_model->drawers()) {
        std::function<void(const QString&)> walk = [&](const QString& folderId) {
            for (const DrawerItem& di : d.items) {
                if ((di.folderId.isEmpty() ? QString() : di.folderId) != folderId) continue;
                if (!sel.itemIds.contains(di.id)) continue;
                ++counter;
                Item it;
                it.id = QStringLiteral("doc_%1").arg(counter);
                it.title = di.title.trimmed().isEmpty() ? subTr(QT_TRANSLATE_NOOP("Exporter", "Documento")) : di.title;
                it.filename = it.id + QStringLiteral(".xhtml");
                it.body = itemBodyXhtml(itemHtml(di), sel.includeMarkers,
                                        images, imageMimes, imgCounter);
                items.append(it);
            }
            for (const Folder& f : d.folders)
                if ((f.parentId.isEmpty() ? QString() : f.parentId) == folderId) walk(f.id);
        };
        walk(QString());
    }

    if (items.isEmpty()) return {};

    // ── Metadados ──
    // Título/sinopse/capa do manuscrito quando a seleção é de 1 só; senão
    // (2+ manuscritos combinados no mesmo epub, ou nenhum) usa o projeto —
    // é a identidade da "saga", correta pra um omnibus.
    const Manuscript* soloMs = singleManuscriptInSelection(sel);
    const QString rawTitle = soloMs ? m_model->manuscriptEffectiveTitle(soloMs->id) : m_model->projectName();
    const QString title = rawTitle.trimmed().isEmpty() ? subTr(QT_TRANSLATE_NOOP("Exporter", "Projeto")) : rawTitle;
    const QString author = m_model->projectAuthor();
    const QString synopsis = soloMs ? m_model->manuscriptEffectiveSynopsis(soloMs->id) : m_model->projectSynopsis();
    const QString genres = m_model->projectGenres();
    const QString bookId = QStringLiteral("urn:uuid:")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString now = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddThh:mm:ss"))
        + QStringLiteral("Z");

    // ── Capa ──
    const QString coverSrc = soloMs ? m_model->manuscriptEffectiveCoverDataUrl(soloMs->id) : m_model->projectCoverDataUrl();
    bool hasCover = false;
    QString coverFilename, coverMime;
    QByteArray coverBytes;
    if (parseDataUrl(coverSrc, coverMime, coverBytes)) {
        // Capa de e-book tem teto útil ~1600px no lado maior (recomendação
        // Amazon/Kobo). Reduz só pra baixo + re-encoda em JPEG q90 (ou mantém PNG
        // se tiver transparência) — derruba o tamanho sem perda visível em tela.
        QImage img = QImage::fromData(coverBytes);
        if (!img.isNull()) {
            if (img.height() > 1600)
                img = img.scaledToHeight(1600, Qt::SmoothTransformation);
            // Capa não precisa de transparência: achata sobre fundo branco (se
            // houver alpha) e grava JPEG q90 — bem mais leve que PNG, sem perda
            // visível. O achatamento evita lixo em pixels semitransparentes.
            if (img.hasAlphaChannel()) {
                QImage flat(img.size(), QImage::Format_RGB32);
                flat.fill(Qt::white);
                QPainter p(&flat);
                p.drawImage(0, 0, img);
                p.end();
                img = flat;
            }
            QByteArray out;
            QBuffer ob(&out);
            ob.open(QIODevice::WriteOnly);
            if (img.save(&ob, "JPEG", 90)) {
                ob.close();
                if (!out.isEmpty() && out.size() < coverBytes.size()) {
                    coverBytes = out;
                    coverMime = QStringLiteral("image/jpeg");
                }
            }
        }
        coverFilename = QStringLiteral("images/cover.") + mimeToExt(coverMime);
        hasCover = true;
    }

    // ── CSS ──
    const QString css = previewCss(QColor(0x1a, 0x1a, 0x1a), QColor());

    // ── Monta os XMLs ──
    QString manifest, spine, navList, ncxNav;
    for (int i = 0; i < items.size(); ++i) {
        const Item& it = items.at(i);
        manifest += QStringLiteral("    <item id=\"%1\" href=\"%2\" media-type=\"application/xhtml+xml\"/>\n")
            .arg(it.id, it.filename);
        spine += QStringLiteral("    <itemref idref=\"%1\"/>\n").arg(it.id);
        navList += QStringLiteral("      <li><a href=\"%1\">%2</a></li>\n").arg(it.filename, escXml(it.title));
        ncxNav += QStringLiteral("    <navPoint id=\"%1\" playOrder=\"%2\"><navLabel><text>%3</text></navLabel><content src=\"%4\"/></navPoint>\n")
            .arg(it.id, QString::number(i + 1), escXml(it.title), it.filename);
    }
    for (int i = 0; i < images.size(); ++i) {
        manifest += QStringLiteral("    <item id=\"bimg_%1\" href=\"%2\" media-type=\"%3\"/>\n")
            .arg(QString::number(i + 1), images.at(i).first, imageMimes.at(i));
    }
    if (hasCover) {
        manifest += QStringLiteral("    <item id=\"cover-image\" href=\"%1\" media-type=\"%2\" properties=\"cover-image\"/>\n")
            .arg(coverFilename, coverMime);
        manifest += QStringLiteral("    <item id=\"cover-page\" href=\"cover.xhtml\" media-type=\"application/xhtml+xml\"/>\n");
    }

    QString metaExtra;
    if (hasCover) metaExtra += QStringLiteral("    <meta name=\"cover\" content=\"cover-image\"/>\n");
    if (!author.trimmed().isEmpty())
        metaExtra += QStringLiteral("    <dc:creator>%1</dc:creator>\n").arg(escXml(author));
    if (!synopsis.trimmed().isEmpty())
        metaExtra += QStringLiteral("    <dc:description>%1</dc:description>\n").arg(escXml(synopsis));
    for (const QString& g : genres.split(QChar(','), Qt::SkipEmptyParts))
        metaExtra += QStringLiteral("    <dc:subject>%1</dc:subject>\n").arg(escXml(g.trimmed()));

    const QString opf =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<package version=\"3.0\" xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"book-id\" xml:lang=\"pt-BR\">\n"
        "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
        "    <dc:identifier id=\"book-id\">") + bookId + QStringLiteral("</dc:identifier>\n"
        "    <dc:title>") + escXml(title) + QStringLiteral("</dc:title>\n"
        "    <dc:language>pt-BR</dc:language>\n"
        "    <meta property=\"dcterms:modified\">") + now + QStringLiteral("</meta>\n")
        + metaExtra + QStringLiteral("  </metadata>\n"
        "  <manifest>\n"
        "    <item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n"
        "    <item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n"
        "    <item id=\"style\" href=\"style.css\" media-type=\"text/css\"/>\n")
        + manifest + QStringLiteral("  </manifest>\n"
        "  <spine toc=\"ncx\">\n")
        + (hasCover ? QStringLiteral("    <itemref idref=\"cover-page\" linear=\"no\"/>\n") : QString())
        + spine + QStringLiteral("  </spine>\n"
        "</package>\n");

    const QString nav =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\" xml:lang=\"pt-BR\" lang=\"pt-BR\">\n"
        "<head><meta charset=\"UTF-8\"/><title>Índice</title></head>\n"
        "<body>\n  <nav epub:type=\"toc\" id=\"toc\">\n    <h1>Índice</h1>\n    <ol>\n")
        + navList + QStringLiteral("    </ol>\n  </nav>\n</body>\n</html>\n");

    const QString ncx =
        QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">\n"
        "  <head>\n    <meta name=\"dtb:uid\" content=\"") + bookId + QStringLiteral("\"/>\n"
        "    <meta name=\"dtb:depth\" content=\"1\"/>\n"
        "    <meta name=\"dtb:totalPageCount\" content=\"0\"/>\n"
        "    <meta name=\"dtb:maxPageNumber\" content=\"0\"/>\n  </head>\n"
        "  <docTitle><text>") + escXml(title) + QStringLiteral("</text></docTitle>\n  <navMap>\n")
        + ncxNav + QStringLiteral("  </navMap>\n</ncx>\n");

    auto pageXhtml = [](const QString& t, const QString& body) {
        return QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE html>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"pt-BR\" lang=\"pt-BR\">\n"
            "<head><meta charset=\"UTF-8\"/><title>") + escXml(t)
            + QStringLiteral("</title><link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"/></head>\n"
            "<body>\n<h1 class=\"chapter-title\">") + escXml(t) + QStringLiteral("</h1>\n")
            + body + QStringLiteral("\n</body>\n</html>\n");
    };

    // ── Empacota (mimetype PRIMEIRO e sem compressão — ZipWriter é stored) ──
    ZipWriter zip;
    zip.addFile(QStringLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"),
                /*compress=*/false);
    zip.addFile(QStringLiteral("META-INF/container.xml"), QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
        "  <rootfiles>\n"
        "    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
        "  </rootfiles>\n"
        "</container>\n"));
    zip.addFile(QStringLiteral("OEBPS/style.css"), css.toUtf8());
    for (const Item& it : items)
        zip.addFile(QStringLiteral("OEBPS/") + it.filename, pageXhtml(it.title, it.body).toUtf8());
    for (const auto& img : images)
        zip.addFile(QStringLiteral("OEBPS/") + img.first, img.second, /*compress=*/false);
    if (hasCover) {
        zip.addFile(QStringLiteral("OEBPS/") + coverFilename, coverBytes, /*compress=*/false);
        const QString coverPage =
            QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE html>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"pt-BR\" lang=\"pt-BR\">\n"
            "<head><meta charset=\"UTF-8\"/><title>Capa</title>\n"
            "<style>html,body{margin:0;padding:0;text-align:center;}img{max-width:100%;max-height:100vh;height:auto;}</style>\n"
            "</head>\n<body><img src=\"") + coverFilename + QStringLiteral("\" alt=\"Capa\"/></body>\n</html>\n");
        zip.addFile(QStringLiteral("OEBPS/cover.xhtml"), coverPage.toUtf8());
    }
    zip.addFile(QStringLiteral("OEBPS/content.opf"), opf.toUtf8());
    zip.addFile(QStringLiteral("OEBPS/nav.xhtml"), nav.toUtf8());
    zip.addFile(QStringLiteral("OEBPS/toc.ncx"), ncx.toUtf8());
    return zip.finish();
}

bool Exporter::run(const Selection& sel, QWidget* dialogParent,
                   QString* error, bool* nothingExported) {
    if (nothingExported) *nothingExported = false;

    // A geração (sobretudo PDF) bloqueia a thread de UI. Mostra um aviso modal
    // pra janela não parecer travada e tranquilizar o usuário durante a espera.
    QList<OutFile> files;
    {
        QProgressDialog progress(
            subTr(QT_TRANSLATE_NOOP("Exporter",
                  "Exportando… Esse processo pode levar alguns instantes.\n"
                  "Não encerre o programa caso ele pare de responder.")),
            QString(), 0, 0, dialogParent);
        progress.setWindowTitle(subTr(QT_TRANSLATE_NOOP("Exporter", "Exportando")));
        progress.setWindowModality(Qt::ApplicationModal);
        progress.setCancelButton(nullptr);
        progress.setMinimumDuration(0);
        progress.setAutoClose(false);
        progress.show();
        QApplication::processEvents();
        files = buildFiles(sel);
    }

    if (files.isEmpty()) {
        if (nothingExported) *nothingExported = true;
        return false;
    }

    const QString projName = safeName(m_model ? m_model->projectName() : QString());

    // Nome sugerido: quando o único arquivo já corresponde a um manuscrito
    // específico (documento único de manuscrito, ou EPUB de 1 manuscrito só),
    // sugere o nome dele em vez do projeto. Conservador de propósito — não
    // mexe no nome sugerido pra itens avulsos de gaveta.
    QString suggestedBaseName = projName;
    if (files.size() == 1) {
        if (sel.format == Format::Epub) {
            if (const Manuscript* soloMs = singleManuscriptInSelection(sel))
                suggestedBaseName = safeName(m_model->manuscriptEffectiveTitle(soloMs->id));
        } else if (files.first().path.startsWith(QStringLiteral("Manuscritos/"))) {
            suggestedBaseName = QFileInfo(files.first().path).completeBaseName();
        }
    }

    if (files.size() == 1) {
        // Único arquivo → salva direto no formato escolhido.
        const QString ext = formatExt(sel.format);
        QString filter, dlgTitle;
        switch (sel.format) {
            case Format::Pdf:
                filter = subTr(QT_TRANSLATE_NOOP("Exporter", "Documento PDF (*.pdf)"));
                dlgTitle = subTr(QT_TRANSLATE_NOOP("Exporter", "Exportar como PDF")); break;
            case Format::Epub:
                filter = subTr(QT_TRANSLATE_NOOP("Exporter", "Livro EPUB (*.epub)"));
                dlgTitle = subTr(QT_TRANSLATE_NOOP("Exporter", "Exportar como EPUB")); break;
            case Format::Docx:
                filter = subTr(QT_TRANSLATE_NOOP("Exporter", "Documento Word (*.docx)"));
                dlgTitle = subTr(QT_TRANSLATE_NOOP("Exporter", "Exportar como DOCX")); break;
            default:
                filter = subTr(QT_TRANSLATE_NOOP("Exporter", "Documento ODF (*.odt)"));
                dlgTitle = subTr(QT_TRANSLATE_NOOP("Exporter", "Exportar como ODT")); break;
        }
        const QString suggested = suggestedBaseName + QStringLiteral(".") + ext;
        const QString dest = QFileDialog::getSaveFileName(
            dialogParent, dlgTitle, suggested, filter);
        if (dest.isEmpty()) return false; // cancelado
        QFile f(dest);
        if (!f.open(QIODevice::WriteOnly)) {
            if (error) *error = subTr(QT_TRANSLATE_NOOP("Exporter", "Não foi possível gravar o arquivo."));
            return false;
        }
        f.write(files.first().bytes);
        f.close();
        return true;
    }

    // Vários arquivos → empacota num .zip.
    ZipWriter zip;
    for (const OutFile& of : files) zip.addFile(of.path, of.bytes);
    const QByteArray zipBytes = zip.finish();

    const QString suggested = projName + QStringLiteral(".zip");
    const QString dest = QFileDialog::getSaveFileName(
        dialogParent, subTr(QT_TRANSLATE_NOOP("Exporter", "Exportar projeto (.zip)")),
        suggested, subTr(QT_TRANSLATE_NOOP("Exporter", "Arquivo ZIP (*.zip)")));
    if (dest.isEmpty()) return false;
    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = subTr(QT_TRANSLATE_NOOP("Exporter", "Não foi possível gravar o arquivo."));
        return false;
    }
    f.write(zipBytes);
    f.close();
    return true;
}

QTextDocument* Exporter::buildPreviewDocument(const QString& manuscriptId,
                                               bool includeMarkers,
                                               const QColor& textColor,
                                               const QColor& backgroundColor,
                                               bool grayscale,
                                               QObject* docParent) const {
    if (!m_model || manuscriptId.isEmpty()) return nullptr;
    const QList<const Chapter*> chapters = m_model->orderedChaptersForManuscript(manuscriptId);
    if (chapters.isEmpty()) return nullptr;

    auto* doc = new QTextDocument(docParent);
    // Precisa ser setado ANTES de inserir conteúdo: insertHtml() só honra o
    // stylesheet vigente no momento em que cada fragmento é parseado (h1,
    // img, strong/em/u/s). Setar depois do loop de capítulos (como estava)
    // é tarde demais — nenhuma regra chega a valer.
    doc->setDefaultStyleSheet(previewCss(textColor, backgroundColor));
    QTextCursor cur(doc);
    bool wroteAnything = false;

    // Capa, se houver — vira "página 0" do preview (device-frame) ou cabeçalho
    // (modo janela), sem lógica extra: é só o primeiro conteúdo do documento.
    QString coverMime;
    QByteArray coverBytes;
    const QString coverSrc = m_model->manuscriptEffectiveCoverDataUrl(manuscriptId);
    if (parseDataUrl(coverSrc, coverMime, coverBytes)) {
        const QImage img = QImage::fromData(coverBytes);
        if (!img.isNull()) {
            const QUrl coverRes(QStringLiteral("readerpreview://cover"));
            doc->addResource(QTextDocument::ImageResource, coverRes, img);
            QTextImageFormat imgFmt;
            imgFmt.setName(coverRes.toString());
            double w = img.width(), h = img.height();
            const double maxW = 360.0;
            if (w > maxW && w > 0) { h *= maxW / w; w = maxW; }
            imgFmt.setWidth(w);
            imgFmt.setHeight(h);

            QTextBlockFormat coverBlock;
            coverBlock.setAlignment(Qt::AlignHCenter);
            cur.setBlockFormat(coverBlock);
            cur.insertImage(imgFmt);
            wroteAnything = true;
        }
    }

    for (const Chapter* ch : chapters) {
        // Quebra de página antes de cada capítulo, menos o primeiro conteúdo
        // do documento (que já começa na "página 0"). Diferente do PDF: um
        // e-reader pagina pelo fluxo de texto, não força quebra por capítulo
        // — só a transição capa→capítulo 1 (ou início do documento) importa.
        QTextBlockFormat titleBlock;
        if (wroteAnything) titleBlock.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        if (wroteAnything) cur.insertBlock(titleBlock);
        else cur.setBlockFormat(titleBlock);
        wroteAnything = true;

        QTextCharFormat titleChar;
        titleChar.setFontWeight(QFont::Bold);
        titleChar.setFontPointSize(16);
        const QString title = ch->title.trimmed().isEmpty()
            ? subTr(QT_TRANSLATE_NOOP("Exporter", "Capítulo")) : ch->title;
        cur.insertText(title, titleChar);

        QTextBlockFormat bodyBlock;
        cur.insertBlock(bodyBlock, QTextCharFormat());
        cur.insertHtml(chapterHtmlPrimary(*ch));
    }

    applyParagraphStyle(*doc);
    forceTextColor(*doc, textColor);
    if (!includeMarkers) stripMarkers(*doc);
    if (grayscale) {
        desaturateImages(*doc);
        desaturateMarkerBackgrounds(*doc);
    }

    // Fundo da página garantido via QTextFrameFormat no frame raiz — mais
    // confiável que depender de "body { background-color }" no CSS: o frame
    // raiz sempre cobre a largura inteira do documento, enquanto regras de
    // nível body em fragmentos inseridos via insertHtml podem não se aplicar
    // de forma uniforme. Documento e margem fixos garantem uma coluna de
    // leitura consistente independente de peculiaridades do parser de HTML.
    QTextFrameFormat rootFmt = doc->rootFrame()->frameFormat();
    rootFmt.setBackground(backgroundColor);
    doc->rootFrame()->setFrameFormat(rootFmt);
    doc->setDocumentMargin(28);

    return doc;
}
