#include "Exporter.h"

#include "ProjectModel.h"
#include "ProjectStorage.h"
#include "SceneUtils.h"
#include "ZipWriter.h"

#include <QBuffer>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QProgressDialog>
#include <QFont>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QRegularExpression>
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
}

Exporter::Exporter(ProjectModel* model, const QString& projectRoot, const DocStyle& style)
    : m_model(model), m_root(projectRoot), m_style(style) {}

void Exporter::applyParagraphStyle(QTextDocument& doc) const {
    if (!m_style.fontFamily.isEmpty())
        doc.setDefaultFont(QFont(m_style.fontFamily, m_style.fontSize));
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
    return fmt == Format::Pdf ? QStringLiteral("pdf") : QStringLiteral("odt");
}

QByteArray Exporter::writeDoc(QTextDocument& doc, Format fmt) const {
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
        const bool pdf = (sel.format == Format::Pdf);
        const QString suggested = projName + QStringLiteral(".") + ext;
        const QString filter = pdf
            ? QStringLiteral("Documento PDF (*.pdf)")
            : QStringLiteral("Documento ODF (*.odt)");
        const QString dest = QFileDialog::getSaveFileName(
            dialogParent,
            pdf ? QStringLiteral("Exportar como PDF") : QStringLiteral("Exportar como ODT"),
            suggested, filter);
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
