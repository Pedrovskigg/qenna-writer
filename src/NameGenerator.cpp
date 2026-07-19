#include "NameGenerator.h"

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QSet>

namespace {

constexpr int kMaxOrder = 3;
const QChar kEnd = QChar('\n'); // terminador (nunca aparece num nome)

// ---------------- Datasets de treino (sementes do Markov) ----------------
// Pequenos de propósito: o Markov generaliza o "som" do estilo. Nomes não têm
// copyright; o que vale é a sonoridade.

const QStringList& elvish() {
    static const QStringList d = {
        "Aelar","Aeris","Caelar","Caelyn","Elenya","Elaria","Faelar","Faelyn",
        "Galadel","Ithilien","Lariel","Lorien","Myriel","Nimue","Sylvar","Thalas",
        "Thandor","Vaelora","Yllae","Aerendil","Elandor","Naeris","Sariel","Taeral",
        "Variel","Aelithil","Celebor","Eowyn","Finrod","Luthien","Myrbor","Vaeledor",
        "Finorod","Sarlithil","Taelar","Finaithil","Lunithil","Loroyn","Luiris","Ithoaria",
        "Nimayn","Finarod","Aeleora","Eliris","Yllaeth","Lorias","Varoandor","Eonwen",
        "Celaria","Faelwen","Aelvar","Nimdor","Saraithil","Larris","Myrlue","Celodor",
        "Varendil","Celeora","Lorowen","Cellilien","Finsar","Elayn","Syldor","Naerar",
        "Syleth","Finwen","Faelandor","Elrae","Sillithil","Varwen","Caelris","Finaendil",
        "Laras","Vaelnue","Sileilien","Loradel","Larsiel","Eliral","Aelarod","Syloral",
        "Larlas","Thanue","Taelis","Celeth","Ithien","Silaria","Sareas","Silivar",
        "Aeroris","Elral","Varaor","Taelendil","Vaelue","Elrithil","Thanrod","Nimora",
        "Elailien","Thanrue","Silseth","Thaneis","Vaelivar","Galearia","Sylleth","Nimieth",
        "Sareral","Celavar","Naelyn","Varoris","Sylris","Faelleth","Sararis","Sylobor",
        "Saras","Aerandor","Faelor","Lorrod","Vaeleyn","Sylathien","Eonis","Faelnor",
        "Elris","Vaelsor","Nimreth","Sileris","Thaloral","Ithiris","Ylleilien","Thalris",
        "Thalobor","Naerral","Ithas","Aernadel","Vaelnae","Naeral","Sarilien","Naenris",
        "Aerlas","Aerar","Syllis","Lorvar","Lariandor","Aelbor","Eloyn","Faelral",
        "Finaral","Naesaria","Naelris","Sarnenya","Vaelral","Nimeris","Caelandor","Thanlis",
        "Yllirod","Sarnora","Caelor","Myrsis","Galue","Lorsien","Larirod","Ithaor",
        "Taervar","Taebor","Syloithil","Naendor","Thalora","Celoas","Finayn","Eonral",
        "Naelar","Varadel","Naeladel","Myrrendil","Myreris","Aelaora","Ludor","Aelis",
        "Caelaria","Silaeth","Sylerod","Variar","Siladel","Thanovar","Yllathien","Thanvar",
        "Nimivar","Caeldor","Luvar","Faeleth","Aelwen","Myroandor","Vaelrod","Varsithil",
        "Myrithil","Finis","Celoilien","Celilien","Galnendil","Naebor","Varlis","Yllaria",
        "Aeradel","Myrendil","Lorora","Thalnor","Larrendil","Taeraria","Ithora","Larladel",
        "Caelebor","Silris","Nimaendil","Naesendil","Lariora","Eovar","Cellenya","Thanoenya",
        "Lorae","Vaelsas","Celathien","Naewen","Thallaria","Sariral","Silneth","Luovar",
        "Ithewen","Celayn","Elrien","Silor","Elleth","Lorenya","Lunadel","Lornien",
        "Faelebor","Celwen","Nimradel","Luleth","Galsithil","Aeleor","Eorral","Taesral",
        "Ithayn","Lorivar","Yllaris","Aelreth","Myrerod","Galar","Silithien","Celris",
        "Lural","Lurithil","Ellendil","Celias","Nimilien","Caelowen","Faelaeth","Aerryn",
        "Aeleyn","Myrdor","Nimlas","Silrora","Aelor","Aelas","Eothien","Yllerod",
        "Saroandor","Sylae","Elaendil","Finoar","Itherod","Luryn","Eloilien","Luowen",
        "Vaeldor","Syleilien","Finvar","Aelearia","Thalidor","Larien","Yllois","Naerod",
        "Aelilien","Aeloeth","Itheyn","Eonyn","Silrendil","Vaelas","Itharod","Silbor",
        "Galiar","Taelaria","Itheilien","Naerbor","Eleas","Faelibor","Aeroyn","Faelvar",
        "Luradel","Celiora","Celseth","Eliel" };
    return d;
}
const QStringList& nordic() {
    static const QStringList d = {
        "Bjorn","Sigrid","Ragnar","Astrid","Eirik","Gunnar",
        "Helga","Ingrid","Leif","Sven","Thora","Ulfar",
        "Frida","Hakon","Sigurd","Yrsa","Brynja","Torvald",
        "Greta","Knut","Solveig","Vidar","Eldgrim","Halvard",
        "Runa","Gudrun","Harald","Ivar","Njal","Skadi",
        "Aki","Alf","Alvar","Amund","Anund","Arne",
        "Arnfinn","Arnljot","Arnor","Arnulf","Asbjorn","Asgaut",
        "Asgeir","Askel","Asmund","Audun","Balder","Baldur",
        "Bard","Bardi","Bergthor","Birger","Birkir","Bjarke",
        "Bjarni","Bjartur","Bo","Bodvar","Bolli","Bragi",
        "Brand","Brynjar","Brynjolf","Dag","Dagfinn","Domar",
        "Egil","Einar","Eindride","Eindridi","Eiolf","Eirikur",
        "Ejnar","Erlend","Erling","Ervin","Eskil","Eyjolf",
        "Eystein","Eyvind","Farbjorn","Finnbogi","Finn","Fjolnir",
        "Floki","Folke","Frey","Freyr","Frodi","Frosti",
        "Fylkir","Galti","Gauti","Geir","Geirmund","Geirolf",
        "Gest","Gizur","Glum","Gorm","Grani","Greip",
        "Grim","Grimkel","Grimnir","Gudbrand","Gudleif","Gudmund",
        "Gunnbjorn","Gunnlaug","Gunnolf","Gunnstein","Gyrd","Hafthor",
        "Hagbard","Halfdan","Hallbjorn","Halldor","Hallfred","Hallgeir",
        "Hallgrim","Hallkell","Hallstein","Hallvard","Halstein","Hamund",
        "Haraldur","Hardar","Hastein","Havard","Hedin","Helgi",
        "Helmer","Hemming","Herjolf","Herleif","Hersir","Hjalmar",
        "Hjalti","Hjortur","Hoskuld","Hrafn","Hrafnkel","Hreidar",
        "Hroar","Hrolf","Hrolleif","Hromund","Hunbogi","Ingimund",
        "Ingjald","Ingolf","Ingvar","Jokull","Jorund","Kalf",
        "Kari","Karl","Ketil","Ketilbjorn","Kjartan","Kjell",
        "Klaengur","Kolbeinn","Kolgrim","Kolskegg","Kori","Kristjan",
        "Kveldulf","Lambi","Leidolf","Loki","Magni","Marteinn",
        "Odd","Oddbjorn","Olaf","Olafur","Oleif","Onund",
        "Orm","Ormar","Ornolf","Oskar","Ospak","Ospakur",
        "Ottar","Ove","Ragnvald","Rand","Randver","Ref",
        "Reidar","Reinald","Roald","Roar","Rodgeir","Rolf",
        "Rollant","Runolf","Sighvat","Sigmund","Sigtrygg","Sigvald",
        "Sindri","Skapti","Skeggi","Skjold","Skuli","Snorri",
        "Sokki","Solmund","Sorli","Starkad","Steinar","Steingrim",
        "Steinolf","Steinthor","Stigandr","Storm","Styrbjorn","Styrkar",
        "Sturla","Svein","Sveinbjorn","Sveinung","Thangbrand","Thjodolf",
        "Thorald","Thorarin","Thorbjorn","Thord","Thorfinn","Thorgeir",
        "Thorgest","Thorgils","Thorgrim","Thorhall","Thorir","Thorkel",
        "Thorlak","Thorleif","Thormod","Thorodd","Thororm","Thorolf",
        "Thorstein","Thorvald","Thorvid","Tofi","Tore","Torgeir",
        "Torgny","Torkild","Torleiv","Torolf","Trond","Tryggvi",
        "Ulf","Ulfljot","Uni","Vagn","Vagnar","Vali",
        "Valgard","Valthjof","Vemund","Vermund","Vestein","Vidfinn",
        "Vigfus","Viglund","Vikar","Vilhjalmur","Volund","Yngvar",
        "Yngvi","Arngrim","Bersi","Bjorgolf","Eyolf","Rurik",
        "Torstein","Aslak","Bergsvein","Aesa","Alfhild","Arnbjorg",
        "Arndis","Arnfrid","Arngunn","Arnhild","Asa","Asdis",
        "Asfrid","Asgerd","Ashild","Aslaug","Asny","Asta",
        "Audhild","Bergljot","Bergthora","Bil","Bodil","Borghild",
        "Bothild","Dagny","Disa","Eir","Embla","Eydis",
        "Freydis","Freyja","Frigg","Gerd","Gudbjorg","Gudlaug",
        "Gudny","Gudrid","Gullveig","Gunnhild","Gunnlod","Gyda",
        "Halla","Hallbera","Hallbjorg","Halldis","Halldora","Hallfrid",
        "Hallgerd","Hallveig","Hedda","Herdis","Hervor","Hild",
        "Hilda","Hildigunn","Holmfrid","Ingeborg","Inger","Ingibjorg",
        "Ingigerd","Ingirid","Ingunn","Ingveig","Iselin","Jofrid",
        "Jorun","Jorunn","Kadlin","Katla","Liv","Ragna",
        "Ragnfrid","Ragnhild","Randi","Rannveig","Saga","Sif",
        "Sigga","Signe","Signy","Sigrunn","Sigyn","Sjofn",
        "Skjaldvor","Steinunn","Svanhild","Thordis","Thorgerd","Thorhild",
        "Thorunn","Thurid","Torborg","Torhild","Torunn","Unn",
        "Valgerd","Vigdis","Yngvild","Alva","Alfdis","Arna",
        "Asvor","Bergdis","Dagrun","Gudfinna","Gyrid","Herborg",
        "Hjordis","Ingerid","Ranveig","Sigdis","Solborg","Steinvor",
        "Thorbjorg","Ylva","Bergrun","Ranghild","Kadrun","Alvhild",
        "Solrun","Tordis","Vebjorg" };
    return d;
}
const QStringList& latin() {
    static const QStringList d = {
        "Marcus","Lucius","Livia","Cassia","Tiberius","Aurelia",
        "Octavia","Valerius","Cornelius","Drusilla","Fabius","Julia",
        "Quintus","Severus","Antonia","Gaius","Flavia","Maximus",
        "Lucretia","Cassius","Decimus","Vespasia","Aelia","Crispus",
        "Sabina","Tullius","Marcella","Albinus","Caelina","Servius",
        "Appius","Aulus","Gnaeus","Manius","Numerius","Publius",
        "Sextus","Titus","Vibius","Agrippa","Ambustus","Ancus",
        "Aemilius","Antoninus","Antonius","Augustus","Aurelianus","Balbus",
        "Barbatus","Bassus","Blandus","Brutus","Caecilius","Caesar",
        "Camillus","Cato","Catullus","Cinna","Claudius","Cocles",
        "Commodus","Constantius","Corvinus","Crassus","Curio","Dentatus",
        "Didius","Domitianus","Domitius","Drusus","Fabricius","Faustus",
        "Felix","Flaccus","Flamininus","Gallus","Galba","Gallienus",
        "Germanicus","Geta","Gracchus","Hadrianus","Hispanus","Horatius",
        "Hortensius","Julianus","Justus","Juvenalis","Laelius","Lentulus",
        "Longinus","Lucanus","Macrinus","Marius","Martialis","Metellus",
        "Murena","Naso","Nepos","Nero","Nerva","Nonius",
        "Numa","Octavianus","Ovidius","Paulinus","Paulus","Perpenna",
        "Pertinax","Piso","Pius","Plancus","Plautus","Plinius",
        "Pompeius","Pomponius","Postumus","Priscus","Probus","Proculus",
        "Regulus","Rufinus","Rufus","Rutilius","Sallustius","Saturninus",
        "Scaevola","Scaurus","Scipio","Seneca","Silanus","Silvanus",
        "Statius","Sulla","Sulpicius","Tacitus","Terentius","Trajanus",
        "Traianus","Trebonius","Turnus","Ulpius","Varro","Varus",
        "Vatinius","Vegetius","Verres","Verus","Vespasianus","Vitellius",
        "Volusius","Zeno","Aquila","Aristus","Avitus","Bibulus",
        "Britannicus","Caligula","Carbo","Cassianus","Celerinus","Celsus",
        "Cethegus","Clemens","Cotta","Damasus","Diocletianus","Faustinus",
        "Firmus","Fronto","Fulvius","Gratianus","Honorius","Julius",
        "Justinianus","Lepidus","Longus","Lucillus","Marcellinus","Maternus",
        "Maurus","Modestus","Narcissus","Numitor","Ovidus","Paternus",
        "Petronius","Placidus","Pollio","Pomponianus","Sabinus","Salvius",
        "Secundus","Serenus","Silvius","Tarquinius","Theodosius","Torquatus",
        "Valens","Valentinianus","Verginius","Vitalis","Casca","Curius",
        "Dolabella","Ennius","Fimbria","Glabrio","Labeo","Merula",
        "Natta","Nigidius","Opimius","Papirius","Rutilus","Salinator",
        "Scapula","Structus","Vopiscus","Aristo","Agrippina","Antonina",
        "Aquilina","Calpurnia","Camilla","Claudia","Cloelia","Constantia",
        "Cornelia","Domitilla","Faustina","Flaminia","Fulvia","Galla",
        "Helena","Hersilia","Hortensia","Julitta","Junia","Laelia",
        "Lavinia","Lepida","Longina","Lucilla","Marcia","Martina",
        "Messalina","Minervina","Nerissa","Numeria","Octavilla","Ovidia",
        "Paulina","Perpetua","Petronia","Placidia","Plautia","Pomponia",
        "Popilia","Porcia","Postuma","Prisca","Regilla","Rufina",
        "Salvia","Scribonia","Secunda","Serena","Servilia","Silvia",
        "Statilia","Sulpicia","Terentia","Tertulla","Theodora","Tullia",
        "Urbana","Valentina","Varinia","Verania","Verginia","Vibia",
        "Vipsania","Vitellia","Volumnia","Aemilia","Antistia","Appia",
        "Baebia","Barbata","Bassa","Blanda","Caecilia","Caesonia",
        "Calvina","Candida","Casta","Clara","Clodia","Domitia" };
    return d;
}
const QStringList& slavic() {
    static const QStringList d = {
        "Mirko","Vesna","Dragan","Zora","Bogdan","Lada",
        "Radomir","Snezana","Vuk","Milena","Branko","Jasna",
        "Stanislav","Dunja","Zoran","Vlatka","Borislav","Nadia",
        "Tomislav","Kresimir","Anja","Goran","Ljuba","Predrag",
        "Slavica","Miroslav","Ivana","Dragica","Velimir","Zlatan",
        "Aleksandar","Aleksej","Anatoli","Andrej","Antonin","Bogomil",
        "Bogoljub","Boris","Bojan","Bratislav","Budimir","Casimir",
        "Dalibor","Danilo","Darko","Desimir","Dimitar","Dobromir",
        "Dobroslav","Dragoslav","Dragomir","Drazen","Dusan","Dushan",
        "Emil","Filip","Gojko","Gradimir","Grigor","Ilya",
        "Ivan","Jaromir","Jaroslav","Jovan","Kazimir","Konstantin",
        "Kostadin","Krasimir","Ladislav","Lech","Lubomir","Ljubomir",
        "Lyudmil","Marko","Mateusz","Metod","Mihail","Mihailo",
        "Mikhail","Milan","Milivoj","Miljenko","Milos","Milovan",
        "Milutin","Miodrag","Mirjan","Mladen","Mstislav","Nebojsa",
        "Nikola","Obrad","Ognjen","Oleg","Ostap","Panteleimon",
        "Pavel","Pavle","Petar","Piotr","Radenko","Radivoj",
        "Radoslav","Radovan","Ratibor","Rostislav","Sasha","Slaven",
        "Slavko","Slavoljub","Sreten","Stanimir","Stanko","Stefan",
        "Stevan","Svetlan","Svetozar","Tihomir","Tomas","Uros",
        "Vaclav","Vadim","Vasil","Velibor","Veljko","Veroslav",
        "Vitomir","Vitold","Vjekoslav","Vladan","Vladimir","Vladislav",
        "Vlado","Vojin","Vojislav","Vojtech","Vratislav","Zbigniew",
        "Zdenko","Zdravko","Zeljko","Zivojin","Zvonimir","Zdeslav",
        "Bozidar","Bozo","Dobrivoje","Kazimierz","Wojciech","Wladyslaw",
        "Wenceslas","Bohdan","Bohuslav","Bronislav","Czeslaw","Grzegorz",
        "Henryk","Jaroslaw","Jerzy","Leszek","Mieszko","Mieczyslaw",
        "Przemyslaw","Radoslaw","Ryszard","Stanislaw","Wieslaw","Zygmunt",
        "Yaroslav","Yevgeni","Svyatoslav","Vsevolod","Vladlen","Igor",
        "Oleksandr","Rurik","Dobrogost","Chvalimir","Jarmil","Ratimir",
        "Radim","Vratko","Zdenek","Bratomil","Domagoj","Hranislav",
        "Ljutomir","Mstivoj","Nemanja","Ninoslav","Rajko","Rastislav",
        "Zvonko","Bratoslav","Vseslav","Radovid","Jaropolk","Izyaslav",
        "Rogvolod","Ana","Andjela","Biljana","Bisera","Bogdana",
        "Bojana","Bosiljka","Branislava","Cvijeta","Danica","Danijela",
        "Darinka","Divna","Dobrila","Draga","Dragana","Dragoslava",
        "Dubravka","Dusanka","Elena","Gordana","Gorica","Grozdana",
        "Iskra","Jagoda","Jasmina","Jelena","Jovanka","Kata",
        "Katarina","Krasna","Ljiljana","Ljubica","Ljudmila","Ludmila",
        "Magdalena","Malina","Marija","Marina","Marta","Mila",
        "Milanka","Milica","Miljana","Mira","Mirjana","Nada",
        "Nadezda","Natasha","Nevena","Olga","Radmila","Radojka",
        "Ratka","Ruza","Ruzica","Sanja","Slavenka","Snjezana",
        "Sofija","Sonja","Stana","Stanislava","Svetlana","Tatjana",
        "Tijana","Verica","Vesela","Violeta","Vishnja","Vjera",
        "Vukica","Yelena","Zdenka","Zivana","Zlata","Zorica",
        "Zorka","Zvezdana","Bozena","Bronislava","Danuta","Halina",
        "Irena","Jadwiga","Jolanta","Krystyna","Malgorzata","Teodora",
        "Wanda","Wieslawa","Zofia","Barbora","Milada","Vlasta",
        "Bozidarka","Vukosava","Milkana","Radoslava","Zdravka","Slavka",
        "Bosanka","Cvetka","Dobrinka","Emilija","Gostimila","Hranislava",
        "Jelka","Kalina","Ksenija","Milodarka","Nastasija","Radinka",
        "Stanica","Vidosava","Zorislava","Bratislava","Domanja","Miloslava",
        "Bogumila" };
    return d;
}
const QStringList& dark() {
    static const QStringList d = {
        "Malachar","Vorath","Nyxara","Drakthar","Mortheus","Velzaroth","Karneth","Sythara",
        "Azrath","Belmoth","Cindrel","Gravorn","Xulthar","Zarael","Morwen","Nethrax",
        "Vaelroth","Skarn","Thessil","Ombrath","Crevan","Dolmar","Erebos","Vexara",
        "Ravok","Malgrim","Tharivol","Zedrik","Korvath","Nephzar","Mororn","Nephuath",
        "Erearn","Voreroth","Thesurik","Skaregrim","Nyxnax","Voreth","Thareth","Drarel",
        "Kararik","Xulrik","Voreoth","Azuwen","Dranok","Nethath","Cinrara","Aznivol",
        "Tharax","Omeath","Morrath","Skarok","Karurik","Dolrivol","Vexurax","Azebos",
        "Xulerax","Nephuorn","Korearn","Vaeluarn","Tharamoth","Korleth","Kornok","Belurel",
        "Velrax","Maluoth","Vorzar","Belroth","Azleth","Cinmar","Azoara","Zarmoth",
        "Azaivol","Ravlath","Morlorn","Morrel","Ravurax","Draknorn","Nethebos","Karsil",
        "Drakuivol","Dolugrim","Zareorn","Omevath","Tharoivol","Karrath","Moraok","Tharuivol",
        "Morax","Ereivol","Tharrath","Vaelael","Vexnarn","Omrel","Nepheus","Veloax",
        "Nyxoara","Tharebos","Zedlara","Malagrim","Morosil","Korogrim","Velroth","Xulrax",
        "Vaelobos","Xulamar","Zedwen","Nyxuara","Drakarn","Dolarn","Nepheok","Zedath",
        "Drakorel","Thesvath","Cinabos","Nephegrim","Voreok","Drakbos","Nyxavath","Karreth",
        "Gramoth","Doluzar","Ereax","Ravnax","Graorn","Nyxeok","Tharmar","Velawen",
        "Maluorn","Vaeluvan","Drauroth","Xulmoth","Beloara","Karnara","Draorn","Zarath",
        "Vorwen","Cinvan","Sythorel","Sythusil","Ravmar","Tharrax","Grawen","Erenorn",
        "Nyxneth","Belrath","Korroth","Vorrel","Azbos","Dolneth","Velleth","Velivol",
        "Ravroth","Skarothar","Koroax","Zedemar","Eremar","Voravan","Karnorn","Nyxlivol",
        "Nyxrarn","Vaelsil","Belara","Ererath","Korevath","Ravearn","Zaroth","Nethael",
        "Omloth","Koruath","Thesovath","Korlarn","Skarvath","Ereomoth","Xullorn","Belerik",
        "Veleth","Azemoth","Tharnath","Beleus","Skarivol","Ombos","Morbos","Erenoth",
        "Ravoeth","Skaroarn","Maleax","Morebos","Omorik","Omozar","Vaelwen","Beleorn",
        "Thesovan","Draovath","Nyxbos","Vexrael","Belrik","Kororn","Malmoth","Omavath",
        "Voramar","Azleus","Malok","Erenarn","Nepharoth","Vaelok","Xuleth","Dolvan",
        "Thareax","Ravwen","Karivol","Omurel","Korara","Dramar","Tharvan","Zedoivol",
        "Ravrok","Sythowen","Drarrik","Dolwen","Cinevan","Ravuoth","Nyxmar","Karomar",
        "Vexath","Nyxnara","Korivol","Belvath","Thesrok","Xulax","Grarax","Zarorn",
        "Cinebos","Morlivol","Ereogrim","Omemoth","Draivol","Skarvan","Drakeorn","Granzar",
        "Sythurax","Vexerax","Cinrath","Vexeth","Dralmar","Thesamar","Omzar","Karezar",
        "Tharerax","Drakagrim","Vaelmoth","Zarnorn","Doloax","Drakebos","Sythamoth","Mornax",
        "Moruorn","Xulethar","Dranath","Doluthar","Nyxnael","Xulmar","Xulrel","Vorlarn",
        "Skarsil","Vexubos","Vorael","Grazar","Belogrim","Zarrarn","Vexroth","Vaeleus",
        "Dravan","Nyxael","Nyxorath","Aznoth","Koruwen","Nyxarik","Zarosil","Azlax",
        "Belabos","Karovan","Graneth","Vexuroth","Vexuax","Nyxeara","Korosil","Drakesil",
        "Drakarel","Ravlivol","Ereneth","Vexnoth","Gralath","Morathar","Drakegrim","Azara",
        "Malael","Karlarn","Koremoth","Azzar" };
    return d;
}
const QStringList& placeFantasy() {
    static const QStringList d = {
        "Eldoria","Valmoor","Brighthold","Ravenmark","Stormgard","Thornwood","Greycliff",
        "Ashfall","Silvermere","Dawnhold","Frosthelm","Duskmoor","Ironvale","Mistral",
        "Highmoor","Blackwater","Goldcrest","Stonereach","Wyrmrest","Emberfall","Westmarch",
        "Oakhaven","Shadowfen","Brackenmoor","Sunhollow","Northwatch","Grimhold","Lakeshire",
        "Southkeep","Wraithrest","Foxfen","Grimreach","Boarrest",
        "Marshbury","Marshwick","Greenrest","Slatethorpe","Wraithgarth","Brackenfen",
        "Birchhaven","Copperfall","Brinegard","Sablehaven","Lakereach",
        "Coppermoor","Greyhaven","Valeford",
        "Ferrousgate","Bearbury","Wispmark","Amberthorpe","Timberwood","Bearspring",
        "Glassglade","Foxspire","Mistshade","Birchgate","Spectrekeep","Petalbury",
        "Northwick","Starhallow","Ivoryspan","Yewhearth",
        "Peatbrake","Peathollow","Rowanhelm","Redcliff","Valeshire",
        "Coralhaven","Wraithspire","Umbravale","Starcross","Wildvale","Hazelglade",
        "Boughhearth","Shadowridge","Moonshade","Thistlebrake","Starthorpe",
        "Duskwatch","Wraithshade","Moonstead","Sableshire","Windhold","Falconbarrow",
        "Wispmere","Amberholt","Suncross","Mossbridge","Sablefall","Longguard",
        "Stagridge","Spectrerest","Blackward","Flintmoor","Westbrook",
        "Firefield","Cedarshade","Ashthorpe","Harthold","Marshcross","Bronzevale",
        "Talonbridge","Valehollow","Sunbury","Boarmoor","Deephallow","Cedarbridge",
        "Vesperreach","Duskport","Sablemere","Sunvale","Westkeep",
        "Misthearth","Dawnspire","Marrowhold","Dawnspan","Sablehollow","Fernhold",
        "Sablegard","Southvale","Moonmoor","Wyrmmark","Bronzebrake","Bronzeholt","Craghfen",
        "Hillhelm","Ashhallow","Frostport","Fallowbarrow","Rimewick",
        "Elmfen","Elderglen","Staghollow","Galewatch","Greydale","Hearthbrake","Ivyspire",
        "Mireglen","Banehaven","Crystalgard","Craghcrest","Windward","Highmere","Wildspan",
        "Snowreach","Brambleglen","Nightshade","Valerest","Wyrmglen","Thornglade",
        "Brinefurrow","Silverwick","Sablemoor","Ivyglen","Harrowford","Argentport",
        "Verdantfen","Goldgard","Brighthollow","Fencross",
        "Crystalrest","Obsidianhearth","Valehearth","Nettleguard","Boughridge","Boarfield",
        "Wraithfall","Wolfcliff","Embermere","Bloomhollow","Bloomfen","Sablespring",
        "Crystalhallow","Sunglen","Chalkvale","Greenfall","Banebrake","Alderkeep",
        "Thorncrest","Rockfall","Lumenspire","Onyxcliff","Stagthorpe",
        "Shalegarth","Gravelbrook","Greenmere","Onyxfield","Loamreach",
        "Northholt","Blightreach","Galeshade","Shadowbarrow","Hollowcross",
        "Longwatch","Lakegarth","Foxglade","Solacefurrow","Alderford","Valeridge",
        "Wrenbarrow","Ironfield","Redreach","Harrowhold","Peathelm","Cobaltvale",
        "Kindlefall","Ravenbarrow","Falconglen","Willowspring","Silverguard",
        "Blightwatch","Deepglade","Squallmark","Valegate","Wintershade","Ironfurrow",
        "Ashward","Hazelfield","Wildford","Petalrest","Beardale","Solacespan","Stonelight",
        "Pearllight","Silverbury","Mossport","Summermoor","Greyfurrow","Aldermoor",
        "Eaststead","Wildholt","Bramblereach","Pearlfield","Crystalglen","Obsidiandale",
        "Windlight","Redford","Brackenguard","Hazelstead","Shadowhearth","Eastgate",
        "Loamwatch","Bronzehollow","Windspan","Winterreach","Eastgard","Rockhelm","Foxlight",
        "Elmgarth","Roothaven","Obsidianfen","Rockward","Marrowspan","Firestead","Bloomfall",
        "Talonfield","Ironkeep","Frostguard","Granitegate","Wispcliff","Glassgard",
        "Gravelcross","Harrowward","Wraithspan","Falconrest","Elmfurrow",
        "Mistmere","Birchrest","Brambleshade","Flintbrook","Rowanshade","Banerest",
        "Deepthorpe","Southmere","Copperbridge","Wrenfurrow","Quartzreach",
        "Boughvale","Talongarth","Ravenscar","Duskholt","Fenwatch","Grimspan",
        "Hollowmere","Larkspur","Oakenfell","Palegate","Rimewatch","Saltmarsh",
        "Thistlewick","Umberfall","Vinehallow","Wickmoor","Yewbrake","Ashenvale",
        "Briarfell","Cragmoor","Drearford","Emberwick" };
    return d;
}
const QStringList& placeNordic() {
    static const QStringList d = {
        "Trondheim","Bergvik","Asgard","Eldnvik","Skarholm","Vidaheim","Frosthavn",
        "Ulfgard","Norvik","Hadenfjord","Ravnsfell","Stormvik","Brynholm","Eikgard",
        "Sundholm","Valdheim","Hjarvik","Drakfjord","Isgard","Solvik","Thornvik",
        "Fjellstrom","Sigborg","Hakonvoll","Ravnheim","Vornstad",
        "Rurikgard","Vindalstrom","Skarnesheim","Stormsund","Bergvoll",
        "Drakstrom","Malmnes","Nordfell","Karlhall","Vetrsund","Lundhavn",
        "Vorndal","Sorheimvik","Sudfjord","Frostviknes","Vindalnes","Valdvik",
        "Snarstrand","Skjoldgaard","Myrkhavn","Skarsund","Thornmark","Skogfjord","Fjellgard",
        "Hjarmark","Isfjorstad","Ulfvoll","Halfdanfell",
        "Isfjordal","Sigdal","Bjorngaard","Ragnstrom","Hallmark","Bakkehavn","Solfell",
        "Ragnhavn","Torsteinstrand","Frostberg","Brumark","Nattnes","Stenvoll","Stengaard",
        "Falkgard","Eikholm","Hakonstad","Isfjornes","Vetrhall","Dagfell","Kragfell",
        "Stenmark","Hildeheim","Ymirvik","Toralvstad","Grimfell","Rurikhavn",
        "Svarhavn","Fjorhavn","Fjorvaag","Vindalstrand","Ravnnes","Fjellhall","Vetrholm",
        "Vardheimhall","Elvfell","Ulfstad","Stenholm","Bjornberg","Myrkstrand","Ravnborg",
        "Halvardholm","Hovdestad","Ulfheim","Sudvaag","Torvstrand",
        "Bjargardsund","Halfdanhavn","Bakkeheim","Vetrfjord","Drakmark","Frostgaard",
        "Snorvoll","Bjorngard","Lundstrom","Yrsdal","Vannborg","Dagstrand","Draksund",
        "Sudvoll","Isfjorhall","Dagvikheim","Dagvikdal","Yrsstrom","Fenvik",
        "Regnfjord","Sjones","Mosegaard","Dalholm","Bergborg","Snardal","Sudgaard",
        "Vannvaag","Nordgaard","Fjellfjord","Natthall","Kaldheimhall","Dalvaag","Vornhall",
        "Erlendfell","Malmdal","Sudgard","Halvardstad","Vindfellstad","Solmarkhavn",
        "Grimnesstad","Borgstrom","Ragnvaag","Mosehall","Skjarholm","Svardal",
        "Ravnarhavn","Hjarstrand","Thorngaard","Borgfell","Myrkgard","Stromgard","Grimvoll",
        "Mosemark","Torvvik","Tindgard","Falkstad","Myrkborg","Grimgard","Rurikborg",
        "Sorheimhall","Lundfell","Hildedal","Regnmark","Vannstrom","Regnfell","Tindvik",
        "Bruvik","Rurikfell","Torvberg","Fensund","Sudvik","Valddal","Halvardhavn",
        "Malmsund","Ornvaag","Borgnes","Vindrfell","Oddstad","Brandsund","Kjellhall",
        "Vetrheim","Mosehavn","Kaldheimstad","Karnholm","Hallvik","Myrksund","Stenfell",
        "Solmarkstad","Nattstrand","Skaldsund","Vindfellhall","Nordfellstrom","Nordfellgaard",
        "Torsteinfjord","Toralvnes","Ornheim","Hovdeborg","Vindrgard","Bjartberg",
        "Vornfjord","Rurikberg","Bardstrom","Bjargardstad","Yrsfjord","Vindfellheim",
        "Skjolddal","Toralvstrand","Bergvaag","Bjornmark","Falknes","Hakonstrand",
        "Kjellvaag","Dagvikhavn","Halfdanvoll","Malmfell","Myrkberg","Kaldhavn","Skargard",
        "Regnstrom","Skoggard","Nordvaag","Frostvikfell","Nordfellborg","Gunnhall",
        "Bjornstrom","Isfjord","Sigvoll","Kaldheimmark","Yrsborg","Torsteingard","Yrsvoll",
        "Snarheim","Grimnesheim","Ymirgaard","Torvikvaag","Ingridstrand",
        "Ymirhall","Kjellheim","Ymirfell","Torvvoll","Nordstad","Nordfellgard",
        "Vinnes","Erlendvaag","Skjoldfjord","Thornheim","Kaldheimvaag",
        "Kaldstad","Ravnstad","Snarsund","Vannfell","Vetrgaard","Snarberg","Barddal",
        "Rurikstrom","Dagvikvoll","Nordfellsund","Vindfellvik","Dagborg","Bardvoll",
        "Stormvaag","Halfdannes","Ingridsund","Skogstad","Hovdeholm","Skogborg","Erlendstad",
        "Snarfell","Dagvikstad","Vindfellborg","Brandborg","Sjostrom","Yrsvaag","Ulfberg",
        "Myrkfell","Lundheim","Skogsund","Fjellvik","Karnheim","Karlfjord","Frostvikstad",
        "Vindrberg","Karlmark","Skaldgard","Bergstrom","Borgvoll","Nordholm","Skarnesfjord",
        "Ravnfjell","Solheimstad","Bjarkvik","Hallnes","Torvbrekka","Eiriksstad",
        "Gunnvoll","Malmvik","Dragstrand","Bjornholm","Vetrnes","Ulvsfjord" };
    return d;
}

const QStringList& female() {
    static const QStringList d = {
        // Lusófonos
        "Sandra","Mariana","Juliana","Adriana","Helena","Beatriz","Camila","Larissa",
        "Fernanda","Gabriela","Isabela","Carolina","Leticia","Patricia","Vanessa","Bianca",
        "Renata","Daniela","Luana","Marina","Amanda","Carla","Aline","Viviane",
        "Cristina","Monica","Simone","Tatiana","Priscila","Fabiana","Roberta","Claudia",
        "Regina","Silvia","Rosana","Debora","Eliane","Karina",
        // Anglo
        "Sarah","Emily","Olivia","Emma","Charlotte","Grace","Hannah","Chloe",
        "Victoria","Samantha","Jessica","Lauren","Natalie","Abigail","Sophie","Audrey",
        "Eleanor","Amelia","Isabella","Mia","Ava","Ella","Scarlett","Zoe",
        "Lily","Megan","Rachel","Rebecca","Laura","Katherine","Elizabeth","Amy",
        "Julia","Claire","Holly","Nicole","Stephanie","Kayla",
        // Italianos
        "Alessandra","Cassandra","Francesca","Giulia","Chiara","Valentina","Martina","Elisa",
        "Federica","Serena","Giorgia","Ilaria","Beatrice","Vittoria","Simona","Paola",
        "Sara","Anna","Teresa","Caterina","Veronica","Rosa","Angela","Emilia",
        "Livia","Ottavia","Gaia","Marta","Alice","Costanza","Ludovica","Arianna",
        "Bruna","Elisabetta","Donatella","Antonella","Raffaella",
        // Franceses
        "Camille","Amelie","Juliette","Celine","Manon","Elodie","Margot","Sylvie",
        "Noemie","Aurelie","Chantal","Nathalie","Veronique","Isabelle","Monique","Josephine",
        "Adele","Colette","Delphine","Estelle","Florence","Genevieve","Helene","Jacqueline",
        "Louise","Madeleine","Odette","Pascale","Renee","Sidonie","Solange","Valerie",
        "Yvette","Blanche","Corinne","Dominique","Francoise",
        // Espanhóis
        "Lucia","Sofia","Carmen","Elena","Isabel","Paloma","Ines","Pilar",
        "Rocio","Alicia","Blanca","Catalina","Cecilia","Consuelo","Dolores","Esperanza",
        "Eva","Gloria","Guadalupe","Josefina","Juana","Leonor","Manuela","Marisol",
        "Mercedes","Milagros","Natalia","Nuria","Ana","Angeles","Antonia","Belen",
        "Candela","Clara","Concepcion","Dulce","Fatima",
        // Germânicos / nórdicos
        "Greta","Ingrid","Sabine","Brigitte","Astrid","Sigrid","Freya","Solveig",
        "Heidi","Ursula","Gertrude","Helga","Birgit","Karin","Monika","Inge",
        "Gisela","Hildegard","Petra","Ute","Frieda","Liesel","Annika","Erika",
        "Signe","Ingeborg","Kirsten","Britt","Maja","Elin","Linnea","Saga",
        "Alva","Wilma","Tove","Karen","Marlene","Gudrun",
        // Eslavos
        "Natasha","Olga","Svetlana","Katarina","Milena","Vesna","Anja","Ludmila",
        "Anastasia","Ekaterina","Irina","Yelena","Tatyana","Larisa","Galina","Zlata",
        "Vera","Nina","Yana","Oksana","Ivana","Bojana","Jovana","Danica",
        "Snezana","Dragana","Jelena","Zorica","Radmila","Slavica","Mira","Tamara",
        "Zora","Iryna","Kateryna","Halyna","Lesya","Zinaida",
        // Gregos / outros
        "Athena","Daphne","Penelope","Thalia","Calliope","Layla","Yasmin","Amira",
        "Nadia","Alexandra","Lisandra","Aurora","Sophia","Eleni","Ariadne","Elektra",
        "Xanthe","Ismene","Melina","Despina","Irini","Katerina","Selin","Amara",
        "Leila","Zahra","Aisha","Samira","Nadira","Rania","Dalia","Farah",
        "Hana","Noor","Yara","Maya","Salma" };
    return d;
}
const QStringList& male() {
    static const QStringList d = {
        // Lusófonos
        "Lucas","Gabriel","Mateus","Rafael","Daniel","Pedro","Felipe","Bruno",
        "Thiago","Rodrigo","Gustavo","Leonardo","Vinicius","Eduardo","Marcelo","Ricardo",
        "Fernando","Carlos","Henrique","Caio","Murilo","Davi","Andre","Leandro",
        "Roberto","Cesar","Otavio","Renato","Fabio","Guilherme","Marcos","Paulo",
        "Igor","Adriano","Vitor","Emerson","Wagner","Anderson",
        // Anglo
        "James","William","Oliver","Henry","Jack","Thomas","George","Edward",
        "Charles","Benjamin","Samuel","Nathan","Oscar","Harry","Dylan","Theodore",
        "Walter","Michael","David","John","Robert","Christopher","Matthew","Andrew",
        "Joseph","Ryan","Nicholas","Tyler","Brandon","Justin","Jonathan","Kevin",
        "Jason","Aaron","Adam","Ethan","Noah","Liam",
        // Italianos
        "Marco","Luca","Matteo","Lorenzo","Giovanni","Alessandro","Francesco","Antonio",
        "Riccardo","Stefano","Enzo","Giuseppe","Salvatore","Vincenzo","Domenico","Paolo",
        "Fabrizio","Massimo","Claudio","Emanuele","Gianluca","Simone","Dario","Cristiano",
        "Alberto","Umberto","Renzo","Piero","Carmine","Gennaro","Raffaele","Ettore",
        "Ivo","Nico","Silvio","Tommaso","Edoardo",
        // Franceses
        "Pierre","Julien","Antoine","Nicolas","Mathis","Hugo","Louis","Etienne",
        "Olivier","Jean","Michel","Philippe","Jacques","Francois","Claude","Bernard",
        "Gerard","Alain","Christophe","Laurent","Patrick","Thierry","Didier","Pascal",
        "Serge","Yves","Marcel","Henri","Maurice","Rene","Gaston","Emile",
        "Fabrice","Sebastien","Vincent","Xavier","Baptiste",
        // Espanhóis
        "Diego","Javier","Alejandro","Pablo","Mateo","Sergio","Manuel","Andres",
        "Jose","Juan","Luis","Francisco","Jorge","Ramon","Emilio","Ernesto",
        "Ignacio","Joaquin","Gonzalo","Salvador","Esteban","Federico","Hector","Julio",
        "Leonel","Nestor","Osvaldo","Ruben","Santiago","Tomas","Vicente","Cristobal",
        "Guillermo","Humberto","Martin","Raul","Rolando",
        // Germânicos / nórdicos
        "Hans","Klaus","Friedrich","Stefan","Felix","Otto","Wolfgang","Bjorn",
        "Erik","Lars","Sven","Magnus","Olaf","Gunnar","Axel","Finn",
        "Dieter","Helmut","Werner","Gunther","Manfred","Uwe","Jurgen","Dietrich",
        "Reinhard","Ingo","Karsten","Bernd","Frank","Holger","Ralf","Volker",
        "Wilhelm","Kurt","Gerhard","Heinrich","Ludwig","Nils",
        // Eslavos
        "Dimitri","Ivan","Boris","Sergei","Vladimir","Mikhail","Nikolai","Pavel",
        "Andrei","Yuri","Roman","Viktor","Maxim","Denis","Oleg","Konstantin",
        "Leonid","Grigori","Anatoly","Vasily","Semyon","Timur","Ruslan","Bogdan",
        "Miroslav","Stanislav","Milos","Vuk","Goran","Dragan","Slobodan","Aleksandar",
        "Petar","Marko","Luka","Tomislav","Vladislav","Yaroslav",
        // Gregos / outros
        "Nikos","Stavros","Theo","Alexios","Omar","Khalid","Yusuf","Karim",
        "Hassan","Alexandre","Miguel","Dimitris","Yannis","Kostas","Petros","Vasilis",
        "Georgios","Ioannis","Andreas","Christos","Spyros","Panagiotis","Achilles","Orestes",
        "Ali","Ahmed","Mohammed","Tariq","Rashid","Farid","Jamal","Nasser",
        "Malik","Rayan","Zayd","Bilal","Amir" };
    return d;
}

const QStringList& datasetFor(const QString& id) {
    static const QStringList empty;
    if (id == QStringLiteral("female"))      return female();
    if (id == QStringLiteral("male"))        return male();
    if (id == QStringLiteral("elvish"))      return elvish();
    if (id == QStringLiteral("nordic"))      return nordic();
    if (id == QStringLiteral("latin"))       return latin();
    if (id == QStringLiteral("slavic"))      return slavic();
    if (id == QStringLiteral("dark"))        return dark();
    if (id == QStringLiteral("place_fantasy")) return placeFantasy();
    if (id == QStringLiteral("place_nordic"))  return placeNordic();
    return empty;
}

QString capitalize(const QString& s) {
    if (s.isEmpty()) return s;
    return s.left(1).toUpper() + s.mid(1);
}

QString pickStr(const QStringList& v) {
    return v.at(QRandomGenerator::global()->bounded(v.size()));
}

} // namespace

QVector<NameGenerator::Style> NameGenerator::characterStyles() {
    // NameGenerator não é QObject (sem Q_OBJECT) — QCoreApplication::translate
    // com contexto explícito funciona em qualquer função, sem depender de tr()
    // herdado (que causaria o mesmo bug de contexto já visto em BookCard/BookRow,
    // ver translationmemories.md).
    return {
        {QStringLiteral("real"),   QCoreApplication::translate("NameGenerator", "Pessoa")},
        {QStringLiteral("elvish"), QCoreApplication::translate("NameGenerator", "Élfico")},
        {QStringLiteral("nordic"), QCoreApplication::translate("NameGenerator", "Nórdico")},
        {QStringLiteral("latin"),  QCoreApplication::translate("NameGenerator", "Latino")},
        {QStringLiteral("slavic"), QCoreApplication::translate("NameGenerator", "Eslavo")},
        {QStringLiteral("dark"),   QCoreApplication::translate("NameGenerator", "Sombrio")},
    };
}

QVector<NameGenerator::Style> NameGenerator::placeStyles() {
    return {
        {QStringLiteral("place_fantasy"), QCoreApplication::translate("NameGenerator", "Fantasia")},
        {QStringLiteral("place_nordic"),  QCoreApplication::translate("NameGenerator", "Nórdico")},
    };
}

void NameGenerator::ensureTrained(const QString& styleId) {
    if (m_models.contains(styleId)) return;

    QVector<QHash<QString, QString>> models(kMaxOrder); // índice = ordem-1
    const QStringList& data = datasetFor(styleId);
    for (const QString& raw : data) {
        const QString w = raw.toLower();
        if (w.isEmpty()) continue;
        for (int o = 1; o <= kMaxOrder; ++o) {
            const QString padded = QString(o, QChar('^')) + w + kEnd;
            for (int i = o; i < padded.size(); ++i) {
                const QString ctx = padded.mid(i - o, o);
                models[o - 1][ctx].append(padded.at(i));
            }
        }
    }
    m_models.insert(styleId, models);
}

QString NameGenerator::markovOne(const QString& styleId, const QString& seed) {
    const auto it = m_models.constFind(styleId);
    if (it == m_models.constEnd()) return QString();
    const QVector<QHash<QString, QString>>& models = it.value();

    QString out = seed; // início plantado (já em minúsculas pelo chamador)
    for (int guard = 0; guard < 40; ++guard) {
        const QString full = QString(kMaxOrder, QChar('^')) + out;
        QChar next;
        bool found = false;
        // Back-off: tenta a maior ordem disponível pro contexto atual.
        for (int o = kMaxOrder; o >= 1; --o) {
            const QString ctx = full.right(o);
            const auto mit = models[o - 1].constFind(ctx);
            if (mit != models[o - 1].constEnd() && !mit.value().isEmpty()) {
                const QString& bag = mit.value();
                next = bag.at(QRandomGenerator::global()->bounded(bag.size()));
                found = true;
                break;
            }
        }
        if (!found || next == kEnd) break;
        out.append(next);
        if (out.size() >= 12) break;
    }
    return out;
}

QStringList NameGenerator::generate(const QString& styleId, int count,
                                    const QString& prefix, const QString& suffix) {
    ensureTrained(styleId);

    const QString pfx = prefix.trimmed().toLower();
    const QString sfx = suffix.trimmed().toLower();

    // Conjunto do dataset (lower) pra evitar "copiar" um nome de treino.
    QSet<QString> seeds;
    for (const QString& s : datasetFor(styleId)) seeds.insert(s.toLower());

    QStringList result;
    QSet<QString> seen;
    int attempts = 0;
    const int maxAttempts = count * 80 + 400;
    while (result.size() < count && attempts++ < maxAttempts) {
        QString w;
        if (sfx.isEmpty()) {
            w = markovOne(styleId, pfx);
            if (w.size() < 3 || w.size() > 14) continue;
            if (seeds.contains(w)) continue; // não "copiar" um nome de treino
        } else {
            // Garante o final: gera um começo no estilo (respeitando o prefixo,
            // se houver) e cola o sufixo pedido. Sempre produz. A faixa larga de
            // comprimento do começo (2..7) é o que dá variedade aos resultados.
            QString body = markovOne(styleId, pfx);
            const int minBody = qMax(pfx.size(), 2);
            const int target = qMax(minBody, 2 + QRandomGenerator::global()->bounded(6)); // 2..7
            if (body.size() > target) body = body.left(target);
            if (body.size() < 2) continue;
            w = body + sfx;
            if (w.size() > 18) continue;
        }
        if (seen.contains(w)) continue;
        seen.insert(w);
        result.append(capitalize(w));
    }
    return result;
}

QStringList NameGenerator::generateWeapons(int count) {
    // "Kit PT-BR" de armas. Cada palavra carrega gênero/número pra concordância
    // (adjetivo + artigo contraído do/da/dos/das). Pra outro idioma no futuro,
    // basta um kit equivalente com seus moldes (a estrutura gramatical muda).
    struct Word { QString w; bool fem; bool plural; };
    struct Adj  { QString masc; QString fem; };

    static const QVector<Word> armas = {
        {QStringLiteral("Lâmina"), true, false},  {QStringLiteral("Espada"), true, false},
        {QStringLiteral("Foice"), true, false},   {QStringLiteral("Machado"), false, false},
        {QStringLiteral("Lança"), true, false},   {QStringLiteral("Adaga"), true, false},
        {QStringLiteral("Martelo"), false, false},{QStringLiteral("Cetro"), false, false},
        {QStringLiteral("Arco"), false, false},   {QStringLiteral("Gládio"), false, false} };
    static const QVector<Adj> adjs = {
        {QStringLiteral("Sombrio"), QStringLiteral("Sombria")},
        {QStringLiteral("Flamejante"), QStringLiteral("Flamejante")}, // invariável
        {QStringLiteral("Gélido"), QStringLiteral("Gélida")},
        {QStringLiteral("Maldito"), QStringLiteral("Maldita")},
        {QStringLiteral("Sagrado"), QStringLiteral("Sagrada")},
        {QStringLiteral("Eterno"), QStringLiteral("Eterna")},
        {QStringLiteral("Silencioso"), QStringLiteral("Silenciosa")},
        {QStringLiteral("Sangrento"), QStringLiteral("Sangrenta")},
        {QStringLiteral("Ancestral"), QStringLiteral("Ancestral")} }; // invariável
    static const QVector<Word> abstratos = {
        {QStringLiteral("Crepúsculo"), false, false}, {QStringLiteral("Alvorecer"), false, false},
        {QStringLiteral("Vazio"), false, false},      {QStringLiteral("Tormento"), false, false},
        {QStringLiteral("Silêncio"), false, false},   {QStringLiteral("Trovão"), false, false},
        {QStringLiteral("Esquecimento"), false, false},{QStringLiteral("Juízo"), false, false},
        {QStringLiteral("Abismo"), false, false},     {QStringLiteral("Inverno"), false, false},
        {QStringLiteral("Tempestade"), true, false},  {QStringLiteral("Penumbra"), true, false},
        {QStringLiteral("Ruína"), true, false},       {QStringLiteral("Aurora"), true, false} };
    static const QVector<Word> alvos = {
        {QStringLiteral("Almas"), true, true},   {QStringLiteral("Sangue"), false, false},
        {QStringLiteral("Mundos"), false, true}, {QStringLiteral("Reis"), false, true},
        {QStringLiteral("Deuses"), false, true}, {QStringLiteral("Sombras"), true, true},
        {QStringLiteral("Céus"), false, true},   {QStringLiteral("Titãs"), false, true} };
    static const QStringList verbos = {
        QStringLiteral("Fende"), QStringLiteral("Bebe"), QStringLiteral("Ceifa"),
        QStringLiteral("Parte"), QStringLiteral("Devora"), QStringLiteral("Rasga"),
        QStringLiteral("Queima") };

    // Artigo contraído "de + artigo" concordando com gênero e número.
    auto contr = [](const Word& w) {
        if (w.plural) return w.fem ? QStringLiteral("das") : QStringLiteral("dos");
        return w.fem ? QStringLiteral("da") : QStringLiteral("do");
    };
    auto pickW = [](const QVector<Word>& v) -> const Word& {
        return v.at(QRandomGenerator::global()->bounded(v.size()));
    };

    QStringList result;
    QSet<QString> seen;
    int attempts = 0;
    const int maxAttempts = count * 40 + 100;
    while (result.size() < count && attempts++ < maxAttempts) {
        QString name;
        switch (QRandomGenerator::global()->bounded(4)) {
        case 0: { // Lâmina Sombria / Machado Sombrio (adjetivo concorda com a arma)
            const Word& a = pickW(armas);
            const Adj& adj = adjs.at(QRandomGenerator::global()->bounded(adjs.size()));
            name = a.w + QStringLiteral(" ") + (a.fem ? adj.fem : adj.masc);
            break; }
        case 1: { // Foice do Crepúsculo / Lança da Tempestade
            const Word& a = pickW(armas);
            const Word& ab = pickW(abstratos);
            name = a.w + QStringLiteral(" ") + contr(ab) + QStringLiteral(" ") + ab.w;
            break; }
        case 2: // Fende-Almas (sem artigo, sem concordância)
            name = pickStr(verbos) + QStringLiteral("-") + pickW(alvos).w;
            break;
        default: { // Adaga das Almas / Espada dos Deuses / Lança do Sangue
            const Word& a = pickW(armas);
            const Word& t = pickW(alvos);
            name = a.w + QStringLiteral(" ") + contr(t) + QStringLiteral(" ") + t.w;
            break; }
        }
        if (seen.contains(name)) continue;
        seen.insert(name);
        result.append(name);
    }
    return result;
}
