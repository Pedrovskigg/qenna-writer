#pragma once

#include <QWidget>

class Thesaurus;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

// Popup de sinônimos, aberto pelo menu de contexto do editor.
//
// Palavra comum tem dezenas de acepções ("triste" tem 43), então cada acepção
// vira um parágrafo próprio, encabeçado pela palavra que dá nome ao sentido, e
// os sinônimos são links que quebram linha sozinhos — layout em grade não
// aguentaria essa variação.
class ThesaurusPopup : public QWidget {
    Q_OBJECT
public:
    explicit ThesaurusPopup(Thesaurus* thesaurus, QWidget* parent = nullptr);

    // context: o trecho em volta do cursor. corpus: texto do manuscrito, usado
    // pra ordenar as acepções por afinidade (ver ThesaurusRanker). Ambos podem
    // vir vazios — aí a ordem é a do dicionário e a UI não promete relevância.
    void presentAt(const QPoint& globalPos, const QString& word,
                   const QString& context = QString(), const QString& corpus = QString());

signals:
    // Usuário escolheu um sinônimo — quem ouve troca a palavra no editor.
    void replaceRequested(const QString& replacement);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void applyTheme();
    void rebuild();
    void showDownloadOffer();
    void clearBody();

    Thesaurus* m_th = nullptr;
    QString m_word;
    QString m_context;
    QString m_corpus;
    int m_senseCount = 0;   // acepções realmente exibidas, para o título
    int m_hiddenCount = 0;  // acepções recolhidas atrás de "outros sentidos"
    bool m_showAll = false; // usuário pediu pra ver todas

    QLabel* m_title = nullptr;
    QScrollArea* m_scroll = nullptr;
    QWidget* m_body = nullptr;
    QVBoxLayout* m_bodyLay = nullptr;
    QPushButton* m_downloadBtn = nullptr;
    QProgressBar* m_progress = nullptr;
};
