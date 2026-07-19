#pragma once

#include <QRegularExpression>
#include <QString>

// Dicionário de verbos de fala/ação e marcadores de 1ª pessoa em INGLÊS —
// equivalente de DialogueVerbs.h (PT-BR), pra DialogueDetector reconhecer
// atribuição de diálogo também em projetos escritos em inglês. Os dois
// dicionários são combinados (nunca um OU outro) em DialogueDetector, então
// funcionam juntos no mesmo projeto sem precisar escolher idioma.
//
// Inglês não conjuga por pessoa como o português — cada verbo aqui traz só
// as formas realmente usadas em tag de diálogo: base, 3ª pessoa do singular
// (-s), gerúndio (-ing) e passado (irregular quando aplicável). Categorias
// espelham DialogueVerbs.h pra facilitar comparação/manutenção futura.
namespace DialogueVerbsEN {

inline const QString& speechVerbsSource()
{
    static const QString src = QStringLiteral(
        // Direct speech
        "say|says|saying|said|"
        "tell|tells|telling|told|"
        "speak|speaks|speaking|spoke|"
        "reply|replies|replying|replied|"
        "answer|answers|answering|answered|"
        "ask|asks|asking|asked|"
        "shout|shouts|shouting|shouted|"
        "yell|yells|yelling|yelled|"
        "scream|screams|screaming|screamed|"
        "whisper|whispers|whispering|whispered|"
        "murmur|murmurs|murmuring|murmured|"
        "mutter|mutters|muttering|muttered|"
        "mumble|mumbles|mumbling|mumbled|"
        "exclaim|exclaims|exclaiming|exclaimed|"
        "cry|cries|crying|cried|"
        "call|calls|calling|called|"
        "roar|roars|roaring|roared|"
        "growl|growls|growling|growled|"
        "snarl|snarls|snarling|snarled|"
        "howl|howls|howling|howled|"
        "groan|groans|groaning|groaned|"
        "moan|moans|moaning|moaned|"
        "sob|sobs|sobbing|sobbed|"
        "sigh|sighs|sighing|sighed|"
        "gasp|gasps|gasping|gasped|"
        "snap|snaps|snapping|snapped|"
        "hiss|hisses|hissing|hissed|"
        "spit|spits|spitting|spat|"
        "laugh|laughs|laughing|laughed|"
        "chuckle|chuckles|chuckling|chuckled|"
        "giggle|giggles|giggling|giggled|"
        "snort|snorts|snorting|snorted|"
        "cough|coughs|coughing|coughed|"
        "sing|sings|singing|sang|"
        "recite|recites|reciting|recited|"
        "stammer|stammers|stammering|stammered|"
        "stutter|stutters|stuttering|stuttered|"
        "yawn|yawns|yawning|yawned|"
        // Attribution and response
        "comment|comments|commenting|commented|"
        "observe|observes|observing|observed|"
        "note|notes|noting|noted|"
        "continue|continues|continuing|continued|"
        "add|adds|adding|added|"
        "interrupt|interrupts|interrupting|interrupted|"
        "cut in|cuts in|cutting in|"
        "retort|retorts|retorting|retorted|"
        "insist|insists|insisting|insisted|"
        "repeat|repeats|repeating|repeated|"
        "hesitate|hesitates|hesitating|hesitated|"
        "agree|agrees|agreeing|agreed|"
        "disagree|disagrees|disagreeing|disagreed|"
        "nod|nods|nodding|nodded|"
        "complain|complains|complaining|complained|"
        "grumble|grumbles|grumbling|grumbled|"
        "lament|laments|lamenting|lamented|"
        "protest|protests|protesting|protested|"
        "joke|jokes|joking|joked|"
        "tease|teases|teasing|teased|"
        // Revelation and declaration
        "confess|confesses|confessing|confessed|"
        "admit|admits|admitting|admitted|"
        "reveal|reveals|revealing|revealed|"
        "declare|declares|declaring|declared|"
        "state|states|stating|stated|"
        "affirm|affirms|affirming|affirmed|"
        "deny|denies|denying|denied|"
        "confirm|confirms|confirming|confirmed|"
        "inform|informs|informing|informed|"
        "explain|explains|explaining|explained|"
        "recount|recounts|recounting|recounted|"
        "narrate|narrates|narrating|narrated|"
        "announce|announces|announcing|announced|"
        "lie|lies|lying|lied|"
        "promise|promises|promising|promised|"
        "swear|swears|swearing|swore|"
        // Request and command
        "beg|begs|begging|begged|"
        "plead|pleads|pleading|pleaded|"
        "order|orders|ordering|ordered|"
        "command|commands|commanding|commanded|"
        "demand|demands|demanding|demanded|"
        "warn|warns|warning|warned|"
        "threaten|threatens|threatening|threatened|"
        // Suggestion and persuasion
        "suggest|suggests|suggesting|suggested|"
        "propose|proposes|proposing|proposed|"
        "persuade|persuades|persuading|persuaded|"
        "convince|convinces|convincing|convinced|"
        "urge|urges|urging|urged|"
        "concede|concedes|conceding|conceded|"
        // Body language and physical action (common in dialogue tags)
        "smile|smiles|smiling|smiled|"
        "grin|grins|grinning|grinned|"
        "frown|frowns|frowning|frowned|"
        "scowl|scowls|scowling|scowled|"
        "shrug|shrugs|shrugging|shrugged|"
        "wink|winks|winking|winked|"
        "blink|blinks|blinking|blinked|"
        "wave|waves|waving|waved|"
        "gesture|gestures|gesturing|gestured|"
        "point|points|pointing|pointed|"
        "stare|stares|staring|stared|"
        "glare|glares|glaring|glared|"
        "glance|glances|glancing|glanced|"
        "look|looks|looking|looked|"
        "watch|watches|watching|watched|"
        "turn|turns|turning|turned|"
        "raise|raises|raising|raised|"
        "lower|lowers|lowering|lowered|"
        "lean|leans|leaning|leaned|"
        "bend|bends|bending|bent|"
        "straighten|straightens|straightening|straightened|"
        "step back|steps back|stepping back|stepped back|"
        "step forward|steps forward|stepping forward|stepped forward|"
        "approach|approaches|approaching|approached|"
        "back away|backs away|backing away|backed away|"
        "stand|stands|standing|stood|"
        "sit|sits|sitting|sat|"
        "kneel|kneels|kneeling|knelt|"
        "lie down|lies down|lying down|"
        "lean over|leans over|leaning over|leaned over|"
        // Movement
        "walk|walks|walking|walked|"
        "march|marches|marching|marched|"
        "run|runs|running|ran|"
        "enter|enters|entering|entered|"
        "leave|leaves|leaving|left|"
        "arrive|arrives|arriving|arrived|"
        "return|returns|returning|returned|"
        "cross|crosses|crossing|crossed|"
        "shake|shakes|shaking|shook|"
        "tremble|trembles|trembling|trembled|"
        "shiver|shivers|shivering|shivered|"
        "breathe|breathes|breathing|breathed|"
        "shrink|shrinks|shrinking|shrank|"
        "pull|pulls|pulling|pulled|"
        "push|pushes|pushing|pushed|"
        "grip|grips|gripping|gripped|"
        "grab|grabs|grabbing|grabbed|"
        "release|releases|releasing|released|"
        "hold|holds|holding|held|"
        "throw|throws|throwing|threw|"
        "drop|drops|dropping|dropped|"
        "toss|tosses|tossing|tossed|"
        "touch|touches|touching|touched|"
        "knock|knocks|knocking|knocked|"
        "open|opens|opening|opened|"
        "close|closes|closing|closed|"
        "bite|bites|biting|bit|"
        "swallow|swallows|swallowing|swallowed|"
        "hug|hugs|hugging|hugged|"
        "embrace|embraces|embracing|embraced|"
        "kiss|kisses|kissing|kissed|"
        "poke|pokes|poking|poked|"
        "pinch|pinches|pinching|pinched|"
        "rub|rubs|rubbing|rubbed|"
        "wipe|wipes|wiping|wiped|"
        "caress|caresses|caressing|caressed|"
        "scratch|scratches|scratching|scratched|"
        "fold|folds|folding|folded|"
        "peek|peeks|peeking|peeked|"
        "stretch|stretches|stretching|stretched|"
        "blush|blushes|blushing|blushed|"
        "stiffen|stiffens|stiffening|stiffened|"
        "relax|relaxes|relaxing|relaxed|"
        "kick|kicks|kicking|kicked|"
        "punch|punches|punching|punched|"
        "strike|strikes|striking|struck|"
        "seize|seizes|seizing|seized|"
        "take|takes|taking|took|"
        "put|puts|putting|"
        "remove|removes|removing|removed|"
        "bring|brings|bringing|brought|"
        "twist|twists|twisting|twisted|"
        "lick|licks|licking|licked|"
        "jump|jumps|jumping|jumped|"
        "leap|leaps|leaping|leapt|"
        "fall|falls|falling|fell|"
        "trip|trips|tripping|tripped|"
        "slip|slips|slipping|slipped|"
        "slide|slides|sliding|slid|"
        "crawl|crawls|crawling|crawled|"
        "drag|drags|dragging|dragged|"
        "spin|spins|spinning|spun|"
        "flee|flees|fleeing|fled|"
        "escape|escapes|escaping|escaped|"
        "vanish|vanishes|vanishing|vanished|"
        "appear|appears|appearing|appeared|"
        "climb|climbs|climbing|climbed|"
        "descend|descends|descending|descended|"
        "recline|reclines|reclining|reclined|"
        "pant|pants|panting|panted|"
        "inhale|inhales|inhaling|inhaled|"
        "exhale|exhales|exhaling|exhaled|"
        "sneeze|sneezes|sneezing|sneezed|"
        "gulp|gulps|gulping|gulped|"
        "sniff|sniffs|sniffing|sniffed|"
        "gag|gags|gagging|gagged|"
        "clear his throat|clears his throat|clearing his throat|cleared his throat|"
        "pale|pales|paling|paled|"
        // Food and consumption
        "drink|drinks|drinking|drank|"
        "eat|eats|eating|ate|"
        "smoke|smokes|smoking|smoked|"
        "chew|chews|chewing|chewed|"
        "sip|sips|sipping|sipped|"
        "serve|serves|serving|served|"
        // Mental and perception
        "think|thinks|thinking|thought|"
        "remember|remembers|remembering|remembered|"
        "notice|notices|noticing|noticed|"
        "realize|realizes|realizing|realized|"
        "imagine|imagines|imagining|imagined|"
        "try|tries|trying|tried|"
        "decide|decides|deciding|decided|"
        "doubt|doubts|doubting|doubted|"
        "believe|believes|believing|believed|"
        "wonder|wonders|wondering|wondered|"
        "wait|waits|waiting|waited|"
        "discover|discovers|discovering|discovered"
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

// Marcadores exclusivos de 1ª pessoa em inglês — "I" + contrações comuns +
// possessivos "my"/"mine". Ao contrário do português, o inglês não tem forma
// verbal própria de 1ª pessoa singular pra maioria dos verbos (só "am"), então
// o pronome "I" já é o sinalizador principal — "\b...\b" garante que só bate
// como palavra isolada (não dentro de outra palavra), então case-insensitive
// não gera falso positivo aqui.
inline const QRegularExpression& firstPersonMarkersRegex()
{
    static const QRegularExpression re(
        QStringLiteral("\\b(I|I'm|I've|I'll|I'd|my|mine|myself|me|am)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

} // namespace DialogueVerbsEN
