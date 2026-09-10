#include "RenameService.h"

#include "DialogueStore.h"
#include "ElementsStore.h"
#include "MemoriesStore.h"
#include "ProjectModel.h"
#include "ProjectStorage.h"

#include <QHash>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <algorithm>

namespace {

struct Range {
    int start = 0;
    int end = 0; // exclusivo
    bool contains(int pos) const { return pos >= start && pos < end; }
};

bool insideAny(const QList<Range>& ranges, int pos) {
    for (const Range& r : ranges)
        if (r.contains(pos)) return true;
    return false;
}

QList<Range> tagRanges(const QString& html) {
    QList<Range> out;
    static const QRegularExpression re(QStringLiteral("<[^>]*>"));
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out.append({ int(m.capturedStart()), int(m.capturedEnd()) });
    }
    return out;
}

QString decodeEntities(QString s) {
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&")); // por último, senão
                                                             // recria entidades
    return s;
}

// Trecho legível ao redor de uma ocorrência. A limpeza do markup vem ANTES do
// recorte final: recortar primeiro faz a janela cair no meio de uma tag e o
// atributo cru (style=" color:rgba(...)") vazar como texto no preview.
QString snippetAround(const QString& html, int pos, int len, const QString& needle) {
    const int ctx = 300;
    const int from = qMax(0, pos - ctx);
    const int to = qMin(int(html.size()), pos + len + ctx);
    QString raw = html.mid(from, to - from);

    // Descarta as tags partidas nas duas pontas da janela.
    const int firstLt = raw.indexOf(QLatin1Char('<'));
    const int firstGt = raw.indexOf(QLatin1Char('>'));
    if (firstGt >= 0 && (firstLt < 0 || firstGt < firstLt))
        raw = raw.mid(firstGt + 1);
    const int lastLt = raw.lastIndexOf(QLatin1Char('<'));
    const int lastGt = raw.lastIndexOf(QLatin1Char('>'));
    if (lastLt >= 0 && lastLt > lastGt) raw = raw.left(lastLt);

    raw.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    raw = decodeEntities(raw).simplified();

    // Centraliza pelo texto já limpo — a posição original é do HTML e não
    // sobrevive à remoção das tags.
    const int side = 45;
    const int hit = needle.isEmpty() ? -1 : raw.indexOf(needle);
    if (hit < 0) return raw.left(2 * side);

    const int s = qMax(0, hit - side);
    const int e = qMin(int(raw.size()), hit + int(needle.size()) + side);
    QString out = raw.mid(s, e - s);
    if (s > 0) out.prepend(QStringLiteral("…"));
    if (e < raw.size()) out.append(QStringLiteral("…"));
    return out;
}

} // namespace

int RenameService::Plan::selectedCount() const {
    int n = 0;
    for (const Occurrence& o : occurrences)
        if (o.selected) ++n;
    return n;
}

int RenameService::Plan::countOf(Certainty c) const {
    int n = 0;
    for (const Occurrence& o : occurrences)
        if (o.certainty == c) ++n;
    return n;
}

RenameService::RenameService(ProjectModel* model, ElementsStore* elements, const QString& projectRoot)
    : m_model(model), m_elements(elements), m_root(projectRoot) {}

QList<RenameService::Term> RenameService::termsFor(const QString& oldName, const QString& newName,
                                                    const QStringList& aliases) {
    QList<Term> terms;
    const QString oldTrim = oldName.trimmed();
    const QString newTrim = newName.trimmed();
    if (oldTrim.isEmpty() || newTrim.isEmpty()) return terms;

    terms.append({ oldTrim, newTrim });

    // A prosa raramente repete o nome inteiro — "Klara Castelo" vira "Klara" na
    // segunda menção em diante. Sem este termo a renomeação pega uns trechos e
    // deixa a maioria.
    const QString oldFirst = oldTrim.section(QLatin1Char(' '), 0, 0);
    const QString newFirst = newTrim.section(QLatin1Char(' '), 0, 0);
    if (oldFirst != oldTrim && !oldFirst.isEmpty() && !newFirst.isEmpty())
        terms.append({ oldFirst, newFirst });

    // Apelidos entram como busca OPCIONAL (desmarcada). Deixá-los de fora
    // parecia certo — "Kaká" não vira "Maria" só porque o nome de registro
    // mudou — mas cria um beco sem saída: o próprio "manter como apelido"
    // transforma cada nome antigo em apelido, e aí ele nunca mais é alcançável
    // por renomeação nenhuma (caso real: 617 "Klara" órfãos depois de duas
    // renomeações em cadeia).
    for (const QString& alias : aliases) {
        const QString a = alias.trimmed();
        if (a.isEmpty() || a == oldTrim || a == newTrim) continue;
        if (a.compare(oldFirst, Qt::CaseInsensitive) == 0) continue; // já coberto
        // Apelido de uma palavra casa com o primeiro nome novo; composto casa
        // com o nome inteiro.
        const QString rep = a.contains(QLatin1Char(' ')) ? newTrim
                          : (newFirst.isEmpty() ? newTrim : newFirst);
        terms.append({ a, rep, true });
    }

    // Maior primeiro: garante que "Klara Castelo" case antes de "Klara" e que o
    // sobrenome não sobre solto no texto.
    std::sort(terms.begin(), terms.end(),
              [](const Term& a, const Term& b) { return a.find.size() > b.find.size(); });
    return terms;
}

QList<RenameService::Occurrence> RenameService::scanHtml(const QString& html,
                                                          const QList<Term>& terms,
                                                          const QString& itemIdForMentions,
                                                          const QString& mentionReplacement,
                                                          Layer mentionLayer, Layer proseLayer) {
    QList<Occurrence> out;
    if (html.isEmpty() || terms.isEmpty()) return out;

    const QList<Range> tags = tagRanges(html);
    QList<Range> consumed;

    // @menção: <a href="ref:<gaveta>:<itemId>">Nome</a>. O texto visível é só
    // rótulo — o id no href é que manda, então trocar é seguro mesmo que o
    // autor tenha editado o texto da âncora à mão.
    if (!itemIdForMentions.isEmpty() && !mentionReplacement.isEmpty()) {
        static const QRegularExpression anchorRe(
            QStringLiteral("<a[^>]*href\\s*=\\s*[\"']ref:([^:\"']*):([^\"']*)[\"'][^>]*>(.*?)</a>"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        auto it = anchorRe.globalMatch(html);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (m.captured(2) != itemIdForMentions) continue;
            const int start = int(m.capturedStart(3));
            const int len = int(m.capturedLength(3));
            if (len <= 0) continue;
            Occurrence o;
            o.layer = mentionLayer;
            o.certainty = Certainty::Linked;
            o.position = start;
            o.length = len;
            o.matched = m.captured(3);
            o.replacement = mentionReplacement;
            o.snippet = snippetAround(html, start, len, decodeEntities(o.matched));
            out.append(o);
            consumed.append({ start, start + len });
        }
    }

    for (const Term& term : terms) {
        if (term.find.isEmpty()) continue;
        QRegularExpression re(
            QStringLiteral("(?<![\\p{L}\\p{N}_])%1(?![\\p{L}\\p{N}_])")
                .arg(QRegularExpression::escape(term.find)),
            QRegularExpression::UseUnicodePropertiesOption);
        auto it = re.globalMatch(html);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const int start = int(m.capturedStart());
            if (insideAny(tags, start)) continue;     // dentro de <...>, é markup
            if (insideAny(consumed, start)) continue; // já contado (menção ou termo maior)
            Occurrence o;
            o.layer = proseLayer;
            o.certainty = Certainty::Loose;
            o.position = start;
            o.length = int(m.capturedLength());
            o.matched = term.find;
            o.replacement = term.replace;
            o.fromAlias = term.fromAlias;
            o.selected = !term.fromAlias; // apelido é escolha consciente do autor
            o.snippet = snippetAround(html, start, o.length, term.find);
            out.append(o);
            consumed.append({ start, start + o.length });
        }
    }

    std::sort(out.begin(), out.end(),
              [](const Occurrence& a, const Occurrence& b) { return a.position < b.position; });
    return out;
}

QString RenameService::applyToHtml(const QString& html, const QList<Occurrence>& occurrences) {
    QList<Occurrence> sel;
    for (const Occurrence& o : occurrences)
        if (o.selected && o.position >= 0 && o.length > 0 && !o.replacement.isEmpty())
            sel.append(o);

    // De trás pra frente: as posições são do HTML original e trocar um nome por
    // outro de tamanho diferente deslocaria tudo que vem depois.
    std::sort(sel.begin(), sel.end(),
              [](const Occurrence& a, const Occurrence& b) { return a.position > b.position; });

    QString out = html;
    for (const Occurrence& o : sel) {
        if (o.position + o.length > out.size()) continue;
        out.replace(o.position, o.length, o.replacement);
    }
    return out;
}

QString RenameService::itemIdForElement(const QString& elementId) const {
    if (!m_model) return QString();
    for (const Drawer& d : m_model->drawers())
        for (const DrawerItem& it : d.items)
            if (it.elementId == elementId) return it.id;
    return QString();
}

void RenameService::scanChapters(Plan& plan) const {
    if (!m_model) return;
    const QString itemId = itemIdForElement(plan.elementId);

    for (const Manuscript& ms : m_model->manuscripts()) {
        const QList<const Chapter*> chapters = m_model->orderedChaptersForManuscript(ms.id);
        for (const Chapter* ch : chapters) {
            if (!ch || ch->file.isEmpty()) continue;
            bool ok = false;
            const QString html = ProjectStorage::readChapter(m_root, ch->file, &ok);
            if (!ok || html.isEmpty()) continue;

            const QString where = ms.title.isEmpty()
                ? ch->title
                : QStringLiteral("%1 — %2").arg(ms.title, ch->title);

            QList<Occurrence> found = scanHtml(html, plan.terms, itemId, plan.newName,
                                               Layer::ChapterMention, Layer::ChapterProse);
            for (Occurrence& o : found) {
                o.where = where;
                o.relFile = ch->file;
                plan.occurrences.append(o);
            }

            // Variações não-ativas moram em arquivo próprio e não aparecem no
            // HTML do capítulo — sem isso elas guardariam o nome antigo.
            for (const Scene& sc : ch->scenes) {
                for (const Variation& v : sc.variations) {
                    if (v.id == sc.activeVariationId) continue;
                    const QString path =
                        ProjectStorage::variationPath(m_root, ch->manuscriptId, sc.id, v.id);
                    bool vok = false;
                    const QString vhtml = ProjectStorage::readText(path, &vok);
                    if (!vok || vhtml.isEmpty()) continue;

                    QList<Occurrence> vfound = scanHtml(vhtml, plan.terms, itemId, plan.newName,
                                                        Layer::VariationMention, Layer::VariationProse);
                    for (Occurrence& o : vfound) {
                        o.where = QStringLiteral("%1 — %2 (%3)")
                                      .arg(where, sc.title.isEmpty() ? QStringLiteral("cena") : sc.title,
                                           v.label.isEmpty() ? QStringLiteral("variação") : v.label);
                        o.filePath = path;
                        plan.occurrences.append(o);
                    }
                }
            }
        }
    }
}

void RenameService::scanDrawerItems(Plan& plan) const {
    if (!m_model) return;
    const QString ownItemId = itemIdForElement(plan.elementId);

    for (const Drawer& d : m_model->drawers()) {
        for (const DrawerItem& it : d.items) {
            const QString where = d.title.isEmpty()
                ? it.title
                : QStringLiteral("%1 — %2").arg(d.title, it.title);

            if (it.isSheet) {
                for (const SheetField& f : it.sheet.fields) {
                    if (f.value.isEmpty()) continue;
                    QList<Occurrence> found = scanHtml(f.value, plan.terms, QString(), QString(),
                                                       Layer::SheetField, Layer::SheetField);
                    for (Occurrence& o : found) {
                        o.layer = Layer::SheetField;
                        o.certainty = Certainty::Loose;
                        o.where = QStringLiteral("%1 · %2").arg(where, f.label);
                        o.ownerId = it.id;
                        o.fieldId = f.id;
                        plan.occurrences.append(o);
                    }
                }
                continue;
            }

            QString html = it.html;
            QString absPath;
            if (!it.hasInlineHtml && !it.file.isEmpty()) {
                absPath = ProjectStorage::joinPath(m_root, it.file);
                bool ok = false;
                const QString txt = ProjectStorage::readText(absPath, &ok);
                if (ok) html = txt;
            }
            if (html.isEmpty()) continue;

            QList<Occurrence> found = scanHtml(html, plan.terms, ownItemId, plan.newName,
                                               Layer::ItemMention, Layer::ItemProse);
            for (Occurrence& o : found) {
                o.where = where;
                o.ownerId = it.id;
                o.filePath = absPath; // vazio = html inline, grava pelo modelo
                plan.occurrences.append(o);
            }
        }
    }
}

RenameService::Plan RenameService::scan(const QString& elementId, const QString& oldName,
                                        const QString& newName) const {
    Plan plan;
    plan.elementId = elementId;
    plan.oldName = oldName.trimmed();
    plan.newName = newName.trimmed();

    if (plan.newName.isEmpty() || plan.oldName.isEmpty() || plan.newName == plan.oldName)
        return plan;

    QStringList aliases;
    if (m_elements) {
        if (const Element* e = m_elements->findElement(elementId)) aliases = e->aliases;
    }
    plan.aliases = aliases;
    plan.terms = termsFor(plan.oldName, plan.newName, aliases);
    if (plan.terms.isEmpty()) return plan;

    scanChapters(plan);
    scanDrawerItems(plan);
    return plan;
}

bool RenameService::apply(const Plan& plan, QString* error) {
    if (!m_model || !m_elements) {
        if (error) *error = QStringLiteral("Projeto não carregado.");
        return false;
    }
    if (plan.newName.isEmpty() || plan.newName == plan.oldName) {
        if (error) *error = QStringLiteral("Nome novo inválido.");
        return false;
    }

    // Agrupa por origem: cada arquivo/campo é lido, reescrito e gravado uma vez.
    QHash<QString, QList<Occurrence>> byChapterFile;
    QHash<QString, QList<Occurrence>> byAbsPath;
    QHash<QString, QList<Occurrence>> byInlineItem;
    QHash<QString, QList<Occurrence>> bySheetField; // chave: itemId + '\n' + fieldId

    for (const Occurrence& o : plan.occurrences) {
        if (!o.selected) continue;
        switch (o.layer) {
            case Layer::ChapterMention:
            case Layer::ChapterProse:
                byChapterFile[o.relFile].append(o);
                break;
            case Layer::VariationMention:
            case Layer::VariationProse:
                byAbsPath[o.filePath].append(o);
                break;
            case Layer::SheetField:
                bySheetField[o.ownerId + QLatin1Char('\n') + o.fieldId].append(o);
                break;
            case Layer::ItemMention:
            case Layer::ItemProse:
                if (o.filePath.isEmpty()) byInlineItem[o.ownerId].append(o);
                else byAbsPath[o.filePath].append(o);
                break;
        }
    }

    for (auto it = byChapterFile.begin(); it != byChapterFile.end(); ++it) {
        if (it.key().isEmpty()) continue;
        bool ok = false;
        const QString html = ProjectStorage::readChapter(m_root, it.key(), &ok);
        if (!ok) continue;
        const QString updated = applyToHtml(html, it.value());
        if (updated == html) continue;
        if (ProjectStorage::writeChapter(m_root, it.key(), updated) != ProjectStorage::WriteOutcome::Written) {
            if (error) *error = QStringLiteral("Falha ao gravar o capítulo %1.").arg(it.key());
            return false;
        }
    }

    for (auto it = byAbsPath.begin(); it != byAbsPath.end(); ++it) {
        if (it.key().isEmpty()) continue;
        bool ok = false;
        const QString html = ProjectStorage::readText(it.key(), &ok);
        if (!ok) continue;
        const QString updated = applyToHtml(html, it.value());
        if (updated == html) continue;
        if (ProjectStorage::writeTextGuarded(it.key(), updated, m_root, QString())
                != ProjectStorage::WriteOutcome::Written) {
            if (error) *error = QStringLiteral("Falha ao gravar %1.").arg(it.key());
            return false;
        }
    }

    for (auto it = byInlineItem.begin(); it != byInlineItem.end(); ++it) {
        const DrawerItem* item = m_model->findDrawerItem(it.key());
        if (!item) continue;
        const QString updated = applyToHtml(item->html, it.value());
        if (updated != item->html) m_model->updateDrawerItemHtml(it.key(), updated);
    }

    for (auto it = bySheetField.begin(); it != bySheetField.end(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('\n'));
        if (parts.size() != 2) continue;
        const DrawerItem* item = m_model->findDrawerItem(parts.at(0));
        if (!item || !item->isSheet) continue;
        CharacterSheet sheet = item->sheet;
        bool touched = false;
        for (SheetField& f : sheet.fields) {
            if (f.id != parts.at(1)) continue;
            const QString updated = applyToHtml(f.value, it.value());
            if (updated != f.value) { f.value = updated; touched = true; }
            break;
        }
        if (touched) m_model->updateDrawerItemSheet(parts.at(0), sheet);
    }

    // Nome canônico. O alias antigo importa: a detecção de presença e o detector
    // de diálogos casam por name+aliases, então sem ele tudo que já foi
    // detectado pelo nome velho deixaria de bater.
    const Element* current = m_elements->findElement(plan.elementId);
    if (current) {
        Element copy = *current;
        copy.name = plan.newName;
        if (plan.keepOldAsAlias && !plan.oldName.isEmpty() && !copy.aliases.contains(plan.oldName))
            copy.aliases.append(plan.oldName);
        m_elements->updateElement(plan.elementId, copy);
    }

    for (const Drawer& d : m_model->drawers()) {
        for (const DrawerItem& it : d.items) {
            if (it.elementId != plan.elementId) continue;
            if (it.title != plan.newName)
                m_model->updateDrawerItemMeta(it.id, plan.newName, it.role);
            break;
        }
    }

    // Snapshots de texto (fala detectada, trecho de memória): são cópias de
    // pedaços do manuscrito e não acompanham a renomeação sozinhas. Só entram
    // os termos que o autor de fato aplicou — o que ele desmarcou continua
    // valendo aqui também.
    QVector<QPair<QString, QString>> appliedTerms;
    for (const Occurrence& o : plan.occurrences) {
        if (!o.selected || o.matched.isEmpty() || o.replacement.isEmpty()) continue;
        const QPair<QString, QString> pair(o.matched, o.replacement);
        if (!appliedTerms.contains(pair)) appliedTerms.append(pair);
    }
    if (!appliedTerms.isEmpty()) {
        if (m_dialogueStore) m_dialogueStore->replaceInTexts(appliedTerms);
        if (m_memoriesStore) m_memoriesStore->replaceInTexts(appliedTerms);
    }

    return true;
}
