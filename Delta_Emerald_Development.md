# Delta Emerald — plan de développement

**Le projet** : transformer Pokémon Rubis Oméga (ORAS) en un « Émeraude » moderne, **Delta Emerald** (Δ, le symbole de
Rayquaza, comme Ω pour Groudon et α pour Kyogre). C'est un mod d'ORAS (LayeredFS) joué dans Pomegrade, construit avec les
outils de `tools/remake/` déjà écrits pour le remake de Sinnoh (zones, maps, warps, dresseurs, rencontres, scripts). Hoenn
existe déjà dans ORAS : il s'agit surtout de **modifier** un jeu qui marche, pas de tout reconstruire.

**Statut** : projet actif (décision du propriétaire, 9 octobre : le remake de Sinnoh est en pause, seulement lui) ; solutions
écrites pour les points 1 à 37 ; points 38 et 39 construits et vus dans l'émulateur sans écran (11 octobre), pas encore sur le
téléphone. Comme pour Sinnoh, les premières étapes sont de l'**outillage**, et la méthode de chaque construction est
présentée au propriétaire avant d'écrire le code.

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
6. Électacle : présente dans ORAS (n° 344, vérifié) ; sa façon de l'apprendre reste à vérifier (1.4)

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

**Ajouté par le propriétaire (10 octobre), à faire en premier** : le vol en mode ciel à la place du Vol classique (point 38,
palier 5) puis sur le Pokémon qui connaît Vol (point 39, palier 6 pour tous les Pokémon ; une liste choisie d'abord).

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

### Point 4 — Tous les Pokémon capturables à Hoenn

**Problème** : beaucoup de Pokémon ne s'obtiennent que par échange, Banque Pokémon ou événement, services fermés ; Émeraude
ajoute Métamorph (Souterrain du désert), Smeargle (Grotte de l'Artisan) et des Pokémon de Johto (Parc Safari).

**Déjà connu** (`tools/remake/ORAS_ENGINE.md` section 6, vérifié en jeu, run `enc2`) : chaque zone a sa table (fichier 3 de la
zone, identique à son entrée du membre 537) ; plusieurs listes par terrain (herbe 12 emplacements, herbe sombre, eau, pêche,
hordes…) ; un emplacement = 4 octets (espèce bits 0-10, forme bits 11-15, niveau minimum, niveau maximum).

1. **Outil** : une commande `remake_tool oras-encounters` qui exporte les rencontres de chaque zone en texte (lieu, terrain,
   emplacement, espèce, niveaux) et réécrit les deux copies ensemble (fichier de la zone et membre 537).
2. **Liste des manquants** : l'outil compare les 721 Pokémon aux rencontres, cadeaux et légendaires du jeu et sort ceux qu'on
   ne peut pas attraper ; c'est elle qui fixe le travail.
3. **Règle** : les ajouts d'Émeraude à leur place (quand les lieux existent, palier 3) ; les autres selon type et habitat, sur
   des emplacements rares pour garder ceux d'origine ; les exclusifs de Saphir Alpha dans Rubis Oméga, plus rares ; niveaux
   suivant la progression. Tableau des placements validé par le propriétaire avant écriture.
4. **Vérification** : une rencontre déclenchée dans une zone modifiée en émulation sans écran (méthode du run `enc2`), puis
   sur le téléphone.

**Risques** : le DexNav et les îles Mirage ont peut-être leurs propres listes (les îles sont choisies par le code, point 29) ;
le type de liste 2 n'est pas identifié (non bloquant).

### Point 5 — Maîtres des capacités d'Émeraude

**Problème** : Émeraude ajoute une trentaine de capacités enseignées (la plupart de celles de RF/VF et 15 anciennes CT de la
2e génération ; celles de la Zone en PC et réutilisables, celles de Hoenn une seule fois). ORAS a ses propres maîtres (île
Combat en PC, quelques-uns dans Hoenn), pas la même liste. Liste d'Émeraude (de mémoire, à confirmer) : Ultimapoing,
Danse-Lames, Ultimawashi, Plaquage, Damoclès, Riposte, Frappe Atlas, Copie, Métronome, E-Coque, Dévorêve, Cage-Éclair,
Explosion, Éboulement, Clonage, Dynamopoing, Roulade, Boost, Ronflement, Vent Glace, Ténacité, Coud'Boue, Poing-Glace,
Vantardise, Blabla Dodo, Météores, Boul'Armure, Poing-Éclair, Poing de Feu, Taillade.

1. **Trouver** : la liste des capacités de chaque maître d'ORAS (table du code ou script du personnage : *à trouver*) et les
   cases « peut apprendre » de chaque Pokémon (dans sa fiche : lues par l'outil du point 2).
2. **Comparer** : l'outil sort les capacités d'Émeraude absentes des maîtres d'ORAS (une partie y est déjà, les poings
   élémentaires par exemple).
3. **Ajouter** : à un maître existant ou à un nouveau personnage, aux places d'Émeraude (Zone de Combat en PC, villes de
   Hoenn) ; qui peut les apprendre : la compatibilité d'Émeraude, et une règle de cohérence (type, famille) pour les Pokémon
   plus récents, tableau validé par le propriétaire ; tous réutilisables (échanges fermés).
4. **Vérification** : apprendre une capacité ajoutée auprès du maître en émulation sans écran, puis sur le téléphone.

**Risques** : une liste de maîtres fixée dans le code sans place libre fait passer le point au palier 5 ; les cases « peut
apprendre » sont en nombre fixe par Pokémon : sans case libre, repli en remplaçant des capacités moins utiles chez les maîtres
existants.

### Point 6 — Électacle (Volt Tackle)

**Vérifié (10 octobre)** : la capacité existe dans ORAS, numéro 344 de la liste des capacités (« Électacle », `a/0/7/4`
membre 14 ligne 344 ; « Volt Tackle » dans `a/0/7/3`), le même numéro que dans Émeraude. **À vérifier** : comment on
l'apprend. Dans Émeraude, un Pikachu ou un Raichu tenant une Balle Lumière pond un Pichu qui la connaît ; les jeux suivants
gardent cette règle de la pension dans le code, ORAS sans doute (de mémoire).

1. **Vérifier en jeu** : en émulation sans écran, Pikachu tenant une Balle Lumière à la pension, l'œuf éclos, les capacités du
   Pichu lues sur une capture (sauvegarde préparée par les outils ou faite par le propriétaire dans son jeu).
2. **Si ça marche** : rien à changer, sauf vérifier qu'une Balle Lumière s'obtient à Hoenn (Pikachu sauvage ou posée) et
   l'ajouter sinon (comme les objets du point 3).
3. **Sinon** : sans toucher au code, par sa fiche d'apprentissage (données) ou par un maître des capacités (point 5, le plus
   proche d'une méthode « spéciale » comme dans Émeraude ; recommandé).

**Risque** : les outils de sauvegarde déplacent le joueur mais n'ajoutent pas encore de Pokémon ni d'objet ; repli : la
sauvegarde faite par le propriétaire.

### Point 7 — Revanches des huit champions d'arène

**Problème** : ORAS ne permet de réaffronter que la Ligue ; dans Émeraude, les champions se rappellent par le Match Call après
la Ligue et reviennent avec des équipes renforcées sur plusieurs paliers (Serebii, Wikipédia).

**Connu** : le combat contre un dresseur est tracé dans le code (`tools/remake/ORAS_ENGINE.md`) ; les scripts de zone se lisent
et se réassemblent (`oras-script`, `oras-zone-script`, assembleur AMX). **Pas encore lus** : les drapeaux de progression (« la
Ligue est battue ») et l'instruction de script qui lance un combat de dresseur.

1. **Lire** : le drapeau de fin de Ligue (comparer une sauvegarde d'avant et d'après le Panthéon : les outils comparent déjà deux
   sauvegardes bloc par bloc) ; l'instruction de combat (dans le script d'une arène d'ORAS, où le champion lance son combat).
2. **Équipes** : huit nouveaux dresseurs par l'outil du point 1, l'équipe de la dernière revanche d'Émeraude (`pokeemerald`)
   adaptée à l'après-jeu d'ORAS (niveaux, capacités, Pokémon plus récents de la même famille) ; tableau validé par le
   propriétaire.
3. **Script de chaque arène** : après la Ligue, le champion propose une revanche ; une par jour (l'idée d'Émeraude sans le
   Match Call, qui relève du palier 5).
4. **Textes** : une phrase de défi et une de défaite par champion, ajoutées aux textes de la zone.
5. **Vérification** : une sauvegarde d'après la Ligue (celle du propriétaire, ou le drapeau posé dans la sienne une fois trouvé),
   la revanche lancée en émulation sans écran (capture de l'équipe), puis sur le téléphone.

Le **premier point à construire** : il teste dresseurs, drapeaux, scripts et textes, dont les points suivants se servent.

**Risques** : un script d'arène trop gros pour les limites connues des fichiers de zone : la revanche passe par un personnage à
part posé dans l'arène ; si l'heure ne se lit pas dans un script, la revanche est disponible à chaque visite.

### Point 8 — Juan champion à Atalanopolis, Marc Maître de la Ligue

**Vérifié (10 octobre)** : Juan n'est pas un dresseur d'ORAS ; son nom n'apparaît que dans un dialogue où Lisia le cite comme le
maître de Marc (textes de zone, `a/0/8/4` membre 447). Pas d'entrée dans les noms de dresseurs ; pas de modèle 3D (probable, non
vérifié). Marc a deux entrées dans les noms de dresseurs (`a/0/7/4` membre 22) : ligne 572 avec les champions, ligne 943 juste
après Pierre (942), une autre version de lui à confirmer en lisant les fiches.

1. **Modèle de Juan** (choix du propriétaire) : a) un modèle de personnage existant proche (gentleman, artiste) ; b) un modèle
   existant recoloré avec les outils de textures (`oras-asset ... recolour`), recommandé pour commencer ; c) un vrai modèle
   (gros chantier).
2. **Dresseur** : Juan créé par l'outil du point 1 (classe Champion ; équipe d'Émeraude : Lovdisc, Barbicha, Phogleur,
   Colhomard, Hyporoi, Serebii ; adaptée au niveau d'ORAS), son nom ajouté aux noms de dresseurs.
3. **Arène d'Atalanopolis** : dans le script de la zone, le combat contre Marc devient le combat contre Juan (personnage, textes
   de défi et de défaite) ; le badge ne change pas ; la disposition d'Émeraude relève du point 11.
4. **Maître de la Ligue** : dans la salle du Maître, Pierre remplacé par Marc (équipe de Maître d'Émeraude adaptée, ou l'entrée
   943 si elle est déjà forte ; personnage, textes, scène finale) ; Pierre va à la Cascade Météore (point 9).
5. **Vérification** : les deux combats lancés en émulation sans écran (sauvegardes avant la 8e arène et devant la salle du
   Maître ; captures de l'adversaire), puis sur le téléphone.

**Risque** : Marc et Pierre apparaissent dans des scènes de l'histoire, dont les scripts relèvent du palier 4. Première version :
seulement les combats et les personnages de l'arène et de la salle du Maître, les scènes d'histoire gardées.

### Point 9 — Pierre en défi caché à la Site Météore

**Vérifié (10 octobre)** : la Cascade Météore s'appelle Site Météore dans ORAS (`a/0/7/4` membre 90 ligne 272), zones 71-74.
La **zone 74** est une salle du fond, montée par un escalier depuis la zone 73 (warp kind 1281), avec un seul élément (modèle
8193, type 4 : sans doute un objet, une supposition) : l'équivalent de la salle de Pierre dans Émeraude.

1. **Lieu** : la zone 74, sans nouvelle map ; un personnage ajouté (l'outillage le fait : `NewCharacter`).
2. **Personnage** : le modèle de Pierre, présent dans l'histoire d'ORAS (numéro à lire dans une zone où il apparaît), placé face
   à l'escalier, devant l'objet actuel ou à sa place (choix du propriétaire).
3. **Dresseur** : une équipe de défi par l'outil du point 1 ; dans Émeraude Airmure, Kaorine, Galeking, Vacilys, Armaldo,
   Métalosse vers le niveau 76-78 (de mémoire, à confirmer dans `pokeemerald`), avec la Méga-Métalosse d'ORAS ; le dresseur
   Pierre de la ligne 942 des noms peut servir de base (fiche à lire).
4. **Script** : Pierre présent après la Ligue (drapeau du point 7) et seulement quand Marc est Maître (point 8) ; un texte puis
   le combat ; ensuite disponible une fois par jour.
5. **Vérification** : sauvegarde d'après la Ligue, Pierre affronté dans la zone 74 en émulation sans écran (capture), puis sur
   le téléphone.

**Risques** : dépend des points 7 et 8 (construit après eux). La Site Météore sert aussi à l'Épisode Delta (Zinnia, de
mémoire) : Pierre n'y apparaît qu'après l'Épisode Delta si ses scènes le demandent.

### Point 10 — Starter de Johto offert par le Prof. Seko

**Émeraude** : le Pokédex de Hoenn complété, le Prof. Seko offre Germignon, Héricendre ou Kaiminus (de mémoire, à confirmer).
**Inconnu** : si ORAS offre déjà des starters d'autres régions après l'histoire (vague souvenir, non vérifié) : à vérifier en
premier.

1. **Vérifier** : lire le script du laboratoire (sa zone à Bourg-en-Vol) et y chercher les dons de Pokémon (espèces 152, 155,
   158) ; s'ils y sont, le point est réglé sans rien faire.
2. **Sinon, ajouter** : dans le script de la zone, la condition « Pokédex de Hoenn complet » (compteur ou drapeau, à lire comme
   au point 7) ; trois Poké Balls sur la table (trois objets de zone), le choix, le Pokémon donné au niveau 5 ; un drapeau
   « déjà reçu ».
3. **Textes** : les phrases du Prof. Seko ajoutées aux textes de la zone.
4. **Vérification** : la condition abaissée le temps du test (« après la Ligue »), le don vérifié en émulation sans écran
   (capture de l'équipe), la vraie condition remise ; puis sur le téléphone.

**Risques** : l'instruction qui donne un Pokémon est à lire dans un script d'ORAS qui en donne déjà un (starter du début,
pension) ; si « Pokédex complet » ne se lit pas dans un script, une autre condition (après la Ligue ou l'Épisode Delta).

### Ce qui sert à plusieurs points (lu avant de commencer les paliers 2 à 6)

- **Plafond des dresseurs** (vérifié, `tools/remake/ORAS_ENGINE.md`, `Trainer_ReadData` 0x453E08) : le jeu lit le dresseur 0 ou
  un numéro au-delà de 949 comme le dresseur 1. Tout dresseur ajouté (points 7, 8, 9, 11, 15, 19, 20, 21) prend donc d'abord
  un numéro existant inutilisé (liste à sortir avec l'outil du point 1 : fiches jamais citées par un script de zone ni par un
  personnage), puis, quand ils manquent, un patch de code relève la borne (une constante dans `Trainer_ReadData`, la même
  méthode que les zones 536 → 1024 : `oras-engine`).
- **Drapeaux et compteurs** : la première lecture se fait au point 7 (sauvegarde avant / après le Panthéon, comparée bloc par
  bloc) ; les points 9, 10, 12, 13, 14, 16, 18, 22 à 24 s'en servent.
- **Nouvelles zones** : `remake_tool oras-sandbox` les construit à partir de 538 (vérifié sans écran) ; pas encore construits :
  les warps entre une zone de Hoenn et une zone nouvelle dans les deux sens, les rebords est/ouest, le relief, les tables de
  rencontres propres. Ces quatre briques précèdent les points 12 à 21 ; chacune se vérifie seule dans un bac à sable avant de
  servir.
- **Code** : un patch s'écrit en `code.ips` (le programme) ou par `cro_patch.py` (un module), appliqués par Azahar depuis le
  dossier du mod (vérifié au point 38). Un module modifié ne passe que dans Azahar, qui ne vérifie pas les empreintes des
  modules ; c'est le cas de Pomegrade.

### Palier 3 — méthode commune

Chaque lieu est un ensemble de zones nouvelles faites par `oras-sandbox`, reliées à Hoenn par un warp ajouté à une zone
existante, avec ses personnages, objets, dresseurs et rencontres. Avant tout bâtiment ajouté sur une carte de Hoenn, une
question se règle une fois pour toutes : peut-on ajouter une porte à un morceau de carte du jeu sans le reconstruire ? Le
modèle 3D porte la façade : une porte « invisible » (un warp sur une case existante, par exemple au fond d'un bâtiment déjà
là) est la voie sûre ; reconstruire le morceau (outillage du remake de Sinnoh) est la voie lourde. Chaque point dit laquelle
il prend.

### Point 11 — Arènes d'Émeraude : dispositions et dresseurs

**Émeraude** (section 2, Serebii) : dispositions et équipes propres à chaque arène. Une partie des mécanismes existe déjà dans
ORAS, remake de Rubis et Saphir qui les partagent en partie avec Émeraude (de mémoire, à vérifier arène par arène).

1. **Inventaire** : pour chacune des huit arènes d'ORAS, sa zone, ses personnages, ses objets, son script et ses mécanismes
   (`oras-inspect zone`, `oras-script`) ; un tableau « mécanisme d'Émeraude → existe dans ORAS / non » (dalles de glace,
   interrupteurs, portes tournantes, téléporteurs, trous).
2. **Référence** : les cartes et dresseurs d'Émeraude dans `pokeemerald` (`data/maps/*Gym*`), pour placer les dresseurs et lire
   les énigmes.
3. **Première version, sans nouvelle géométrie** : les dresseurs et leurs équipes d'Émeraude (outil du point 1) placés aux
   positions d'Émeraude ramenées sur la salle d'ORAS ; les mécanismes qu'ORAS a déjà gardés ; les champions avec leurs
   équipes d'Émeraude (section 2) ; Juan au point 8.
4. **Deuxième version, la géométrie d'Émeraude** : seulement pour les arènes dont l'énigme n'existe pas dans ORAS, une salle
   reconstruite par l'outillage des pièces (`RoomBuilder`, murs et portes calculés) sur la grille d'Émeraude ; présentée au
   propriétaire avant d'être construite.
5. **Vérification** : chaque arène traversée en émulation sans écran (script de touches, captures), le champion affronté ;
   puis sur le téléphone.

**Dépend de** : points 1, 7 (dresseurs, drapeaux), 8 (Juan). **Risque** : une énigme sans équivalent dans ORAS qui demande du
code (par exemple un sol qui change d'état) : gardée dans sa version ORAS et signalée.

### Point 12 — Extension du Parc Safari

**Émeraude** (Giant Bomb) : une partie du Parc Safari ouvre après l'histoire, avec des Pokémon de Johto.

1. **Lire** : les zones du Parc Safari d'ORAS et ce qui en fait une zone Safari (le combat Safari et ses Balls : un mot de l'en-tête
   de zone ou un script ; à trouver en comparant une zone Safari et une zone normale).
2. **Construire** : deux zones nouvelles (`oras-sandbox`), reliées au Parc par un warp ajouté au bout d'une zone existante, avec
   l'en-tête Safari recopié.
3. **Rencontres** : les Pokémon de Johto de l'extension d'Émeraude (`pokeemerald`, rencontres des zones `SafariZone_North*`/
   `South*`), complétés par ceux que le point 4 classe « sans place ».
4. **Accès** : un personnage qui ferme le passage tant que le drapeau d'après-Ligue (point 7) n'est pas posé.
5. **Vérification** : entrer, rencontrer un Pokémon de Johto en combat Safari sans écran ; puis sur le téléphone.

**Dépend de** : brique « tables de rencontres propres », point 7. **Risque** : si l'état Safari ne vient pas de la zone,
l'extension se fait en ajoutant des cases aux zones Safari existantes (morceaux reconstruits) au lieu de zones nouvelles.

### Point 13 — Souterrain du désert

**Émeraude** (section 1.3) : une grotte ouverte après le Panthéon, le fossile non choisi et le seul Métamorph de Hoenn.

1. **Construire** : une zone de grotte nouvelle (`oras-sandbox`), l'entrée par un warp ajouté sur la route où Émeraude la place
   (Route 114 de mémoire, à vérifier dans `pokeemerald`).
2. **Contenu** : le fossile non choisi à la Tour Mirage (point 14), en objet ramassable une seule fois ; rencontres : Métamorph et
   ce qu'Émeraude y met.
3. **Accès** : warp actif seulement après le Panthéon (drapeau du point 7).
4. **Vérification** : sans écran, entrer, ramasser le fossile, rencontrer Métamorph ; puis sur le téléphone.

**Dépend de** : points 7, 14 ; briques warps et rencontres.

### Point 14 — Tour Mirage et ses deux fossiles

**Émeraude** (Serebii) : une tour dans le désert de la Route 111, qui apparaît après avoir parlé au Fou des ruines ; on y choisit
le fossile Racine ou Griffe, puis la tour s'enfonce.

1. **Lire** : comment ORAS donne les fossiles aujourd'hui (script et objet dans le désert : à trouver), pour le retirer ou le
   déplacer.
2. **Construire** : les étages de la tour en zones nouvelles (`oras-sandbox`), l'entrée par un warp ajouté au désert ; les deux
   fossiles posés au sommet.
3. **Script** : la tour visible (warp actif) après le dialogue du Fou des ruines (drapeau posé par son script) ; prendre un
   fossile pose le drapeau « tour enfoncée » : warp coupé, l'autre fossile retiré, et il passe au Souterrain (point 13).
4. **Décor** : la tour visible depuis le désert demande un modèle dans le morceau de carte du désert (voie lourde) ; première
   version : pas de tour visible, une entrée signalée (un personnage ou un panneau), présentée au propriétaire.
5. **Vérification** : sans écran, parler au Fou, entrer, prendre un fossile, vérifier la tour fermée ; puis sur le téléphone.

**Dépend de** : drapeaux (point 7), brique warps. **Risque** : les étages d'Émeraude se montent au Vélo Cross (sol qui
s'effrite) : si ce comportement de case n'existe pas dans ORAS, étages simples sans ce piège.

### Point 15 — Colline des Dresseurs, sans chronomètre

**Émeraude** (section 1.3, PokeTools) : des étages de dresseurs à battre d'affilée, un propriétaire au sommet qui récompense.

1. **Construire** : quatre étages et le toit en zones nouvelles (`oras-sandbox`, qui place déjà des dresseurs), l'entrée par un
   warp ajouté à un bâtiment existant (emplacement d'Émeraude à vérifier dans `pokeemerald`).
2. **Dresseurs** : par l'outil du point 1, sur des numéros libres (plafond, section commune).
3. **Récompense** : le propriétaire donne un objet une fois les étages battus (drapeaux par dresseur, lus comme au point 7) ; la
   boutique d'Émeraude (Potion Max, Guérison) par une boutique d'ORAS recopiée.
4. **Vérification** : sans écran, monter, battre un dresseur par étage, recevoir la récompense ; puis sur le téléphone.

**Dépend de** : point 1, plafond des dresseurs. Le chronomètre est le point 32.

### Point 16 — Grotte Tellurique et Grotte Marine (Groudon et Kyogre)

**Émeraude** (Wikipédia) : après l'histoire, l'Institut Météo indique où apparaît Groudon (Grotte Tellurique) ou Kyogre (Grotte
Marine), sur des routes qui changent. **À vérifier d'abord** : Kyogre se capture-t-il dans Rubis Oméga ? (point 4 : liste des
absents).

1. **Construire** : deux grottes en zones nouvelles (`oras-sandbox`) ; Kyogre au fond de la Grotte Marine, Groudon au fond de la
   Grotte Tellurique seulement si le point 4 le juge utile (Rubis Oméga le donne déjà dans l'histoire).
2. **Combat de légendaire** : le script d'une rencontre fixe d'ORAS recopié (Regirock ou un légendaire de l'Épisode Delta : à
   lire), l'espèce changée ; un drapeau « capturé ».
3. **Accès** : un warp ajouté sur une route ; version simple : une route fixe ; version d'Émeraude (route qui change) seulement
   si le script lit le jour (à vérifier au point 7) ; l'Institut Météo dit où (un texte ajouté).
4. **Vérification** : sans écran, entrer, lancer le combat, capturer ; puis sur le téléphone.

**Dépend de** : point 4, brique warps. **Risque** : la Grotte Marine d'Émeraude se trouve sous l'eau (Plongée) : une entrée de
plongée sur une case de mer est à lire dans une zone de Plongée d'ORAS.

### Point 17 — Îles des légendaires d'événement

**Section 1.3** : Mew, Deoxys, Lugia et Ho-Oh venaient de billets d'événement ; ils doivent s'obtenir en jeu. **À vérifier
d'abord** : lesquels ORAS donne déjà (Deoxys à l'Épisode Delta, Lugia ou Ho-Oh de mémoire) : seuls les absents (liste du
point 4) reçoivent une île.

1. **Accès** : deux voies, choisies avec le propriétaire : a) par le mode ciel, comme les îles Mirage : le module `DllSkyTrip`
   a une table de points de légendaires (`BMLegendPoint`, vue au point 39 : les espèces 380, 381, 386 à `0xDB28`), à lire puis
   étendre ; b) par un personnage au port de Nénucrique qui emmène sur l'île (warp de script, plus simple).
2. **Îles** : une zone nouvelle par île (`oras-sandbox`), la rencontre fixe recopiée comme au point 16.
3. **Condition** : après la Ligue (point 7), et pour Mew comme dans Émeraude, une petite poursuite dans l'herbe
   remplacée par une rencontre directe en première version.
4. **Vérification** : sans écran, atteindre l'île, capturer ; puis sur le téléphone.

**Dépend de** : points 4, 7, et 38-39 pour la voie a.

### Point 18 — Pilier Céleste à deux états

**Émeraude** (Bulbapedia) : intact à la première visite, abîmé ensuite (sols fissurés).

1. **Lire** : les zones du Pilier Céleste d'ORAS et le drapeau de l'histoire qui marque le passage de Rayquaza (point 7).
2. **Construire** : une copie des étages en zones nouvelles, avec la grille des cases changée (fissures, trous) ; mêmes modèles.
3. **Choix de l'état** : à l'entrée, deux warps, l'un vers les étages intacts, l'autre vers la copie abîmée, activés selon le
   drapeau (un script d'entrée de zone).
4. **Vérification** : sans écran, entrer avant et après le drapeau ; puis sur le téléphone.

**Dépend de** : point 7, points 23-24 pour le moment où l'état change. **Risque** : une fissure qui fait tomber à l'étage du
dessous est un comportement de case : à lire dans ORAS (la grotte où l'on tombe existe-t-elle ?) ; sinon, fissures en simples
murs.

### Point 19 — Routes maritimes enrichies d'îlots et de dresseurs

**Section 3.3** : trop d'eau vide.

1. **Îlots** : de petites îles ajoutées dans des cases de mer des routes existantes, en reconstruisant ces morceaux de carte
   (outillage du remake de Sinnoh, qui garde la zone ; limites connues : taille de morceau, 51 textures par modèle).
2. **Dresseurs** : des nageurs et des dresseurs sur les îlots, ajoutés aux zones par `NewCharacter`, sur des numéros libres
   (plafond) ; équipes par l'outil du point 1.
3. **Objets** : quelques objets cachés sur les îlots.
4. **Vérification** : sans écran, naviguer jusqu'à un îlot, combattre ; puis sur le téléphone.

**Limites** : le fichier des événements d'une zone reste sous 0xC84 octets (vérifié) : peu de personnages par zone.

### Point 20 — Bâtiments de la Zone de Combat sur l'île Combat

**Section 1.1** : sept installations ; première version avec les règles de la Maison de Combat d'ORAS.

1. **Lire** : la Maison de Combat d'ORAS (zone, script, les fonctions natives qui lancent une série, comptent les victoires et les
   PC) : c'est la base de chaque installation.
2. **Construire** : les salles d'accueil en zones nouvelles (`oras-sandbox`), une par installation, reliées par des warps
   ajoutés dans l'île Combat (voie sûre : portes au fond de bâtiments existants ou dans la Maison elle-même ; voie lourde :
   façades ajoutées au morceau de l'île, présentée au propriétaire).
3. **Règles** : chaque guichet lance une série de la Maison (simple, double, triple, rotation) ; le meneur (Frontier Brain) en
   dresseur au 21e et au 49e combat (numéros libres), avec son Symbole en objet clé ou en drapeau.
4. **PC et récompenses** : les PC de la Maison partagés ; les maîtres des capacités de la Zone au point 5.
5. **Grotte de l'Artisan** : une grotte en zone nouvelle avec Smeargle (point 4).
6. **Vérification** : sans écran, une série lancée et gagnée contre le meneur ; puis sur le téléphone.

**Dépend de** : point 1, plafond des dresseurs. Les règles propres de chaque installation sont le point 36.

### Point 21 — Tentes de Combat, bâtiments seulement

**Section 1.3** : trois tentes (Poivressel, Vergazon, Vermilava de mémoire, à vérifier) avant la Ligue.

1. **Construire** : une salle par tente (`oras-sandbox`), entrée par une porte au fond d'un bâtiment existant de la ville (voie
   sûre) ou par une façade ajoutée (voie lourde, avec le propriétaire).
2. **Règles** : une série courte de la Maison (3 combats), niveau 50 ; prix en objets au lieu de PC.
3. **Vérification** : sans écran, une série gagnée ; puis sur le téléphone.

**Dépend de** : point 20 (la série de la Maison lancée depuis une zone nouvelle).

### Palier 4 — méthode commune

L'histoire d'ORAS vit dans les scripts de zone (lisibles et réassemblables : `oras-script`, `amx-asm`) et dans des scènes. Une
hypothèse décide de tout le palier, à vérifier avant le reste : **la cartouche de Rubis Oméga contient-elle les cartes et les
scripts d'Alpha Saphir** (repaire Aqua, scènes de Kyogre), choisis par un test de version dans les scripts ? Si oui, Delta
Emerald se construit en forçant, scène par scène, la branche Magma ou Aqua qu'Émeraude suit ; sinon, les scènes manquantes se
recomposent avec celles d'Omega Ruby (personnages changés), beaucoup plus long. Le premier travail est donc de chercher dans
les scripts la fonction native de version et les noms de zones d'Alpha Saphir (`a/0/7/4` membre 90).

### Point 22 — Les deux repaires, Magma et Aqua

1. **Vérifier** : quels repaires existent dans la cartouche (noms de zones, zones et scripts) ; Rubis Oméga a le repaire Magma.
2. **Si le repaire Aqua est dans la cartouche** : rendre son entrée accessible (warp, personnage) et ses scripts actifs dans
   Rubis Oméga en forçant leur test de version.
3. **Sinon** : le repaire Aqua construit en zones nouvelles (`oras-sandbox`) sur le plan d'Émeraude (`pokeemerald`), avec les
   modèles du repaire Magma recolorés (`oras-asset recolour`) et les sbires Aqua (dresseurs existants de la classe Aqua s'ils
   sont dans la cartouche).
4. **Vérification** : sans écran, traverser les deux repaires et y combattre ; puis sur le téléphone.

**Dépend de** : la vérification du palier, points 7 (drapeaux) et 23 (à quel moment de l'histoire).

### Point 23 — Histoire à deux équipes, Magma et Aqua

**Émeraude** (section 2) : Magma et Aqua agissent tous les deux ; chacun a ses scènes.

1. **Carte de l'histoire** : chaque scène d'Émeraude (de `pokeemerald`, scripts des cartes) mise en face de la scène d'ORAS au
   même lieu, avec l'équipe qui y agit dans Émeraude ; validée par le propriétaire avant tout changement.
2. **Construire** : scène par scène, la branche de l'autre équipe forcée (si la cartouche l'a) ou la scène recomposée ; les
   drapeaux de progression gardés dans l'ordre d'ORAS pour ne pas casser la suite.
3. **Vérification** : chaque scène modifiée jouée sans écran depuis une sauvegarde juste avant ; à la fin, une partie complète
   sur le téléphone.

**Risque** : le plus gros point du plan ; il se fait en plusieurs livraisons, une ville à la fois.

### Point 24 — Groudon et Kyogre ensemble, Rayquaza appelé pendant l'histoire

**Émeraude** (section 2) : Groudon et Kyogre réveillés s'affrontent à Atalanopolis ; le joueur réveille Rayquaza au Pilier
Céleste pour les calmer. ORAS a déjà Rayquaza au Pilier Céleste dans l'Épisode Delta.

1. **Lire** : les scènes de l'éveil de Groudon (Rubis Oméga), celles de Kyogre (si la cartouche les a), et celles de Rayquaza
   au Pilier Céleste (Épisode Delta).
2. **Construire** : l'ordre d'Émeraude : réveil des deux (scènes existantes ou recomposées), Atalanopolis sous le climat
   extrême, montée au Pilier Céleste (scène de Rayquaza de l'Épisode Delta, déplacée dans l'histoire), retour à Atalanopolis ;
   l'Épisode Delta adapté pour qu'il ne rejoue pas la rencontre.
3. **Vérification** : la séquence jouée sans écran, sauvegarde par étape ; puis sur le téléphone.

**Dépend de** : points 18, 22, 23. **Risque** : les scènes (caméras, animations) sont peut-être des fichiers à part des scripts :
à lire avant de promettre un déplacement.

### Palier 5 — méthode commune

Comme pour le point 38 : 1) l'action déclenchée dans le jeu, la trace du code comparée à une trace de repos (interpréteur,
`trace on/off`) pour isoler les fonctions en cause ; 2) décompilation et lecture ; 3) le plus petit patch possible, présenté avant
d'être écrit, avec ses contrôles (refus si le code n'est pas celui attendu) ; 4) vérification sans écran puis sur le
téléphone. Deux points du palier se règlent peut-être sans code ; ils sont signalés.

### Point 25 — Combats doubles « libres »

1. **Vérifier d'abord** : deux dresseurs qui voient le joueur en même temps le combattent-ils ensemble dans ORAS ? (sans écran,
   deux dresseurs placés côte à côte dans un bac à sable ; la sixième génération le fait de mémoire, à vérifier).
2. **Si oui** : seulement des données : des paires de dresseurs placées comme dans Émeraude.
3. **Sinon** : lire le déclenchement par la vue (`_TrainerEyeBattleStart`, déjà nommé) et le combat à deux dresseurs
   (`Trainer_BuildTeams`, qui construit plusieurs dresseurs : lu, pas encore vu en marche) ; patch du déclenchement.

### Point 26 — Multi Exp. désactivé au départ

ORAS laisse allumer et éteindre le Multi Exp. depuis les objets rares (de mémoire, à vérifier). **Sans code probablement** : son
état est un octet ou un drapeau de la sauvegarde (à trouver en comparant deux sauvegardes, allumé et éteint) ; le script qui le
donne le pose éteint. **Vérification** : recevoir l'objet sans écran, lire l'état ; puis sur le téléphone.

### Point 27 — Mode difficile optionnel

1. **Interrupteur** : un objet clé ou un personnage qui pose un drapeau « difficile » (choix du joueur, réversible).
2. **Effet** : un patch dans la construction d'une équipe de dresseur (`Trainer_BuildOneTeam` 0x454EA4, déjà lue) : si le
   drapeau est posé, niveaux relevés (pourcentage à régler) et meilleure IA (l'octet d'IA de la fiche, à lire au point 1).
3. **Vérification** : un même dresseur combattu sans écran, mode normal puis difficile (captures des niveaux) ; puis sur le
   téléphone.

### Point 28 — Méga-Évolutions limitées à l'après-jeu

**Probablement sans code** : la Méga-Gourmette (objet clé qui permet la Méga) est donnée par un script de l'histoire ; ce don
est déplacé après la Ligue, et les Méga-Gemmes restent trouvables. **À vérifier** : aucune scène obligatoire ne demande au
joueur de méga-évoluer avant la Ligue. **Vérification** : sans écran, un combat avant la Ligue sans option Méga, après avec.

### Point 29 — Accès fixe aux îles Mirage

1. **Lire** : le choix de l'île du jour (module `DllSkyTrip`, classe `BMHikyo` vue au point 39 ; la sauvegarde a un bloc
   `RandomGroup`, bon candidat pour la graine du jour).
2. **Patch** : le tirage remplacé par un calendrier fixe (une île selon le jour de la semaine) ou par un objet qui choisit
   l'île ; choix du propriétaire.
3. **Vérification** : sans écran, l'île visible le jour voulu (heure du système de l'émulateur réglée) ; puis sur le téléphone.

### Point 30 — Se passer des CS

1. **Lire** : la vérification « un Pokémon de l'équipe connaît la capacité et le badge est obtenu », sur un obstacle (arbre à
   couper, rocher) et au menu de l'équipe (déjà vu au point 38 pour Vol).
2. **Patch** : avec le badge, la vérification réussit sans Pokémon qui connaît la capacité (le premier de l'équipe sert pour
   l'animation) ; Surf, Plongée et Cascade compris.
3. **Vérification** : sans écran, un obstacle franchi avec une équipe sans la capacité ; puis sur le téléphone.

### Point 31 — Revanches des dresseurs de route

1. **Vérifier d'abord** : ORAS a-t-il un moyen de réaffronter les dresseurs de route (une fonction du PokéMultinav, de mémoire
   pas sûr) ?
2. **Version sans téléphone** : les drapeaux « battu » d'une liste de dresseurs remis à zéro chaque jour (si le script lit le
   jour : point 7) ou après la Ligue, équipes renforcées sur des numéros libres.
3. **Version Match Call** : un appel du PokéMultinav, code à trouver (la plus longue, après la version simple).

### Point 32 — Colline des Dresseurs : son chronomètre

1. **Chercher** : une fonction native de script qui lit l'heure ou un compteur d'images (parmi les 799 natives nommées).
2. **Si elle existe** : le chronomètre en script (heure d'entrée gardée, temps calculé au sommet) et un affichage par message.
3. **Sinon** : patch de code (un compteur dans la mise à jour de la zone), après le point 15.

### Point 33 — Boutique de vêtements de X/Y

1. **Vérifier** : la sauvegarde d'ORAS a un bloc `Fashion` (vérifié, nom de classe) ; reste à savoir s'il est encore lu, et si
   les modules (liste des 145 modules) et les modèles de vêtements de X/Y sont dans la cartouche.
2. **Si oui** : un personnage boutique qui ouvre le module, et le joueur habillé selon le bloc.
3. **Sinon** : des tenues prédéfinies (textures du joueur recolorées par `oras-asset`) choisies par un personnage.

### Point 34 — Concours plus fins

1. **Récompenses** (données et scripts, sans code) : des objets utiles donnés par les scripts des salles de concours.
2. **Jugement** (code) : le module des concours lu par trace (un concours joué sans écran), puis les barèmes ajustés.

### Point 35 — Bases Secrètes : partage par fichier, bases hors ligne

1. **Vérifier d'abord** : ORAS partage les bases par QR code (de mémoire, à vérifier) ; Azahar peut donner une image à la caméra
   émulée : partager une base serait alors déjà possible avec un fichier image, sans rien changer au jeu.
2. **Bases hors ligne** : des bases de dresseurs ajoutées aux données que le jeu lit pour les bases d'autres joueurs (format à
   lire, bloc `SecretBase` de la sauvegarde).
3. **Plus de meubles et d'emplacements** : tables des meubles et des emplacements, données à lire.

### Palier 6 — méthode commune

Deux gros chantiers qui s'appuient sur tout le reste. Ils se découpent en livraisons qui marchent chacune seule.

### Point 36 — Règles propres des installations de la Zone de Combat

Dans l'ordre du plus faisable au moins faisable :
1. **Pyramide et Battle Pike** (maps et scripts, pas de règle de combat) : salles en zones nouvelles tirées au hasard par script,
   soins et pièges en script, la visibilité réduite de la Pyramide par la lumière de zone (réglable dans l'outillage).
2. **Usine** (Pokémon de location) : l'équipe du joueur mise de côté et remplacée par des Pokémon prêtés ; à lire : les fonctions
   natives qui manipulent l'équipe (la Maison de Combat et la pension en ont, à vérifier).
3. **Arène, Palais, Dôme** (règles de combat) : patches du module de combat (jugement après trois tours, choix des capacités
   selon la nature, tableau de tournoi) : lus par trace d'un combat, les plus longs.

### Point 37 — Pokémon qui suit le joueur

1. **Modèle** : le premier Pokémon de l'équipe en modèle de terrain, fait comme Giratina au point 39 (modèle de combat, shader
   de terrain, échelle, textures réduites) : la méthode est vérifiée pour un Pokémon ; reste à la faire pour tous, avec la
   mémoire qu'ils prennent.
2. **Personnage** : un personnage de zone qui suit les pas du joueur (le code de déplacement des personnages, à lire par trace ;
   le nombre de personnages est déjà relevé).
3. **Animations** : marche et attente tirées des animations de combat (comme au point 39).
4. **Livraisons** : d'abord un seul Pokémon fixe qui suit, puis le premier de l'équipe, puis tous.

### Points 38 et 39 — Le vol en mode ciel à la place du Vol, sur le Pokémon qui connaît Vol

**Demande du propriétaire (10 octobre)**, commencée en premier.

**Vérifié dans le jeu (lecture seule)** :
- le mode ciel est le module `DllSkyTrip.cro` (73 Ko) ; ses classes : `BuildModel` (la maquette), `BMLegendPoint` (points des
  légendaires), `BMHikyo` (îles Mirage), `BMShip` (bateau), `BMEncount` (rencontres), `Camera` ; il nomme un os `Waist`, où le
  joueur est sans doute attaché au modèle de Latios ou Latias (une supposition) ;
- la maquette de Hoenn : `a/2/5/1` membre 3, un modèle `sky_trip_map` (36 maillages, 54 861 sommets, 50 749 triangles,
  15 textures), séparé des vraies cartes ; les décors (bateau, îles Mirage, Goélise) au membre 0.

**Vérifié dans le code puis dans le jeu, sans écran (10 octobre)** — sur une copie de la sauvegarde du propriétaire, hors du
dépôt, avec la Flûte Éon (objet 775, poche Objets rares), un Pokémon de l'équipe qui connaît Vol et les badges 1 à 6 ajoutés
(sommes de contrôle refaites) :
- la Flûte : la fonction de retour du Sac `0x3ef000` voit l'objet `0x307`, appelle le constructeur du mode ciel `0x3d19f8`
  (événement de table virtuelle `0x5df0bc`, espèce `0x17D` Latios écrite en dur) et marque l'événement en cours (`+0x1d = 3`) ;
  vu dans le jeu : Latios au-dessus de la maquette, « Voulez-vous atterrir ici ? » ;
- le Vol : la fonction de retour du menu de l'équipe `0x3ef7b8`, quand le menu rend l'action 3 avec le code `0x13`, vérifie
  `0x3dc534(…, 2)` (sans doute « peut-on voler ici », supposé) puis crée l'événement du Vol (0xC0 octets, table virtuelle
  `0x5dee48`) avec la destination choisie et le Pokémon de l'équipe qui vole, et marque `+0x1d = 3` comme la Flûte. Trouvé par
  une trace du code (interpréteur) du choix de la destination jusqu'à l'atterrissage, moins la trace de la carte au repos ;
  vu dans le jeu : l'envol avec la silhouette du Pokémon, puis l'arrivée à Bourg-en-Vol ;
- la sauvegarde : les badges sont l'octet `0x420C` (bloc 11, « Misc », +0xC ; un bit par badge ; sans le 6e : « Vous devez
  obtenir un autre Badge ») ; l'équipe au bloc 18 (`0x14200`, 6 × 260 octets, le nombre à +0x618), chiffrée comme le
  documente PKHeX (vérifié : déchiffrer puis rechiffrer redonne les mêmes octets).

**Pas encore trouvé** : la table des endroits où se poser ; le chargement du modèle de Latios ou Latias et l'attache du
joueur.

**Point 38 — le Vol ouvre le mode ciel** (palier 5) : trouver où l'utilisation de Vol hors combat ouvre la carte et y lancer le
mode ciel comme le fait la Flûte Éon ; le mode ciel pose déjà le joueur aux mêmes destinations. Proposé, pas encore fait :
dans `0x3ef7b8`, la branche du code `0x13` appelle `0x3d19f8` comme la Flûte au lieu de créer l'événement du Vol (la
vérification `0x3dc534` gardée) ; puis, deuxième étape, le menu de l'équipe (`DllPokeList`) lance le vol sans ouvrir la carte.
Inconnu : si le mode ciel accepte le contexte du menu de l'équipe au lieu de celui du Sac, et comment le patch de code est
installé dans la chaîne des mods.
**Fait et vu sans écran (11 octobre)** : `tools/remake/prototype/fly_soaring_ips.py` écrit le `code.ips` (7 instructions à
`0x3ef858`, ce que fait la Flûte : `0x3d19f8`, puis `+0x1d = 3`) ; Azahar l'applique depuis
`load/mods/000400000011C400/code.ips` (journal : « code.ips patching code.bin »). Vol dans le menu de l'équipe → la carte →
l'envol du mode ciel → le mode ciel (sur Giratina avec le mod du point 39). Le mode ciel accepte donc le contexte du menu de
l'équipe. Reste : la carte s'ouvre encore avant (sa destination n'est pas utilisée ; la retirer demande `DllPokeList`) ;
non testé sur le téléphone.

**Point 39 — voler sur le Pokémon qui connaît Vol, avec son modèle** (palier 6 pour tous ; par étapes) :
1. le code charge le Pokémon de l'équipe qui connaît Vol au lieu de Latios ou Latias (patch plus gros que le 38) ;
2. un point d'attache du joueur par Pokémon (les os diffèrent : `Waist` est celui de Latios), dans une table à créer (position,
   échelle) ;
3. une taille pendant le vol (Roucool est minuscule à côté de Latios) et une animation de vol existante du modèle.
Première version : une liste de Pokémon choisie par le propriétaire, réglés à la main ; Latios ou Latias pour les autres.

**Vérifié pour le point 39 (11 octobre)** :
- le modèle de la monture ne dépend pas de l'espèce : le constructeur `0x3d19f8` écrit l'espèce (`0x17D`), la mise à jour
  `0x3e8fe0` n'en tire qu'un octet (`0x17C` → 0, sinon 1) passé au module `DllSkyTrip` ; le module ouvre quatre archives
  (identifiants `0xFB`-`0xFE`, soit `a/2/5/1`, `a/2/5/2`, `a/2/5/4`, `a/2/5/3`) et charge de `a/2/5/4` les membres
  {2 ou 3, l'octet} : membre 0 Latias, 1 Latios (conteneurs TM de 18 fichiers : `pm0380a_00_fi` / `pm0381a_00_fi`, le modèle et
  15 animations de vol), 2 et 3 les joueurs (16 fichiers : le modèle `rstr000x_00_fi` et 15 animations `fi52_pm*`) ; vu dans le
  jeu : l'espèce changée en mémoire en Giratina (487) laisse Latios ;
- le joueur est placé à l'os `Waist` de la monture (fonction `0x107303bc` du module) ; Giratina Originel (`a/0/0/8` membres
  5435-5441 : modèle, textures, animations de combat et de Poké Récré) a aussi un os `Waist`, mais aucune animation de vol ;
- plusieurs montures sont possibles (supposé, non construit) : de nouveaux membres dans `a/2/5/4`, une table espèce → membre
  à la place du test `0x17C`, l'octet utilisé tel quel comme numéro de membre dans `DllSkyTrip` ; deux autres lectures de cet
  octet dans le module restent à adapter.

**Giratina monture dans le jeu, vu sans écran (11 octobre)** : mod de données seul, sans patch de code ; `a/2/5/4` membre 1
(la monture « Latios ») remplacé par un conteneur TM fait ainsi, avec `tools/remake/spica` et `remake_tool oras-copy
… a/2/5/4 1=file:<TM>` :
- fichier 17 (le modèle) : `spica bch` du modèle de Giratina Originel (`a/0/0/8` 5435, fichier 0) et de ses textures (5437,
  fichier 0), `--scale 0.2 --rename pm0381a_00_fi --texmax 64 --material-from` le modèle de Latios ;
- fichiers 0-16 (sauf 5, 11 et 14, qui ne sont pas des animations de squelette, gardés) : `spica anim` sur chaque animation de
  Latios avec la pose de combat 0 de Giratina (`spica pose`), `Waist` à y = -15 (le dos au niveau où le jeu assied le joueur) ;
  0 et 1 (les attentes) en entier, les autres sur une seule image ;
- vu dans le jeu (émulateur sans écran, Flûte Éon) : Giratina vole en mode ciel, à la bonne taille, le joueur assis sur lui ;
- trois causes d'échec trouvées en route : le shader de combat (`PokePack`) n'est pas chargé en mode ciel, le jeu plante dans
  `Gfx_BindStandardUniforms` (→ `--material-from`, shader `FieldChar`) ; l'échelle d'un os racine est ignorée (→ échelle mise
  dans la géométrie) ; 5,7 Mo d'animations empêchent le chargement (écran noir, jeu non figé) (→ 1 Mo : courbes constantes
  écrites une fois, poses fixes) ;
- mouvements de vol (même jour) : le vol généré de la visionneuse capturé sur un cycle d'onde (`captureFlight` dans
  `ride_viewer.html`) puis écrit par `spica anim` : croisière → `pmwalk01`, virages → `pmturnL01`/`pmturnR01` (vitesse de
  rotation ±1,1 rad/s), accélération → `speedup01` (2 × la vitesse max), attente → `pmwait01` (combat 0) ; les autres positions
  sur une pose fixe ; conteneur de 1,08 Mo, chargé ; vu dans le jeu : la pose change dans les virages ;
- pas encore : le boost (bosse, plongeon) n'est pas une animation du jeu ; le joueur garde les animations de Latios ; non testé
  sur le téléphone.

 (`tools/remake/render/build_ride_viewer.py`, sans les
données du jeu)** :
- attente : son animation de combat 0 telle quelle ; vol : une onde verticale générée qui part de la tête et descend jusqu'à
  la queue sur toute la colonne remise en ligne (la tête dans l'axe du corps), amplitude menée par la tête, le torse à 120 %,
  la queue en fouet ; tentacules en chaînes souples qui traînent vers l'arrière ; transition attente ↔ vol par mélange ;
- virages en anguille : le corps suit la trace de la tête, petit roulis seulement ; la vitesse change l'onde (plus rapide,
  plus plate) et plaque les tentacules ;
- boost : la tête monte (bosse courbée vers le haut), plonge, la roquette part 12,25 % de la figure après le pic, sans onde
  pendant la montée et la descente, figure mesurée en longueurs de corps pour que la tête mène ;
- le joueur : collé au dos (os de la colonne le plus proche, contact avec la surface gardé à chaque image), ses animations
  du jeu selon la situation (attente, virage, freinage, accélération pendant tout le boost), bassin fixé ;
- rien de cela n'est encore dans le jeu : il faudra jouer ces mouvements dans `DllSkyTrip` (ou en faire de vraies animations
  de vol) et y remplacer le modèle (point 39, étapes 1-3).

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
