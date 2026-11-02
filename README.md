# Pomegrade

Pomegrade est une version modifiée de [melonDS-android](https://github.com/rafaelvcaetano/melonDS-android) qui ajoute le **remplacement de textures HD** pour la 3D.

## Contenu du dépôt

| Dossier ou fichier | Rôle |
|---|---|
| `melonDS-android/` | L'application Android (Kotlin + JNI) |
| `melonDS-android/melonDS-android-lib/` | Le cœur de l'émulateur (C++), intégré directement au dépôt et non plus comme sous-module |
| `azahar/` | Le cœur 3DS (Azahar, GPLv2+), module Gradle `melonDS-android/azahar/` |
| `mgba/` | Le cœur GBA ([mGBA](https://github.com/mgba-emu/mgba) 0.10.5, MPL 2.0), module Gradle `melonDS-android/mgba/` (pont JNI et écran de jeu GBA de Pomegrade) |
| `dolphin/` | Le cœur GameCube et Wii ([Dolphin](https://dolphin-emu.org) 2606a, GPLv2+), module Gradle `melonDS-android/dolphin/` ; ses bibliothèques (`dolphin/Externals/`) sont des sous-modules git |
| `eden/` | Le cœur Switch ([Eden](https://git.eden-emu.dev/eden-emu/eden), branche master du 26 octobre 2026 avec [Eden-NX2](https://git.eden-emu.dev/Takoroka/eden-nx2) (prise en charge expérimentale de la Switch 2), GPLv3+), module Gradle `melonDS-android/eden/` ; ses dépendances C++ sont téléchargées à la première compilation (`eden/.cache/`, ignoré) |
| `assets/` | Le logo de l'app (`branding/`) et des logos et modèles 3D de consoles (`consoles/`) |
| `docs/` | Feuille de route et plans de développement |
| `tools/`, `tests/` | Outils du remake Pokémon, tests sur PC |
| `reference/melonDS-master.zip` | Les sources de melonDS pour PC (référence) |
| `reference/melonDS.v.2.0.1.PS.apk` | La version officielle de melonDS-android (référence) |

Les BIOS et firmwares Nintendo ont été retirés du dépôt pour des raisons de copyright. Gardez-les sur votre appareil et choisissez-les dans les paramètres de l'application.

## Le dossier Punica

Punica range tout dans un seul dossier, `Punica`, choisi au premier lancement (par défaut dans le stockage interne) : un dossier par console, avec les mêmes six dossiers dans chacun.

```
Punica/
├── DS/  GBA/  3DS/  Switch/  GC-Wii/
│   ├── Games/     les jeux
│   ├── Saves/     les sauvegardes
│   ├── States/    les états de sauvegarde
│   ├── System/    BIOS, clés, firmware, mémoire système de la console
│   ├── Mods/      packs de textures et mods
│   └── Core/      les fichiers propres à l'émulateur (réglages, caches, journaux)
├── Settings/      une copie des réglages de Punica
└── Backups/       les copies faites avant un déplacement ou un import
```

Deux exceptions, imposées par les consoles : les sauvegardes 3DS sont sur la carte SD de la console (`3DS/System/sdmc`) et les sauvegardes Wii dans sa mémoire interne (`GC-Wii/System/Wii`), avec les jeux et chaînes installés ; un fichier dans leur `Saves/` le rappelle. Un dossier rangé autrement par une version précédente (`Roms/`, `BIOS/`, `Textures/`…) est réorganisé une fois, à l'ouverture de la liste de jeux, après une copie des sauvegardes dans `Backups/`.

**Switch** : les jeux `.nsp`, `.xci`, `.nca` et `.nro` apparaissent dans la liste (sous leur nom de fichier) et se lancent dans le cœur Eden, sur téléphone ARM 64 bits avec Vulkan. Les clés (`prod.keys`, `title.keys`) et le firmware (ses `.nca`, dans `Firmware/`) de votre console se mettent dans `Switch/System` du dossier Punica : Punica les installe dans Eden au lancement (ou par Réglages → logo Switch). Les sauvegardes Switch sont dans `Switch/Saves`, les mises à jour et DLC dans `Switch/System` (`nand/`, `sdmc/`) : elles restent si l'application est désinstallée ; elles ne sont jamais dans le dépôt.

**GameCube et Wii** : les jeux (`.iso`, `.gcm`, `.rvz`, `.wia`, `.ciso`, `.gcz`, `.wbfs`, `.wad`, …) apparaissent dans la liste et se lancent dans le cœur Dolphin, sur téléphone ARM 64 bits. Ses cartes mémoire sont dans `GC-Wii/Saves/GameCube`, la mémoire interne de la Wii (et ses sauvegardes) dans `GC-Wii/System/Wii`, le BIOS GameCube facultatif (`IPL.bin`) dans `GC-Wii/System/<USA, EUR ou JAP>`. Ses réglages : Réglages → logo GameCube / Wii.

**GBA** : les jeux `.gba` apparaissent dans la liste avec les autres et se lancent dans le cœur mGBA. La sauvegarde est dans `GBA/Saves` (`<jeu>.sav`), les sauvegardes d'état dans `GBA/States` (`<jeu>.ss0` à `.ss9`). Le BIOS est facultatif : mGBA a le sien ; un BIOS de votre console, `gba_bios.bin` dans `GBA/System` du dossier Punica, est pris s'il est là.

## Textures HD

### Prérequis

- Le moteur de rendu **« OpenGL »** (fonctionne sur tous les téléphones compatibles OpenGL ES 3.2) ou **« Compute »** (GPU Adreno uniquement). Le moteur « Logiciel » n'est pas pris en charge.
- **Seules les textures 3D sont concernées.** Les éléments 2D (menus, sprites, décors 2D) ne sont pas remplacés.
- Avec le moteur OpenGL, une texture HD fait au maximum 1024×1024, et les textures HD occupent au plus 128 Mo de mémoire vidéo. Au-delà, les textures supplémentaires restent en résolution d'origine (un message apparaît dans logcat).

### Dossiers

```
Punica/DS/Mods/<CODE DU JEU>/
├── dump/                       ← textures exportées depuis le jeu (taille d'origine)
│   └── tex_64x32_1a2b3c4d5e6f7a8b.png
└── (n'importe quel sous-dossier, sauf dump/)
    └── tex_64x32_1a2b3c4d5e6f7a8b.png   ← votre version HD
```

`Punica` est le dossier choisi au premier lancement (voir « Le dossier Punica ») : n'importe quel gestionnaire de fichiers y a accès. Sans lui, les textures sont dans `Android/data/<paquet de l'appli>/files/textures/`, et elles sont déplacées dans `Punica/DS/Mods` quand le dossier est choisi.

`<CODE DU JEU>` est le code à 4 caractères inscrit dans la ROM (par exemple `AMCE`). Le dossier `dump/` est créé automatiquement, ce qui permet de retrouver le code du jeu.

### Créer un pack de textures

1. Activez **« Dump textures »** dans Paramètres → Vidéo, puis jouez. Chaque texture 3D affichée est enregistrée dans `dump/`. Le jeu peut ralentir pendant l'export.
2. Agrandissez les images que vous voulez remplacer, par exemple avec un upscaler IA comme Real-ESRGAN, ou retouchez-les à la main.
   - **Gardez le même nom de fichier.**
   - La taille doit valoir **exactement 1×, 2×, 4×, 8× ou 16×** l'originale, avec le même facteur en largeur et en hauteur. Une texture de 64×32 en 4× fait donc 256×128.
   - Au maximum 4096 pixels de côté, et pas plus que ce que le GPU accepte.
3. Placez les images HD dans `DS/Mods/<CODE DU JEU>/`, dans le dossier lui-même ou un sous-dossier, mais **pas dans `dump/`**.
4. Désactivez « Dump textures », activez **« HD textures »**, puis relancez le jeu.

Les fichiers invalides (mauvaise taille, image illisible) sont ignorés et signalés dans le journal (logcat).

### Fonctionnement technique

- Une texture est identifiée par une empreinte (`xxHash64`) de son contenu **décodé**, donc palette appliquée. La même texture est reconnue où que le jeu la place en mémoire vidéo.
- **Moteur Compute** : le remplacement se fait dans son cache de textures (`GPU3D_Texcache.h`). Le shader lit les textures avec des coordonnées normalisées, donc une texture HD s'affiche correctement sans autre modification.
- **Moteur OpenGL** : ce moteur décode les textures directement dans le shader, à partir de la mémoire vidéo émulée. Les textures qui ont une version HD sont placées dans un atlas (`GPU3D_OpenGL_HDTextures.cpp`), et chaque polygone transmet au shader l'emplacement de sa texture dans l'atlas. Le shader lit alors l'atlas au lieu de la mémoire émulée, en respectant la répétition et le miroir des textures DS.
- La transparence est préservée : pour les formats DS sans transparence partielle, l'alpha est arrondi à « opaque » ou « transparent » pour ne pas changer le rendu des polygones.
- Les fichiers HD sont lus en arrière-plan : la première fois qu'une texture apparaît, elle s'affiche une ou deux images en résolution d'origine, le temps de lire son fichier, puis en HD, sans à-coup.
- Les textures HD déjà chargées restent en mémoire (cache de 128 Mo) pour éviter de relire le disque quand un jeu les recharge.

Code : `melonDS-android-lib/src/GPU3D_TextureReplacement.{h,cpp}`. Lecture et écriture des PNG avec [stb](https://github.com/nothings/stb), dans le domaine public.

### Limites connues

- Les textures 2D ne sont pas remplacées.
- Une texture HD sortie du cache de 128 Mo s'affiche de nouveau une ou deux images en résolution d'origine quand le jeu la réaffiche.

## Compilation

Le projet se compile avec Android Studio (Gradle et le NDK). Ouvrez le dossier `melonDS-android/`, après avoir récupéré les sous-modules (oboe, faad2, enet) :

```
git submodule update --init --recursive
```

Ces sous-modules pointent sur les versions récentes de chaque projet, pas forcément celles qu'utilisait melonDS-android d'origine. Si la compilation échoue dans l'un d'eux, il faudra revenir à une version plus ancienne.

## Tests

`tests/hd-textures/` contient deux tests qui tournent sur un PC Linux, sans téléphone ni carte graphique (rendu logiciel Mesa) :

- `replacement_test` vérifie le module de remplacement : export, chargement d'une texture 4×, transparence, et refus des images de mauvaise taille.
- `opengl_renderer_test` fait tourner **le vrai moteur OpenGL** sur un polygone texturé et compare l'image pixel par pixel : rendu d'origine, export, texture HD 4×, répétition et miroir des textures, et retour exact à l'image d'origine quand l'option est désactivée.

```
sudo apt install libegl-dev libgles-dev libegl-mesa0 ninja-build cmake
cmake -S tests/hd-textures -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-tests replacement_test opengl_renderer_test
cd build-tests && ./replacement_test && ./opengl_renderer_test
```
