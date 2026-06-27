#pragma once

#include <QSet>
#include <QString>

class ProjectModel;
class QWidget;
class QTextDocument;
struct Chapter;
struct DrawerItem;

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

    struct Selection {
        QSet<QString> chapterIds;
        QSet<QString> itemIds;
        Format format = Format::Odt;
        ManuscriptMode manuscriptMode = ManuscriptMode::SingleDocument;
        bool includeMarkers = true;   // marca-textos saem no documento?
    };

    Exporter(ProjectModel* model, const QString& projectRoot, const DocStyle& style);

    // Abre um diálogo de destino e grava. true em sucesso; *error em falha.
    // *nothingExported vira true se a seleção não produziu nenhum arquivo.
    bool run(const Selection& sel, QWidget* dialogParent,
             QString* error = nullptr, bool* nothingExported = nullptr);

private:
    struct OutFile {
        QString path;     // caminho relativo dentro do zip (usa "/")
        QByteArray bytes; // conteúdo já no formato final (.odt)
    };

    // HTML do capítulo com a variação PRIMÁRIA de cada cena.
    QString chapterHtmlPrimary(const Chapter& ch) const;
    QString itemHtml(const DrawerItem& it) const;

    // Monta o QTextDocument e serializa no formato pedido (ODT ou PDF).
    QByteArray exportItem(const QString& html, bool includeMarkers, Format fmt) const;
    QByteArray exportChapters(const QList<const Chapter*>& chapters, bool includeMarkers, Format fmt) const;
    QByteArray writeDoc(QTextDocument& doc, Format fmt) const;

    // DOCX (OOXML): zip com XMLs do WordprocessingML. O Qt não tem writer nativo,
    // então serializamos o QTextDocument à mão — bloco a bloco, fragmento a
    // fragmento — preservando negrito/itálico/sublinhado/tachado, marca-textos,
    // alinhamento, recuo, espaçamento e imagens embutidas.
    QByteArray docxFromDocument(QTextDocument& doc) const;

    QList<OutFile> buildFiles(const Selection& sel) const;

    // EPUB 3: um único arquivo com todos os itens selecionados como capítulos
    // navegáveis (ignora manuscriptMode), capa, metadados e índice.
    QByteArray buildEpub(const Selection& sel) const;
    QString itemBodyXhtml(const QString& rawHtml, bool includeMarkers,
                          QList<QPair<QString, QByteArray>>& imagesOut,
                          QStringList& imageMimesOut, int& imgCounter) const;

    static QString safeName(const QString& s);
    static QString formatExt(Format fmt);

    void applyParagraphStyle(QTextDocument& doc) const;

    ProjectModel* m_model;
    QString m_root;
    DocStyle m_style;
};
