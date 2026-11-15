// Moving pictures for blows and spells, drawn over the map for a moment and never waited on. Ported from
// Avarenthia's anim.py (NilioDev/Avarenthia): bolts with a fading tail, round bursts, waves, swings, spins, crits,
// blood, rising light, lines from the sky and motes drawn inward.
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace fx {

struct Rgb { int r, g, b; };
struct Cell { int x, y; int ch; Rgb col; };

Rgb rgb(int c);
Rgb mix(Rgb a, Rgb b, double f);
Rgb scale(Rgb c, double f);

/** How long (delay included) an effect stays on screen, in seconds. */
double life(const nlohmann::json& kw);
/** The cells an effect lights at this age. */
std::vector<Cell> cells(const nlohmann::json& kw, double age);

}  // namespace fx
