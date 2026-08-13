#pragma once

#include <QColor>
#include <QIcon>
#include <QList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QWidget>

class ImageOverlay;
class QAction;
class QButtonGroup;
class QComboBox;
class QFontComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QTimer;

// Widget compartilhado de edição rich-text — toolbar de formatação (negrito/
// itálico/sublinhado/tachado/fonte/tamanho/alinhamento/espaçamento, todos
// GLOBAIS ao documento inteiro, não por seleção), inserir imagem com overlay
// de redimensionamento, e Modo Foco. Extraído do ConstrutorWindow original
// (mesma lógica, mesmo comportamento) para ser reaproveitado pelo Criador de
// Mundos — o ConstrutorWindow em si continua com sua própria implementação
// interna por ora (extração de baixo risco: código novo, não um refactor do
// que já está em produção).
//
// Quem hospeda o widget decide QUANDO persistir: connecta contentChanged()
// (emitido após 600ms de debounce, mesmo intervalo do Construtor) e lê
// content() pra gravar na store certa.
class WorldContentEditor : public QWidget {
    Q_OBJECT
public:
    explicit WorldContentEditor(QWidget* parent = nullptr);

    // Carrega HTML/plain text, normaliza indentação/entrelinha/margens pelas
    // preferências globais persistidas, sincroniza a toolbar. Substitui todo
    // o conteúdo do documento (equivalente a ConstrutorWindow::loadContentIntoEditor).
    void setContent(const QString& content);
    QString content() const; // toHtml() do documento atual

    void setEditorEnabled(bool enabled);
    void setPlaceholderText(const QString& text);

    // Texto livre no canto direito da toolbar (ex.: "Editado em dd/MM/yyyy") —
    // quem hospeda decide o texto, o widget só exibe.
    void setStatusText(const QString& text);

    bool focusModeEnabled() const { return m_focusModeEnabled; }
    void setFocusModeEnabled(bool enabled);

signals:
    // Debounced (600ms) — dispara depois que o usuário para de digitar.
    void contentChanged();

private slots:
    void applyTheme();
    void applyPageLayout();
    void onTextChanged();
    void onCurrentCharFormatChanged(const QTextCharFormat& fmt);
    void onInsertImage();
    void updateFocusedBlock();

private:
    void buildUi();
    void updateToolbarState(const QTextCharFormat& fmt);
    void applyGlobalAlignment(Qt::Alignment align);
    void applyLineHeight(int percent);
    void applyParaSpaceBefore(int px);
    void applyParaSpaceAfter(int px);
    void updateLineHeightMenuChecks();
    void buildSpacingMenu();
    void applyFocusTextColor();

    bool findImageAt(const QPoint& viewportPos, QTextCursor& imageCursor) const;
    void showOverlayForImage(const QTextCursor& imageCursor);
    void hideOverlay();
    void changeSelectedImageWidth(int delta);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPushButton*   m_boldBtn        = nullptr;
    QPushButton*   m_italicBtn      = nullptr;
    QPushButton*   m_underlineBtn   = nullptr;
    QPushButton*   m_strikeBtn      = nullptr;
    QPushButton*   m_indentBtn      = nullptr;
    QFontComboBox* m_fontCombo      = nullptr;
    QComboBox*     m_sizeCombo      = nullptr;
    QButtonGroup*  m_alignGroup     = nullptr;
    QPushButton*   m_alignLeftBtn   = nullptr;
    QPushButton*   m_alignCenterBtn = nullptr;
    QPushButton*   m_alignRightBtn  = nullptr;
    QPushButton*   m_spacingBtn     = nullptr;
    QPushButton*   m_focusBtn       = nullptr;
    QIcon          m_focusOffIcon;
    QIcon          m_focusOnIcon;
    QPushButton*   m_insertImageBtn = nullptr;
    QLabel*        m_statusLabel    = nullptr;
    QScrollArea*   m_pageScroll     = nullptr;
    QWidget*       m_pageColumn     = nullptr;
    QTextEdit*     m_contentEdit    = nullptr;
    ImageOverlay*  m_imageOverlay   = nullptr;
    QTextCursor    m_selectedImageCursor;

    bool    m_focusModeEnabled       = false;
    bool    m_firstLineIndentEnabled = false;
    QColor  m_baseTextColor;

    QLabel*         m_paraBeforeLabel   = nullptr;
    QLabel*         m_paraAfterLabel    = nullptr;
    QList<QAction*> m_lineHeightActions;
    int             m_lineHeightPercent = 115;
    int             m_paraSpaceBefore   = 0;
    int             m_paraSpaceAfter    = 8;

    bool    m_updatingFmt = false;
    QTimer* m_saveTimer   = nullptr;
};
