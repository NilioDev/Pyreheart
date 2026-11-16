# Pyreheart

A coffeebreak roguelike in ASCII. Sixteen floors down, about half an hour from the top to the end, alone or with up to three friends.

Long ago a forgotten dwarven clan unearthed the Pyreheart, the molten crystal core that kept the world warm. They dug too deep beneath its altar and woke the Ashen King, a fallen god, who slaughtered the clan and forced the crystal into his own hollowed chest. The world has been freezing ever since. Now the Ashen King stirs beneath the backwater town of Charnfen: ash poisons the wells, and the dead claw their way out of the graveyard mud. You have come to Charnfen at the edge of ruin. Descend, and tear the heart back out.

- Four acts: the Crypt, the Caves of the old dwarven mines, the Forge of the fallen dwarves, and the Deep. A great foe waits at the end of each.
- After each of the first three, a waystation: a fountain, a trader, and the fallen wake again.
- Three callings: the warrior, the ranger and the adept. Each has two skills and a handful of perks to choose from as it levels.
- Loot in the old way: magic, rare and unique things, with blue champions and gold uniques that carry the better ones.
- Co-op over the network for up to four heroes. Loot can be given to each other, or put down for anyone to take.
- Music that writes itself: slow dungeon synth, made sample by sample while you play, with its own mood for each act and never the same twice. Turn it off on the title screen or in the [Esc] menu.
- Plays on Windows, Linux, macOS, Android phones and Wear OS watches.

## Keys

| key | what it does |
| --- | --- |
| [arrows] [numpad] | walk; walk into a foe to strike it. Diagonals: numpad, or [Home] [PgUp] [End] [PgDn] |
| [.] [numpad 5] | wait a moment |
| [1] [2] | your two skills. A skill that needs a target lets you aim: [Tab] to the next foe, [Enter] to let go |
| [f] | shoot with a bow |
| [q] [w] | drink a healing or a mana potion |
| [i] | your pack: [Enter] wear or use, [d] put down, [t] give to the hero next to you |
| [p] | choose a perk after going up a level |
| [>] | go down the stairs |
| [l] | look around |
| [?] | help |
| [Esc] | close a window; the game menu |

On a phone, tap where you want to go, and hold to keep walking until a foe comes into sight. The buttons along the bottom do the rest.

On a watch, tap a side of the dial to step that way. Turn the crown and a ring of actions appears round the edge; the crown chooses, the middle shows what is chosen, and a tap does it.

## Playing together

One player hosts ("Host a game for friends"), the others join with the host's address. On the same network the game finds hosts by itself.

Over the internet, the host must open **port 7777** (TCP, and UDP for finding the game) on their router to the hosting computer, and give the others their public address. The world moves on when every hero has chosen what to do; whoever has not chosen four seconds after the first did, waits a turn.

## Building

Desktop, with CMake 3.24 or newer and a C++20 compiler. libtcod, SDL3 and nlohmann/json are fetched by CMake.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The game is `build/pyreheart` with its `assets` folder next to it. `ctest --test-dir build` runs the level maker test, a co-op test over a real socket, and bots that play the game.

Android and Wear OS, with the Android SDK and NDK:

```
cd android
./gradlew assemblePhoneRelease assembleWearRelease
```

GitHub Actions builds all of these on every push; the desktop games are attached to each run, and the newest APKs are always on the [Latest build](https://github.com/NilioDev/Pyreheart/releases/tag/latest) release.

To put the watch app on a Wear OS watch without a computer: turn on developer options and wireless debugging on the watch, download `pyreheart-wear.apk` on the paired phone, and send it over with a sideloading app such as Wear Installer 2.

`build/music_preview <folder> [seconds]` renders each mood of the music to a WAV file.

To see another layout on the desktop, set `PYREHEART_LAYOUT=tall` (a phone) or `PYREHEART_LAYOUT=watch`.

## Credits

- Built on [libtcod](https://github.com/libtcod/libtcod) and [SDL](https://libsdl.org).
- The dungeon maker and the spell effects grew out of [Avarenthia](https://github.com/NilioDev/Avarenthia).
- The font is Px437 IBM CGA from [The Ultimate Oldschool PC Font Pack](https://int10h.org/oldschool-pc-fonts/) by VileR, under CC BY-SA 4.0 (see `assets/LICENSE-Px437.txt`).
- The story of Charnfen and the Pyreheart is by Niels.
- `ref/` holds the Python sources the game's tables and lore are generated from (`tools/`).
