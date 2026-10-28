#pragma once
// O grupo do documento novo (23/10, "Ligar os pontos"): uma etiqueta pequena no
// topo da folha ("● Trenoria ▾") e a lista dos grupos, que abre DENTRO da folha
// (janela suspensa sai transparente no Windows sobre a folha translúcida).
// Os territórios do mapa são grupos: o documento escolhido aqui aparece no
// território quando a pessoa clica nele no Explorar do mapa.

#include <QObject>
#include <QString>

#include <functional>

class ProjectModel;
class QAbstractButton;
class QFrame;
class QVBoxLayout;
class QWidget;

class GroupPicker : public QObject {
    Q_OBJECT
public:
    // sem grupo nenhum no projeto, a etiqueta nem aparece (available() = false)
    // popup = a lista abre numa janelinha embaixo da etiqueta (DocHeader); senão, dentro da folha
    GroupPicker(const ProjectModel* model, QWidget* parent, bool popup = false);
    bool available() const;   // o projeto tem algum grupo
    QAbstractButton* tag() const { return m_tag; }      // vai no cabeçalho da folha
    QFrame* list() const { return m_list; }             // vai no corpo da folha (começa fechada)
    QString groupId() const { return m_id; }
    void setGroupId(const QString& id);
    void setOnPhoto(bool on);                           // a etiqueta por cima da foto do pôster
    void setMaxTagWidth(int w);                         // o espaço que sobra no cabeçalho
    std::function<void()> resized;                      // a lista abriu ou fechou: a folha se ajusta
    std::function<void()> changed;                      // a etiqueta mudou de texto e de largura
    std::function<void(const QString&)> picked;         // a pessoa escolheu um grupo na lista

private:
    void toggle();
    void sync();
    void rebuild();
    const ProjectModel* m_model;
    QAbstractButton* m_tag = nullptr;
    QFrame* m_list = nullptr;
    QFrame* m_popup = nullptr;
    QVBoxLayout* m_rows = nullptr;
    QString m_id;
    int m_maxW = 150;
};
