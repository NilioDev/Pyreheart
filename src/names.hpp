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
    "Burkhard", "Cadell", "Cadfael", "Cadoc", "Caedmon", "Cael", "Caius", "Calder", "Cara", "Caradoc",
    "Carrick", "Carys", "Caspar", "Catrin", "Cecily", "Cedric", "Ceinwen", "Cenric", "Cenwulf", "Ceolric",
    "Cerdic", "Ceridwen", "Clemence", "Coll", "Colm", "Conrad", "Corbin", "Corentin", "Cormac", "Corran",
    "Corwin", "Cressida", "Cuthbert", "Cwen", "Cyneburg", "Cynric", "Dag", "Dagfinn", "Dagmar", "Dagny",
    "Dagr", "Dagrid", "Dagrun", "Dain", "Dalibor", "Dalla", "Dalric", "Dankrad", "Darian", "Davor", "Delwyn",
    "Derrick", "Dervla", "Derwen", "Desmond", "Dorota", "Dorran", "Dorte", "Drest", "Drifa", "Drogo",
    "Duncan", "Dunstan", "Durstan", "Dyre", "Dyrfinna", "Eadburh", "Eadgar", "Eadgyth", "Eadmund", "Eadric",
    "Eadwulf", "Eamon", "Eanflaed", "Eardwulf", "Ebba", "Eberhard", "Eckhart", "Ector", "Edda", "Edelgard",
    "Edelina", "Edgar", "Edith", "Edla", "Edmund", "Edric", "Edwin", "Egbert", "Egil", "Egon", "Eilif",
    "Eilika", "Eilwen", "Einar", "Eira", "Eirik", "Eirlys", "Eldbjorg", "Eldgrim", "Eldor", "Eldred",
    "Eldric", "Eldrid", "Elfa", "Elfled", "Elfrida", "Elin", "Ellery", "Ellinor", "Ellisif", "Elowen",
    "Elric", "Elsa", "Elspeth", "Eluned", "Elva", "Embla", "Emer", "Emeric", "Emma", "Emmerich", "Emrys",
    "Engel", "Engelbert", "Engelina", "Engla", "Erik", "Erland", "Erlend", "Erling", "Ermengard",
    "Ermentrude", "Ermina", "Erna", "Ernulf", "Erwin", "Esa", "Esben", "Esger", "Esmond", "Estrid",
    "Ethelind", "Evrard", "Ewald", "Ewan", "Eydis", "Eystein", "Eyvind", "Falk", "Falko", "Faramond",
    "Fastolf", "Fastrada", "Feldric", "Fenella", "Fenna", "Fenric", "Fenwick", "Feodor", "Fergus", "Ffion",
    "Finn", "Finnar", "Fionnuala", "Folke", "Folkvar", "Freya", "Freydis", "Frida", "Fridda", "Frideswide",
    "Fridgerd", "Fridleif", "Fridolf", "Frigga", "Frode", "Frodi", "Fulk", "Gamel", "Gandulf", "Gardar",
    "Gareth", "Garm", "Garrick", "Garron", "Gaut", "Gaute", "Gautrek", "Gavin", "Geir", "Geirlaug",
    "Geirmund", "Gerald", "Gerda", "Gerhard", "Gerlind", "Gerlinde", "Gerolt", "Gerrit", "Gerulf", "Gerwin",
    "Gest", "Gilbert", "Gilles", "Gisa", "Gisbert", "Gisela", "Gisla", "Gisli", "Gizur", "Gjaflaug", "Glum",
    "Godfrey", "Godgifu", "Godric", "Godwin", "Godwulf", "Gorm", "Gormflaith", "Gottfried", "Greer", "Gregor",
    "Grete", "Grettir", "Grim", "Grimar", "Grimbald", "Grimhild", "Grimkel", "Grimwulf", "Groa", "Gudlaug",
    "Gudmund", "Gudny", "Gudrid", "Gudrun", "Gunhild", "Gunnar", "Gunnbjorg", "Gunnhild", "Gunnlaug",
    "Gunnvor", "Gunther", "Gunvor", "Guthlac", "Guthrum", "Guy", "Gwen", "Gwenllian", "Gwyneth", "Gyda",
    "Gyrth", "Gytha", "Hadric", "Haf", "Hafdis", "Hafgrim", "Hagan", "Hagen", "Haki", "Hakon", "Halbert",
    "Haldane", "Haldis", "Haldor", "Halfdan", "Halla", "Hallam", "Hallbjorn", "Halldis", "Halldora",
    "Hallfrid", "Hallgerd", "Hallvard", "Hallveig", "Halsten", "Halvard", "Hamar", "Hamlin", "Hamund",
    "Harald", "Harbin", "Harek", "Harlan", "Hartmut", "Hartwig", "Hartwin", "Hastein", "Hauk", "Hauskuld",
    "Hawise", "Hedda", "Hedric", "Hedwig", "Heidr", "Heidrun", "Heimo", "Helga", "Helgi", "Helka", "Helm",
    "Helvi", "Hemming", "Herbrand", "Herdis", "Hereward", "Heriot", "Hermod", "Herrick", "Hertha", "Herve",
    "Hervor", "Hesper", "Hilda", "Hilde", "Hildebrand", "Hildeburg", "Hildegard", "Hildulf", "Hildur",
    "Hjalmar", "Hjalti", "Hjordis", "Hlif", "Hodd", "Holt", "Horik", "Hoskuld", "Howel", "Hrafn", "Hrafnkel",
    "Hrani", "Hrefna", "Hroa", "Hrodgar", "Hrodna", "Hrodny", "Hrolf", "Hroswitha", "Hroth", "Hugh", "Hugo",
    "Hulda", "Humbert", "Ida", "Idonea", "Idony", "Idris", "Inga", "Inge", "Ingeborg", "Ingibjorg",
    "Ingimund", "Ingjald", "Ingolf", "Ingolv", "Ingram", "Ingrid", "Ingrith", "Ingrun", "Ingunn", "Ingvar",
    "Ingveig", "Irmgard", "Irwin", "Isa", "Isabeau", "Isak", "Isbel", "Isgerd", "Isleif", "Isolde", "Isolt",
    "Ivar", "Ivo", "Ivor", "Jarek", "Jarl", "Jarngerd", "Jarvald", "Jarvis", "Jasper", "Jessamy", "Jofrid",
    "Jofrida", "Jokul", "Jora", "Jorid", "Jorund", "Jorunn", "Jorvik", "Jory", "Joss", "Jostein", "Jutta",
    "Kael", "Kaia", "Kalf", "Kari", "Karin", "Kaspar", "Kelda", "Kelric", "Kenna", "Kenric", "Kerensa",
    "Kester", "Ketil", "Ketilbjorn", "Ketilridr", "Ketta", "Kjell", "Knut", "Kolbein", "Kolfinna", "Kolgrim",
    "Kolla", "Kormak", "Kristin", "Kunigunde", "Lambert", "Lambi", "Lars", "Leif", "Lena", "Lennart",
    "Leofric", "Leofrun", "Leofwin", "Leopold", "Liadan", "Liesel", "Linn", "Linnea", "Lioba", "Liudolf",
    "Liutgard", "Liv", "Ljot", "Ljotunn", "Ljufa", "Lleucu", "Lodin", "Lofn", "Lorcan", "Lorin", "Lothar",
    "Lothric", "Lowena", "Lucan", "Ludo", "Lynet", "Lyulf", "Mabyn", "Mael", "Maelis", "Magnar", "Magnus",
    "Mairead", "Malcolm", "Malin", "Manfred", "Mar", "Mardoc", "Marek", "Maren", "Margery", "Marit", "Marrit",
    "Marten", "Mathild", "Mathilde", "Mathis", "Mattias", "Maud", "Mauger", "Mechtild", "Melisent", "Meret",
    "Merrick", "Mervyn", "Mette", "Mikkel", "Mildred", "Modolf", "Moira", "Morcar", "Mord", "Mordaunt",
    "Morna", "Morten", "Morwen", "Nan", "Nanna", "Narve", "Ned", "Nell", "Nerys", "Nessa", "Nesta", "Nia",
    "Nils", "Nithard", "Njal", "Nora", "Norbert", "Norrin", "Notker", "Oda", "Odd", "Oddbjorn", "Oddlaug",
    "Oddny", "Oddrun", "Oddvar", "Odela", "Odelia", "Odger", "Odile", "Odo", "Odvar", "Ofeig", "Olaf",
    "Olvir", "Olwen", "Ordulf", "Oriel", "Orla", "Orm", "Ormar", "Ormund", "Orvar", "Orwin", "Osbert",
    "Osgar", "Osgod", "Oskar", "Osmund", "Osric", "Oswald", "Oswin", "Oswyn", "Osyth", "Otkel", "Ottar",
    "Ottilia", "Ottilie", "Otto", "Ottoline", "Pellam", "Pepin", "Perrin", "Perrine", "Petra", "Piers",
    "Radbod", "Radulf", "Rafa", "Rafe", "Rafnhild", "Ragna", "Ragnald", "Ragnar", "Ragnfrid", "Ragnheid",
    "Ragnhild", "Ragnvor", "Rainer", "Ralf", "Ranald", "Randal", "Randver", "Rannveig", "Rannvi", "Ranulf",
    "Ref", "Reginlind", "Reinhold", "Reyn", "Rhian", "Rhiannon", "Rhona", "Rhosyn", "Rhun", "Ricard",
    "Ricbert", "Richenza", "Rigmor", "Rigolf", "Rikard", "Rikke", "Riordan", "Roald", "Roar", "Roderic",
    "Rodger", "Rodric", "Rognvald", "Roland", "Rolf", "Rorik", "Rosalind", "Rosamund", "Rothild", "Rowan",
    "Rowena", "Rudolf", "Runa", "Rune", "Rurik", "Sabeth", "Sabine", "Sabra", "Saebjorn", "Saemund", "Saeunn",
    "Saga", "Sander", "Sassa", "Saxon", "Sebold", "Sela", "Selma", "Senna", "Seren", "Seward", "Sibyl",
    "Sievert", "Sif", "Sigbert", "Sigfast", "Sigga", "Sigmund", "Signe", "Signy", "Sigrfljod", "Sigrid",
    "Sigrun", "Sigthrud", "Sigtrygg", "Sigurd", "Sigurlin", "Sigvald", "Sigwald", "Sigyn", "Silas", "Silke",
    "Sindri", "Siv", "Skapti", "Skeggi", "Skuli", "Snorri", "Solmund", "Solrun", "Solveig", "Solvi", "Sorcha",
    "Soren", "Sorin", "Starkad", "Steinar", "Steinthor", "Steinunn", "Steinvor", "Stellan", "Sten", "Stig",
    "Sturla", "Styr", "Sunna", "Sunniva", "Svala", "Svana", "Svanhild", "Svanlaug", "Svein", "Sven",
    "Swanhild", "Sweyn", "Swithin", "Taddeo", "Talwyn", "Tamlin", "Tamsin", "Tancred", "Tangwystl", "Tarn",
    "Tegan", "Teodor", "Thane", "Thangbrand", "Theobald", "Theron", "Thiodolf", "Thjodhild", "Thora",
    "Thorald", "Thorbern", "Thorbjorg", "Thordis", "Thorgerd", "Thorgrim", "Thorhild", "Thorin", "Thorkel",
    "Thorlaug", "Thormod", "Thorstein", "Thorunn", "Thorvald", "Thrain", "Thrand", "Thurid", "Thurstan",
    "Thyra", "Thyri", "Tilda", "Tobin", "Tofa", "Tora", "Torben", "Tordis", "Torgil", "Torold", "Torsten",
    "Torunn", "Torvik", "Tostig", "Tove", "Tristan", "Trygve", "Tuva", "Tyr", "Tyra", "Ubbe", "Ulf", "Ulfar",
    "Ulfgar", "Ulfhild", "Ulfkel", "Ulfrun", "Ulla", "Ulric", "Ulrik", "Ulrika", "Ulvhild", "Una", "Unna",
    "Unnr", "Unwin", "Urien", "Uthred", "Vagn", "Vala", "Valborg", "Valda", "Valdis", "Valgard", "Valgerd",
    "Vali", "Vanja", "Vardan", "Varin", "Vedis", "Vemund", "Verena", "Vermund", "Vestein", "Vidar", "Vigdis",
    "Vigdisa", "Vigfus", "Vigga", "Vilborg", "Volker", "Wace", "Walburga", "Waldo", "Walter", "Waltheof",
    "Waltrud", "Warin", "Warrick", "Wendel", "Wendell", "Wendrin", "Wenna", "Werbert", "Werner", "Wiborada",
    "Wiglaf", "Wilda", "Wilfred", "Wilhelm", "Wilhelmina", "Willem", "Wilma", "Wilmar", "Wimund", "Winifred",
    "Wolfhard", "Wolfram", "Wolmar", "Wolter", "Wren", "Wulfe", "Wulfric", "Wulfrun", "Wulfstan", "Wynn",
    "Wystan", "Yngvar", "Yngve", "Yngvild", "Yngvildr", "Yorick", "Yrr", "Yrsa", "Yrsel", "Yrsela", "Ysbel",
    "Ysmay", "Ysolt", "Yvaine", "Yves",
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
