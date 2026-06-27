#include "Exporter.h"

#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"
#include "ZipWriter.h"

#include <QBuffer>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QProgressDialog>
#include <QFont>
#include <QImage>
#include <QMarginsF>
#include <QPainter>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QStringList>
#include <QUuid>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentWriter>
#include <algorithm>
#include <functional>

namespace {
// O HTML salvo carrega a cor de texto do tema da tela (clara, pra fundo escuro),
// que sobre o papel branco do ODT fica ilegível/apagada. Na exportação queremos
// tinta preta: força o foreground de todo o documento pra preto, preservando
// negrito/itálico/sublinhado e os marca-textos (que são cor de fundo).
void forceBlackText(QTextDocument& doc) {
    QTextCursor c(&doc);
    c.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setForeground(QColor(Qt::black));
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

void Exporter::applyParagraphStyle(QTextDocument& doc) const {
    if (!m_style.fontFamily.isEmpty()) {
        QFont f(m_style.fontFamily);
        f.setPointSizeF(m_style.fontSize);
        doc.setDefaultFont(f);
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

QString Exporter::safeName(const QString& s) {
    QString out = s;
    out.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    out = out.trimmed();
    return out.isEmpty() ? QStringLiteral("Documento") : out;
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

QByteArray Exporter::writeDoc(QTextDocument& doc, Format fmt) const {
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
        writer.setPageMargins(QMarginsF(20, 18, 20, 18), QPageLayout::Millimeter);
        writer.setTitle(m_model ? m_model->projectName() : QString());
        // doc.print pagina automaticamente para o QPagedPaintDevice.
        doc.print(&writer);
    } else {
        QTextDocumentWriter writer(&buf, "ODF");
        writer.write(&doc);
    }
    buf.close();
    return bytes;
}

QByteArray Exporter::docxFromDocument(QTextDocument& doc) const {
    // EMU (English Metric Units): unidade de tamanho do DrawingML. 1 px (96dpi) =
    // 9525 EMU. Twips (1/20 pt): unidade de medida do WordprocessingML; 1 px = 15.
    constexpr qint64 kEmuPerPx  = 9525;
    constexpr int    kTwipsPerPx = 15;
    // Largura útil da página A4 com margens de ~20mm (= 9638 twips → EMU).
    constexpr qint64 kMaxImgCx  = 9638LL * 635;

    struct Img { QString path; QByteArray bytes; QString rId; };
    QList<Img> images;
    int drawingId = 0;

    auto colorHex = [](const QColor& c) {
        return QString::asprintf("%02X%02X%02X", c.red(), c.green(), c.blue());
    };

    // <w:rPr> a partir do formato do fragmento. Foreground é ignorado: forceBlackText
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
        "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
        "<w:pgMar w:top=\"1134\" w:right=\"1134\" w:bottom=\"1134\" w:left=\"1134\" "
        "w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/></w:sectPr>"
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
    rels += QStringLiteral("</Relationships>\n");

    // ── Empacota ──
    ZipWriter zip;
    zip.addFile(QStringLiteral("[Content_Types].xml"), QByteArrayLiteral(
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
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
        "</Types>\n"));
    zip.addFile(QStringLiteral("_rels/.rels"), QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"word/document.xml\"/></Relationships>\n"));
    zip.addFile(QStringLiteral("word/document.xml"), document.toUtf8());
    zip.addFile(QStringLiteral("word/styles.xml"), styles.toUtf8());
    zip.addFile(QStringLiteral("word/_rels/document.xml.rels"), rels.toUtf8());
    for (const Img& im : images)
        zip.addFile(QStringLiteral("word/") + im.path, im.bytes, /*compress=*/false);
    return zip.finish();
}

QByteArray Exporter::exportItem(const QString& html, bool includeMarkers, Format fmt) const {
    QTextDocument doc;
    doc.setHtml(html.isEmpty() ? QStringLiteral("<p></p>") : html);
    applyParagraphStyle(doc);
    forceBlackText(doc);
    if (!includeMarkers) stripMarkers(doc);
    return writeDoc(doc, fmt);
}

QByteArray Exporter::exportChapters(const QList<const Chapter*>& chapters, bool includeMarkers, Format fmt) const {
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
            ? QStringLiteral("Capítulo") : ch->title;
        cur.insertText(title, titleChar);

        // Corpo: novo bloco, formatação limpa, conteúdo do capítulo.
        QTextBlockFormat bodyBlock;
        cur.insertBlock(bodyBlock, QTextCharFormat());
        cur.insertHtml(chapterHtmlPrimary(*ch));
    }

    applyParagraphStyle(doc);
    forceBlackText(doc);
    if (!includeMarkers) stripMarkers(doc);
    return writeDoc(doc, fmt);
}

QList<Exporter::OutFile> Exporter::buildFiles(const Selection& sel) const {
    QList<OutFile> files;
    if (!m_model) return files;

    // EPUB: um único arquivo com tudo dentro (ignora documento único/separado).
    if (sel.format == Format::Epub) {
        const QByteArray epub = buildEpub(sel);
        if (!epub.isEmpty())
            files.append({ safeName(m_model->projectName()) + QStringLiteral(".epub"), epub });
        return files;
    }

    // ── Manuscritos ──
    for (const Manuscript& ms : m_model->manuscripts()) {
        QList<const Chapter*> selected;
        for (const Chapter& ch : m_model->chapters()) {
            const QString msId = ch.manuscriptId.isEmpty() ? ms.id : ch.manuscriptId;
            if (msId == ms.id && sel.chapterIds.contains(ch.id))
                selected.append(&ch);
        }
        if (selected.isEmpty()) continue;
        std::sort(selected.begin(), selected.end(),
                  [](const Chapter* a, const Chapter* b) { return a->order < b->order; });

        const QString msTitle = safeName(ms.title.isEmpty() ? QStringLiteral("Manuscrito") : ms.title);
        const QString ext = formatExt(sel.format);

        if (sel.manuscriptMode == ManuscriptMode::SingleDocument) {
            files.append({ QStringLiteral("Manuscritos/%1.%2").arg(msTitle, ext),
                           exportChapters(selected, sel.includeMarkers, sel.format) });
        } else {
            for (int i = 0; i < selected.size(); ++i) {
                const Chapter* ch = selected.at(i);
                const QString chTitle = safeName(ch->title.isEmpty()
                    ? QStringLiteral("Capítulo %1").arg(i + 1) : ch->title);
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
    forceBlackText(doc);
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
        for (const Chapter& ch : m_model->chapters()) {
            const QString msId = ch.manuscriptId.isEmpty() ? ms.id : ch.manuscriptId;
            if (msId == ms.id && sel.chapterIds.contains(ch.id)) chaps.append(&ch);
        }
        std::sort(chaps.begin(), chaps.end(),
                  [](const Chapter* a, const Chapter* b) { return a->order < b->order; });
        for (const Chapter* ch : chaps) {
            ++counter;
            Item it;
            it.id = QStringLiteral("ch_%1").arg(counter);
            it.title = ch->title.trimmed().isEmpty() ? QStringLiteral("Capítulo") : ch->title;
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
                it.title = di.title.trimmed().isEmpty() ? QStringLiteral("Documento") : di.title;
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
    const QString title = m_model->projectName().trimmed().isEmpty()
        ? QStringLiteral("Projeto") : m_model->projectName();
    const QString author = m_model->projectAuthor();
    const QString synopsis = m_model->projectSynopsis();
    const QString genres = m_model->projectGenres();
    const QString bookId = QStringLiteral("urn:uuid:")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString now = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-ddThh:mm:ss"))
        + QStringLiteral("Z");

    // ── Capa ──
    bool hasCover = false;
    QString coverFilename, coverMime;
    QByteArray coverBytes;
    if (parseDataUrl(m_model->projectCoverDataUrl(), coverMime, coverBytes)) {
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
    const QString lh = QString::number(m_style.lineHeightPercent / 100.0, 'f', 2);
    const QString indent = m_style.firstLineIndent ? QStringLiteral("1.5em") : QStringLiteral("0");
    const QString css =
        QStringLiteral("body { font-family: Georgia, 'Times New Roman', serif; line-height: ")
        + lh + QStringLiteral("; margin: 0 5%; color: #1a1a1a; }\n")
        + QStringLiteral("p { margin: 0 0 0.4em; text-indent: ") + indent + QStringLiteral("; }\n")
        + QStringLiteral("h1.chapter-title { text-align: center; font-weight: bold; margin: 1em 0 1.5em; text-indent: 0; }\n")
        + QStringLiteral("strong, b { font-weight: bold; } em, i { font-style: italic; }\n")
        + QStringLiteral("u { text-decoration: underline; } s, del { text-decoration: line-through; }\n")
        + QStringLiteral("img { max-width: 100%; height: auto; display: block; margin: 1em auto; }\n");

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
            QStringLiteral("Exportando… Esse processo pode levar alguns instantes.\n"
                           "Não encerre o programa caso ele pare de responder."),
            QString(), 0, 0, dialogParent);
        progress.setWindowTitle(QStringLiteral("Exportando"));
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

    if (files.size() == 1) {
        // Único arquivo → salva direto no formato escolhido.
        const QString ext = formatExt(sel.format);
        QString filter, dlgTitle;
        switch (sel.format) {
            case Format::Pdf:
                filter = QStringLiteral("Documento PDF (*.pdf)");
                dlgTitle = QStringLiteral("Exportar como PDF"); break;
            case Format::Epub:
                filter = QStringLiteral("Livro EPUB (*.epub)");
                dlgTitle = QStringLiteral("Exportar como EPUB"); break;
            case Format::Docx:
                filter = QStringLiteral("Documento Word (*.docx)");
                dlgTitle = QStringLiteral("Exportar como DOCX"); break;
            default:
                filter = QStringLiteral("Documento ODF (*.odt)");
                dlgTitle = QStringLiteral("Exportar como ODT"); break;
        }
        const QString suggested = projName + QStringLiteral(".") + ext;
        const QString dest = QFileDialog::getSaveFileName(
            dialogParent, dlgTitle, suggested, filter);
        if (dest.isEmpty()) return false; // cancelado
        QFile f(dest);
        if (!f.open(QIODevice::WriteOnly)) {
            if (error) *error = QStringLiteral("Não foi possível gravar o arquivo.");
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
        dialogParent, QStringLiteral("Exportar projeto (.zip)"),
        suggested, QStringLiteral("Arquivo ZIP (*.zip)"));
    if (dest.isEmpty()) return false;
    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Não foi possível gravar o arquivo.");
        return false;
    }
    f.write(zipBytes);
    f.close();
    return true;
}
