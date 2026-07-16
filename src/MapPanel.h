#pragma once

#include <QFrame>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPoint>
#include <QRect>

class QLabel;
class QLineEdit;
class QComboBox;
class QFrame;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidget;
class QListWidgetItem;
class MapView;
class MapPinsStore;
class ProjectModel;

// Painel flutuante próprio do Mapa-múndi (≈500x700). Aberto por um botão
// discreto no cabeçalho do Pensário. Arrastável pelo header; adota o tema.
class MapPanel : public QFrame {
    Q_OBJECT
public:
    explicit MapPanel(MapPinsStore* pins = nullptr, ProjectModel* model = nullptr,
                      QWidget* parent = nullptr);

    void togglePanel();
    void openPanel();
    void closePanel();
    bool isPanelOpen() const;
    void setTopInset(int px) { m_topInset = px; }

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void applyTheme();
    void showFeatureInfo(QString title, QString subtitle, QStringList details, QString flagIso);
    void toggleNavTree();
    void onNavItemExpanded(QTreeWidgetItem* item);
    void onNavItemClicked(QTreeWidgetItem* item, int column);
    void searchAndGo();
    void updateSuggestions(const QString& text); // autocomplete da busca
    void acceptSuggestion(QListWidgetItem* item);
    void positionSuggest();
    void onPinRequested(double lon, double lat);
    void onPinClicked(QString id);
    void savePinFromPopup();
    void deletePinFromPopup();

private:
    enum class ResizeEdge { None, Left, Right, Top, Bottom, TL, TR, BL, BR };
    void centerInParent();
    void layoutResizeHandles();
    void toggleMaximize();
    void buildInfoCard();
    void positionInfoCard();
    void buildPinPopup();
    void openPinPopup();
    void fillPinLink(const QString& selectedId); // popula o seletor de vínculo
    void buildIndex();          // país -> estado -> cidades (lazy)
    void populateCountries();
    void positionNavTree();

    MapView* m_map = nullptr;
    QWidget* m_header = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_maxBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;

    // Redimensionamento por alças nas bordas + maximizar.
    QWidget* m_hL = nullptr; QWidget* m_hR = nullptr;
    QWidget* m_hT = nullptr; QWidget* m_hB = nullptr;
    QWidget* m_hTL = nullptr; QWidget* m_hTR = nullptr;
    QWidget* m_hBL = nullptr; QWidget* m_hBR = nullptr;
    bool m_resizing = false;
    ResizeEdge m_activeEdge = ResizeEdge::None;
    QRect m_resizeStartGeom;
    QPoint m_resizeStartMouse;
    bool m_maximized = false;
    QRect m_normalGeom;

    QFrame* m_infoCard = nullptr;
    QLabel* m_infoFlag = nullptr;
    QLabel* m_infoTitle = nullptr;
    QLabel* m_infoSub = nullptr;
    QLabel* m_infoBody = nullptr;

    MapPinsStore* m_pins = nullptr;
    ProjectModel* m_model = nullptr;

    QToolButton* m_navBtn = nullptr;
    QToolButton* m_pinBtn = nullptr;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_navTree = nullptr;
    QListWidget* m_suggest = nullptr; // popup de sugestões da busca

    // Popup de criar/editar pin.
    QFrame* m_pinPopup = nullptr;
    QLabel* m_pinTitle = nullptr;
    QLineEdit* m_pinLabelEdit = nullptr;
    QLineEdit* m_pinNoteEdit = nullptr;
    QComboBox* m_pinLink = nullptr;
    QToolButton* m_pinDeleteBtn = nullptr;
    QString m_editPinId;
    double m_pinLon = 0.0;
    double m_pinLat = 0.0;
    QMap<QString, QMap<QString, QList<int>>> m_index; // cc -> estado -> índices de places
    QHash<QString, QString> m_countryNames;           // cc -> nome do país
    bool m_indexBuilt = false;
    int m_topInset = 0;
    bool m_positioned = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
};
