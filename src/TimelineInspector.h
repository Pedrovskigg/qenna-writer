#pragma once
// Painel lateral da Timeline nova: abre quando o usuário clica num evento e
// fecha no clique no vazio / Esc / ×. Só mostra; toda ação sobe por sinal
// pro TimelinePanel, que é quem mexe no modelo.

#include "TimelineTracksTypes.h"

#include <QWidget>

class QLineEdit;
class QLabel;
class QScrollArea;
class QVBoxLayout;

class TimelineInspector : public QWidget {
    Q_OBJECT
public:
    explicit TimelineInspector(QWidget* parent = nullptr);

    // Reconstrói o conteúdo pro evento `id` (vazio = limpa).
    void showFor(const Tracks::Data& data, const QString& id);
    QString eventId() const { return m_id; }

signals:
    void closeRequested();
    void openInEditorRequested(const QString& id);
    void editRequested(const QString& id);
    void exportRequested(const QString& id);
    void fillMarkerRequested(const QString& id, const QString& marker);
    void undoMoveRequested(const QString& id);
    void selectRequested(const QString& id);
    void characterClicked(const QString& charId);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void clear();
    void applyTheme();

    QString       m_id;
    QScrollArea*  m_scroll = nullptr;
    QWidget*      m_body = nullptr;
    QVBoxLayout*  m_lay = nullptr;
    class QToolButton* m_close = nullptr;
};
