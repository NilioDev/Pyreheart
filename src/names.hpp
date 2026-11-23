#pragma once
// Random hero names: human first names of the cold north, plain ASCII (plan.md, 2026-11-23).
// No bynames. Leave out the names of Charnfen's own people (Ysolde, Brann, Ebb) and of the unique foes.
// Add or remove names freely: the list is read as it is. Keep a name at 11 letters or fewer, so that
// "Name XVIII" still fits the 16 letters of the name screen.
#include <random>
#include <set>
#include <string>
#include <vector>

namespace names {

inline const std::vector<std::string> HERO = {
    "Aasa", "Abbo", "Adalbert", "Adalind", "Adalric", "Adela", "Adeline", "Aedan", "Aedre", "Aeldra",
    "Aelfric", "Aelfwyn", "Aelgifu", "Aelith", "Aelred", "Aethelflaed", "Aethelwulf", "Agathe", "Agilulf",
    "Agmund", "Agna", "Agnes", "Aiken", "Ailith", "Alard", "Alaric", "Alban", "Albrecht", "Alcuin", "Alda",
    "Aldgyth", "Aldhelm", "Aldis", "Aldith", "Aldo", "Aldreda", "Aldric", "Aldwin", "Alfdis", "Alfeid",
    "Alfgar", "Alfhere", "Alfhild", "Alfrun", "Algar", "Almund", "Alruna", "Alva", "Alvar", "Alwen", "Alys",
    "Ama", "Amalia", "Amice", "Amund", "Anlaf", "Annis", "Ansel", "Anselm", "Ansgar", "Ansgard", "Anund",
    "Anwen", "Ardith", "Arla", "Arn", "Arnald", "Arnbjorg", "Arnbjorn", "Arnfinn", "Arngerd", "Arngrim",
    "Arngunn", "Arnkel", "Arnor", "Arnora", "Arnulf", "Arvid", "Asbjorn", "Asdis", "Asger", "Asgerd",
    "Ashild", "Askel", "Asketil", "Aslaug", "Asmund", "Asny", "Asta", "Astrid", "Astridr", "Asvor",
    "Athelric", "Athelstan", "Aubrey", "Aud", "Audhild", "Audra", "Audric", "Audulf", "Audun", "Audwin",
    "Aveline", "Axel", "Baldric", "Baldwin", "Bard", "Bardolf", "Barric", "Barthold", "Bartold", "Bastian",
    "Beatrix", "Bebba", "Beda", "Belda", "Benedek", "Bennet", "Benno", "Beorhtric", "Beorn", "Bera", "Berend",
    "Berengar", "Bergit", "Bergljot", "Berit", "Berold", "Bersi", "Bertha", "Berthold", "Bertil", "Bertrada",
    "Bertram", "Bettrys", "Birger", "Birgit", "Bjarni", "Bjorg", "Bjorn", "Blaise", "Blanche", "Bodil",
    "Bodo", "Bodulf", "Bodvar", "Bodvild", "Bolli", "Borghild", "Borin", "Bothvar", "Botolf", "Bram", "Brand",
    "Brandur", "Brannor", "Branwen", "Brede", "Brenna", "Brennan", "Brenric", "Brigid", "Briony", "Brisa",
    "Brock", "Brokk", "Bronwen", "Brunhild", "Bryda", "Brynhild", "Brynja", "Brynjolf", "Burchard",
    "Burkhard", "Caddoc", "Cadell", "Cadfael", "Cadoc", "Caedmon", "Cael", "Caitrin", "Caius", "Calder",
    "Calla", "Calum", "Cara", "Caradoc", "Carin", "Carl", "Carrick", "Carsten", "Carys", "Caspar", "Catla",
    "Catrin", "Cecilie", "Cecily", "Cedric", "Ceinwen", "Celyn", "Cenric", "Cenwulf", "Ceolric", "Ceolwulf",
    "Cerdic", "Ceridwen", "Cinnia", "Clara", "Clemence", "Coira", "Colban", "Coll", "Colm", "Conal", "Conn",
    "Conrad", "Corbin", "Corentin", "Cormac", "Corra", "Corran", "Corvin", "Corwin", "Cressida", "Cuthbert",
    "Cwen", "Cyneburg", "Cyneric", "Cynric", "Cyra", "Dag", "Dagfinn", "Dagfrid", "Dagmar", "Dagmund",
    "Dagna", "Dagny", "Dagr", "Dagrid", "Dagrun", "Dain", "Dalia", "Dalibor", "Dalla", "Dalric", "Dalvin",
    "Dankrad", "Danr", "Darian", "Darrel", "Davor", "Delphine", "Delwyn", "Deorwin", "Derrick", "Dervla",
    "Derwen", "Desmond", "Dirk", "Dirna", "Disa", "Doran", "Dorota", "Dorran", "Dorte", "Dorthe", "Dregan",
    "Drest", "Drifa", "Drogo", "Duncan", "Dunstan", "Durstan", "Dyre", "Dyrfinna", "Dyveke", "Eadburh",
    "Eadgar", "Eadgyth", "Eadmund", "Eadric", "Eadwulf", "Eamon", "Eanflaed", "Eardwulf", "Ebba", "Eberhard",
    "Eckhart", "Ector", "Edda", "Edelgard", "Edelina", "Edgar", "Edith", "Edla", "Edmund", "Edric", "Edwin",
    "Egbert", "Egil", "Egon", "Eilif", "Eilika", "Eilwen", "Einar", "Eira", "Eirik", "Eirlys", "Eldbjorg",
    "Eldgrim", "Eldor", "Eldred", "Eldric", "Eldrid", "Elfa", "Elfled", "Elfrida", "Elin", "Ellery",
    "Ellinor", "Ellisif", "Elowen", "Elric", "Elsa", "Elspeth", "Eluned", "Elva", "Embla", "Emer", "Emeric",
    "Emma", "Emmerich", "Emrys", "Engel", "Engelbert", "Engelina", "Engla", "Erik", "Erland", "Erlend",
    "Erling", "Ermengard", "Ermentrude", "Ermina", "Erna", "Ernulf", "Erwin", "Esa", "Esben", "Esger",
    "Esmond", "Estrid", "Ethelind", "Evrard", "Ewald", "Ewan", "Eydis", "Eystein", "Eyvind", "Falgar", "Falk",
    "Falko", "Fara", "Faramond", "Farold", "Fastny", "Fastolf", "Fastrada", "Felda", "Feldric", "Fenella",
    "Fenja", "Fenna", "Fenric", "Fenrik", "Fenwick", "Feodor", "Feran", "Fergus", "Ferna", "Ffion", "Fianna",
    "Finn", "Finnar", "Fionnuala", "Fiska", "Fiske", "Fjola", "Flora", "Flosi", "Folke", "Folkmar", "Folkvar",
    "Freja", "Frey", "Freya", "Freydis", "Frida", "Fridda", "Frideswide", "Fridgerd", "Fridleif", "Fridolf",
    "Fridrik", "Frigga", "Frode", "Frodi", "Frodis", "Fulk", "Gamel", "Gandulf", "Gardar", "Gareth", "Garm",
    "Garrick", "Garron", "Gaut", "Gaute", "Gautrek", "Gavin", "Geir", "Geirlaug", "Geirmund", "Gerald",
    "Gerda", "Gerhard", "Gerlind", "Gerlinde", "Gerolt", "Gerrit", "Gerulf", "Gerwin", "Gest", "Gilbert",
    "Gilles", "Gisa", "Gisbert", "Gisela", "Gisla", "Gisli", "Gizur", "Gjaflaug", "Glum", "Godfrey",
    "Godgifu", "Godric", "Godwin", "Godwulf", "Gorm", "Gormflaith", "Gottfried", "Greer", "Gregor", "Grete",
    "Grettir", "Grim", "Grimar", "Grimbald", "Grimhild", "Grimkel", "Grimwulf", "Groa", "Gudlaug", "Gudmund",
    "Gudny", "Gudrid", "Gudrun", "Gunhild", "Gunnar", "Gunnbjorg", "Gunnhild", "Gunnlaug", "Gunnvor",
    "Gunther", "Gunvor", "Guthlac", "Guthrum", "Guy", "Gwen", "Gwenllian", "Gwyneth", "Gyda", "Gyrth",
    "Gytha", "Hadric", "Haf", "Hafdis", "Hafgrim", "Hagan", "Hagen", "Haki", "Hakon", "Halbert", "Haldane",
    "Haldis", "Haldor", "Halfdan", "Halla", "Hallam", "Hallbjorn", "Halldis", "Halldora", "Hallfrid",
    "Hallgerd", "Hallvard", "Hallveig", "Halsten", "Halvard", "Hamar", "Hamlin", "Hamund", "Harald", "Harbin",
    "Harek", "Harlan", "Hartmut", "Hartwig", "Hartwin", "Hastein", "Hauk", "Hauskuld", "Hawise", "Hedda",
    "Hedric", "Hedwig", "Heidr", "Heidrun", "Heimo", "Helga", "Helgi", "Helka", "Helm", "Helvi", "Hemming",
    "Herbrand", "Herdis", "Hereward", "Heriot", "Hermod", "Herrick", "Hertha", "Herve", "Hervor", "Hesper",
    "Hilda", "Hilde", "Hildebrand", "Hildeburg", "Hildegard", "Hildulf", "Hildur", "Hjalmar", "Hjalti",
    "Hjordis", "Hlif", "Hodd", "Holt", "Horik", "Hoskuld", "Howel", "Hrafn", "Hrafnkel", "Hrani", "Hrefna",
    "Hroa", "Hrodgar", "Hrodna", "Hrodny", "Hrolf", "Hroswitha", "Hroth", "Hugh", "Hugo", "Hulda", "Humbert",
    "II", "Ida", "Idonea", "Idonia", "Idony", "Idris", "Ilfrid", "Illugi", "Ilse", "Inga", "Ingald", "Inge",
    "Ingeborg", "Ingeborga", "Ingibjorg", "Ingimund", "Ingjald", "Ingmar", "Ingolf", "Ingolv", "Ingram",
    "Ingrid", "Ingrith", "Ingrun", "Ingunn", "Ingvald", "Ingvar", "Ingveig", "Ingvild", "Inka", "Iona",
    "Irja", "Irmgard", "Irwin", "Isa", "Isabeau", "Isak", "Isarn", "Isbel", "Isgerd", "Isgrim", "Isla",
    "Isleif", "Isolde", "Isolt", "Ivald", "Ivar", "Ivelda", "Iver", "Ivo", "Ivor", "Izard", "Jakob", "Jalmar",
    "Jannik", "Jarand", "Jarek", "Jaren", "Jarl", "Jarna", "Jarngerd", "Jarngrim", "Jarvald", "Jarvis",
    "Jasper", "Jensine", "Jesper", "Jessa", "Jessamy", "Joachim", "Jofrid", "Jofrida", "Johanna", "Jokul",
    "Jokull", "Jolanda", "Jonas", "Jonna", "Jora", "Joran", "Jorgen", "Jorid", "Jorinde", "Jorund", "Jorunn",
    "Jorvald", "Jorvik", "Jory", "Joss", "Jostein", "Jovia", "Juna", "Jutta", "Jytte", "Kael", "Kaia", "Kaja",
    "Kalf", "Kalle", "Kamma", "Kari", "Karin", "Karla", "Karsten", "Kaspar", "Katla", "Keira", "Keld",
    "Kelda", "Kelric", "Kenna", "Kenning", "Kenric", "Kerensa", "Kerstin", "Kester", "Ketil", "Ketilbjorn",
    "Ketilridr", "Ketta", "Kirk", "Kjartan", "Kjell", "Kjersti", "Klara", "Klaus", "Knut", "Kolbein",
    "Kolbjorn", "Kolfinna", "Kolfrid", "Kolgrim", "Kolla", "Konrad", "Kormak", "Kort", "Kristin", "Kristof",
    "Kunigunde", "Kylla", "Lagi", "Laila", "Lambert", "Lambi", "Lambrecht", "Landric", "Lara", "Lars",
    "Lavena", "Leidolf", "Leif", "Leikny", "Lena", "Lenna", "Lennart", "Lennic", "Leofgar", "Leofric",
    "Leofrun", "Leofwin", "Leopold", "Liadan", "Liesel", "Lilja", "Lindorm", "Linn", "Linnea", "Linnet",
    "Lioba", "Lisbet", "Liudolf", "Liutgard", "Liv", "Ljot", "Ljotunn", "Ljufa", "Lleucu", "Lodin", "Lodmund",
    "Lofn", "Lorcan", "Lorin", "Lorna", "Lothair", "Lothar", "Lothric", "Lova", "Lowena", "Lucan", "Ludo",
    "Lufu", "Lynet", "Lyting", "Lyulf", "Mabyn", "Mael", "Maelis", "Magnar", "Magnus", "Mairead", "Malcolm",
    "Malin", "Manfred", "Mar", "Mardoc", "Marek", "Maren", "Margery", "Marit", "Marrit", "Marten", "Mathild",
    "Mathilde", "Mathis", "Mattias", "Maud", "Mauger", "Mechtild", "Melisent", "Meret", "Merrick", "Mervyn",
    "Mette", "Mikkel", "Mildred", "Modolf", "Moira", "Morcar", "Mord", "Mordaunt", "Morna", "Morten",
    "Morwen", "Nan", "Nanna", "Narve", "Nathair", "Ned", "Neil", "Nelda", "Nell", "Nerys", "Nessa", "Nesta",
    "Nevin", "Nia", "Niall", "Niamh", "Nidhild", "Nils", "Nilsine", "Ninian", "Nithard", "Njal", "Njord",
    "Noemi", "Nora", "Norbert", "Norgard", "Norna", "Norrin", "Norvald", "Notker", "Nykel", "Nyle", "Nyssa",
    "Oda", "Odd", "Oddbjorn", "Oddlaug", "Oddny", "Oddrun", "Oddvar", "Odela", "Odelia", "Odger", "Odile",
    "Odo", "Odvar", "Ofeig", "Olaf", "Olvir", "Olwen", "Ordulf", "Oriel", "Orla", "Orm", "Ormar", "Ormund",
    "Orvar", "Orwin", "Osbert", "Osgar", "Osgod", "Oskar", "Osmund", "Osric", "Oswald", "Oswin", "Oswyn",
    "Osyth", "Otkel", "Ottar", "Ottilia", "Ottilie", "Otto", "Ottoline", "Pallin", "Parvin", "Peder", "Pell",
    "Pella", "Pellam", "Pelle", "Pelleas", "Penda", "Pepin", "Percival", "Perdita", "Pernille", "Peronel",
    "Perrin", "Perrine", "Petra", "Petronel", "Petur", "Philippa", "Piers", "Pirkko", "Pollard", "Pomfret",
    "Prisca", "Pyrran", "Quarin", "Quenburga", "Quendel", "Quendrida", "Quenell", "Quenhild", "Quentin",
    "Querith", "Quillon", "Quinlan", "Quinna", "Quoren", "Radbod", "Radulf", "Rafa", "Rafe", "Rafnhild",
    "Ragna", "Ragnald", "Ragnar", "Ragnfrid", "Ragnheid", "Ragnhild", "Ragnvor", "Rainer", "Ralf", "Ranald",
    "Randal", "Randver", "Rannveig", "Rannvi", "Ranulf", "Ref", "Reginlind", "Reinhold", "Reyn", "Rhian",
    "Rhiannon", "Rhona", "Rhosyn", "Rhun", "Ricard", "Ricbert", "Richenza", "Rigmor", "Rigolf", "Rikard",
    "Rikke", "Riordan", "Roald", "Roar", "Roderic", "Rodger", "Rodric", "Rognvald", "Roland", "Rolf", "Rorik",
    "Rosalind", "Rosamund", "Rothild", "Rowan", "Rowena", "Rudolf", "Runa", "Rune", "Rurik", "Sabeth",
    "Sabine", "Sabra", "Saebjorn", "Saemund", "Saeunn", "Saga", "Sander", "Sassa", "Saxon", "Sebold", "Sela",
    "Selma", "Senna", "Seren", "Seward", "Sibyl", "Sievert", "Sif", "Sigbert", "Sigfast", "Sigga", "Sigmund",
    "Signe", "Signy", "Sigrfljod", "Sigrid", "Sigrun", "Sigthrud", "Sigtrygg", "Sigurd", "Sigurlin",
    "Sigvald", "Sigwald", "Sigyn", "Silas", "Silke", "Sindri", "Siv", "Skapti", "Skeggi", "Skuli", "Snorri",
    "Solmund", "Solrun", "Solveig", "Solvi", "Sorcha", "Soren", "Sorin", "Starkad", "Steinar", "Steinthor",
    "Steinunn", "Steinvor", "Stellan", "Sten", "Stig", "Sturla", "Styr", "Sunna", "Sunniva", "Svala", "Svana",
    "Svanhild", "Svanlaug", "Svein", "Sven", "Swanhild", "Sweyn", "Swithin", "Taddeo", "Talwyn", "Tamlin",
    "Tamsin", "Tancred", "Tangwystl", "Tarn", "Tegan", "Teodor", "Thane", "Thangbrand", "Theobald", "Theron",
    "Thiodolf", "Thjodhild", "Thora", "Thorald", "Thorbern", "Thorbjorg", "Thordis", "Thorgerd", "Thorgrim",
    "Thorhild", "Thorin", "Thorkel", "Thorlaug", "Thormod", "Thorstein", "Thorunn", "Thorvald", "Thrain",
    "Thrand", "Thurid", "Thurstan", "Thyra", "Thyri", "Tilda", "Tobin", "Tofa", "Tora", "Torben", "Tordis",
    "Torgil", "Torold", "Torsten", "Torunn", "Torvik", "Tostig", "Tove", "Tristan", "Trygve", "Tuva", "Tyr",
    "Tyra", "Ubbe", "Udela", "Uffe", "Ugo", "Uhtred", "Ulbrand", "Ulf", "Ulfa", "Ulfar", "Ulfgar", "Ulfhild",
    "Ulfkel", "Ulfrik", "Ulfrun", "Ulla", "Ulmar", "Ulric", "Ulrich", "Ulrik", "Ulrika", "Ulrikke", "Ulvar",
    "Ulvhild", "Ulvhilde", "Una", "Undine", "Unna", "Unni", "Unnr", "Unwen", "Unwin", "Urban", "Urd", "Urien",
    "Urrik", "Ursa", "Ursel", "Ursula", "Uta", "Uthred", "Uvelin", "Uwe", "Vagn", "Vala", "Valborg", "Valda",
    "Valdis", "Valgard", "Valgerd", "Vali", "Valla", "Valtyr", "Vanadis", "Vandil", "Vanja", "Vardan",
    "Vardr", "Varin", "Veda", "Vedis", "Vegard", "Veikko", "Velda", "Vemund", "Vendela", "Venn", "Verena",
    "Vermund", "Verna", "Vesla", "Vestein", "Vibeke", "Vidar", "Vidkun", "Vigdis", "Vigdisa", "Vigfus",
    "Vigga", "Vigleik", "Vilborg", "Viljar", "Vilmar", "Vinther", "Viveka", "Volker", "Volund", "Vora",
    "Wace", "Walburga", "Waldo", "Walter", "Waltheof", "Waltrud", "Warin", "Warrick", "Wendel", "Wendell",
    "Wendrin", "Wenna", "Werbert", "Werner", "Wiborada", "Wiglaf", "Wilda", "Wilfred", "Wilhelm",
    "Wilhelmina", "Willem", "Wilma", "Wilmar", "Wimund", "Winifred", "Wolfhard", "Wolfram", "Wolmar",
    "Wolter", "Wren", "Wulfe", "Wulfric", "Wulfrun", "Wulfstan", "Wynn", "Wystan", "Xandor", "Xandra",
    "Xaver", "Xeldis", "Xenna", "Xerold", "Xylda", "Xyrrel", "Yarrick", "Yarrow", "Ydris", "Ygerna", "Ylva",
    "Ynes", "Ynga", "Yngling", "Yngmar", "Yngvar", "Yngve", "Yngvild", "Yngvildr", "Yolande", "Yorath",
    "Yorick", "Yorke", "Yrjan", "Yrla", "Yrr", "Yrsa", "Yrsel", "Yrsela", "Yrsula", "Ysane", "Ysarn", "Ysbel",
    "Yseult", "Ysgard", "Ysmay", "Ysolda", "Ysolt", "Yvain", "Yvaine", "Yveline", "Yves", "Zarn", "Zeigard",
    "Zennor", "Zigmar", "Zigrun", "Zilla", "Zoren", "Zorna", "Zurik",
};

// 2 -> "II", 14 -> "XIV"
inline std::string roman(int n) {
  static const std::pair<int, const char*> R[] = {{1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"},
                                                {50, "L"}, {40, "XL"}, {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}};
  std::string s;
  for (auto& [v, r] : R) while (n >= v) { s += r; n -= v; }
  return s;
}

// A random name that no hero in the graves has carried yet. Once every name lies in the graves, a name comes
// back with the next number of its line: Aldric, then Aldric II, Aldric III... so the list never runs out.
inline std::string roll(const std::set<std::string>& taken, std::mt19937& rng, const std::string& not_this = "") {
  std::vector<const std::string*> free;
  for (auto& n : HERO) if (!taken.count(n) && n != not_this) free.push_back(&n);
  if (!free.empty()) return *free[std::uniform_int_distribution<size_t>(0, free.size() - 1)(rng)];
  for (;;) {
    const std::string& base = HERO[std::uniform_int_distribution<size_t>(0, HERO.size() - 1)(rng)];
    for (int i = 2;; ++i) {
      std::string n = base + " " + roman(i);
      if (n.size() > 16) break;  // this line is full: try another name
      if (!taken.count(n) && n != not_this) return n;
    }
  }
}

}  // namespace names
