#pragma once

#include <QSet>
#include <QString>

class ProjectModel;
class QWidget;
class QTextDocument;
class QTextCursor;
class QColor;
class QObject;
struct Chapter;
struct DrawerItem;
struct Manuscript;

// Exporta capítulos e documentos de gaveta selecionados. Etapa 1: ODT.
// O conteúdo é lido do disco/modelo — a MainWindow salva o projeto antes de
// chamar, garantindo que o disco reflita as edições atuais.
class Exporter {
public:
    enum class Format { Odt, Pdf, Epub, Docx };
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
        SubmissionInfo submission;
    };

    // O preset de submissão é sobre PÁGINA PAGINADA: num EPUB (texto que
    // reflui) nada disso significa coisa alguma, e em capítulos separados a
    // contagem de palavras e o cabeçalho corrido perdem sentido.
    static bool submissionApplies(const Selection& sel) {
        return sel.submissionFormat && sel.format != Format::Epub
            && sel.manuscriptMode == ManuscriptMode::SingleDocument;
    }

    Exporter(ProjectModel* model, const QString& projectRoot, const DocStyle& style);

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
    QTextDocument* buildPreviewDocument(const QString& manuscriptId,
                                         bool includeMarkers,
                                         const QColor& textColor,
                                         const QColor& backgroundColor,
                                         bool grayscale,
                                         QObject* docParent = nullptr) const;

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
    QByteArray docxFromDocument(QTextDocument& doc,
                                const QString& runningHeader = QString()) const;

    QList<OutFile> buildFiles(const Selection& sel) const;

    // EPUB 3: um único arquivo com todos os itens selecionados como capítulos
    // navegáveis (ignora manuscriptMode), capa, metadados e índice.
    QByteArray buildEpub(const Selection& sel) const;

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
