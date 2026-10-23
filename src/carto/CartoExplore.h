#pragma once

// Explorar: os documentos do projeto presos aos territórios do mapa.
// Cada território tem um GRUPO (o mesmo dos Grupos da LeftBar: um documento de
// qualquer gaveta mora em no máximo um grupo) e um LUGAR no Criador de Mundos
// (com os documentos e as pastas dele). Clicar no território abre o cartão com
// as duas listas; clicar num documento abre o leitor 3×4 (ler, editar, ampliar);
// as conexões entre Lugares viram arcos no mapa e abrem o lado a lado.
// O mapa não conhece o editor principal: o MainWindow passa uma ponte.

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QVector>

#include <functional>
#include <vector>

class ProjectModel;
class ElementsStore;
class TerritorioStore;
class DocCache;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QTimer;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace carto {

class Canvas;
struct RealmDef;
struct Doc;

// o que o mapa precisa do projeto (o MainWindow preenche)
struct ProjectLink {
    ProjectModel* model = nullptr;
    ElementsStore* elements = nullptr;
    TerritorioStore* places = nullptr;
    DocCache* cache = nullptr;
    QString root;
    std::function<void()> syncEditor;                                  // o editor principal descarrega no cache
    std::function<void(const QString& itemId)> itemEdited;             // doc de gaveta mudou por fora: recarrega o editor se for ele
    std::function<void(const QString& itemId)> openItem;               // abre no editor (ficha abre a janela da ficha)
    std::function<void(const QString& placeId, const QString& nodeId)> openPlace;   // Criador de Mundos
    std::function<void()> save;                                        // grava o projeto
    bool ok() const { return model && places; }
};

class Explorer : public QObject {
    Q_OBJECT
public:
    Explorer(QWidget* host, Canvas* canvas, Doc* doc);

    void setLink(const ProjectLink& link);
    void setActive(bool on);                        // a ferramenta ligou ou desligou
    bool active() const { return m_active; }
    void clicked(int realm, const QString& linkId);
    void hovered(int realm, const QString& linkId, QPoint at);
    bool escape();                                  // fecha a camada de cima
    void close();
    void mapReplaced();                             // outro mapa, gerar, desfazer
    // cada território é um grupo do projeto: o que não tem grupo ganha (o de mesmo nome ou um novo);
    // com o estado de antes, nome e cor trocados no mapa passam pro grupo
    void syncGroups(const std::vector<RealmDef>* before);
    void place();                                   // reposiciona depois de redimensionar
    void applyTheme();
    bool owns(const QWidget* w) const;              // o foco está num painel do Explorar
    QString hint() const;

    std::function<void()> mapEdited;                // ligou grupo ou lugar a um território: o mapa precisa ser salvo
    std::function<void(const QString&)> flash;      // aviso na barra de dica
    std::function<int()> cartelaTop;                // o cartão não desce por cima da Cartela (−1 = sem Cartela)

    // um documento da lista do território
    struct XDoc {
        enum Kind { Drawer, PlaceNode, PlaceAbout } kind = Drawer;
        QString id;            // item da gaveta, nó do lugar ou o próprio lugar (Sobre)
        QString drawerKey, drawerTitle, folder, folderId;
        QString title, elementId;
        QColor color;
        bool sheet = false;
        bool valid() const { return !id.isEmpty(); }
        bool same(const XDoc& o) const { return kind == o.kind && id == o.id; }
    };
    struct XLink { QString id; int a = -1, b = -1; QString label; QColor color; };

protected:
    bool eventFilter(QObject* o, QEvent* e) override;   // arrastar os painéis pelo cabeçalho

private:
    enum class CardMode { List, New, Add };

    // dados
    int realmCount() const;
    QString realmName(int r) const;
    QColor realmColor(int r) const;
    QString groupOf(int r, bool create);
    QString placeOf(int r, bool create);
    QVector<XDoc> drawerDocs(int r);
    QVector<XDoc> placeDocs(int r);
    QVector<XLink> linksOf(int r, int* offMap = nullptr);
    int realmOfPlace(const QString& placeId);
    QString rawHtml(const XDoc& d) const;
    QString linkHtml(const QString& linkId) const;
    QString linkLabel(const QString& linkId) const;
    void pushView();

    // telas
    void buildCard();
    void buildReader();
    void buildDuo();
    void renderCard();
    void renderReader();
    void renderLinksBox();
    void renderDuo();
    void openDoc(const XDoc& d);
    void openDuo(const QString& linkId, int from);
    void loadBody(QTextEdit* ed, const XDoc& d, bool editing, bool compact);
    void loadHtml(QTextEdit* ed, const QString& html, bool editing, bool compact);
    void fitBody(QTextEdit* ed, bool compact);
    void setEditing(bool on);
    void createDoc();
    void insertImage(QTextEdit* ed);
    QPushButton* row(QWidget* parent, const QString& title, const QString& right, const QColor& dot, bool selected, bool sheet, const std::function<void()>& onClick);
    QLabel* caption(QWidget* parent, const QString& text, const QColor& dot = QColor());

    // gravar o que se escreve
    void bodyEdited(QTextEdit* ed, const XDoc& d);
    void flushSave();

    QWidget* m_host;
    Canvas* m_canvas;
    Doc* m_doc;
    ProjectLink m_link;
    bool m_active = false;
    int m_sel = -1, m_hov = -1;

    // cartão do território
    QFrame* m_card = nullptr;
    QLabel *m_cardSwatch = nullptr, *m_cardTitle = nullptr, *m_cardSub = nullptr;
    QScrollArea* m_cardScroll = nullptr;
    QWidget* m_cardBody = nullptr;
    QWidget* m_cardFoot = nullptr;
    CardMode m_mode = CardMode::List;
    QString m_newWhere = QStringLiteral("drawer"), m_newDrawer, m_newFolder, m_newTitleText, m_query;
    QPointer<QLineEdit> m_newTitle, m_queryEdit;
    QPointer<QComboBox> m_newCombo;
    QPoint m_cardAt{ -1, -1 }, m_readerAt{ -1, -1 }, m_duoAt{ -1, -1 };

    // leitor
    QFrame* m_reader = nullptr;
    QLabel *m_rChip = nullptr, *m_rFoot = nullptr, *m_rPhoto = nullptr, *m_rAlias = nullptr, *m_rRole = nullptr;
    QWidget *m_rEl = nullptr, *m_rTools = nullptr;
    QFrame* m_rLinks = nullptr;
    QPushButton *m_rEdit = nullptr, *m_rUngroup = nullptr;
    QToolButton *m_rWide = nullptr, *m_rLinksBtn = nullptr, *m_rOpen = nullptr, *m_rClose = nullptr;
    QLineEdit* m_rTitle = nullptr;
    QTextEdit* m_rBody = nullptr;
    XDoc m_cur;
    bool m_edit = false, m_wide = false, m_links = false, m_pickLink = false;

    // lado a lado
    QFrame* m_duo = nullptr;
    QLabel* m_dTitle = nullptr;
    struct Col { QLabel* chip = nullptr; QComboBox* pick = nullptr; QToolButton* pen = nullptr; QLabel* title = nullptr; QTextEdit* body = nullptr; XDoc doc; };
    Col m_col[3];   // 0 = território A · 1 = a conexão · 2 = território B
    QString m_duoLink;
    int m_duoA = -1, m_duoB = -1, m_duoEdit = -1;

    // hover
    QLabel* m_chip = nullptr;

    // escrita (com espera curta, como o editor)
    QTimer* m_saveTimer = nullptr;
    QPointer<QTextEdit> m_pendingEd;
    XDoc m_pendingDoc;
    QString m_pendingLink;
    bool m_loading = false;

    // as cores do tema
    QColor m_ink, m_muted, m_line, m_accent, m_panel, m_panel2;
};

} // namespace carto
