#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QSet>
#include <QSizeF>
#include <QString>
#include <functional>
#include <memory>
#include <QVector>
#include <QStringList>

class ProjectModel;
class QWidget;
class QTextDocument;
class QTextCursor;
class GlossaryStore;
class ConstrutorStore;
class TerritorioStore;
class MapPinsStore;
class QObject;
class QPainter;
struct Chapter;
struct DrawerItem;
struct Manuscript;

// Exporta capítulos e documentos de gaveta selecionados. Etapa 1: ODT.
// O conteúdo é lido do disco/modelo — a MainWindow salva o projeto antes de
// chamar, garantindo que o disco reflita as edições atuais.
class Exporter {
public:
    enum class Format { Odt, Pdf, Epub, Docx, Fountain };   // Fountain: só roteiro
    enum class ManuscriptMode { SingleDocument, SeparateChapters };

    // Estilo de parágrafo do projeto — não fica no HTML salvo, o editor aplica em
    // runtime. Precisamos replicá-lo no documento exportado pra manter recuo,
    // espaçamento e entrelinha.
    struct DocStyle {
        QString fontFamily;
        double fontSize = 12;
        int lineHeightPercent = 100;
        bool firstLineIndent = true;
        int spacingBefore = 0;
        int spacingAfter = 0;
    };

    // Dados que o formato de submissão exige e que não vivem no projeto: o
    // nome LEGAL (quem assina o contrato) pode ser diferente do nome de
    // publicação, e endereço/contato não são assunto de manuscrito. Ficam em
    // QSettings, globais ao autor, não por projeto.
    struct SubmissionInfo {
        QString legalName;   // nome real, canto superior esquerdo da 1ª página
        QString address;     // endereço postal, uma linha por quebra
        QString contact;     // e-mail / telefone
        QString byline;      // nome de publicação ("by ..."), pode ser pseudônimo
        QString titleShort;  // título curto pro cabeçalho corrido
        // Carrega/grava em QSettings (chaves "submission/*").
        static SubmissionInfo load();
        void save() const;
    };

    struct Selection {
        QSet<QString> chapterIds;
        QSet<QString> itemIds;
        Format format = Format::Odt;
        ManuscriptMode manuscriptMode = ManuscriptMode::SingleDocument;
        bool includeMarkers = true;   // marca-textos saem no documento?
        // Formato de manuscrito para submissão (padrão Shunn): Courier 12,
        // entrelinha dupla, margens de 1 polegada, página de título com
        // contato e contagem de palavras, cabeçalho corrido, "#" como quebra
        // de cena. Só faz sentido no manuscrito em documento único — ver
        // submissionApplies().
        bool submissionFormat = false;
        bool sceneNumbers = false;    // roteiro: número da cena nas margens
        bool dropCap = true;          // EPUB: letra capitular no começo de cada capítulo
        SubmissionInfo submission;
    };

    // O preset de submissão é sobre PÁGINA PAGINADA: num EPUB (texto que
    // reflui) nada disso significa coisa alguma, e em capítulos separados a
    // contagem de palavras e o cabeçalho corrido perdem sentido.
    static bool submissionApplies(const Selection& sel) {
        return sel.submissionFormat && sel.format != Format::Epub
            && sel.manuscriptMode == ManuscriptMode::SingleDocument;
    }

    // Fontes da bíblia do universo que não vivem no ProjectModel. Qualquer uma
    // pode vir nula — a seção correspondente simplesmente não aparece.
    struct BibleSources {
        GlossaryStore* glossary = nullptr;
        ConstrutorStore* construtor = nullptr;
        TerritorioStore* territorios = nullptr;
        MapPinsStore* mapPins = nullptr;
    };

    Exporter(ProjectModel* model, const QString& projectRoot, const DocStyle& style);

    // Bíblia do universo: gavetas + vínculos + glossário + territórios +
    // sistemas do mundo + locais do mapa num documento só, para quem precisa
    // consultar o universo sem o app (editora, co-autor, ilustrador). Pede o
    // destino e grava. EPUB não é suportado — ver runBible.
    bool runBible(const BibleSources& src, Format fmt, QWidget* dialogParent,
                  QString* error = nullptr);

    // Abre um diálogo de destino e grava. true em sucesso; *error em falha.
    // *nothingExported vira true se a seleção não produziu nenhum arquivo.
    bool run(const Selection& sel, QWidget* dialogParent,
             QString* error = nullptr, bool* nothingExported = nullptr);

    // Monta um QTextDocument pronto pra EXIBIÇÃO (não exportação) com o
    // manuscrito inteiro: capa (se houver) seguida dos capítulos em ordem,
    // cor de tinta/fundo parametrizáveis (preview de e-reader tem paleta
    // clara e escura independentes do tema do app) e grayscale opcional
    // (dessatura capa, imagens de corpo e fundo de marca-texto). Retorna
    // nullptr se o manuscrito não existir ou não tiver capítulos. docParent
    // recebe a posse do QTextDocument (é um QObject).
    // Páginas do roteiro no formato padrão (Carta, Courier 12, 6 linhas por
    // polegada) — a mesma paginação da exportação. Cada item é o HTML de um
    // capítulo; 1 página ≈ 1 minuto de tela.
    static int screenplayPageCount(const QStringList& chapterHtmls);
    // Tamanho de cada cena (na ordem dos cabeçalhos), em páginas do formato
    // padrão, medido na mesma paginação: é dele que saem os oitavos.
    static QVector<qreal> screenplaySceneLengths(const QStringList& chapterHtmls);

    // ── Prévia do painel de exportação (15/11/2026) ──
    // As primeiras páginas montadas do jeito que a exportação monta (mesmo
    // estilo, mesmas quebras, mesmo formato de editora ou de roteiro), pra
    // desenhar na tela. Só os primeiros capítulos (o bastante pra folhear);
    // o total do arquivo é estimado pelas palavras.
    struct PagePreview {
        enum Kind { Paged, Submission, Screenplay, Reader, PlainText };
        Kind kind = Paged;
        std::shared_ptr<QTextDocument> doc;
        QSizeF pagePx;          // a página inteira, na régua de 96 dpi
        QRectF contentPx;       // onde o texto fica dentro dela
        QString runningHeader;  // formato de editora: "Lobato / TRAVESSIA"
        bool titlePage = false; // a página 1 é a de rosto (editora, roteiro)
        int pages = 0;          // páginas montadas
        int estimatedTotal = 0; // do arquivo inteiro (aproximado)
        struct SceneMark { int page; qreal y; int number; };
        QList<SceneMark> sceneMarks;   // roteiro: número da cena nas margens
        bool isEmpty() const { return !doc || pages <= 0; }
    };
    // totalWords: palavras de tudo que foi escolhido (estima o total de
    // páginas); 0 = mostra só as montadas.
    PagePreview buildPagePreview(const Selection& sel, int totalWords = 0) const;
    // Desenha a página `index` na largura pedida (px lógicos), papel incluso.
    static QImage renderPreviewPage(const PagePreview& p, int index, int widthPx, qreal dpr);

    // ---- E-book como leitor (E-reader Lab, 18/11/2026) ----
    // O livro inteiro como o EPUB vai sair (capa, sumário, aberturas de
    // capítulo, capitular, gavetas), paginado na tela de um leitor. O mesmo
    // corpo XHTML do EPUB entra aqui; só a fonte, o tamanho e as cores são do
    // leitor, como no Kindle.
    struct EbookLook {
        QString fontFamily;
        qreal fontPx = 16;        // tamanho do texto, em px da tela do leitor
        qreal lineHeight = 1.6;   // entrelinha (fator)
        QColor ink, muted;        // tinta do texto e dos rótulos
    };
    struct EbookSection {
        enum Kind { Toc, Chapter, Item };
        Kind kind = Chapter;
        QString name;     // como aparece no sumário
        int page = 0;     // tela do documento onde começa (sem a capa)
        int words = 0;
        int number = 0;   // capítulo numerado: 1, 2, 3...; 0 = sem número
    };
    struct EbookPreview {
        std::shared_ptr<QTextDocument> doc;
        QImage cover;             // vazio = o livro não tem capa
        QSizeF contentPx;         // a área do texto numa tela
        QList<EbookSection> sections;
        int docPages = 0;
        QString title, tocLabel;
        QColor ink;
        bool isEmpty() const { return !doc || docPages <= 0; }
        int coverPages() const { return cover.isNull() ? 0 : 1; }
        int pages() const { return docPages + coverPages(); }
        // a seção da tela (contando a capa): -1 = capa
        int sectionAt(int screen) const;
        // a tela (contando a capa) onde a seção começa
        int screenOf(int section) const { return section < 0 || section >= sections.size() ? 0 : sections.at(section).page + coverPages(); }
        // o link do sumário naquele ponto da tela de documento, ou -1
        int linkAt(int docPage, const QPointF& pos) const;
    };
    EbookPreview buildEbookPreview(const Selection& sel, const EbookLook& look, const QSizeF& contentPx) const;
    // Desenha a tela de documento `docPage` com a origem no canto do texto.
    static void renderEbookPage(QPainter& g, const EbookPreview& b, int docPage);

private:
    struct OutFile {
        QString path;     // caminho relativo dentro do zip (usa "/")
        QByteArray bytes; // conteúdo já no formato final (.odt)
    };

    // HTML do capítulo com a variação PRIMÁRIA de cada cena.
    QString chapterHtmlPrimary(const Chapter& ch) const;
    QString itemHtml(const DrawerItem& it) const;

    // Monta o QTextDocument e serializa no formato pedido (ODT ou PDF).
    // docTitle vazio cai no fallback de sempre (nome do projeto) — só passado
    // explicitamente quando o documento corresponde a um manuscrito específico.
    QByteArray exportItem(const QString& html, bool includeMarkers, Format fmt,
                          const QString& docTitle = QString()) const;
    QByteArray exportChapters(const QList<const Chapter*>& chapters, bool includeMarkers, Format fmt,
                              const QString& docTitle = QString()) const;
    // O documento dos capítulos em sequência (título + corpo, quebra de página
    // entre eles), já com o estilo de papel. exportChapters e a prévia usam.
    void buildChaptersDocument(QTextDocument& doc, const QList<const Chapter*>& chapters,
                               bool includeMarkers) const;

    // ── Bíblia do universo ──
    QByteArray buildBible(const BibleSources& src, Format fmt) const;
    static QString biblePlainOf(const QString& html);

    // ── Formato de submissão (padrão Shunn) ──
    // Monta o manuscrito inteiro no formato que editora/revista espera, em vez
    // do estilo de leitura do projeto. Devolve os bytes já serializados porque
    // a paginação (cabeçalho corrido, número de página) precisa ser feita na
    // hora de escrever, não depois.
    QByteArray exportSubmission(const QList<const Chapter*>& chapters,
                                const QString& manuscriptTitle,
                                const SubmissionInfo& info,
                                bool includeMarkers, Format fmt) const;
    // Corpo do manuscrito com a formatação Shunn aplicada (sem a página de
    // título, que é montada à parte por precisar de layout próprio).
    void buildSubmissionBody(QTextDocument& doc, const QList<const Chapter*>& chapters,
                             bool includeMarkers) const;
    // Página de título: contato à esquerda, contagem à direita, título e
    // byline no meio da página.
    void insertSubmissionTitlePage(QTextCursor& cur, const QString& manuscriptTitle,
                                   const SubmissionInfo& info, int wordCount) const;
    // PDF paginado à mão, porque QTextDocument::print() não sabe desenhar
    // cabeçalho corrido com número de página.
    QByteArray submissionPdf(QTextDocument& doc, const QString& runningHeader,
                             const QString& docTitle) const;
    QByteArray writeDoc(QTextDocument& doc, Format fmt, const QString& docTitle = QString()) const;

    // DOCX (OOXML): zip com XMLs do WordprocessingML. O Qt não tem writer nativo,
    // então serializamos o QTextDocument à mão — bloco a bloco, fragmento a
    // fragmento — preservando negrito/itálico/sublinhado/tachado, marca-textos,
    // alinhamento, recuo, espaçamento e imagens embutidas.
    // runningHeader não-vazio adiciona cabeçalho corrido (canto superior
    // direito, com número de página via campo PAGE do Word) a partir da
    // segunda página — exigência do formato de submissão.
    // Página do DOCX em twips. O padrão é A4 com 1 polegada em volta; o
    // roteiro troca por Carta, 1,5 polegada à esquerda e cabeçalho só com o
    // número ("2."), com a página de rosto numa seção própria.
    struct DocxPage {
        int widthTw = 11906, heightTw = 16838;
        int topTw = 1440, rightTw = 1440, bottomTw = 1440, leftTw = 1440;
        bool pageNumberOnly = false;
        int titleEndState = -1;   // userState do bloco que fecha a página de rosto
    };
    QByteArray docxFromDocument(QTextDocument& doc,
                                const QString& runningHeader = QString(),
                                const DocxPage* page = nullptr) const;

    // ── Roteiro ──
    // Projeto tipo roteiro sai no formato da indústria: Carta, Courier 12,
    // margem de 1,5 polegada à esquerda, elementos nas colunas de praxe,
    // página de rosto e número no canto a partir da segunda página.
    // sceneNumbers: o PDF desenha o número nas duas margens; DOCX e ODT levam
    // o número na margem esquerda (recuo deslocado + tabulação).
    QByteArray exportScreenplay(const QList<const Chapter*>& chapters, const QString& title,
                                const SubmissionInfo& info, bool titlePage,
                                bool includeMarkers, Format fmt, bool sceneNumbers = false,
                                int firstSceneNumber = 1) const;
    void buildScreenplayDocument(QTextDocument& doc, const QList<const Chapter*>& chapters,
                                 const QString& title, const SubmissionInfo& info,
                                 bool titlePage, bool includeMarkers,
                                 bool inlineSceneNumbers = false, int firstSceneNumber = 1) const;
    QByteArray screenplayPdf(QTextDocument& doc, bool hasTitlePage, const QString& docTitle,
                             bool sceneNumbers = false, int firstSceneNumber = 1) const;
    static void appendScreenplayBody(QTextDocument& doc, QTextCursor& cur, bool& firstBlock,
                                     const QStringList& chapterHtmls, bool includeMarkers,
                                     bool breakBefore, bool inlineSceneNumbers = false,
                                     int firstSceneNumber = 1);
    // Decide as quebras de página (e aplica, se pedido); devolve o total de páginas.
    static int paginateScreenplay(QTextDocument& doc, bool applyBreaks);

    QList<OutFile> buildFiles(const Selection& sel) const;

    // EPUB 3: um único arquivo com todos os itens selecionados como capítulos
    // navegáveis (ignora manuscriptMode), capa, metadados e índice.
    QByteArray buildEpub(const Selection& sel) const;
    // As partes do EPUB na ordem do livro: capítulos escolhidos (manuscrito
    // por manuscrito) e depois os documentos de gaveta.
    struct EpubPart { const Chapter* chapter = nullptr; const DrawerItem* item = nullptr; QString title; };
    QList<EpubPart> epubParts(const Selection& sel) const;
    // A abertura do capítulo no estilo que o projeto usa no editor (BookSeams).
    // basePx <= 0 = EPUB (medidas em em); > 0 = prévia (px). muted e ink são as cores.
    QString chapterOpeningHtml(const Chapter& ch, qreal basePx, const QString& muted, const QString& ink) const;
    QString epubCss() const;
    // O idioma do livro (o do corretor) e os rótulos de sumário e capa nele.
    QString bookLanguage(QString* tocLabel, QString* coverLabel) const;

    // Manuscrito cujos capítulos aparecem na seleção, SE for o único com
    // capítulos selecionados; nullptr se 0 ou 2+ manuscritos representados
    // (nesse caso o output combina vários livros — usa identidade do projeto).
    const Manuscript* singleManuscriptInSelection(const Selection& sel) const;
    QString itemBodyXhtml(const QString& rawHtml, bool includeMarkers,
                          QList<QPair<QString, QByteArray>>& imagesOut,
                          QStringList& imageMimesOut, int& imgCounter) const;

    static QString safeName(const QString& s);
    static QString formatExt(Format fmt);

    // bodyPointSize > 0 REESCALA o corpo pra esse tamanho (usado na exportação
    // pra papel, ver kExportBodyPt). 0 mantém o tamanho do editor — é o que o
    // preview de e-reader quer, porque lá é leitura em tela.
    void applyParagraphStyle(QTextDocument& doc, double bodyPointSize = 0.0) const;

    // CSS base (fonte serif, recuo, título de capítulo) reaproveitado pelo
    // EPUB e pelo preview de e-reader. bg inválido (QColor() default) omite
    // a linha background-color — usado pelo EPUB, que não define fundo.
    QString previewCss(const QColor& fg, const QColor& bg) const;

    ProjectModel* m_model;
    QString m_root;
    DocStyle m_style;
};
