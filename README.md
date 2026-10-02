# Pomegrade

Pomegrade est une version modifiée de [melonDS-android](https://github.com/rafaelvcaetano/melonDS-android) qui ajoute le **remplacement de textures HD** pour la 3D.

## Contenu du dépôt

| Dossier ou fichier | Rôle |
|---|---|
| `melonDS-android/` | L'application Android (Kotlin + JNI) |
| `melonDS-android/melonDS-android-lib/` | Le cœur de l'émulateur (C++), intégré directement au dépôt et non plus comme sous-module |
| `melonDS-master.zip` | Les sources de melonDS pour PC (référence) |
| `melonDS.v.2.0.1.PS.apk` | La version officielle de melonDS-android (référence) |

Les BIOS et firmwares Nintendo ont été retirés du dépôt pour des raisons de copyright. Gardez-les sur votre appareil et choisissez-les dans les paramètres de l'application.

## Textures HD

### Prérequis

- Le moteur de rendu **« Compute »** (Paramètres → Vidéo → Moteur de rendu). Les textures HD ne fonctionnent pas avec les moteurs « OpenGL » et « Logiciel ».
- Ce moteur demande **OpenGL ES 3.2 et un GPU Qualcomm Adreno**. Sur les autres appareils, l'application ne le propose pas : cette restriction vient de melonDS-android d'origine.
- **Seules les textures 3D sont concernées.** Les éléments 2D (menus, sprites, décors 2D) ne sont pas remplacés.

### Dossiers

```
Android/data/<paquet de l'appli>/files/textures/<CODE DU JEU>/
├── dump/                       ← textures exportées depuis le jeu (taille d'origine)
│   └── tex_64x32_1a2b3c4d5e6f7a8b.png
└── (n'importe quel sous-dossier, sauf dump/)
    └── tex_64x32_1a2b3c4d5e6f7a8b.png   ← votre version HD
```

`<CODE DU JEU>` est le code à 4 caractères inscrit dans la ROM (par exemple `AMCE`). Le dossier `dump/` est créé automatiquement, ce qui permet de retrouver le code du jeu.

### Créer un pack de textures

1. Activez **« Dump textures »** dans Paramètres → Vidéo, puis jouez. Chaque texture 3D affichée est enregistrée dans `dump/`. Le jeu peut ralentir pendant l'export.
2. Agrandissez les images que vous voulez remplacer, par exemple avec un upscaler IA comme Real-ESRGAN, ou retouchez-les à la main.
   - **Gardez le même nom de fichier.**
   - La taille doit valoir **exactement 1×, 2×, 4×, 8× ou 16×** l'originale, avec le même facteur en largeur et en hauteur. Une texture de 64×32 en 4× fait donc 256×128.
   - Au maximum 4096 pixels de côté, et pas plus que ce que le GPU accepte.
3. Placez les images HD dans `textures/<CODE DU JEU>/`, dans le dossier lui-même ou un sous-dossier, mais **pas dans `dump/`**.
4. Désactivez « Dump textures », activez **« HD textures »**, puis relancez le jeu.

Les fichiers invalides (mauvaise taille, image illisible) sont ignorés et signalés dans le journal (logcat).

### Fonctionnement technique

- Une texture est identifiée par une empreinte (`xxHash64`) de son contenu **décodé**, donc palette appliquée. La même texture est reconnue où que le jeu la place en mémoire vidéo.
- Le remplacement se fait dans le cache de textures du moteur Compute (`GPU3D_Texcache.h`). Le shader lit les textures avec des coordonnées normalisées, donc une texture HD s'affiche correctement sans autre modification.
- La transparence est préservée : pour les formats DS sans transparence partielle, l'alpha est arrondi à « opaque » ou « transparent » pour ne pas changer le rendu des polygones.
- Les textures HD déjà chargées restent en mémoire (cache de 128 Mo) pour éviter de relire le disque quand un jeu les recharge.

Code : `melonDS-android-lib/src/GPU3D_TextureReplacement.{h,cpp}`. Lecture et écriture des PNG avec [stb](https://github.com/nothings/stb), dans le domaine public.

### Limites connues

- Les textures HD sont chargées pendant la partie : de légers à-coups peuvent apparaître la première fois qu'une texture apparaît.
- Les textures 2D ne sont pas remplacées.
- Sur Android 11 et plus, certains gestionnaires de fichiers n'ont pas accès à `Android/data/`. Utilisez un ordinateur en USB, ou un gestionnaire qui y a accès.

## Compilation

Le projet se compile avec Android Studio (Gradle et le NDK). Ouvrez le dossier `melonDS-android/`, après avoir récupéré les sous-modules (oboe, faad2, enet) :

```
git submodule update --init --recursive
```

Ces sous-modules pointent sur les versions récentes de chaque projet, pas forcément celles qu'utilisait melonDS-android d'origine. Si la compilation échoue dans l'un d'eux, il faudra revenir à une version plus ancienne.
