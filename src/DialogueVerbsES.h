#pragma once

#include <QRegularExpression>
#include <QString>

// Dicionário de verbos de fala/ação e marcadores de 1ª pessoa em ESPANHOL —
// mesmo papel de DialogueVerbs.h (PT-BR) e DialogueVerbsEN.h (inglês).
// DialogueDetector combina os três dicionários (nunca escolhe um só), então
// funcionam juntos no mesmo projeto sem precisar configurar idioma.
//
// Formas trazidas por verbo: 3ª pessoa do pretérito + presente + gerúndio —
// mesma lógica do dicionário PT-BR (as formas que aparecem de verdade em tag
// de diálogo em 3ª pessoa). Categorias espelham DialogueVerbs.h.
namespace DialogueVerbsES {

inline const QString& speechVerbsSource()
{
    static const QString src = QStringLiteral(
        // Habla directa
        "dijo|dice|diciendo|"
        "habló|habla|hablando|"
        "respondió|responde|respondiendo|"
        "preguntó|pregunta|preguntando|"
        "gritó|grita|gritando|"
        "chilló|chilla|chillando|"
        "susurró|susurra|susurrando|"
        "murmuró|murmura|murmurando|"
        "musitó|musita|musitando|"
        "exclamó|exclama|exclamando|"
        "bramó|brama|bramando|"
        "vociferó|vocifera|vociferando|"
        "clamó|clama|clamando|"
        "rugió|ruge|rugiendo|"
        "gruñó|gruñe|gruñendo|"
        "aulló|aúlla|aullando|"
        "resopló|resopla|resoplando|"
        "jadeó|jadea|jadeando|"
        "gimió|gime|gimiendo|"
        "sollozó|solloza|sollozando|"
        "lloró|llora|llorando|"
        "rió|ríe|riendo|"
        "tosió|tose|tosiendo|"
        "cantó|canta|cantando|"
        "recitó|recita|recitando|"
        "suspiró|suspira|suspirando|"
        "tartamudeó|tartamudea|tartamudeando|"
        "balbuceó|balbucea|balbuceando|"
        "bostezó|bosteza|bostezando|"
        "carcajeó|carcajea|carcajeando|"
        // Atribución y respuesta
        "comentó|comenta|comentando|"
        "observó|observa|observando|"
        "notó|nota|notando|"
        "continuó|continúa|continuando|"
        "añadió|añade|añadiendo|"
        "interrumpió|interrumpe|interrumpiendo|"
        "intervino|interviene|interviniendo|"
        "replicó|replica|replicando|"
        "objetó|objeta|objetando|"
        "insistió|insiste|insistiendo|"
        "repitió|repite|repitiendo|"
        "vaciló|vacila|vacilando|"
        "asintió|asiente|asintiendo|"
        "concordó|concuerda|concordando|"
        "discrepó|discrepa|discrepando|"
        "se quejó|se queja|quejándose|"
        "lamentó|lamenta|lamentando|"
        "protestó|protesta|protestando|"
        "bromeó|bromea|bromeando|"
        "ironizó|ironiza|ironizando|"
        // Revelación y declaración
        "confesó|confiesa|confesando|"
        "admitió|admite|admitiendo|"
        "reveló|revela|revelando|"
        "declaró|declara|declarando|"
        "afirmó|afirma|afirmando|"
        "negó|niega|negando|"
        "confirmó|confirma|confirmando|"
        "informó|informa|informando|"
        "explicó|explica|explicando|"
        "contó|cuenta|contando|"
        "narró|narra|narrando|"
        "anunció|anuncia|anunciando|"
        "mintió|miente|mintiendo|"
        "prometió|promete|prometiendo|"
        "juró|jura|jurando|"
        // Petición y orden
        "llamó|llama|llamando|"
        "pidió|pide|pidiendo|"
        "ordenó|ordena|ordenando|"
        "exigió|exige|exigiendo|"
        "suplicó|suplica|suplicando|"
        "imploró|implora|implorando|"
        "advirtió|advierte|advirtiendo|"
        "alertó|alerta|alertando|"
        "amenazó|amenaza|amenazando|"
        // Sugerencia y persuasión
        "sugirió|sugiere|sugiriendo|"
        "propuso|propone|proponiendo|"
        "persuadió|persuade|persuadiendo|"
        "convenció|convence|convenciendo|"
        "cedió|cede|cediendo|"
        // Lenguaje corporal y acción física
        "sonrió|sonríe|sonriendo|"
        "frunció|frunce|frunciendo|"
        "arqueó|arquea|arqueando|"
        "guiñó|guiña|guiñando|"
        "parpadeó|parpadea|parpadeando|"
        "saludó|saluda|saludando|"
        "gesticuló|gesticula|gesticulando|"
        "señaló|señala|señalando|"
        "miró|mira|mirando|"
        "observó|observa|observando|"
        "giró|gira|girando|"
        "volteó|voltea|volteando|"
        "levantó|levanta|levantando|"
        "bajó|baja|bajando|"
        "inclinó|inclina|inclinando|"
        "encogió|encoge|encogiendo|"
        "curvó|curva|curvando|"
        "retrocedió|retrocede|retrocediendo|"
        "avanzó|avanza|avanzando|"
        "acercó|acerca|acercando|"
        "alejó|aleja|alejando|"
        "sentó|sienta|sentando|"
        "agachó|agacha|agachando|"
        "arrodilló|arrodilla|arrodillando|"
        "acostó|acuesta|acostando|"
        "recostó|recuesta|recostando|"
        "apoyó|apoya|apoyando|"
        // Movimiento
        "caminó|camina|caminando|"
        "marchó|marcha|marchando|"
        "corrió|corre|corriendo|"
        "entró|entra|entrando|"
        "salió|sale|saliendo|"
        "llegó|llega|llegando|"
        "volvió|vuelve|volviendo|"
        "cruzó|cruza|cruzando|"
        "balanceó|balancea|balanceando|"
        "sacudió|sacude|sacudiendo|"
        "tembló|tiembla|temblando|"
        "estremeció|estremece|estremeciendo|"
        "respiró|respira|respirando|"
        "tiró|tira|tirando|"
        "empujó|empuja|empujando|"
        "apretó|aprieta|apretando|"
        "soltó|suelta|soltando|"
        "agarró|agarra|agarrando|"
        "lanzó|lanza|lanzando|"
        "tocó|toca|tocando|"
        "golpeó|golpea|golpeando|"
        "abrió|abre|abriendo|"
        "cerró|cierra|cerrando|"
        "mordió|muerde|mordiendo|"
        "tragó|traga|tragando|"
        "abrazó|abraza|abrazando|"
        "besó|besa|besando|"
        "pellizcó|pellizca|pellizcando|"
        "frotó|frota|frotando|"
        "limpió|limpia|limpiando|"
        "acarició|acaricia|acariciando|"
        "rascó|rasca|rascando|"
        "dobló|dobla|doblando|"
        "estiró|estira|estirando|"
        "se sonrojó|se sonroja|sonrojándose|"
        "tensó|tensa|tensando|"
        "relajó|relaja|relajando|"
        "pateó|patea|pateando|"
        "arañó|araña|arañando|"
        "tomó|toma|tomando|"
        "dejó|deja|dejando|"
        "puso|pone|poniendo|"
        "quitó|quita|quitando|"
        "trajo|trae|trayendo|"
        "torció|tuerce|torciendo|"
        "lamió|lame|lamiendo|"
        "saltó|salta|saltando|"
        "brincó|brinca|brincando|"
        "cayó|cae|cayendo|"
        "tropezó|tropieza|tropezando|"
        "resbaló|resbala|resbalando|"
        "deslizó|desliza|deslizando|"
        "arrastró|arrastra|arrastrando|"
        "huyó|huye|huyendo|"
        "escapó|escapa|escapando|"
        "desapareció|desaparece|desapareciendo|"
        "apareció|aparece|apareciendo|"
        "atravesó|atraviesa|atravesando|"
        "subió|sube|subiendo|"
        "enderezó|endereza|enderezando|"
        "inspiró|inspira|inspirando|"
        "exhaló|exhala|exhalando|"
        "estornudó|estornuda|estornudando|"
        "palideció|palidece|palideciendo|"
        // Comida y consumo
        "bebió|bebe|bebiendo|"
        "comió|come|comiendo|"
        "fumó|fuma|fumando|"
        "masticó|mastica|masticando|"
        "sirvió|sirve|sirviendo|"
        // Mental y percepción
        "pensó|piensa|pensando|"
        "recordó|recuerda|recordando|"
        "percibió|percibe|percibiendo|"
        "imaginó|imagina|imaginando|"
        "intentó|intenta|intentando|"
        "decidió|decide|decidiendo|"
        "dudó|duda|dudando|"
        "creyó|cree|creyendo|"
        "esperó|espera|esperando|"
        "descubrió|descubre|descubriendo"
    );
    return src;
}

inline const QRegularExpression& speechVerbsRegex()
{
    static const QRegularExpression re(
        QStringLiteral("\\b(%1)\\b").arg(speechVerbsSource()),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

// Marcadores de 1ª pessoa em espanhol. Diferente do inglês, o espanhol
// (como o português) é pro-drop — "yo" costuma vir OMITIDO porque a própria
// conjugação verbal já indica a pessoa ("dije" já é "eu disse", sem precisar
// do pronome). Por isso a lista inclui "yo" + possessivos ("mi", "mío/mía")
// + as formas de 1ª pessoa (presente/pretérito) dos verbos mais comuns em
// tag de diálogo — não é exaustivo pra tudo do dicionário acima (seria uma
// lista enorme de conjugações), mas cobre os casos mais frequentes.
inline const QRegularExpression& firstPersonMarkersRegex()
{
    static const QRegularExpression re(QStringLiteral(
        "\\b(yo|mí|mi|mío|mía|míos|mías|conmigo|me\\s+\\w|"
        "dije|digo|hablé|hablo|respondí|respondo|pregunté|pregunto|"
        "grité|grito|susurré|susurro|murmuré|murmuro|exclamé|exclamo|"
        "lloré|lloro|reí|río|tosí|toso|canté|canto|suspiré|suspiro|"
        "balbuceé|balbuceo|comenté|comento|observé|observo|noté|noto|"
        "continué|continúo|añadí|añado|interrumpí|interrumpo|insistí|insisto|"
        "repetí|repito|asentí|asiento|confesé|confieso|admití|admito|"
        "revelé|revelo|declaré|declaro|afirmé|afirmo|negué|niego|"
        "confirmé|confirmo|informé|informo|expliqué|explico|conté|cuento|"
        "anuncié|anuncio|mentí|miento|prometí|prometo|juré|juro|"
        "llamé|llamo|pedí|pido|ordené|ordeno|exigí|exijo|"
        "supliqué|suplico|advertí|advierto|sugerí|sugiero|propuse|propongo|"
        "convencí|convenzo|sonreí|sonrío|miré|miro|caminé|camino|"
        "corrí|corro|entré|entro|salí|salgo|llegué|llego|volví|vuelvo|"
        "tomé|tomo|dejé|dejo|puse|pongo|quité|quito|traje|traigo|"
        "salté|salto|caí|caigo|huí|huyo|escapé|escapo|"
        "bebí|bebo|comí|como|pensé|pienso|recordé|recuerdo|"
        "intenté|intento|decidí|decido|dudé|dudo|creí|creo|"
        "esperé|espero|descubrí|descubro)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

} // namespace DialogueVerbsES
