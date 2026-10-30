#pragma once
// As tags livres do documento aberto (29/10/2026), no canto direito do DocHeader,
// ao lado da etiqueta de grupo: "# guerra · monarquia" (ou "# Tags" apagado, sem
// nenhuma). Clicar abre uma janelinha com o campo de tags; cada tag posta ou
// tirada já vale (picked), sem botão de salvar.

#include <QObject>
#include <QStringList>

#include <functional>

class QAbstractButton;
class QFrame;
class TagField;
class QWidget;

class DocTagsPicker : public QObject {
    Q_OBJECT
public:
    explicit DocTagsPicker(QWidget* parent);
    QAbstractButton* tag() const { return m_tag; }
    void setTags(const QStringList& tags);
    QStringList tags() const { return m_tags; }

    std::function<void()> changed;                       // a etiqueta mudou de texto e de largura
    std::function<void(const QStringList&)> picked;      // a pessoa pôs ou tirou uma tag

protected:
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    void open();
    void sync();
    void apply(const QStringList& tags);
    QAbstractButton* m_tag = nullptr;
    QFrame* m_popup = nullptr;
    TagField* m_field = nullptr;
    QStringList m_tags;
};
