#include "BondTypes.h"

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
        fam.name = QStringLiteral("Família");
        fam.options = {
            { QStringLiteral("Pai"),              QStringLiteral("Mãe") },
            { QStringLiteral("Filho"),            QStringLiteral("Filha") },
            { QStringLiteral("Irmão"),            QStringLiteral("Irmã") },
            { QStringLiteral("Avô"),              QStringLiteral("Avó") },
            { QStringLiteral("Tio"),              QStringLiteral("Tia") },
            { QStringLiteral("Primo"),            QStringLiteral("Prima") },
            { QStringLiteral("Cônjuge"),          QStringLiteral("Cônjuge") },
            { QStringLiteral("Divorciados"),      QStringLiteral("Divorciadas") },
            { QStringLiteral("Padrinho"),         QStringLiteral("Madrinha") },
            { QStringLiteral("Parentes distantes"), QStringLiteral("Parentes distantes") },
        };
        g.append(fam);
    }
    {
        BondTypeGroup rom;
        rom.name = QStringLiteral("Romântico");
        rom.options = {
            { QStringLiteral("Namorados"),         QStringLiteral("Namoradas") },
            { QStringLiteral("Ex-namorados"),      QStringLiteral("Ex-namoradas") },
            { QStringLiteral("Interesse amoroso"), QStringLiteral("Interesse amoroso") },
            { QStringLiteral("Casados"),           QStringLiteral("Casadas") },
            { QStringLiteral("Separados"),         QStringLiteral("Separadas") },
            { QStringLiteral("Ficantes"),          QStringLiteral("Ficantes") },
        };
        g.append(rom);
    }
    {
        BondTypeGroup soc;
        soc.name = QStringLiteral("Social");
        soc.options = {
            { QStringLiteral("Amigos íntimos"),    QStringLiteral("Amigas íntimas") },
            { QStringLiteral("Velhos amigos"),     QStringLiteral("Velhas amigas") },
            { QStringLiteral("Conhecidos"),        QStringLiteral("Conhecidas") },
            { QStringLiteral("Colegas"),           QStringLiteral("Colegas") },
            { QStringLiteral("Parceiros"),         QStringLiteral("Parceiras") },
        };
        g.append(soc);
    }
    {
        BondTypeGroup con;
        con.name = QStringLiteral("Conflito");
        con.options = {
            { QStringLiteral("Inimigos"),          QStringLiteral("Inimigas") },
            { QStringLiteral("Rivais"),            QStringLiteral("Rivais") },
            { QStringLiteral("Desconfiança mútua"), QStringLiteral("Desconfiança mútua") },
            { QStringLiteral("Credor/Devedor"),    QStringLiteral("Credora/Devedora") },
            { QStringLiteral("Traição"),           QStringLiteral("Traição") },
        };
        g.append(con);
    }
    {
        BondTypeGroup pwr;
        pwr.name = QStringLiteral("Poder");
        pwr.options = {
            { QStringLiteral("Mentor / Aprendiz"),    QStringLiteral("Mentora / Aprendiz") },
            { QStringLiteral("Líder / Subordinado"),  QStringLiteral("Líder / Subordinada") },
            { QStringLiteral("Aliados"),              QStringLiteral("Aliadas") },
            { QStringLiteral("Protetor / Protegido"), QStringLiteral("Protetora / Protegida") },
            { QStringLiteral("Mestre / Discípulo"),   QStringLiteral("Mestra / Discípula") },
        };
        g.append(pwr);
    }
    return g;
}

} // namespace BondTypes
