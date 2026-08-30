#pragma once

#include "Exporter.h"

#include <QWidget>

class QLabel;
class QToolButton;
class QSlider;
class QTextDocument;
class QHideEvent;
class ProjectModel;
class ReaderPreviewDeviceView;

// Preview de leitura do manuscrito inteiro (todos os capítulos concatenados,
// como um livro só), em moldura de dispositivo paginada (boa pra
// screenshots). Painel persistente — uma instância por MainWindow (ver
// MainWindow::ensureReaderPreviewPanel), não recriado a cada abertura como o
// ExportPanel.
class ReaderPreviewPanel : public QWidget {
    Q_OBJECT
public:
    ReaderPreviewPanel(ProjectModel* model, const QString& projectRoot,
                        const Exporter::DocStyle& style, QWidget* parent = nullptr);
    ~ReaderPreviewPanel() override;

    // Troca o manuscrito exibido, restaurando a última posição de leitura
    // salva (se houver). Rebuilda os documentos sob demanda.
    void setManuscript(const QString& manuscriptId);

protected:
    void hideEvent(QHideEvent* event) override;

private:
    void buildUi();
    void loadSettings();
    void rebuildDocuments();
    void applyPositionFraction(double frac);
    void savePositionForCurrentManuscript();
    void updateTitleLabel();
    void updateChromeStyle();

    ProjectModel* m_model;
    Exporter m_exporter;
    QString m_manuscriptId;

    QTextDocument* m_deviceDoc = nullptr;

    double m_positionFraction = 0.0;
    bool m_darkMode = false;
    bool m_grayscale = false;
    bool m_syncingSlider = false;

    QLabel* m_titleLabel = nullptr;
    QToolButton* m_darkModeBtn = nullptr;
    QToolButton* m_grayscaleBtn = nullptr;
    QToolButton* m_helpBtn = nullptr;
    QToolButton* m_closeBtn = nullptr;
    ReaderPreviewDeviceView* m_deviceView = nullptr;
    QSlider* m_positionSlider = nullptr;
};
