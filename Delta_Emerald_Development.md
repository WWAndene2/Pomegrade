# Delta Emerald — plan de développement

**Le projet** : transformer Pokémon Rubis Oméga (ORAS) en un « Émeraude » moderne, **Delta Emerald** (Δ, le symbole de
Rayquaza, comme Ω pour Groudon et α pour Kyogre). C'est un mod d'ORAS (LayeredFS) joué dans Pomegrade, construit avec les
outils de `tools/remake/` déjà écrits pour le remake de Sinnoh (zones, maps, warps, dresseurs, rencontres, scripts). Hoenn
existe déjà dans ORAS : il s'agit surtout de **modifier** un jeu qui marche, pas de tout reconstruire.

**Statut** : projet actif (décision du propriétaire, 9 octobre : le remake de Sinnoh est en pause, seulement lui) ; plan
seulement, rien n'est commencé. Comme pour Sinnoh, les premières étapes sont de l'**outillage**, et la méthode de chaque
construction est présentée au propriétaire avant d'écrire le code.

**Comment lire ce document** : chaque point dit ce qu'il faut faire et d'où il vient. La colonne « Faisabilité » suit la
règle de `ORAS_LITTLEROOT.md` : **outil** (l'outillage actuel le fait ou presque), **données** (format lu, à écrire),
**code** (demande de trouver et patcher du code du jeu, emplacement inconnu), **gros chantier**. Ce qui vient d'une source
en ligne est cité (section Sources) ; ce qui vient de mémoire est marqué *à vérifier* et doit être contrôlé dans le jeu
d'origine avant d'être construit.

---

## 1. À intégrer — ce qu'Émeraude a et qu'ORAS n'a pas

### 1.1 La Zone de Combat (Battle Frontier)

La plus grosse absence d'ORAS, citée en premier par la critique (Nintendo World Report, KeenGamer, Shacknews). Dans
Émeraude, elle ouvre après la victoire contre la Ligue ; un personnage rencontré plusieurs fois (Scott, *à vérifier*)
donne l'accès. Les défis rapportent des Points Combat (PC/BP) échangés contre des objets et des capacités enseignées.

| Installation | Règle | Meneur (Frontier Brain) | Symbole | Faisabilité |
|---|---|---|---|---|
| Tour de Combat | séries de combats classiques | Anabel *à vérifier* | Capacité *à vérifier* | maps : outil ; règles : celles de la Maison de Combat d'ORAS |
| Dôme de Combat | tournoi à élimination, aperçu des adversaires | Tucker | Tactique | code |
| Palais de Combat | les Pokémon agissent selon leur nature | Spenser *à vérifier* | Esprit *à vérifier* | code |
| Arène de Combat | combats courts jugés si personne n'est K.O. | Greta | Cran | code |
| Usine de Combat | Pokémon de location | Noland | Savoir | code |
| Battle Pike (nom français *à vérifier*) | choix de salles, risques et soins incertains | Lucy *à vérifier* | Chance *à vérifier* | code |
| Pyramide de Combat | exploration, visibilité limitée, ressources | Brandon | Courage | code (proche d'un donjon : maps + scripts) |

Chaque Symbole a deux rangs, argent et or. À intégrer aussi :
- **la Grotte de l'Artisan** (Artisan Cave), dans la Zone : seul lieu de Smeargle sauvage ; un Simularbre (Sudowoodo) unique.
- **les maîtres des capacités de la Zone**, payés en PC et réutilisables.
- **l'emplacement** : l'île Combat d'ORAS (Battle Resort) est l'endroit naturel. Ses bâtiments se construisent avec les
  outils de maps ; en première version, chaque installation tourne avec les règles de la Maison de Combat qui existent déjà.

### 1.2 Les revanches

- **Champions d'arène** : dans Émeraude, on les rappelle par le Match Call du PokéNav et ils reviennent avec des équipes
  renforcées. ORAS n'a pas de revanches d'arène. **Faisabilité** : données (équipes) + scripts + un drapeau d'après-jeu
  (drapeaux de progression d'ORAS pas encore lus).
- **Dresseurs de route** : revanches par le Match Call. ORAS a le PokéMultinav ; le mécanisme d'appel est à étudier
  (code).
- **Pierre (Steven) à la Cascade Météore** après la Ligue, comme dans Émeraude. Faisabilité : maps + dresseur + script.

### 1.3 Nouveaux lieux d'Émeraude

| Lieu | Ce qu'il apporte | Faisabilité |
|---|---|---|
| Tour Mirage (désert, Route 111) | les fossiles Racine/Griffe ; la tour s'enfonce après le choix ; apparaît après avoir parlé au Fou des ruines de Vermilava (Serebii) | maps + script |
| Souterrain du désert | le second fossile après le Panthéon ; seul lieu de Métamorph à Hoenn | maps + rencontres |
| Colline des Dresseurs (Trainer Hill) | défi de dresseurs par étages, comme la Tour Dresseurs de RF/VF ; vend Potion Max, Guérison (PokeTools) ; emplacement *à vérifier* | maps + dresseurs ; le chronomètre : code |
| Extension du Parc Safari | Pokémon de Johto après l'histoire (Giant Bomb) | maps + rencontres |
| Grotte Altérée (Route 103) *à vérifier* | rencontres changées par événement | rencontres |
| Île du Sud | Latios/Latias | existe dans ORAS (îles Mirage) : à rapprocher |
| Grotte Tellurique / Grotte Marine *à vérifier* | Groudon et Kyogre après l'histoire, suivis par l'Institut Météo (Wikipedia) | maps + script |
| Îles des événements (Île Lointaine — Mew, Île Aurore — Deoxys, Nombril de la Mer — Lugia/Ho-Oh) *à vérifier* | légendaires d'événements par billets ; serveurs fermés : les rendre accessibles en jeu | maps + scripts |
| Tentes de Combat (Poivressel, Vergazon, Vermilava) *à vérifier* | mini-installations de la Zone avant la Ligue | maps ; règles : code |
| Repaire Magma *à vérifier* (Chemin Escarpé) | les deux repaires, Magma et Aqua, dans le même jeu | maps + scripts |

### 1.4 Le reste d'Émeraude

- **Combats doubles « libres »** : deux dresseurs qu'on peut affronter séparément s'associent si on passe devant eux
  ensemble (Wikipedia). Faisabilité : données de dresseurs + code (déclenchement).
- **Starter de Johto** offert par le Prof. Seko après le Pokédex national *à vérifier*. Données + script.
- **Maîtres des capacités** : la plupart de ceux de RF/VF (sauf Végé-Attaque, Rafale Feu, Hydroblast) et 15 capacités
  d'anciennes CT de la 2e génération ; ceux de Hoenn n'enseignent qu'une fois (Bulbapedia via la recherche). Données (les
  maîtres d'ORAS existent : à étendre).
- **Électacle (Volt Tackle)** par reproduction de Pikachu avec une Balle Lumière (StrategyWiki via la recherche). Déjà
  présent dans les générations suivantes : à vérifier dans ORAS.
- **Animations des Pokémon** : déjà présentes dans ORAS, rien à faire.

---

## 2. À changer — ORAS vers l'histoire et la structure d'Émeraude

| Point | ORAS (Rubis Oméga) | Delta Emerald | Faisabilité |
|---|---|---|---|
| Équipes adverses | une seule équipe (Magma) | Magma **et** Aqua, chacune avec son repaire et ses scènes | scripts + maps : gros morceau |
| Légendaires de l'histoire | Groudon réveillé | Groudon et Kyogre réveillés ensemble, s'affrontent ; le joueur appelle **Rayquaza** au Pilier Céleste pour les calmer à Atalanopolis | scripts + scènes : l'Épisode Delta d'ORAS a déjà Rayquaza au Pilier Céleste |
| Pilier Céleste | état d'ORAS | intact à la première visite, abîmé une fois Rayquaza capturable (Bulbapedia) | maps (deux états) + drapeau |
| 8e arène (Atalanopolis) | Marc (Wallace) | **Juan** (Eau) : Lovdisc, Barbicha, Phogleur, Colhomard, Hyporoi ; casse-glace, chaque dalle une seule fois (Serebii) | données + script ; modèle de Juan : présence dans ORAS à vérifier |
| Maître de la Ligue | Pierre (Steven) | **Marc (Wallace)** ; Pierre en défi caché à la Cascade Météore | données + scripts |
| Arènes | dispositions d'ORAS | dispositions et équipes d'Émeraude : Roxanne (Racaillou ×2, Tarinor), Bastien (Machoc, Méditikka, Makuhita ; l'arène s'éclaire en battant les dresseurs), Voltère (Voltorbe, Dynavolt, Magnéton, Élecsprint ; murs électriques à interrupteurs), Adriane (Chamallot, Limagma, Camérupt, Chartor ; trous entre deux niveaux), Norman (Spinda, Vigoroth, Linéon, Monaflèmit ; quiz par salles), Alizée (Tylton, Tropius, Bekipan, Airmure, Altaria ; portes tournantes), Lévy & Tatia en double (Kaorine, Xatu, Séléroc, Solaroc ; dalles de téléportation) (Serebii) | maps (outil) + données de dresseurs |
| Fossiles | choix au désert | Tour Mirage puis Souterrain du désert (1.3) | maps + scripts |
| Après-jeu | Épisode Delta, Maison de Combat | Épisode Delta **gardé** + Zone de Combat + revanches + lieux de 1.3 | voir 1.1 à 1.3 |

À **garder** d'ORAS, parce qu'appréciés (Shacknews, critiques) : le PokéRadar furtif (DexNav), le vol sur Latios/Latias et
les îles Mirage, l'Épisode Delta, les Bases Secrètes, les Méga-Évolutions (voir 3.6), les Concours Live.

---

## 3. À corriger — les défauts reprochés à ORAS

| # | Défaut | Correction | Faisabilité |
|---|---|---|---|
| 3.1 | Trop facile (Multi Exp. dès le début, pas de mode difficile) | niveaux et équipes d'Émeraude ou plus hauts ; Multi Exp. désactivé au départ ; mode difficile optionnel (niveaux plus hauts, IA plus agressive) | équipes : données ; Multi Exp. et mode : code |
| 3.2 | Après-jeu court | 1.1, 1.2, 1.3 | voir plus haut |
| 3.3 | Trop d'eau, dépendance aux CS | un objet ou un Pokémon de déplacement qui remplace Surf/Plongée/Cascade ; routes maritimes enrichies (îlots, dresseurs) | maps : outil ; sans CS : code |
| 3.4 | Pas de personnalisation du héros (Shacknews) | réactiver la boutique de vêtements de X/Y si son code existe encore dans le moteur ; à défaut, tenues prédéfinies | inconnue |
| 3.5 | Performances (chutes d'images en combat double/triple) | côté Pomegrade : rendu en haute résolution, 120 fps par génération d'images, réglages par appareil (déjà au plan, `DEVELOPMENT_NOTE.md`) | émulateur |
| 3.6 | Méga-Évolutions déséquilibrées | option : réservées à l'après-jeu, ou rééquilibrées | statistiques : données ; conditions : code |
| 3.7 | Îles Mirage au hasard (et StreetPass) | accès fixe : une île par jour selon un calendrier connu, ou un objet qui choisit l'île | code |
| 3.8 | Concours simplifiés | appréciations plus fines, récompenses utiles | code (module des concours non lu) |
| 3.9 | Bases Secrètes figées (serveurs fermés) | plus de meubles et d'emplacements, bases de dresseurs hors ligne, partage par fichier ou QR code | données + code |
| 3.10 | Pas de Pokémon qui suit | l'ajouter au moteur (le premier Pokémon de l'équipe suit les pas du joueur) | gros chantier, rien d'étudié |
| 3.11 | Pokédex incomplet, échanges en ligne fermés | tous les Pokémon capturables à Hoenn ; évolutions par échange remplacées par objet ou niveau ; légendaires d'événements accessibles en jeu (1.3) | rencontres et évolutions : données |
| 3.12 | Accès à la Zone seulement après toute l'histoire (critique d'IGN sur Émeraude) | Tentes de Combat avant la Ligue (1.3) | maps ; règles : code |

---

## 4. Ordre proposé (à valider)

Comme pour le remake de Sinnoh : d'abord des étapes d'outillage courtes, chacune vérifiée dans l'émulateur sans écran puis
sur le téléphone du propriétaire.

1. **D1 — Revanches des champions** : équipes de revanche d'Émeraude pour les huit champions, déclenchées après la Ligue.
   Valide d'un coup les briques de base : dresseurs, drapeaux de progression, scripts d'ORAS.
2. **D2 — Juan et Marc** : Juan à Atalanopolis, Marc Maître de la Ligue, Pierre à la Cascade Météore.
3. **D3 — Arènes d'Émeraude** : dispositions et équipes (2).
4. **D4 — Lieux d'Émeraude** : Tour Mirage, Souterrain du désert, Parc Safari étendu, Colline des Dresseurs (1.3).
5. **D5 — Zone de Combat, bâtiments** sur l'île Combat, règles de la Maison de Combat en première version (1.1).
6. **D6 — Histoire à deux équipes et Rayquaza** (2) : le plus gros morceau de scénario.
7. **D7 — Corrections** (3), au fur et à mesure que le code du jeu est lu (mode difficile, Multi Exp., CS, îles Mirage).
8. **D8 — Règles propres des installations de la Zone** (1.1) et Pokémon qui suit (3.10) : gros chantiers, en dernier.

---

## Sources

- Wikipédia, *Pokémon Emerald* : https://en.wikipedia.org/wiki/Pok%C3%A9mon_Emerald (combats doubles, PokéNav, revanches,
  Zone de Combat, histoire à deux équipes, Latias/Latios, Pierre à la Cascade Météore, Groudon/Kyogre, critiques).
- Serebii, *Emerald Gyms* : https://serebii.net/emerald/gym.shtml (arènes, équipes, dispositions, Juan, revanches par Match
  Call) ; *Mirage Tower* : https://www.serebii.net/emerald/illusionpillar.shtml.
- NationalDex.io, différences de version : https://nationaldex.io/guide/pokemon-ruby-sapphire-emerald/version-differences ;
  Zone de Combat : https://nationaldex.io/guide/pokemon-ruby-sapphire-emerald/battle-frontier.
- Bulbapedia, *Symbol* : https://bulbapedia.bulbagarden.net/wiki/Symbol (Symboles argent/or, Noland/Savoir, Greta/Cran,
  Tucker/Tactique, Brandon/Courage) ; *Pokémon Emerald Version* (via moteur de recherche) :
  https://bulbapedia.bulbagarden.net/wiki/PKMN_E (Tour Mirage, Pilier Céleste, maîtres des capacités, Smeargle,
  Simularbre).
- PokémonDB, *Emerald Gym Leaders & Elite Four* : https://pokemondb.net/emerald/gymleaders-elitefour.
- PokeTools, *Trainer Hill* : https://www.poketools.com/emerald/trainer-hill ; *Artisan Cave* :
  https://www.poketools.com/emerald/artisan-cave.
- Giant Bomb, *Pokémon Emerald* : https://www.giantbomb.com/games/3030-11552/ (Souterrain, Métamorph, Parc Safari étendu).
- Critiques d'ORAS : Nintendo World Report (https://nintendoworldreport.com/editorial/39139), KeenGamer
  (https://www.keengamer.com/articles/reviews/retrospective-review-pokemon-omega-ruby-and-alpha-sapphire/), Shacknews
  (https://shacknews.com/article/87314/pokemon-alpha-sapphire-review-cant-go-hoenn-again).

Les pages complètes de Bulbapedia et StrategyWiki n'ont pas pu être lues directement (accès refusé) : les points marqués
*à vérifier* viennent de mémoire et doivent être contrôlés avant construction.
