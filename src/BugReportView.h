#pragma once
// A seção "Reportar um bug" das Configurações (dados em BugReports.h).
// Usuário: Novo relato · Todos os relatos (público) · Meus bugs. Autor: a Central de bugs,
// com filtros por estágio, resposta e "resolvido na versão".

#include <QWidget>

class QStackedWidget;
class QVBoxLayout;
class QPushButton;
class QHBoxLayout;
struct BugReport;

class BugReportView : public QWidget {
    Q_OBJECT
public:
    explicit BugReportView(QWidget* parent = nullptr);

    // A seção ficou visível: busca de novo e apaga o pontinho de novidade.
    void activate();

signals:
    void newsChanged(bool news);

private:
    void build();
    void applyTheme();
    void showTab(int tab);
    QWidget* buildForm();
    void refreshLists();
    QWidget* bugCard(const BugReport& b, bool author, QWidget* parent, bool showName = false, bool mine = false);
    QList<QPushButton*> makeFilters(QWidget* parent, QHBoxLayout* row, QString* target);
    void paintFilters(const QList<QPushButton*>& btns, const QList<BugReport>& list, const QString& current);
    QWidget* pill(const QString& stage, const QString& version, QWidget* parent);

    bool m_author = false;
    int m_tab = 1;          // 0 novo, 1 todos os relatos, 2 meus
    QString m_filter = QStringLiteral("all");         // central do autor
    QString m_pubFilter = QStringLiteral("all");      // "Todos os relatos"
    QList<QPushButton*> m_pubFilters;
    QVBoxLayout* m_publicList = nullptr;
    QList<QPushButton*> m_tabs;
    QList<QPushButton*> m_filters;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_form = nullptr;
    QVBoxLayout* m_mineList = nullptr;
    QVBoxLayout* m_allList = nullptr;
    QWidget* m_userBox = nullptr;
    QWidget* m_authorBox = nullptr;
};
