#pragma once
// Silhuetas pra personagem sem foto (30/10, ideia do Pe a partir de um print
// da Marley). 38 bustos recortados de fotos do Unsplash (19 masculinos, 19
// femininos) em :/silhouettes/<id>.png, cada um com as partes nos canais:
// R = parte (60 cabelo · 130 pele · 200 roupa · 255 óculos), G = filete onde
// uma coisa passa na frente da outra, A = silhueta. Escolhida, a silhueta é
// pintada numa cor e vira a foto do personagem (data URL), como uma foto
// carregada: aparece em todo lugar que já mostra foto.

#include "SheetDialog.h"

#include <QImage>
#include <QString>
#include <QStringList>

class QButtonGroup;
class QGridLayout;
class SheetChoice;

namespace Silhouettes {
QStringList ids(bool male);
constexpr int kHues = 10;
int hueFor(const QString& seed);                    // tom estável pelo nome do personagem
QImage render(const QString& id, int hue, int px);  // a silhueta pintada no tom (0..kHues-1)
}

class SilhouettePickerDialog : public SheetDialog {
    Q_OBJECT
public:
    // male: aba que abre (a forma de tratamento do personagem, se marcada).
    // seed: o nome, pra cor e silhueta iniciais.
    SilhouettePickerDialog(QWidget* parent, bool male, const QString& seed);
    // Abre o seletor; devolve a data URL da foto ou vazio se cancelar.
    static QString pick(QWidget* parent, bool male, const QString& seed);
    QString resultDataUrl() const;

private:
    void rebuildGrid();
    void refreshSwatches();
    bool m_male = true;
    int m_hue = 0;
    QString m_id;
    SheetChoice* m_sex = nullptr;
    QGridLayout* m_grid = nullptr;
    QButtonGroup* m_cells = nullptr;
    QButtonGroup* m_swatches = nullptr;
};
