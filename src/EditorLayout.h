#pragma once

#include <QObject>

namespace EditorLayout {

// Configuração de layout da "página" de escrita — largura e margens internas.
// Independente do tema (que cuida só de cor/aparência). Persistido em QSettings
// (global, não por projeto).
class Manager : public QObject {
    Q_OBJECT
public:
    static Manager* instance();

    int pageWidth() const         { return m_pageWidth; }
    int pageHeight() const        { return m_pageHeight; } // 0 = automático (preenche a viewport)
    int horizontalMargin() const  { return m_horizontalMargin; }
    int verticalMargin() const    { return m_verticalMargin; }

    void setPageWidth(int px);
    void setPageHeight(int px);
    void setHorizontalMargin(int px);
    void setVerticalMargin(int px);

    // Limites usados pela UI (spinboxes).
    static int minPageWidth()        { return 400; }
    static int maxPageWidth()        { return 2400; }
    static int minPageHeight()       { return 0; }   // 0 = automático
    static int maxPageHeight()       { return 2400; }
    static int minHorizontalMargin() { return 0; }
    static int maxHorizontalMargin() { return 200; }
    static int minVerticalMargin()   { return 0; }
    static int maxVerticalMargin()   { return 200; }

signals:
    void layoutChanged();

private:
    Manager();
    void load();
    void save() const;

    int m_pageWidth = 960;
    int m_pageHeight = 0; // 0 = automático
    int m_horizontalMargin = 40;
    int m_verticalMargin = 24;
};

// Free functions para callsites curtos.
int pageWidth();
int pageHeight();
int horizontalMargin();
int verticalMargin();

} // namespace EditorLayout
