"""The story in pieces: the act openings, what is written on the floors, the words of the great foes.

Under the mountain village of Ashmoor the Hearthwardens, an old order of miners and smiths, built the Deephold to keep
the Cinderfire: the glow that kept the earth warm and alive. The Hearthwardens dug too deep and woke what slept beneath the fire, the Ashen King, a fallen
god of fire who now wears the Cinderfire as his heart. Since then ash creeps out of the pit and the dead walk in the
old crypt. You are hired blades, going down to put out the fire, or take it back."""
import random

INTRO = [
    "Ashmoor. Grey snow that is not snow falls on the roofs. The headman pays in old Hearthwarden gold: go down, he says,",
    "and do not come back until the fire under the mountain is quenched or ours again.",
]

ACT_OPEN = [
    ['THE CRYPT', 'Ashmoor buried its dead here for nine hundred years. They no longer lie still.'],
    ['THE CAVES', 'The old mine ways of the Hearthwardens. The heat from below has changed what lives in them.'],
    ['THE FORGE', 'The Deephold, the halls of the Hearthwardens. Its anvils ring again, for a new master.'],
    ['THE DEEP', 'The heart of the Cinderfire. Something vast breathes in the glow below.'],
]

# writings found on the floors (the sign ?), per act; each is read once by whoever steps on it
WRITINGS = [
    ["A gravestone: 'Here lies Edda, who tended the bees. The earth was warm under her feet.'",
     "Scratched on a coffin lid from the inside: 'not dead not dead not dead'",
     "A sexton's note: 'Third grave open this week. The ash gets into everything.'",
     "A prayer card: 'Cinderfire under the hill, keep our fields green and our dead asleep.'",
     "Carved in the wall: 'The miners came up for the feast days, then stopped coming. Then the ash.'"],
    ["A miner's tally: forty marks, then the word 'HOT' pressed deep into the wood.",
     "A sign, worn: 'Lowest shaft closed by order of the Hearthwardens.'",
     "A torn page: '...the spiders grow fat on the warmth. The hounds have no eyes now, and need none...'",
     "Chalk on stone: 'Brannoc's crew went down the closed shaft. We heard singing.'",
     "A lamp, cold, and a note: 'The deeper you dig, the more the stone sings back.'"],
    ["A ledger of the Hearthwardens: 'The Cinderfire flickers. We dig to find what feeds on it.'",
     "Runes on an anvil: 'Strike true, and the stone remembers.' Someone has scratched out 'stone'.",
     "A smith's last words, burnt into a door: 'He does not need us alive to work the bellows.'",
     "The seal of the Hearthwardens' Master, cracked. On the back: 'Forgive us, Ashmoor.'",
     "A golem's work order, still in its chest: 'Carry the fire upward. Burn what is above.'"],
    ["Words in the heat-shimmer, in no voice at all: 'I was a god when your mountains were sand.'",
     "The Master's chain of office, melted into the floor.",
     "Ash shaped into letters: 'THE FIRE IS MY HEART. COME AND TAKE IT.'",
     "A last prayer, half burnt: 'If the heart of the fire is cut out, the fire will be ours again.'",
     "Bones of a great hound, curled around a miner's child's toy."],
]

BOSS_WAKE = {
    'gravewarden': "The Gravewarden rises from the sexton's chair. 'Ashmoor's dead are mine to keep.'",
    'webmother': "Something huge unfolds from the ceiling. The Webmother has grown fat on the warmth.",
    'furnace': "The great furnace opens like an eye. The Furnace Lord steps out, dripping slag.",
    'ashenking': "The Cinderfire turns towards you. In its heart, the Ashen King stands up.",
}
BOSS_DIES = {
    'gravewarden': "The Gravewarden falls apart into old bones and older dust. The way down opens.",
    'webmother': "The Webmother curls up and is still. The webs go slack across the cave.",
    'furnace': "The Furnace Lord cools to black iron and cracks. The anvils of the Deephold fall silent.",
    'ashenking': "The Ashen King breaks like a coal under a hammer. The Cinderfire gutters... and is yours.",
}
WAYSTATION = "A Hearthwarden waystation, still warm. A fountain runs clear here, and someone has set up shop."
VICTORY = [
    "The Cinderfire burns quiet and golden again, as it did before the Hearthwardens dug too deep.",
    "Far above, the ash stops falling on Ashmoor. The dead in the crypt lie down.",
]


def writing(act, used):
    """A writing of this act not yet found in this run, or None."""
    left = [w for w in WRITINGS[act] if w not in used]
    return random.choice(left) if left else None
