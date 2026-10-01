#pragma once

#include <QHash>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class QListWidget;
class SpellEditor;

// Autocompletar do roteiro (mesmo molde do MentionPopup: lista presa à janela,
// logo abaixo do cursor, setas/Enter/Tab/Esc). Só age com o cursor no fim da
// linha de um manuscrito de roteiro:
//  - Personagem: "CI" → CIDA (elenco do projeto, com o nome completo do lado,
//    e quem já falou neste documento);
//  - Cena: "I" → INT. / EXT.; depois do prefixo, os locais já usados no
//    roteiro inteiro; depois do " - ", a hora do dia.
class ScreenplayCompleter : public QObject {
    Q_OBJECT
public:
    ScreenplayCompleter(QWidget* ownerWindow, QObject* parent = nullptr);

    void attach(SpellEditor* editor);

    // Deixa (MAIÚSCULAS) → nome completo do personagem ("" quando a deixa já é o nome).
    void setCast(const QHash<QString, QString>& cueToName) { m_cast = cueToName; }
    // Local (MAIÚSCULAS, sem prefixo nem hora) → em quantas cenas aparece,
    // somando os outros capítulos. O documento aberto é lido ao vivo.
    void setLocations(const QMap<QString, int>& locations) { m_locations = locations; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Candidate { QString insert; QString display; QString hint; };

    void update();
    void showList(const QList<Candidate>& items);
    void hide();
    void accept();
    void moveSel(int delta);

    QWidget* m_owner;
    SpellEditor* m_editor = nullptr;
    QListWidget* m_list = nullptr;
    QHash<QString, QString> m_cast;
    QMap<QString, int> m_locations;
    // Trecho da linha que a escolha substitui: do início dele até o cursor.
    int m_replaceFrom = -1;
    bool m_inserting = false;
};
