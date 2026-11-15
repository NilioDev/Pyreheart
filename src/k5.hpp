// K5: the maker of classic dungeons, the way of Brogue. Ported from Avarenthia's k5.py (NilioDev/Avarenthia), without
// its drawn pieces. Rooms of many shapes grow onto the level one at a time, each slid along until its door meets a wall
// of what is already dug, mostly close to the middle so the level stays compact. Main ways are 3 wide and join rooms;
// narrow ways are 1 wide and short. Every passage runs from room to room; an arm no room joined is filled in again.
// Loops only where walking round is long, lakes only where all floor stays reachable. Doors only in a straight wall,
// never two walls side by side.
#pragma once
#include "defs.hpp"

namespace k5 {

struct Room {
  int id;
  PSet cells;
  std::string kind, shape;
  P middle(const PMap<std::string>& floor) const;
};

struct Plan {
  int w = 0, h = 0;
  PMap<std::string> floor;  // room, cave, hall, water, shallow, rubble
  PSet doors, arches;
  std::vector<Room> rooms;
  P up, down;
  std::string mood;
  std::vector<int> themes;  // indexes of rooms behind a single entrance: for treasure
  int bad_doors = 0, doubles = 0;
};

Plan make(int w, int h, Rng& rnd, const std::string& mood = "", int rooms_wanted = 0);
PMap<int> walk(const PMap<std::string>& floor, P start, const PSet& block = {}, int limit = 0);

}  // namespace k5
