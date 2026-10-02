#include "GrannaNotifier.h"

#include "DialogueDetector.h"
#include "DialogueStore.h"
#include "DialogueVoices.h"
#include "DocHeaderBar.h"
#include "EditorHost.h"
#include "ElementsStore.h"
#include "GrannaPickPopup.h"
#include "ProjectModel.h"

#include <QSet>
#include <QTextBlock>
#include <QTextEdit>
#include <algorithm>
#include <climits>

namespace {

// Filtro barato antes de pedir um scan: parágrafo sem travessão nem aspas não
// tem fala, e não vale ler o capítulo inteiro de novo a cada Enter da narração.
bool mayContainSpeech(const QString& text)
{
    if (text.startsWith(QLatin1String("- "))) return true;
    static const QString marks = QStringLiteral("—–\"“”„«»‹›");
    for (const QChar c : text)
        if (marks.contains(c)) return true;
    return false;
}

// Parágrafos como o scan os enxerga: o texto do scan vem de toPlainText(),
// que troca o espaço inseparável por espaço comum; QTextBlock::text() não.
QStringList scanParagraphs(QString text)
{
    text.replace(QChar(0x00A0), QLatin1Char(' '));
    return DialogueDetector::paragraphsOf(text);
}

} // namespace

GrannaNotifier::GrannaNotifier(const Deps& deps, QObject* parent)
    : QObject(parent), d(deps)
{
    // Curto de propósito: só o bastante pro Enter terminar de criar o
    // parágrafo novo. O aviso tem que parecer resposta ao Enter.
    m_timer.setSingleShot(true);
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &GrannaNotifier::announce);

    if (d.editor)
        connect(d.editor, &QTextEdit::cursorPositionChanged, this, &GrannaNotifier::onCursorMoved);
    if (d.header) {
        connect(d.header, &DocHeaderBar::grannaMarkClicked, this, &GrannaNotifier::openPicker);
        // Aviso de fala nova acabou: se o cursor está numa fala que já
        // existia, a foto dela volta a ficar.
        connect(d.header, &DocHeaderBar::grannaAnnounceEnded, this, [this]() {
            m_announcing = false;
            showCurrent();
        });
    }
}

void GrannaNotifier::reset()
{
    m_timer.stop();
    m_tracking = false;
    m_track = QTextCursor();
    m_enterText.clear();
    m_enterState.clear();
    m_markId.clear();
    m_stickyKey.clear();
    m_announcing = false;
    if (m_popup) m_popup->close();
    if (d.header) d.header->hideGrannaMark();
}

bool GrannaNotifier::activeDoc(QString* chapterId) const
{
    if (!d.host || !d.model || !d.dialogues || !d.editor) return false;
    if (d.enabled && !d.enabled()) return false;
    // Roteiro: o nome do personagem já vem escrito em cima de cada fala.
    if (d.model->isScreenplay()) return false;
    const EditorHost::ViewMode vm = d.host->viewMode();
    if (vm.type != EditorHost::ChapterDoc && vm.type != EditorHost::SceneDoc) return false;
    if (chapterId) *chapterId = vm.chapterId;
    return true;
}

void GrannaNotifier::trackBlock(const QTextBlock& block)
{
    m_track = QTextCursor(block);
    // Sem isto o cursor de apoio anda junto com o texto digitado na posição
    // dele: numa linha nova ele ia parar no fim da fala e, no Enter, pulava
    // pra linha de baixo — e o parágrafo "nunca terminava".
    m_track.setKeepPositionOnInsert(true);
    m_tracking = true;
    m_enterText = block.text();
    m_enterState.clear();
    QString chapterId;
    const QStringList paras = scanParagraphs(m_enterText);
    if (activeDoc(&chapterId) && !paras.isEmpty() && mayContainSpeech(m_enterText)) {
        const QSet<QString> wanted(paras.begin(), paras.end());
        for (const DialogueStore::Dialogue& dl : d.dialogues->dialogues())
            if (dl.chapterId == chapterId && wanted.contains(dl.text))
                m_enterState.insert(dl.text, { dl.id, dl.characterId, dl.confidence });
    }
    showCurrent();
}

const DialogueStore::Dialogue* GrannaNotifier::dialogueById(const QString& id) const
{
    if (id.isEmpty() || !d.dialogues) return nullptr;
    for (const DialogueStore::Dialogue& dl : d.dialogues->dialogues())
        if (dl.id == id) return &dl;
    return nullptr;
}

const DialogueStore::Dialogue* GrannaNotifier::currentDialogue(const QString& chapterId) const
{
    if (!m_tracking || !d.dialogues) return nullptr;
    // Pelo texto de agora (a fala pode ter sido editada e reescaneada)...
    const QStringList paras = scanParagraphs(m_track.block().text());
    const DialogueStore::Dialogue* found = nullptr;
    int foundAt = -1;
    for (const DialogueStore::Dialogue& dl : d.dialogues->dialogues()) {
        if (dl.chapterId != chapterId) continue;
        const int at = paras.indexOf(dl.text);
        if (at > foundAt) { found = &dl; foundAt = at; }
    }
    if (found) return found;
    // ...ou pela fala que estava ali quando o cursor entrou.
    for (const Before& b : m_enterState)
        if (const DialogueStore::Dialogue* dl = dialogueById(b.id)) return dl;
    return nullptr;
}

void GrannaNotifier::releaseSticky()
{
    if (m_stickyKey.isEmpty()) return;
    m_stickyKey.clear();
    if (d.header) d.header->releaseGrannaMark();
}

void GrannaNotifier::showCurrent()
{
    if (m_popup || m_announcing || !d.header) return;
    QString chapterId;
    // Só fala que JÁ existia quando o cursor entrou: a que está sendo
    // escrita agora continua quieta até o Enter.
    if (!activeDoc(&chapterId) || m_enterState.isEmpty()) { releaseSticky(); return; }
    const DialogueStore::Dialogue* cur = currentDialogue(chapterId);
    if (!cur || cur->isExtra()) { releaseSticky(); return; }
    const QString key = cur->id + QLatin1Char('|') + cur->characterId + QLatin1Char('|') + cur->confidence;
    if (key == m_stickyKey) return;
    m_stickyKey = key;
    showMark(*cur, true);
}

void GrannaNotifier::showMark(const DialogueStore::Dialogue& dl, bool sticky)
{
    const Element* el = (d.elements && !dl.characterId.isEmpty()) ? d.elements->findElement(dl.characterId) : nullptr;
    const QString name = el ? el->name : QString();
    DocHeaderBar::GrannaMark mark = DocHeaderBar::GrannaMark::Certain;
    QString tip;
    if (dl.characterId.isEmpty() || !el) {
        mark = DocHeaderBar::GrannaMark::Unknown;
        tip = tr("Fala sem dono. Clique para dizer quem falou.");
    } else if (dl.isProbable()) {
        mark = DocHeaderBar::GrannaMark::Probable;
        tip = tr("Provavelmente %1. Clique para confirmar ou trocar.").arg(name);
    } else {
        tip = tr("Fala de %1. Clique para trocar.").arg(name);
    }
    m_markId = dl.id;
    d.header->showGrannaMark(mark, el ? el->image : QString(), name,
                             voiceOf(dl.characterId, dl.manuscriptId), tip, sticky);
}

void GrannaNotifier::onCursorMoved()
{
    if (!d.editor) return;
    const QTextBlock current = d.editor->textCursor().block();
    if (m_tracking && m_track.document() != d.editor->document()) m_tracking = false;
    if (!m_tracking) {
        trackBlock(current);
        return;
    }
    const QTextBlock left = m_track.block();
    if (left == current) return;

    // Saiu de um parágrafo. Só interessa se ele foi editado e pode ter fala.
    const QString text = left.isValid() ? left.text() : QString();
    QString chapterId;
    if (text != m_enterText && mayContainSpeech(text) && activeDoc(&chapterId)) {
        const QStringList paras = scanParagraphs(text);
        if (!paras.isEmpty()) {
            m_pendingChapter = chapterId;
            m_pendingTexts = paras;
            m_pendingBefore = m_enterState;
            m_timer.start();
        }
    }
    trackBlock(current);
}

QColor GrannaNotifier::voiceOf(const QString& characterId, const QString& manuscriptId) const
{
    const QHash<QString, int> rank = DialogueVoices::rankForManuscript(d.dialogues->dialogues(), manuscriptId);
    return DialogueVoices::color(rank.value(characterId, -1));
}

void GrannaNotifier::announce()
{
    QString chapterId;
    if (!activeDoc(&chapterId) || chapterId != m_pendingChapter) return;
    if (m_popup) return; // escolhendo quem disse outra fala: não atropelar
    if (d.scanNow) d.scanNow();

    // A fala do parágrafo que terminou (a última, se uma quebra de linha
    // dividiu o parágrafo em duas).
    const DialogueStore::Dialogue* found = nullptr;
    int foundAt = -1;
    for (const DialogueStore::Dialogue& dl : d.dialogues->dialogues()) {
        if (dl.chapterId != chapterId) continue;
        const int at = m_pendingTexts.indexOf(dl.text);
        if (at > foundAt) { found = &dl; foundAt = at; }
    }
    if (!found || found->isExtra()) { showCurrent(); return; }
    const DialogueStore::Dialogue dl = *found;

    // Atribuição nova? A mesma fala (ou ela antes de editada) com o mesmo
    // dono e a mesma certeza não é notícia.
    const Before* before = nullptr;
    auto it = m_pendingBefore.constFind(dl.text);
    if (it != m_pendingBefore.constEnd()) before = &it.value();
    if (!before) {
        double best = 0.6;
        for (auto b = m_pendingBefore.constBegin(); b != m_pendingBefore.constEnd(); ++b) {
            const double sim = DialogueStore::textSimilarity(b.key(), dl.text);
            if (sim >= best) { best = sim; before = &b.value(); }
        }
    }
    if (before && before->characterId == dl.characterId && before->confidence == dl.confidence) {
        showCurrent();
        return;
    }
    // O aviso passa na frente da foto fixa; quando ele some, ela volta.
    m_stickyKey.clear();
    m_announcing = true;
    showMark(dl, false);
}

void GrannaNotifier::openPicker()
{
    if (!d.dialogues || !d.elements || !d.header || m_popup) return;
    const DialogueStore::Dialogue* found = dialogueById(m_markId);
    QString chapterId;
    if (!found && !m_stickyKey.isEmpty() && activeDoc(&chapterId)) {
        // A fala fixa foi editada e reescaneada: o id pode ter mudado.
        if (d.scanNow) d.scanNow();
        found = currentDialogue(chapterId);
    }
    if (!found) { d.header->hideGrannaMark(); m_stickyKey.clear(); return; }
    const DialogueStore::Dialogue dl = *found;

    // Quem está na cena: presença confirmada no capítulo/cena (mesmo critério
    // do "Trocar quem disse…" do Pensário) + quem já fala nela.
    const QString chapterKey = ElementsStore::elementDocKeyForChapter(dl.manuscriptId, dl.chapterId);
    QString sceneKey;
    QSet<QString> present;
    for (const QString& id : d.elements->docElementIds(chapterKey)) present.insert(id);
    if (dl.sceneIndex >= 0 && d.model) {
        if (const Chapter* ch = d.model->findChapter(dl.chapterId); ch && dl.sceneIndex < ch->scenes.size()) {
            sceneKey = ElementsStore::elementDocKeyForScene(dl.manuscriptId, dl.chapterId,
                                                            ch->scenes.at(dl.sceneIndex).id);
            for (const QString& id : d.elements->docElementIds(sceneKey)) present.insert(id);
        }
    }
    for (const DialogueStore::Dialogue& o : d.dialogues->dialogues())
        if (o.chapterId == dl.chapterId && o.sceneIndex == dl.sceneIndex && !o.characterId.isEmpty())
            present.insert(o.characterId);

    const QHash<QString, int> rank = DialogueVoices::rankForManuscript(d.dialogues->dialogues(), dl.manuscriptId);
    QList<Element> chars;
    for (const Element& e : d.elements->elements())
        if (e.type == QLatin1String("character")) chars.append(e);
    // Quem mais fala primeiro; quem nunca falou, por nome.
    std::sort(chars.begin(), chars.end(), [&rank](const Element& a, const Element& b) {
        const int ra = rank.value(a.id, INT_MAX), rb = rank.value(b.id, INT_MAX);
        if (ra != rb) return ra < rb;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    QVector<GrannaPickPopup::Person> scene, others;
    const int faceSize = GrannaPickPopup::faceSize();
    for (const Element& e : chars) {
        const QColor voice = DialogueVoices::color(rank.value(e.id, -1));
        const QString name = e.name.isEmpty() ? tr("(sem nome)") : e.name;
        GrannaPickPopup::Person p { e.id, name, DialogueVoices::face(e.image, name, voice, faceSize), voice };
        (present.contains(e.id) ? scene : others).append(p);
    }
    if (scene.isEmpty() && others.isEmpty()) return;

    QString hint;
    if (!dl.characterId.isEmpty()) {
        const Element* cur = d.elements->findElement(dl.characterId);
        const QString curName = cur ? cur->name : QString();
        if (dl.isProbable()) hint = tr("O Granna acha que é %1.").arg(curName);
        else hint = tr("Salva como %1. Escolha outra pessoa para corrigir.").arg(curName);
    }

    auto* popup = new GrannaPickPopup(dl.text, hint, scene, others, dl.characterId);
    m_popup = popup;
    d.header->holdGrannaMark(true);
    const QString dlgId = dl.id;
    connect(popup, &GrannaPickPopup::closed, this, [this]() {
        m_popup = nullptr;
        if (d.header) d.header->holdGrannaMark(false);
    });
    // A fala escolhida é a do parágrafo onde o cursor está (foto fixa)?
    auto inCurrent = [this](const QString& id) {
        for (const Before& b : m_enterState)
            if (b.id == id) return true;
        return false;
    };
    connect(popup, &GrannaPickPopup::characterChosen, this,
            [this, dlgId, chapterKey, sceneKey, inCurrent](const QString& characterId) {
        m_popup = nullptr;
        if (!d.dialogues || !d.elements || !d.header) return;
        d.dialogues->setCharacter(dlgId, characterId);
        // Atribuir também confirma a presença na cena (igual ao Pensário).
        d.elements->addDocElement(chapterKey, characterId);
        if (!sceneKey.isEmpty()) d.elements->addDocElement(sceneKey, characterId);
        // A foto escolhida desliza de novo: é o "salvei". Na fala onde o
        // cursor está, ela fica; senão, é um aviso que some.
        d.header->holdGrannaMark(false);
        m_stickyKey.clear();
        if (inCurrent(dlgId) && !m_announcing) {
            showCurrent();
        } else if (const DialogueStore::Dialogue* dl = dialogueById(dlgId)) {
            m_announcing = true;
            showMark(*dl, false);
        }
    });
    auto dropMark = [this]() {
        m_popup = nullptr;
        m_markId.clear();
        m_stickyKey.clear();
        m_announcing = false;
        if (d.header) d.header->hideGrannaMark();
    };
    connect(popup, &GrannaPickPopup::extraChosen, this, [this, dlgId, dropMark]() {
        if (d.dialogues) d.dialogues->setExtra(dlgId);
        dropMark();
    });
    connect(popup, &GrannaPickPopup::notSpeechChosen, this, [this, dlgId, dropMark]() {
        if (d.dialogues) d.dialogues->remove(dlgId);
        dropMark();
    });
    popup->popupBelow(d.header->grannaMarkGlobalRect());
}
