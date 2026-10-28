# What X/Y's zone scripts can still run without the story

Made by `prototype/xy_zone_cases.py` (its header says how). X/Y zone z is the whole-Kalos build's zone 538 + z.

Counts: character dialogue: 762, character gift/service: 100, character scene: 86, character state: 19, step dialogue: 92, step scene: 20, step state: 5

| X/Y zone | Mod zone | Place | What | Character | Case | Category | Natives that sort it |
|---|---|---|---|---|---|---|---|
| 2 | 540 | Bourg Croquis | character | 6 | 11 | scene | FadeRequestOut |
| 8 | 546 | Quarellis | character | 7 | 15 | gift/service | ItemAdd |
| 9 | 547 | Quarellis | character | 2 | 1 | scene | FadeRequestOut |
| 12 | 550 | Fort-Vanitas | character | 1 | 6 | gift/service | ItemAdd |
| 12 | 550 | Fort-Vanitas | character | 5 | 5 | gift/service | ItemAdd |
| 17 | 555 | Fort-Vanitas | character | 1 | 2 | gift/service | ItemAdd |
| 18 | 556 | Fort-Vanitas | character | 0 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo, ItemAdd |
| 19 | 557 | Fort-Vanitas | character | 0 | 4 | gift/service | ItemAdd |
| 19 | 557 | Fort-Vanitas | character | 2 | 1 | state | FlagSet, WorkSet |
| 20 | 558 | Fort-Vanitas | character | 0 | 2 | gift/service | ItemAdd |
| 20 | 558 | Fort-Vanitas | character | 1 | 1 | scene | CallPokeSelect |
| 21 | 559 | Cromlac’h | step |  | 27 | scene | BuildMdlEventAnimeSetEnd, BuildMdlGetHandleByGPos |
| 24 | 562 | Cromlac’h | character | 0 | 3 | gift/service | ItemAdd |
| 26 | 564 | Cromlac’h | character | 0 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo, ItemAdd |
| 27 | 565 | Cromlac’h | character | 0 | 2 | gift/service | ItemAdd |
| 29 | 567 | La Frescale | character | 6 | 10 | gift/service | ItemAdd, PlayerSubMoney |
| 33 | 571 | La Frescale | character | 0 | 2 | gift/service | ItemAdd |
| 35 | 573 | La Frescale | character | 1 | 1 | gift/service | ItemAdd |
| 36 | 574 | La Frescale | character | 0 | 2 | gift/service | ItemAdd |
| 36 | 574 | La Frescale | character | 1 | 1 | gift/service | ItemAdd |
| 37 | 575 | La Frescale | character | 0 | 1 | gift/service | ItemAdd |
| 38 | 576 | Mozheim | character | 5 | 8 | gift/service | ItemAdd |
| 38 | 576 | Mozheim | character | 7 | 7 | gift/service | ItemAdd, PlayerSubMoney |
| 41 | 579 | Mozheim | character | 1 | 2 | gift/service | ItemAdd |
| 42 | 580 | Mozheim | character | 0 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo, ItemAdd |
| 43 | 581 | Mozheim | character | 0 | 3 | scene | FadeRequestOut |
| 44 | 582 | Mozheim | character | 3 | 4 | gift/service | ItemAdd |
| 45 | 583 | Roche-sur-Gliffe | character | 1 | 8 | gift/service | ItemAdd, ItemSub |
| 45 | 583 | Roche-sur-Gliffe | character | 7 | 3 | gift/service | ItemAdd |
| 46 | 584 | Porte de R. s/Gliffe | character | 2 | 3 | gift/service | ItemAdd |
| 48 | 586 | Aquarium R. s/Gliffe | character | 2 | 6 | gift/service | ItemAdd |
| 49 | 587 | Roche-sur-Gliffe | character | 2 | 6 | gift/service | ItemAdd |
| 49 | 587 | Roche-sur-Gliffe | character | 4 | 1 | gift/service | ItemAdd |
| 50 | 588 | Roche-sur-Gliffe | character | 1 | 1 | gift/service | ItemAdd |
| 51 | 589 | Roche-sur-Gliffe | character | 3 | 5 | gift/service | ItemAdd |
| 52 | 590 | Roche-sur-Gliffe | character | 0 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo, ItemAdd |
| 53 | 591 | Neuvartault | character | 4 | 1 | scene | CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, _CallTrainerBattleCore |
| 55 | 593 | Neuvartault | character | 0 | 4 | scene | EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam |
| 56 | 594 | Neuvartault | character | 0 | 2 | scene | BadgeSetFlag, BuildMdlEventAnimeIsEnd, BuildMdlEventAnimeStart, BuildMdlGetHandleByGPos, CallFieldCaptureDemo, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, GymSetVictoryInfo, MdlDel, MdlSetGPos, _CallTrainerBattleCore |
| 60 | 598 | Neuvartault | character | 0 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo |
| 61 | 599 | Neuvartault | character | 0 | 2 | gift/service | ItemAdd |
| 62 | 600 | Neuvartault | character | 3 | 2 | gift/service | ItemAdd |
| 66 | 604 | Illumis | character | 12 | 1 | state | FlagSet |
| 79 | 617 | Tour Prismatique | character | 0 | 3 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 79 | 617 | Tour Prismatique | character | 1 | 2 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 79 | 617 | Tour Prismatique | character | 2 | 4 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 80 | 618 | Tour Prismatique | character | 0 | 3 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 80 | 618 | Tour Prismatique | character | 1 | 2 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 80 | 618 | Tour Prismatique | character | 2 | 4 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 81 | 619 | Tour Prismatique | character | 0 | 3 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 81 | 619 | Tour Prismatique | character | 1 | 2 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 81 | 619 | Tour Prismatique | character | 2 | 4 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 82 | 620 | Tour Prismatique | character | 0 | 3 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 82 | 620 | Tour Prismatique | character | 1 | 2 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 82 | 620 | Tour Prismatique | character | 2 | 4 | scene | CallFieldCaptureDemoSimplePoke, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _CallTrainerBattleCore, _MapChangeCore |
| 84 | 622 | Illumis | character | 0 | 1 | gift/service | ItemAdd |
| 84 | 622 | Illumis | character | 3 | 4 | gift/service | ItemAdd |
| 85 | 623 | Illumis | character | 0 | 8 | gift/service | ItemAdd |
| 86 | 624 | Illumis | character | 4 | 1 | state | WorkSet |
| 87 | 625 | Illumis | character | 2 | 3 | state | FlagSet |
| 91 | 629 | Illumis | character | 2 | 2 | gift/service | ItemAdd |
| 92 | 630 | Illumis | character | 3 | 2 | gift/service | ItemAdd |
| 92 | 630 | Illumis | character | 4 | 3 | gift/service | ItemAdd |
| 100 | 638 | Labos Lysandre | character | 1 | 3 | gift/service | ItemAdd |
| 104 | 642 | Illumis | character | 3 | 1 | gift/service | PlayerSubMoney |
| 104 | 642 | Illumis | character | 4 | 2 | state | WorkSet |
| 104 | 642 | Illumis | character | 9 | 21 | gift/service | ItemAdd |
| 107 | 645 | Illumis | character | 0 | 9 | gift/service | ItemAdd |
| 107 | 645 | Illumis | character | 3 | 4 | state | WorkSet |
| 107 | 645 | Illumis | character | 5 | 3 | scene | FadeRequestOut, LayoutFadeIsEnd, LayoutFadeRequestIn, LayoutFadeRequestOut |
| 108 | 646 | Illumis | character | 0 | 1 | scene | FadeRequestOut, MdlSetGPos |
| 110 | 648 | Illumis | character | 2 | 4 | scene | EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, MdlAcmdMakeMoveEx, MdlSetVisible |
| 111 | 649 | Illumis | character | 0 | 2 | gift/service | ItemAdd |
| 113 | 651 | Illumis | character | 1 | 1 | gift/service | ItemAdd |
| 116 | 654 | Illumis | character | 2 | 3 | gift/service | ItemAdd |
| 116 | 654 | Illumis | character | 5 | 4 | gift/service | ItemAdd |
| 117 | 655 | Illumis | character | 3 | 1 | scene | CallFieldCaptureDemoSimplePoke, CallPokeSelect, LayoutFadeIsEnd, LayoutFadeRequestIn, LayoutFadeRequestOut |
| 117 | 655 | Illumis | character | 4 | 2 | scene | CallPokeSelect |
| 118 | 656 | Illumis | character | 0 | 1 | scene | FadeRequestOut |
| 120 | 658 | Illumis | character | 0 | 1 | state | FlagSet, WorkSet |
| 123 | 661 | Illumis | character | 3 | 1 | state | WorkSet |
| 124 | 662 | Illumis | character | 1 | 1 | scene | BuildMdlGetHandleByGPos, BuildMdlSetVisible, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, MdlSetGPos, MdlSetPos |
| 126 | 664 | Illumis | character | 9 | 2 | scene | GlobalCall |
| 128 | 666 | Illumis | character | 3 | 1 | gift/service | ItemAdd, PlayerSubMoney |
| 129 | 667 | Illumis | character | 2 | 7 | state | WorkSet |
| 129 | 667 | Illumis | character | 3 | 8 | scene | GlobalCall |
| 129 | 667 | Illumis | character | 8 | 4 | state | WorkSet |
| 131 | 669 | Illumis | character | 2 | 1 | gift/service | ItemAdd |
| 133 | 671 | Illumis | character | 0 | 8 | scene | GlobalCall |
| 133 | 671 | Illumis | character | 1 | 2 | scene | EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, FadeRequestOut, GlobalCall, MdlAdd, MdlDel, MdlSetGPos, MdlSetVisible |
| 133 | 671 | Illumis | character | 2 | 1 | scene | GlobalCall |
| 134 | 672 | Illumis | character | 3 | 2 | scene | _MapChangeRailCore |
| 135 | 673 | Illumis | character | 2 | 4 | scene | EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, LayoutFadeIsEnd, LayoutFadeRequestIn, LayoutFadeRequestOut |
| 135 | 673 | Illumis | character | 13 | 7 | scene | EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam |
| 140 | 678 | Illumis | character | 4 | 15 | scene | FadeRequestOut, MdlSetGPos |
| 140 | 678 | Illumis | character | 10 | 14 | scene | FadeRequestOut |
| 140 | 678 | Illumis | character | 16 | 2 | scene | GlobalCall |
| 141 | 679 | Illumis | character | 8 | 2 | scene | GlobalCall |
| 142 | 680 | Illumis | character | 0 | 5 | gift/service | ItemAdd |
| 142 | 680 | Illumis | character | 1 | 1 | state | WorkSet |
| 143 | 681 | Illumis | character | 5 | 1 | state | WorkSet |
| 146 | 684 | Illumis | character | 19 | 2 | scene | GlobalCall |
| 150 | 688 | Illumis | character | 5 | 2 | state | WorkSet |
| 150 | 688 | Illumis | character | 6 | 3 | state | FlagSet |
| 151 | 689 | Illumis | character | 7 | 5 | scene | FadeRequestOut |
| 157 | 695 | Relifac-le-Haut | character | 16 | 2 | gift/service | ItemAdd, PlayerSubMoney |
| 159 | 697 | Relifac-le-Haut | character | 11 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo |
| 160 | 698 | Relifac-le-Haut | character | 1 | 1 | scene | BadgeSetFlag, CallFieldCaptureDemo, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, GymSetVictoryInfo, MdlSetGPos, _CallTrainerBattleCore |
| 161 | 699 | Relifac-le-Haut | character | 3 | 5 | gift/service | ItemAdd |
| 161 | 699 | Relifac-le-Haut | character | 4 | 6 | gift/service | ItemAdd |
| 162 | 700 | Relifac-le-Haut | character | 0 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo, ItemAdd |
| 162 | 700 | Relifac-le-Haut | character | 2 | 3 | gift/service | ItemAdd |
| 163 | 701 | Relifac-le-Haut | character | 4 | 9 | scene | CallFieldCaptureDemoSimplePoke |
| 163 | 701 | Relifac-le-Haut | character | 5 | 10 | scene | CallFieldCaptureDemoSimplePoke |
| 163 | 701 | Relifac-le-Haut | character | 6 | 11 | scene | CallFieldCaptureDemoSimplePoke |
| 163 | 701 | Relifac-le-Haut | character | 7 | 8 | scene | FadeRequestOut, MdlSetGPos, _MapChangeCore |
| 164 | 702 | Relifac-le-Haut | character | 2 | 4 | scene | CallFieldCaptureDemoSimplePoke |
| 164 | 702 | Relifac-le-Haut | character | 3 | 3 | scene | FadeRequestOut, MdlSetGPos, _MapChangeCore |
| 167 | 705 | Relifac-le-Haut | character | 2 | 1 | scene | FadeRequestOut |
| 168 | 706 | Relifac-le-Haut | character | 1 | 1 | gift/service | ItemAdd |
| 170 | 708 | Relifac-le-Haut | character | 1 | 1 | gift/service | ItemAdd |
| 171 | 709 | Relifac-le-Haut | character | 1 | 2 | gift/service | ItemAdd |
| 172 | 710 | Yantreizh | character | 11 | 17 | gift/service | ItemAdd, ItemSub |
| 176 | 714 | Yantreizh | character | 1 | 1 | scene | FadeRequestOut |
| 176 | 714 | Yantreizh | character | 2 | 2 | scene | CallPokeSelect |
| 177 | 715 | Yantreizh | character | 3 | 4 | gift/service | ItemAdd |
| 178 | 716 | Yantreizh | character | 0 | 3 | gift/service | ItemAdd |
| 179 | 717 | Yantreizh | character | 0 | 1 | gift/service | ItemAdd, ItemSub |
| 181 | 719 | Tour Maîtrise | character | 6 | 9 | state | WorkSet |
| 185 | 723 | Tour Maîtrise | character | 0 | 1 | gift/service | ItemAdd |
| 187 | 725 | Port Tempères | character | 1 | 2 | gift/service | ItemAdd |
| 187 | 725 | Port Tempères | character | 3 | 12 | gift/service | ItemAdd |
| 189 | 727 | Porte de P. Tempères | character | 1 | 2 | gift/service | ItemAdd |
| 191 | 729 | Port Tempères | character | 1 | 46 | scene | BadgeSetFlag, CallFieldCaptureDemo, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, FadeRequestOut, GymSetVictoryInfo, MdlSetGPos, _CallTrainerBattleCore |
| 192 | 730 | Port Tempères | character | 3 | 1 | scene | FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _MapChangeCore |
| 193 | 731 | Port Tempères | character | 0 | 3 | gift/service | ItemAdd |
| 193 | 731 | Port Tempères | character | 4 | 1 | scene | FadeRequestOut, MdlAcmdMakeMoveEx, ReserveScript, _MapChangeCore |
| 194 | 732 | Port Tempères | character | 1 | 5 | gift/service | ItemAdd |
| 195 | 733 | Port Tempères | character | 0 | 3 | scene | FadeRequestOut |
| 195 | 733 | Port Tempères | character | 1 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo, ItemAdd |
| 196 | 734 | Port Tempères | character | 0 | 2 | gift/service | ItemAdd |
| 199 | 737 | Port Tempères | character | 2 | 2 | scene | GlobalCall |
| 199 | 737 | Port Tempères | character | 3 | 4 | gift/service | ItemAdd |
| 200 | 738 | Romant-sous-Bois | character | 0 | 12 | gift/service | ItemAdd |
| 200 | 738 | Romant-sous-Bois | character | 1 | 5 | gift/service | ItemAdd |
| 200 | 738 | Romant-sous-Bois | character | 5 | 13 | gift/service | ItemAdd |
| 200 | 738 | Romant-sous-Bois | character | 9 | 10 | gift/service | ItemAdd |
| 203 | 741 | Romant-sous-Bois | character | 1 | 27 | scene | BadgeSetFlag, CallFieldCaptureDemo, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, FadeRequestOut, GymSetVictoryInfo, MdlSetGPos, _CallTrainerBattleCore |
| 204 | 742 | Romant-sous-Bois | character | 4 | 9 | scene | CallFieldCaptureDemoSimplePoke |
| 204 | 742 | Romant-sous-Bois | character | 5 | 10 | scene | CallFieldCaptureDemoSimplePoke |
| 204 | 742 | Romant-sous-Bois | character | 6 | 11 | scene | CallFieldCaptureDemoSimplePoke |
| 204 | 742 | Romant-sous-Bois | character | 7 | 8 | scene | FadeRequestOut, MdlSetGPos, _MapChangeCore |
| 205 | 743 | Romant-sous-Bois | character | 0 | 4 | scene | CallFieldCaptureDemoSimplePoke |
| 205 | 743 | Romant-sous-Bois | character | 1 | 3 | scene | FadeRequestOut, MdlSetGPos, _MapChangeCore |
| 209 | 747 | Romant-sous-Bois | character | 1 | 5 | state | FlagSet |
| 214 | 752 | Flusselles | character | 2 | 3 | gift/service | ItemAdd |
| 215 | 753 | Flusselles | character | 6 | 1 | scene | BadgeSetFlag, CallFieldCaptureDemo, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, FadeRequestOut, GymSetVictoryInfo, _CallTrainerBattleCore |
| 216 | 754 | Flusselles | character | 4 | 9 | scene | CallFieldCaptureDemoSimplePoke |
| 216 | 754 | Flusselles | character | 5 | 10 | scene | CallFieldCaptureDemoSimplePoke |
| 216 | 754 | Flusselles | character | 6 | 11 | scene | CallFieldCaptureDemoSimplePoke |
| 216 | 754 | Flusselles | character | 7 | 8 | scene | FadeRequestOut, MdlSetGPos, _MapChangeCore |
| 217 | 755 | Flusselles | character | 3 | 4 | scene | CallFieldCaptureDemoSimplePoke |
| 217 | 755 | Flusselles | character | 4 | 3 | scene | FadeRequestOut, MdlSetGPos, _MapChangeCore |
| 225 | 763 | Auffrac-les-Congères | character | 13 | 1 | gift/service | ItemAdd |
| 226 | 764 | Auffrac-les-Congères | character | 1 | 5 | scene | BadgeSetFlag, CallFieldCaptureDemo, CallTrainerLose, EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, GymSetVictoryInfo, MdlSetGPos, _CallTrainerBattleCore |
| 229 | 767 | Auffrac-les-Congères | character | 3 | 1 | gift/service | CallFieldTradeBoxSelect, CallFieldTradeDemo |
| 230 | 768 | Auffrac-les-Congères | character | 0 | 2 | scene | FadeRequestOut |
| 231 | 769 | Auffrac-les-Congères | character | 2 | 1 | scene | GlobalCall |
| 232 | 770 | Auffrac-les-Congères | character | 0 | 1 | gift/service | ItemAdd |
| 233 | 771 | Batisques | character | 0 | 12 | state | FlagSet |
| 235 | 773 | Batisques | character | 0 | 1 | scene | FadeRequestOut |
| 236 | 774 | Gare de Batisques | character | 0 | 1 | gift/service | ItemAdd |
| 237 | 775 | Batisques | character | 0 | 1 | scene | CallTrainerLose, _CallTrainerBattleCore |
| 238 | 776 | Batisques | character | 0 | 3 | gift/service | ItemAdd |
| 239 | 777 | Batisques | character | 0 | 2 | gift/service | ItemAdd |
| 240 | 778 | Batisques | character | 1 | 3 | gift/service | ItemAdd |
| 241 | 779 | Maison de Combat | character | 2 | 1 | gift/service | ItemAdd |
| 242 | 780 | Maison de Combat | character | 1 | 2 | scene | EVCameraBegin, EVCameraEnd, EVCameraIsEndFrame, EVCameraSetDstParam, EVCameraSetFrameParam, EVCameraSetGameCameraParam, EVCameraSetInitParam, EVCameraSetSlideParam, FadeRequestOut, GlobalCall, MdlAcmdMakeMoveEx, MdlSetGPos, MdlSetVisible |
| 244 | 782 | Ligue Pokémon | character | 9 | 1 | gift/service | ItemAdd |
| 264 | 802 | Route 7 | step |  | 33 | scene | MdlChangeMoveCode |
| 264 | 802 | Route 7 | step |  | 33 | scene | MdlChangeMoveCode |
| 264 | 802 | Route 7 | step |  | 8 | scene | IEReqChangeScript |
| 264 | 802 | Route 7 | step |  | 9 | scene | IEReqChangeScript |
| 267 | 805 | Route 9 | character | 1 | 4 | scene | FadeRequestOut |
| 270 | 808 | Route 12 | character | 1 | 4 | scene | GlobalCall |
| 271 | 809 | Route 12 | character | 0 | 2 | gift/service | ItemAdd |
| 273 | 811 | Route 14 | character | 13 | 18 | gift/service | ItemAdd |
| 277 | 815 | Route 16 | character | 0 | 1 | gift/service | ItemAdd |
| 277 | 815 | Route 16 | character | 1 | 2 | gift/service | ItemAdd |
| 286 | 824 | Forêt de Neuvartault | step |  | 4 | state | WorkSet |
| 286 | 824 | Forêt de Neuvartault | step |  | 5 | scene | IEReqChangeScript |
| 286 | 824 | Forêt de Neuvartault | step |  | 7 | scene | IEReqChangeScript |
| 286 | 824 | Forêt de Neuvartault | step |  | 8 | scene | IEReqChangeScript |
| 286 | 824 | Forêt de Neuvartault | step |  | 9 | state | WorkSet |
| 286 | 824 | Forêt de Neuvartault | step |  | 10 | state | WorkSet |
| 286 | 824 | Forêt de Neuvartault | step |  | 12 | scene | IEReqChangeScript |
| 287 | 825 | Palais Chaydeuvre | character | 7 | 6 | gift/service | ItemAdd |
| 302 | 840 | Palais Chaydeuvre | step |  | 6 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 6 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 6 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 7 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 7 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 7 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 8 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 8 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 8 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 5 | scene | IEReqChangeScript |
| 302 | 840 | Palais Chaydeuvre | step |  | 5 | scene | IEReqChangeScript |
| 305 | 843 | Grotte Miroitante | character | 0 | 6 | gift/service | ItemAdd |
| 306 | 844 | Grotte Miroitante | character | 0 | 3 | scene | FadeRequestOut |
| 311 | 849 | Usine de Poké Balls | character | 2 | 5 | scene | FadeRequestOut |
| 314 | 852 | Caverne Gelée | character | 0 | 1 | scene | FadeRequestOut |
| 318 | 856 | Village Pokémon | character | 0 | 2 | gift/service | ItemAdd |
| 318 | 856 | Village Pokémon | character | 2 | 6 | gift/service | ItemAdd |
| 321 | 859 | Porte de la Ligue | character | 0 | 1 | scene | CallTrainerLose, MdlAcmdMakeMoveEx, _CallTrainerBattleCore |
| 325 | 863 | Route Victoire | character | 0 | 2 | scene | FadeRequestOut |
| 329 | 867 | Château de Combat | character | 0 | 4 | scene | GlobalCall |
| 329 | 867 | Château de Combat | character | 2 | 3 | scene | GlobalCall |
| 334 | 872 | Cave Connecterre | character | 1 | 3 | gift/service | ItemAdd |
| 334 | 872 | Cave Connecterre | character | 2 | 2 | scene | FadeRequestOut |
| 338 | 876 | Centrale de Kalos | character | 1 | 1 | gift/service | ItemAdd |
| 338 | 876 | Centrale de Kalos | character | 3 | 3 | gift/service | ItemAdd, PlayerSubMoney |
| 338 | 876 | Centrale de Kalos | character | 5 | 5 | gift/service | ItemAdd |
| 340 | 878 | Repaire Team Flare | step |  | 8 | state | FlagSet |
| 340 | 878 | Repaire Team Flare | step |  | 9 | state | FlagSet |
| 349 | 887 | Hôtel Désolation | character | 17 | 7 | gift/service | ItemAdd |
| 353 | 891 | Route 7 | character | 1 | 3 | state | FlagSet, WorkSet |
| 353 | 891 | Route 7 | character | 2 | 2 | state | WorkSet |
| 355 | 893 | Safari des Amis | character | 0 | 1 | scene | MdlAcmdMakeMoveEx, _MapChangeCore |
| 357 | 895 | Baie Azur | character | 0 | 2 | gift/service | ItemAdd |
