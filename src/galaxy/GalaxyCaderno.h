#pragma once

// O Caderno: a galáxia como livro de consulta. Árvore à esquerda, página no meio e, escrevendo, a ficha técnica
// do lugar ao lado do documento (o que era o Atlas Lab). Cobre o mapa inteiro enquanto aberto.

#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace galaxy {

class Canvas;
class Doc;
class Panel;
struct Body;
struct DocLink;
struct Ref;
struct System;

class Caderno : public QWidget {
    Q_OBJECT
public:
    Caderno(Panel* panel, Canvas* canvas, Doc* doc);
    void open(const QString& key, const QString& fromTool);
    void closeIt();
    QString returnTool() const { return m_from; }
    void refresh() { render(); }
    bool escape();    // sobe um nível; false quando não tem mais pra onde subir
    void applyTheme();

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    void render();
    void renderTree(QVBoxLayout* box);
    void renderPage(QVBoxLayout* pg);
    void renderFicha(QVBoxLayout* box, const QString& k);
    QWidget* hero(const QString& k, const Ref& o);
    QWidget* crumbs(const QString& k, bool withLast);
    QPushButton* docCard(DocLink* d, const QString& from);
    QWidget* ringedBall(const Body& p, int px);
    QStringList path(const QString& k) const;
    QString parentKey(const QString& k) const;
    QString label(const QString& k) const;
    bool valid(const QString& k) const;
    QString catOf(const Body& p) const;
    bool pMatch(const System& s, const Body& p) const;
    bool sysVisible(const System& s, const QString& q) const;
    std::vector<System*> systemsOf(const QString& k) const;
    void go(const QString& k);
    void openDoc(DocLink* d);
    void leaveDoc();
    void treeRow(QVBoxLayout* box, const QString& k, int depth, bool hasKids, QWidget* icon, const QString& sub, bool region, const std::function<void()>& kids, bool forceOpen);

    Panel* m_panel;
    Canvas* m_canvas;
    Doc* m_doc;
    QString m_key = QStringLiteral("GAL"), m_from = QStringLiteral("pan"), m_q, m_filter = QStringLiteral("tudo");
    DocLink* m_cdDoc = nullptr;
    bool m_pick = false, m_ficha = true;
    QSet<QString> m_open;
    QTimer m_qTimer;
    QPointer<QLineEdit> m_qEdit;
    QPointer<QScrollArea> m_treeScroll;
    QWidget* m_content = nullptr;
};

} // namespace galaxy
