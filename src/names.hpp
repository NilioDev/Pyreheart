#pragma once
// Random hero names: human first names of the cold north, plain ASCII, at most 16 letters (plan.md, 2026-11-23).
// No bynames. Leave out the names of Charnfen's own people (Ysolde, Brann, Ebb) and of the unique foes.
// Add or remove names freely: the list is read as it is.
#include <random>
#include <set>
#include <string>
#include <vector>

namespace names {

inline const std::vector<std::string> HERO = {
    "Aasa", "Adalind", "Adela", "Aedan", "Aelfwyn", "Aelith", "Aelred", "Agathe", "Agmund", "Agna", "Ailith",
    "Alaric", "Alban", "Alda", "Aldgyth", "Aldis", "Aldith", "Aldo", "Aldreda", "Aldric", "Aldwin", "Alfgar",
    "Alfhild", "Alva", "Alwen", "Alys", "Ama", "Amice", "Amund", "Annis", "Ansel", "Anselm", "Anwen", "Arla",
    "Arn", "Arnald", "Arngrim", "Arnora", "Arnulf", "Arvid", "Asger", "Ashild", "Askel", "Aslaug", "Asmund",
    "Asta", "Astrid", "Athelric", "Aubrey", "Aud", "Audra", "Audric", "Audun", "Aveline", "Axel", "Baldric",
    "Baldwin", "Bardolf", "Barric", "Bartold", "Beatrix", "Bebba", "Belda", "Benedek", "Bennet", "Beorn",
    "Bera", "Berend", "Bergit", "Berit", "Berold", "Bertha", "Bertram", "Bettrys", "Birgit", "Bjorn",
    "Blaise", "Blanche", "Bodil", "Bodo", "Bodvar", "Borghild", "Borin", "Bothvar", "Bram", "Brand",
    "Brannor", "Branwen", "Brede", "Brenna", "Brennan", "Brenric", "Brigid", "Brisa", "Brock", "Brokk",
    "Brunhild", "Bryda", "Brynja", "Cadoc", "Caedmon", "Cael", "Caius", "Calder", "Cara", "Carrick", "Catrin",
    "Cecily", "Cedric", "Ceinwen", "Cenric", "Ceridwen", "Clemence", "Coll", "Colm", "Conrad", "Corbin",
    "Cormac", "Corran", "Corwin", "Cuthbert", "Cwen", "Dag", "Dagmar", "Dagny", "Dagr", "Dagrun", "Dain",
    "Dalla", "Dalric", "Davor", "Derrick", "Dervla", "Desmond", "Dorota", "Dorte", "Drest", "Drifa", "Drogo",
    "Duncan", "Dunstan", "Durstan", "Eadgyth", "Eadric", "Eadwulf", "Eamon", "Ebba", "Eberhard", "Ector",
    "Edda", "Edgar", "Edith", "Edla", "Edmund", "Edric", "Egbert", "Egil", "Eilwen", "Einar", "Eira", "Eirik",
    "Eirlys", "Eldor", "Eldred", "Eldrid", "Elfa", "Elfled", "Elfrida", "Elin", "Ellisif", "Elric", "Elsa",
    "Elspeth", "Eluned", "Elva", "Embla", "Emer", "Emeric", "Emma", "Emrys", "Engel", "Engla", "Erland",
    "Erling", "Ermengard", "Ermina", "Erna", "Erwin", "Esa", "Esben", "Esmond", "Estrid", "Ethelind",
    "Evrard", "Ewald", "Ewan", "Eydis", "Falk", "Faramond", "Feldric", "Fenna", "Fenwick", "Fergus", "Finn",
    "Finnar", "Fionnuala", "Folke", "Freya", "Frida", "Frideswide", "Fridgerd", "Fridolf", "Frode", "Fulk",
    "Gamel", "Gareth", "Garm", "Garrick", "Gaut", "Gaute", "Gavin", "Geir", "Gerald", "Gerda", "Gerlind",
    "Gerrit", "Gerulf", "Gest", "Gilbert", "Gilles", "Gisa", "Gisla", "Gisli", "Godfrey", "Godgifu", "Godric",
    "Godwin", "Gorm", "Greer", "Gregor", "Grete", "Grim", "Grimar", "Grimwulf", "Gudny", "Gudrun", "Gunhild",
    "Gunnar", "Gunnhild", "Gunnvor", "Gunther", "Guthrum", "Guy", "Gwen", "Gwenllian", "Gyda", "Gyrth",
    "Gytha", "Hadric", "Hafdis", "Hakon", "Halbert", "Haldis", "Haldor", "Halfdan", "Halla", "Hallam",
    "Halldora", "Hallveig", "Halsten", "Halvard", "Hamlin", "Hamund", "Harald", "Harbin", "Harlan", "Hartmut",
    "Hartwin", "Hastein", "Hauk", "Hawise", "Hedda", "Hedric", "Hedwig", "Heidrun", "Helga", "Helm", "Helvi",
    "Hemming", "Herdis", "Hereward", "Herrick", "Hertha", "Herve", "Hilda", "Hilde", "Hildebrand", "Hildur",
    "Hjalmar", "Hjordis", "Hodd", "Holt", "Horik", "Howel", "Hrafn", "Hrolf", "Hroswitha", "Hroth", "Hugh",
    "Hugo", "Hulda", "Ida", "Idony", "Idris", "Inga", "Inge", "Ingeborg", "Ingolf", "Ingram", "Ingrid",
    "Ingunn", "Ingvar", "Isa", "Isabeau", "Isak", "Isbel", "Isgerd", "Isolde", "Ivar", "Ivo", "Jarl",
    "Jarvis", "Jofrid", "Jora", "Jorund", "Jorunn", "Jorvik", "Jory", "Joss", "Jostein", "Jutta", "Kael",
    "Kaia", "Kari", "Karin", "Kelda", "Kenna", "Kenric", "Kester", "Ketil", "Ketta", "Kjell", "Knut",
    "Kolbein", "Kolfinna", "Kolla", "Kristin", "Lambert", "Lars", "Leif", "Lena", "Leofric", "Leofrun",
    "Leopold", "Liadan", "Liesel", "Linn", "Linnea", "Lioba", "Liv", "Lofn", "Lorcan", "Lothar", "Lucan",
    "Ludo", "Lynet", "Lyulf", "Mael", "Maelis", "Magnar", "Mairead", "Malcolm", "Malin", "Marek", "Maren",
    "Margery", "Marit", "Marrit", "Mathild", "Mathis", "Maud", "Mauger", "Melisent", "Meret", "Merrick",
    "Mette", "Mikkel", "Mildred", "Moira", "Morcar", "Mord", "Mordaunt", "Morna", "Morwen", "Nan", "Nanna",
    "Ned", "Nell", "Nessa", "Nesta", "Nils", "Njal", "Nora", "Norbert", "Norrin", "Oda", "Odd", "Oddny",
    "Oddvar", "Odger", "Odile", "Odo", "Olaf", "Olwen", "Oriel", "Orla", "Orm", "Ormund", "Orvar", "Osbert",
    "Osgar", "Oskar", "Osric", "Oswin", "Osyth", "Ottar", "Ottilie", "Otto", "Perrine", "Petra", "Piers",
    "Radulf", "Rafa", "Ragna", "Ragnar", "Ragnhild", "Ragnvor", "Rainer", "Ralf", "Ranald", "Randal",
    "Randver", "Rannveig", "Ranulf", "Reinhold", "Reyn", "Rhian", "Rhona", "Rhun", "Ricard", "Rigmor",
    "Rikard", "Rikke", "Roald", "Roderic", "Rodric", "Roland", "Rolf", "Rorik", "Rosalind", "Rosamund",
    "Rowan", "Rowena", "Rudolf", "Runa", "Rune", "Rurik", "Sabeth", "Sabine", "Saemund", "Saeunn", "Saga",
    "Sander", "Sassa", "Saxon", "Sebold", "Sela", "Selma", "Senna", "Seward", "Sibyl", "Sif", "Sigbert",
    "Sigga", "Sigmund", "Signe", "Sigrid", "Sigrun", "Sigurd", "Sigvald", "Sigyn", "Silas", "Silke", "Sindri",
    "Siv", "Skuli", "Snorri", "Solveig", "Solvi", "Sorcha", "Soren", "Steinar", "Steinunn", "Stellan", "Sten",
    "Stig", "Styr", "Sunna", "Sunniva", "Svala", "Svana", "Svanhild", "Sven", "Sweyn", "Swithin", "Tamlin",
    "Tamsin", "Tancred", "Tarn", "Tegan", "Teodor", "Thane", "Theobald", "Thora", "Thorald", "Thorbern",
    "Thorin", "Thorvald", "Thrand", "Thurid", "Thurstan", "Thyra", "Thyri", "Tilda", "Tobin", "Tora",
    "Torben", "Tordis", "Torgil", "Torsten", "Torvik", "Tove", "Tristan", "Tuva", "Tyr", "Tyra", "Ubbe",
    "Ulf", "Ulfar", "Ulfrun", "Ulla", "Ulric", "Ulrika", "Ulvhild", "Una", "Unna", "Unwin", "Urien", "Vala",
    "Valborg", "Valda", "Valdis", "Vali", "Vanja", "Varin", "Vedis", "Vemund", "Vidar", "Vigdis", "Vigga",
    "Volker", "Wace", "Waldo", "Walter", "Waltrud", "Warrick", "Wendel", "Wendrin", "Wenna", "Werner",
    "Wiglaf", "Wilda", "Wilfred", "Wilhelmina", "Willem", "Wilma", "Wilmar", "Winifred", "Wolfram", "Wolmar",
    "Wren", "Wulfe", "Wulfric", "Wulfrun", "Wynn", "Wystan", "Yngvar", "Yngvild", "Yorick", "Yrsa", "Yrsel",
    "Ysmay", "Ysolt", "Yvaine", "Yves",
};

// A random name that no hero in the graves has carried yet; once every name lies in the graves, any name.
inline std::string roll(const std::set<std::string>& taken, std::mt19937& rng, const std::string& not_this = "") {
  std::vector<const std::string*> free;
  for (auto& n : HERO) if (!taken.count(n) && n != not_this) free.push_back(&n);
  if (free.empty()) for (auto& n : HERO) if (n != not_this) free.push_back(&n);
  return *free[std::uniform_int_distribution<size_t>(0, free.size() - 1)(rng)];
}

}  // namespace names
