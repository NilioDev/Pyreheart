"""The story in pieces: the opening crawl, the act openings, what is written on the floors, the words of the great foes.

Long ago a dwarven clan, long since gone from the world, found the Pyreheart: the living crystal core that pumped the
Cinderfire through the stone and kept the world warm. Digging beneath its altar they broke the prison of the Ashen King,
a fallen god chained under the crystal. He slaughtered the clan and forced the Pyreheart into his own hollowed chest;
the world has been freezing ever since. The village of Charnfen was built, unknowing, over the collapsed dwarven mines,
warmed by the fever of the sleeping god. Now he stirs: ash in the wells, the dead out of their graves. You have come to
Charnfen to go down and tear the heart back out. (Story by Niels.)"""
import random

# the opening crawl on the title screen; an empty line is a gap between paragraphs
INTRO = [
    "Long ago, a forgotten dwarven clan unearthed the Pyreheart: the molten crystal core that kept the world warm. "
    "In their devotion, they dug too deep beneath its altar and woke the Ashen King. The fallen god slaughtered the clan, "
    "ripped the crystal from the stone, and forced it into his own hollowed chest.",
    "",
    "With the earth's heart stolen, a slow frost gripped the world. Across the dying lands, scattered settlements struggle "
    "to survive the bitter cold, none more cursed than Charnfen. Built unknowingly atop the collapsed dwarven mines, this "
    "bleak backwater town clung for generations to the strange, sickly heat leaking from the ground, blind to the truth "
    "that they were warming themselves on the fever of the sleeping god below.",
    "",
    "Now, the Ashen King stirs beneath the town. Ash poisons the wells, the soil reeks of fever, and the dead claw their "
    "way out of the graveyard mud, drawn downward by the cursed heartbeat below. You have traveled to Charnfen at the "
    "edge of ruin. Descend into the filth, and tear the heart back out.",
]

ACT_OPEN = [
    ['THE CRYPT', "Beneath Charnfen's cracked gravestones, the wet dirt is warm like fresh meat. Bloated corpses and "
                  "ash-caked skeletons stalk the flooded tombs, their shattered ribs glowing with a sickly, dark ember."],
    ['THE CAVES', "Beyond the breached crypt walls lie the old dwarven mines. Sightless, hairless beasts and giant rats "
                  "have mutated in the unnatural heat, their blistered skin hardened into armour dripping with black grease."],
    ['THE FORGE', "The smoke-filled halls of the fallen dwarves echo again with iron on iron. Iron golems and half-melted "
                  "smiths hurl bone and scrap into the relit fires, forging cursed weapons for their master."],
    ['THE DEEP', "The scorched pit of the Cinderfire, among rivers of boiling metal and charred bone. Somewhere below, "
                 "the stolen Pyreheart beats behind blackened ribs."],
]

# writings found on the floors (the sign ?), per act; each is read once by whoever steps on it
WRITINGS = [
    ["A gravestone: 'Here lies Wenna, who kept the well. Taken by the cough, like her mother.'",
     "Scratched on a coffin lid from the inside: 'not dead not dead not dead'",
     "A sexton's note: 'Third grave open this week. The ash gets into everything, even the shrouds.'",
     "A prayer card: 'Warm ground, keep our roots alive and our dead asleep.'",
     "A child's slate: 'Mam says the floor is warm because the old ones under it are still working.'"],
    ["Dwarven runes on a pit prop, and under them in chalk: 'NO ONE LEFT TO READ THESE'",
     "A Charnfen digger's tally: forty marks, then the word 'HOT' pressed deep into the wood.",
     "A torn page: '...the rats grow big as dogs down here. Their skin is like boot leather, and it sweats black...'",
     "A dwarven skull, small and broad, with a pickaxe still in its hands.",
     "A lamp, cold, and a note: 'Listen to the walls. Something down there has a heartbeat.'"],
    ["Carved over a forge in the old tongue, and scratched beside it in common: 'The Heart provides.'",
     "A dwarven ledger, the last page: 'We dig beneath the altar. The crystal sings louder every day.'",
     "Burnt into a door: 'The smiths did not stop when they died. They only stopped screaming.'",
     "A child's iron toy, dwarf-made, half melted into the floor.",
     "A golem's work order, still in its chest: 'Feed the fire. Feed the fire. Feed the fire.'"],
    ["Words in the heat-shimmer, in no voice at all: 'I was a god when your mountains were sand.'",
     "A broken chain, each link as thick as a man. The seal on it is dwarven, and very old.",
     "Ash shaped into letters: 'THE HEART IS MINE. COME AND TAKE IT.'",
     "A dwarven altar, cracked in two. Something large was torn out of it.",
     "A last prayer, half burnt: 'Sun-Breath, Deep-Tide, Cinderfire: forgive us. We only wanted to be warm.'"],
]

BOSS_WAKE = {
    'gravewarden': "The Gravewarden rises from the sexton's chair. 'Charnfen's dead are mine to keep.'",
    'webmother': "Something huge unfolds from the ceiling, blistered and greasy. The Webmother has grown fat on the heat.",
    'furnace': "The great furnace opens like an eye. The Furnace Lord steps out, dripping slag.",
    'ashenking': "On a throne of black stone the Ashen King stands up. Behind his ribs the Pyreheart beats, faster.",
}
BOSS_DIES = {
    'gravewarden': "The Gravewarden falls apart into old bones and older dust. The way down opens.",
    'webmother': "The Webmother curls up and is still. The webs go slack across the cave.",
    'furnace': "The Furnace Lord cools to black iron and cracks. The anvils of the dwarves fall silent again.",
    'ashenking': "The Ashen King breaks like a coal under a hammer. You tear the Pyreheart from his chest, and it beats in your hands.",
}
WAYSTATION = "A dwarven waystation, empty for an age. A fountain still runs clear here, and someone from Charnfen has set up shop."
VICTORY = [
    "The Pyreheart is home on its altar, and the Cinderfire runs warm through the stone again.",
    "Far above, the ash stops rising in Charnfen. The wells clear, and the dead in the graveyard lie down.",
]


def writing(act, used):
    """A writing of this act not yet found in this run, or None."""
    left = [w for w in WRITINGS[act] if w not in used]
    return random.choice(left) if left else None
