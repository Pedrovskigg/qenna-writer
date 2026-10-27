#pragma once
// O narrador do capítulo aberto, no DocHeader (27/10): a etiqueta do "POV à
// vista" ("(rosto) Klara ▾") e a lista dos personagens, que abre numa janelinha
// embaixo dela — irmã da etiqueta de grupo (GroupPicker), que mora no mesmo
// canto nos documentos de gaveta. Também vai no topo da folha de capítulo, com
// a lista aberta DENTRO da folha (janela suspensa sai transparente no Windows
// sobre a folha translúcida). Não sabe de ProjectModel: quem monta as opções é
// o MainWindow, com as regras da gaveta (sugerido, cor da linha).

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

class QAbstractButton;
class QFrame;
class QVBoxLayout;
class QWidget;

class PovPicker : public QObject {
    Q_OBJECT
public:
    struct Option {
        QString id;          // Element id; vazio = sem narrador
        QString name;
        QString image;       // data-URL da foto (vazio = inicial na cor)
        QColor color;
        bool suggested = false;
    };

    // popup = a lista abre numa janelinha embaixo da etiqueta (DocHeader); senão, dentro da folha
    explicit PovPicker(QWidget* parent, bool popup = true);
    QAbstractButton* tag() const { return m_tag; }
    QFrame* list() const { return m_list; }             // vai no corpo da folha (começa fechada)
    QString currentId() const { return m_id; }
    void setCurrent(const Option& current);             // id vazio = "Quem narra?"
    std::function<QList<Option>()> options;             // a lista, montada na hora de abrir
    std::function<void()> changed;                      // a etiqueta mudou de texto e de largura
    std::function<void(const QString&)> picked;         // a pessoa escolheu um narrador
    std::function<void()> resized;                      // a lista abriu ou fechou: a folha se ajusta

private:
    void toggle();
    void rebuild();
    QAbstractButton* m_tag = nullptr;
    QFrame* m_popup = nullptr;
    QFrame* m_list = nullptr;
    QVBoxLayout* m_rows = nullptr;
    QString m_id;
};
