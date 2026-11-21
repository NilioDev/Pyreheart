// The Delver's Handbook (plan.md section 5): chapters of short pages, opened with [?]. In the text, [k] is drawn as a
// gold keycap and {Ember} in its element's colour. Lists that come from the game's data (the keys, the zones you have
// seen) are added by the client when it draws the page.
#pragma once
#include <string>
#include <vector>

namespace book {

struct Block {
  char kind;  // 'h' a heading, 'p' a paragraph, 'k' a key and what it does (key \t text), '-' a gap
  std::string text;
};
struct Chapter {
  std::string key, title;
  std::vector<Block> blocks;
};

inline const std::vector<Chapter>& chapters() {
  static const std::vector<Chapter> C = {
      {"first", "First steps",
       {{'p', "Long ago a dwarven clan dug up the Pyreheart and woke the Ashen King, who put it in his own chest. The world has been freezing ever since. You go down under Charnfen to tear it back out."},
        {'h', "The way down"},
        {'p', "Every dive starts in Charnfen. Take a weapon from the gatekeeper's rack by the gate and a coal from the great fire, then go round through the graveyard to the ruined cathedral; under its choir a hole leads down into the Crypt."},
        {'p', "Each zone has three maps; a great foe waits on the third. After it comes a waystation, and there you choose which stairs to take next."},
        {'h', "Staying alive"},
        {'p', "Walk into a foe to strike it. Drink a healing potion with [q] before it is too late: when your life runs low the edge of the map pulses red."},
        {'p', "Death ends the dive for good. A dive alone is saved after every step, so you can stop at any moment."}}},
      {"moving", "Moving",
       {{'p', "Walk with the arrow keys or the number pad. Diagonals: the number pad, or [Home] [PgUp] [End] [PgDn]."},
        {'p', "Wait a moment with [.] or [numpad 5]. Look at what you see with [l]: move the marker, and [Tab] jumps to the next foe."},
        {'p', "Stairs down: stand on them and press [>]. With friends, the map changes when all of you stand on the stairs."},
        {'p', "On a phone, tap where you want to go, and hold to keep walking until a foe comes into sight."}}},
      {"fighting", "Fighting",
       {{'p', "Walk into a foe to strike it. A bow, staff or other ranged weapon shoots with [f]: choose the foe with [Tab] and let go with [Enter]."},
        {'p', "Your skills sit on [1] to [5]. [1] is your weapon's own skill; [2] to [5] come as your styles grow. They cost fury or mana, shown in the colour of their orb. A skill that needs a target lets you aim first."},
        {'p', "Fury fills as you strike and are struck in close, and drains away when the fight is over. Mana comes back slowly; a mana potion [w] also stirs your fury."},
        {'h', "Reading a foe"},
        {'p', "IN SIGHT lists the foes you see, the most urgent first. A foe that shows the sign of a sun with the word heavy is winding up a heavy blow: step away."},
        {'p', "Blue foes are champions and gold ones uniques: stronger, and they carry better things."},
        {'h', "Marks on the ground"},
        {'p', "Heavy blows show as a count on the ground before they land. Red: step off it. Purple: a spell; stun the foe or hurt it hard to break it. Gold: the only safe ground, so stand in it."}}},
      {"elements", "Elements",
       {{'p', "Before the first dive, take a coal from the edge of Charnfen's great fire: walk into it. Its element burns in all your hits from then on."},
        {'h', "Stacks"},
        {'p', "Every hit lays one stack of its element on the target, up to 5. A stack falls off after 3 turns without a new hit. The panel shows the stacks on you, IN SIGHT the stacks on your foes."},
        {'p', "A heavy hit sets off all the stacks of its own element at once. Your skills marked {sun} do that; the ones marked ■ lay stacks."},
        {'h', "The five"},
        {'p', "{Ember} burns a little every turn; set off, the fire leaps to all around."},
        {'p', "{Rime} slows; set off, it shatters in a burst of harm."},
        {'p', "{Ash} gets in the eyes, so the foe misses more; set off, it is smothered and loses its turn."},
        {'p', "{Rot} stops healing; set off, all the rot bursts at once."},
        {'p', "{Blood} bleeds; set off, the bleeding heals the one who drinks it."},
        {'h', "The same for foes"},
        {'p', "Foes lay stacks on you too. A foe that hits hard and slow winds up a smash when you carry two or more of its element: step away, or shake the stacks off."},
        {'p', "Water puts out {Ember}. A fire thaws {Rime} and dries {Rot}. Fresh air in an open doorway blows {Ash} away."},
        {'h', "Resistances"},
        {'p', "Each zone's foes resist one element and fear another. The Hero screen [c] shows how well you resist each; every Frost level takes 10% from all of them."}}},
      {"hero", "Your hero",
       {{'p', "Your hero has no class. You start with one weapon from the gatekeeper's rack and become what you do."},
        {'h', "Three styles"},
        {'p', "Melee, Ranged and Magic each have their own rank. A blow struck in close teaches Melee, whatever is in your hand; [f] teaches the style of your weapon; every skill teaches its own style. The harm counts when the foe dies."},
        {'p', "New skills come at style ranks 1, 3, 6 and 10. Melee skills cost fury, the others mana. Your best style sets how your life and mana grow."},
        {'h', "Weapon and element"},
        {'p', "Your weapon has one trait and its own skill on [1]. Fighting with it brings mastery, up to 5: more harm, and a stronger trait. Any weapon you find can be used, but a new kind starts from nothing."},
        {'p', "Your element grows with every kill too, and its stacks bite harder."},
        {'h', "Level and title"},
        {'p', "Levels come slowly, about two a zone. Each brings a perk to choose: [p]. Your best style and your element give your hero a title, such as Forgeborn or Rimecaller. Each title brings its own talent, shown on the Hero screen [c]. A style that reaches rank 5 opens two paths, chosen at a waystation's fire."},
        {'p', "The panel shows your name, title and level, and three orbs for life, fury and mana. The Hero screen [c] shows the rest."},
        {'h', "One hero, one life"},
        {'p', "Your hero is kept. Beat the Ashen King and you wake in Charnfen with all you grew and carry, and go down again one Frost level deeper, without end. Only then may the Pyre's Sparks be taken back."},
        {'p', "Fall, and the hero is gone with all it carried. A stone is raised in Charnfen's graveyard: walk into a grave to read it. The best lie nearest the great fire."}}},
      {"gear", "Gear",
       {{'p', "Open your pack with [i]. [Enter] wears or uses the chosen thing, [d] puts it down for anyone to take, [t] gives it to the hero next to you."},
        {'p', "Every thing is marked against what you wear: a green ▲ is better, a red ▼ worse, and ~ a trade. Choose it and every number that would change is listed."},
        {'p', "Things you find are yours alone; what you put down, anyone may take. Gold shows in your pack and at the trader."}}},
      {"foes", "Great foes",
       {{'p', "A great foe waits on the last map of each zone. The bar at the top of the screen shows its life, with two marks where the fight turns: at two thirds and at one third it fights harder."},
        {'p', "After its greatest blow a great foe is spent for a moment and takes more harm. Strike then."},
        {'p', "The first time you slay a zone's great foe on a Frost level, the Pyre gives you a Spark."}}},
      {"rest", "Waystations",
       {{'p', "After a zone's great foe comes a waystation: a fountain that closes your wounds, a trader, and two stairs to choose from."},
        {'p', "Look at the stairs with [l] to see where each leads. The chart in the corner shows the way you came in gold."},
        {'p', "Walk into the fire to rest by it. There you choose which skills sit on keys [2] to [5], take a path once a style reaches rank 5, and take a second coal: each key can then lay either of your two elements."},
        {'p', "In co-op, a friend who fell can be rekindled at a waystation's fire: carry their ember there."}}},
      {"coop", "Co-op",
       {{'p', "Up to four heroes can dive together over the network: host a dive from the title screen, and the others join it."},
        {'p', "The map changes when every hero stands on the stairs. The PARTY block on the panel shows your friends' life."},
        {'p', "Give a thing to the hero next to you with [t] in your pack."},
        {'p', "A friend joins only at a Frost level their own hero has reached."},
        {'h', "Embers and ghosts"},
        {'p', "A hero who falls in co-op drops an ember, a glowing * where they fell, and walks on as a ghost after the others. Walk over the ember to carry it."},
        {'p', "An ember glows: foes see its bearer from further off, and every third turn its weight holds them back. Bring it to a waystation's fire and the friend is rekindled, with half their life and a scar: a tenth of their life is gone for good."},
        {'p', "If the bearer falls, or the ember is left behind when the map changes, it goes out, and the friend is gone for good. If all fall, the dive ends."}}},
      {"charnfen", "Charnfen",
       {{'p', "The last village above the dark. The great fire burns on the square; beyond it snow falls on the graves of the many who went down."},
        {'p', "Ysolde the healer, Brann the smith and old Ebb the gravedigger still live here."}}},
      {"frost", "The Pyre and Frost",
       {{'p', "Sparks from slain great foes are spent on the Pyre, from the title screen: gifts that stay with you from dive to dive."},
        {'p', "Beating the Ashen King opens the next Frost level: the same way down, colder and harder. The bar above the log shows the Frost level you are on."}}},
      {"signs", "Signs",
       {{'h', "In the log"},
        {'p', "{sun} lore and the world   {?} a tip   {!} danger   {dagger} a death   {loot} something found"},
        {'p', "Older lines fade; the newest are brightest."},
        {'h', "On things"},
        {'p', "▲ better than what you wear, ▼ worse, ~ a trade, = no different."},
        {'h', "The frame"},
        {'p', "The metal of the frame follows the zone you are in: bone in the Crypt, iron in the Caves, verdigris in the Cistern, bronze in the Forge."}}},
      {"keys", "All the keys", {}},
  };
  return C;
}

// every key, for the last chapter
inline const std::vector<std::pair<std::string, std::string>> KEYS = {
    {"[arrows] [numpad]", "walk; walk into a foe to strike it"},
    {"[Home] [PgUp] [End] [PgDn]", "walk diagonally"},
    {"[.] [numpad 5]", "wait a moment"},
    {"[1] [2] [3] [4] [5]", "your skills; [Tab] to the next foe, [Enter] to let go"},
    {"[f]", "shoot, throw, stab at 2 or carve a rune, by weapon"},
    {"[q] [w]", "drink a healing or a mana potion"},
    {"[i]", "inventory: [Enter] wear, [d] put down, [t] give"},
    {"[c]", "the Hero screen"},
    {"[p]", "choose a perk after going up a level"},
    {"[>]", "go down the stairs"},
    {"[l]", "look around"},
    {"[?]", "this handbook"},
    {"[o]", "options: music, sound, numbers"},
    {"[Esc]", "close a window; the main menu"},
    {"[F11]", "full screen and back"},
    {"[F12] [`]", "debug and testing"},
};

inline int index_of(const std::string& key) {
  auto& c = chapters();
  for (size_t i = 0; i < c.size(); ++i) if (c[i].key == key) return int(i);
  return 0;
}

}  // namespace book
