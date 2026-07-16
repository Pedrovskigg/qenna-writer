#include "BondTypes.h"

#include <QCoreApplication>

namespace BondTypes {

QList<QString> presetColors() {
    return {
        QStringLiteral("#e05555"), QStringLiteral("#f5a623"),
        QStringLiteral("#ffca28"), QStringLiteral("#2cc07a"),
        QStringLiteral("#4a9eff"), QStringLiteral("#bb86fc"),
        QStringLiteral("#f06292"), QStringLiteral("#90a4ae"),
    };
}

QString defaultColor() {
    return QStringLiteral("#4a9eff");
}

QList<BondTypeGroup> presetGroups() {
    QList<BondTypeGroup> g;
    {
        BondTypeGroup fam;
        fam.name = QT_TRANSLATE_NOOP("BondPopup", "Família");
        fam.options = {
            { QT_TRANSLATE_NOOP("BondPopup", "Pai"),              QT_TRANSLATE_NOOP("BondPopup", "Mãe") },
            { QT_TRANSLATE_NOOP("BondPopup", "Filho"),            QT_TRANSLATE_NOOP("BondPopup", "Filha") },
            { QT_TRANSLATE_NOOP("BondPopup", "Irmão"),            QT_TRANSLATE_NOOP("BondPopup", "Irmã") },
            { QT_TRANSLATE_NOOP("BondPopup", "Avô"),              QT_TRANSLATE_NOOP("BondPopup", "Avó") },
            { QT_TRANSLATE_NOOP("BondPopup", "Tio"),              QT_TRANSLATE_NOOP("BondPopup", "Tia") },
            { QT_TRANSLATE_NOOP("BondPopup", "Primo"),            QT_TRANSLATE_NOOP("BondPopup", "Prima") },
            { QT_TRANSLATE_NOOP("BondPopup", "Cônjuge"),          QT_TRANSLATE_NOOP("BondPopup", "Cônjuge") },
            { QT_TRANSLATE_NOOP("BondPopup", "Divorciados"),      QT_TRANSLATE_NOOP("BondPopup", "Divorciadas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Padrinho"),         QT_TRANSLATE_NOOP("BondPopup", "Madrinha") },
            { QT_TRANSLATE_NOOP("BondPopup", "Parentes distantes"), QT_TRANSLATE_NOOP("BondPopup", "Parentes distantes") },
        };
        g.append(fam);
    }
    {
        BondTypeGroup rom;
        rom.name = QT_TRANSLATE_NOOP("BondPopup", "Romântico");
        rom.options = {
            { QT_TRANSLATE_NOOP("BondPopup", "Namorados"),         QT_TRANSLATE_NOOP("BondPopup", "Namoradas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Ex-namorados"),      QT_TRANSLATE_NOOP("BondPopup", "Ex-namoradas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Interesse amoroso"), QT_TRANSLATE_NOOP("BondPopup", "Interesse amoroso") },
            { QT_TRANSLATE_NOOP("BondPopup", "Casados"),           QT_TRANSLATE_NOOP("BondPopup", "Casadas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Separados"),         QT_TRANSLATE_NOOP("BondPopup", "Separadas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Ficantes"),          QT_TRANSLATE_NOOP("BondPopup", "Ficantes") },
        };
        g.append(rom);
    }
    {
        BondTypeGroup soc;
        soc.name = QT_TRANSLATE_NOOP("BondPopup", "Social");
        soc.options = {
            { QT_TRANSLATE_NOOP("BondPopup", "Amigos íntimos"),    QT_TRANSLATE_NOOP("BondPopup", "Amigas íntimas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Velhos amigos"),     QT_TRANSLATE_NOOP("BondPopup", "Velhas amigas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Conhecidos"),        QT_TRANSLATE_NOOP("BondPopup", "Conhecidas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Colegas"),           QT_TRANSLATE_NOOP("BondPopup", "Colegas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Parceiros"),         QT_TRANSLATE_NOOP("BondPopup", "Parceiras") },
        };
        g.append(soc);
    }
    {
        BondTypeGroup con;
        con.name = QT_TRANSLATE_NOOP("BondPopup", "Conflito");
        con.options = {
            { QT_TRANSLATE_NOOP("BondPopup", "Inimigos"),          QT_TRANSLATE_NOOP("BondPopup", "Inimigas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Rivais"),            QT_TRANSLATE_NOOP("BondPopup", "Rivais") },
            { QT_TRANSLATE_NOOP("BondPopup", "Desconfiança mútua"), QT_TRANSLATE_NOOP("BondPopup", "Desconfiança mútua") },
            { QT_TRANSLATE_NOOP("BondPopup", "Credor/Devedor"),    QT_TRANSLATE_NOOP("BondPopup", "Credora/Devedora") },
            { QT_TRANSLATE_NOOP("BondPopup", "Traição"),           QT_TRANSLATE_NOOP("BondPopup", "Traição") },
        };
        g.append(con);
    }
    {
        BondTypeGroup pwr;
        pwr.name = QT_TRANSLATE_NOOP("BondPopup", "Poder");
        pwr.options = {
            { QT_TRANSLATE_NOOP("BondPopup", "Mentor / Aprendiz"),    QT_TRANSLATE_NOOP("BondPopup", "Mentora / Aprendiz") },
            { QT_TRANSLATE_NOOP("BondPopup", "Líder / Subordinado"),  QT_TRANSLATE_NOOP("BondPopup", "Líder / Subordinada") },
            { QT_TRANSLATE_NOOP("BondPopup", "Aliados"),              QT_TRANSLATE_NOOP("BondPopup", "Aliadas") },
            { QT_TRANSLATE_NOOP("BondPopup", "Protetor / Protegido"), QT_TRANSLATE_NOOP("BondPopup", "Protetora / Protegida") },
            { QT_TRANSLATE_NOOP("BondPopup", "Mestre / Discípulo"),   QT_TRANSLATE_NOOP("BondPopup", "Mestra / Discípula") },
        };
        g.append(pwr);
    }
    return g;
}

QString displayName(const QString& raw)
{
    if (raw.isEmpty()) return raw;
    return QCoreApplication::translate("BondPopup", raw.toUtf8().constData());
}

} // namespace BondTypes
