#pragma once

#include <QWidget>
#include <QString>

class LousaScene;
class LousaView;
class QLabel;
class QPushButton;
class QToolButton;

class LousaPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LousaPanel(QWidget* parent = nullptr);

    void setProjectRoot(const QString& root);
    void refreshEmptyState();

signals:
    void closeRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPickColor();
    void applyTheme();

private:
    void buildUi();
    void save() const;
    void load();
    void updateColorBtn();

    LousaScene*  m_scene        = nullptr;
    LousaView*   m_view         = nullptr;
    QWidget*     m_toolbar      = nullptr;
    QToolButton* m_colorBtn     = nullptr;
    QLabel*      m_emptyLabel   = nullptr;
    QLabel*      m_zoomLabel    = nullptr;

    QString m_projectRoot;
};
