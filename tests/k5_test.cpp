// The level maker: every level whole (all floor reachable from the stairs up), stairs apart, big enough.
#include <cstdio>
#include "k5.hpp"

int main(int argc, char** argv) {
  int bad = 0, total = 0, rooms = 0, doubles = 0, small = 0;
  for (int seed = 0; seed < 30; ++seed) {
    Rng rnd(seed);
    auto p = k5::make(76, 42, rnd, "", 13);
    auto reach = k5::walk(p.floor, p.up);
    if (p.floor.size() < 300) { ++small; continue; }  // the game throws it away and makes another
    if (reach.size() != p.floor.size() || p.up == p.down || !p.floor.count(p.down)) {
      std::printf("seed %d: bad level (%zu of %zu reachable)\n", seed, reach.size(), p.floor.size());
      ++bad;
    }
    total += int(p.floor.size());
    doubles += p.doubles;
    for (auto& r : p.rooms) if (r.kind != "hall") ++rooms;
  }
  std::printf("avg floor %d, rooms %.1f, double walls %d, too small %d, over 30 levels\n", total / 30, rooms / 30.0, doubles, small);
  if (small > 5) ++bad;
  if (argc > 1) {
    Rng rnd(std::atoi(argv[1]));
    auto p = k5::make(76, 42, rnd, "", 13);
    for (int y = 0; y < 42; ++y) {
      for (int x = 0; x < 76; ++x) {
        P c{x, y};
        char ch = '#';
        auto f = p.floor.find(c);
        if (c == p.up) ch = '<'; else if (c == p.down) ch = '>'; else if (p.doors.count(c)) ch = '+';
        else if (f != p.floor.end()) ch = f->second == "water" ? '~' : f->second == "shallow" ? '-' : f->second == "rubble" ? '%' : f->second == "hall" ? ',' : '.';
        std::putchar(ch);
      }
      std::putchar('\n');
    }
  }
  return bad ? 1 : 0;
}
