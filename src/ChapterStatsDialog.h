#pragma once

#include <QDialog>
#include <QString>

class WordCounter;
class DialogueStore;
class ElementsStore;
class ProjectModel;

// Estatísticas de um capítulo — disparado ao clicar na pilula de
// diálogo/narração no ManuscriptPanel. Mesmo molde visual do
// WritingStatsDialog (dialog sem chrome, header com botão fechar, seções com
// linhas ícone-ausente label+valor), mas "burro" que nem aquele: calcula tudo
// na construção, não reage a mudanças depois de aberto (reabrir de novo já
// resolve, e evita complexidade de invalidação pra uma janela de consulta
// rápida).
class ChapterStatsDialog : public QDialog {
    Q_OBJECT
public:
    ChapterStatsDialog(WordCounter* wordCounter, DialogueStore* dialogueStore,
                        ElementsStore* elementsStore, ProjectModel* model,
                        const QString& manuscriptId, const QString& chapterId,
                        QWidget* parent = nullptr);

private:
    void buildUi();

    WordCounter* m_wordCounter;
    DialogueStore* m_dialogueStore;
    ElementsStore* m_elementsStore;
    ProjectModel* m_model;
    QString m_manuscriptId;
    QString m_chapterId;
};
