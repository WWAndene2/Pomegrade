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

## 4. Ordre : du plus facile et rapide au plus long (décision du propriétaire, 9 octobre)

Classé par ce que demande chaque point, pas par importance. Les durées sont des ordres de grandeur estimés, pas mesurés ;
elles supposent que la brique de base du palier (lire un format, un drapeau, un script) marche du premier coup, ce qui n'est
pas garanti. Chaque point se termine par une vérification dans l'émulateur sans écran, puis sur le téléphone.

**Palier 1 — données seules** (formats déjà lus ou presque ; quelques heures à un jour chacun)
1. Équipes des dresseurs plus dures, façon Émeraude (3.1, équipes seulement)
2. Statistiques des Méga-Évolutions rééquilibrées (3.6, statistiques seulement)
3. Évolutions par échange remplacées par objet ou niveau (3.11)
4. Tables de rencontres : tous les Pokémon capturables à Hoenn, Métamorph, Smeargle, Pokémon de Johto (3.11, 1.3)
5. Maîtres des capacités étendus, ceux d'Émeraude (1.4)
6. Électacle : vérifier sa présence dans ORAS (1.4)

**Palier 2 — données + un script simple ou un drapeau** (la première fois, il faut lire les drapeaux d'ORAS ; ensuite
rapide)
7. Revanches des huit champions avec équipes d'Émeraude (1.2) — le premier à faire : il valide drapeaux et scripts
8. Juan champion à Atalanopolis, Marc Maître de la Ligue (2)
9. Pierre en défi caché à la Cascade Météore (1.2)
10. Starter de Johto offert après le Pokédex national (1.4)

**Palier 3 — maps avec l'outillage existant** (comme les maisons de Sinnoh ; quelques jours chacun)
11. Arènes d'Émeraude : dispositions et dresseurs (2)
12. Extension du Parc Safari (1.3)
13. Souterrain du désert (1.3)
14. Tour Mirage et ses deux fossiles (1.3, 2)
15. Colline des Dresseurs, sans chronomètre (1.3)
16. Grotte Tellurique et Grotte Marine pour Groudon et Kyogre (1.3)
17. Îles des légendaires d'événement accessibles en jeu (1.3, 3.11)
18. Pilier Céleste à deux états (2)
19. Routes maritimes enrichies d'îlots et de dresseurs (3.3, maps seulement)
20. Bâtiments de la Zone de Combat sur l'île Combat, avec les règles de la Maison de Combat (1.1, première version)
21. Tentes de Combat, bâtiments seulement (1.3, 3.12)

**Palier 4 — scénario** (scripts de l'histoire d'ORAS à comprendre d'abord ; semaines)
22. Repaire Magma ajouté, les deux repaires (1.3)
23. Histoire à deux équipes, Magma et Aqua (2)
24. Groudon et Kyogre ensemble, Rayquaza appelé pendant l'histoire, en réutilisant les scènes de l'Épisode Delta (2)

**Palier 5 — code du jeu à trouver puis patcher** (emplacement inconnu : de quelques jours à impossible)
25. Combats doubles « libres » : d'abord vérifier si ORAS les a déjà (1.4)
26. Multi Exp. désactivé au départ (3.1)
27. Mode difficile optionnel (3.1)
28. Méga-Évolutions limitées à l'après-jeu (3.6, conditions)
29. Accès fixe aux îles Mirage (3.7)
30. Se passer des CS (3.3)
31. Match Call pour les revanches des dresseurs de route (1.2)
32. Colline des Dresseurs : son chronomètre (1.3)
33. Boutique de vêtements de X/Y (3.4)
34. Concours plus fins (3.8)
35. Bases Secrètes : partage par fichier, bases hors ligne (3.9)

**Palier 6 — gros chantiers** (mois)
36. Règles propres des installations de la Zone de Combat : Usine, Dôme, Palais, Arène, Reptile, Pyramide (1.1)
37. Pokémon qui suit le joueur (3.10)

**Côté Pomegrade, hors mod** : les performances (3.5) sont déjà dans `DEVELOPMENT_NOTE.md`.

## 5. Solutions, point par point (dans l'ordre de la section 4)

Chaque solution est donnée au propriétaire une à la fois (sa demande, 9 octobre) et notée ici. Ce sont des méthodes, pas du
travail fait : rien n'est construit tant que le propriétaire ne l'a pas validée.

### Point 1 — Équipes de dresseurs plus dures, façon Émeraude

**Problème** : équipes petites et niveaux bas dans ORAS ; le Multi Exp. met le joueur en sur-niveau.

1. **Outil** : une commande `remake_tool oras-trainers` qui exporte tous les dresseurs d'ORAS dans un fichier texte lisible
   (nom, classe, lieu, Pokémon, niveaux, capacités, objets) et réécrit le fichier modifié dans le mod. Connu : le format de
   base d'un dresseur (classe, équipe ; `tools/remake/ORAS_ENGINE.md`). À lire d'abord : les formats 1 à 3 (probablement les
   capacités et les objets : une supposition).
2. **Référence** : les équipes d'Émeraude, publiques dans la décompilation communautaire `pokeemerald` (liste des dresseurs,
   Pokémon, niveaux), chaque dresseur d'Émeraude mis en face de son équivalent d'ORAS par nom et lieu.
3. **Règle par dresseur** : l'équipe d'Émeraude si elle est plus forte que celle d'ORAS, sinon celle d'ORAS ; niveaux +10 à
   15 % pour compenser le Multi Exp. (à régler en jouant) ; champions, Ligue et rivaux avec capacités et objets choisis ; les
   Pokémon absents d'Émeraude gardés quand ils vont mieux à la région.
4. **Vérification** : un combat contre un dresseur modifié en émulation sans écran (capture de l'équipe adverse), puis un test
   de difficulté sur le téléphone.

**Risque** : si les formats 1 à 3 ne se lisent pas, la première version ne change que les espèces et les niveaux.

### Point 2 — Statistiques des Méga-Évolutions rééquilibrées

**Problème** : certaines Méga écrasent le jeu (souvent citées, de mémoire, à confirmer avec le propriétaire : Méga-Rayquaza,
Méga-Kangourex et son talent Amour Filial, Méga-Drattak, Méga-Ectoplasma, Méga-Mysdibule), d'autres sont inutiles
(Méga-Dardargnan, Méga-Nanméouïe, Méga-Steelix).

1. **Outil** : une commande `remake_tool oras-personal` qui exporte la table des fiches des Pokémon (statistiques de base,
   types, talents ; chaque Méga y est une forme à part) dans un fichier texte et la réécrit dans le mod. À confirmer :
   l'emplacement de la table (archive `a/1/9/5` *à vérifier*, de mémoire).
2. **Règle** : la règle officielle gardée (une Méga gagne exactement +100 points sur sa forme de base), seule la répartition
   change ; trop fortes : moins dans la statistique écrasante, et leur talent remplacé par un autre talent existant quand c'est
   lui le problème (Amour Filial, Pouvoir Pur ; le talent est une valeur de la fiche) ; faibles : les points vers ce qui leur
   manque.
3. **Liste** : un tableau avant/après des Méga concernées, validé par le propriétaire avant toute modification.
4. **Vérification** : une Méga-Évolution en combat en émulation sans écran (capture des statistiques), puis un essai sur le
   téléphone.

**Risques** : la table peut ne pas être où on le pense (à trouver d'abord avec les outils de recherche). Méga-Rayquaza se
passe de Méga-Gemme par une règle du code, pas de la fiche : la limiter est du palier 5.

### Point 3 — Évolutions par échange remplacées

**Problème** : les échanges en ligne de la 3DS sont fermés ; les Pokémon qui n'évoluent qu'échangés ne peuvent plus évoluer
seul. Concernés (de mémoire, à confirmer dans les données) : échange simple — Kadabra, Machopeur, Gravalanch, Spectrum,
Géolithe, Ouvrifier ; échange en tenant un objet — Onix et Insécateur (Peau Métal), Têtarte et Ramoloss (Roche Royale),
Hypocéan (Écaille Draco), Porygon (Améliorator), Porygon2 (CD Douteux), Élektek (Électriseur), Magmar (Magmariseur),
Rhinoféros (Protecteur), Téraclope (Tissu Fauche), Coquiperl (Dent ou Écaille Océan), Barpau (Bel'Écaille), Fluvetin
(Sachet Senteur), Sucroquin (Chantibonbon) ; échange entre deux Pokémon — Carabing et Escargaume.

1. **Outil** : une commande `remake_tool oras-evolutions` qui exporte pour chaque Pokémon sa méthode d'évolution, son
   paramètre et l'espèce obtenue, et réécrit la table dans le mod. À confirmer : l'emplacement de la table et les valeurs des
   codes de méthode (échange, échange avec objet, utiliser un objet : codes distincts, valeurs *à vérifier*).
2. **Règle** : échange simple → niveau (37 pour Kadabra, Machopeur, Gravalanch, Spectrum ; 40 pour Géolithe, Ouvrifier :
   valeurs courantes de la communauté, à valider) ; échange avec objet → utiliser l'objet sur le Pokémon, comme une pierre ;
   Coquiperl garde ses deux objets ; Carabing et Escargaume → niveau ou objet.
3. **Objets** : chaque objet d'évolution trouvable à Hoenn (posé sur la carte, vendu ou offert) ; ceux qui manquent dans
   ORAS ajoutés aux objets des maps ou aux magasins.
4. **Vérification** : une évolution par niveau (combat) et une par objet (Sac) en émulation sans écran, puis sur le
   téléphone.

**Risque** : si le jeu refuse d'utiliser un objet autre qu'une pierre sur un Pokémon, « utiliser l'objet » passe au palier 5
(code) ; repli : évolution par niveau en tenant l'objet, méthode qui existe déjà (celle de Scorplane).

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
