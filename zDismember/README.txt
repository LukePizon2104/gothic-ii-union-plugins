zDismember - odcinanie konczyn i finishery dla Gothic II: Noc Kruka (Union)
===========================================================================

Instalacja
----------
Skopiuj zDismember.vdf do katalogu Data gry. Wymagany Union (1.0m lub nowszy).
Dziala z rendererem DirectX 7 i z GD3D11.

Co robi
-------
- Zabojczy cios bronia sieczna (miecz, topor) odcina glowe, reke albo noge.
  Odcieta czesc odlatuje, odbija sie od scian i ziemi, na ciele zostaje kikut z krwia.
  Dotyczy ludzi, orkow i prawie wszystkich potworow (takze walk NPC z NPC).
- Kikuty zostaja po zapisie i wczytaniu gry. Odlatujace czesci nie trafiaja do zapisu.
- Powalony czlowiek kleczy przez kilka sekund. Atak w tym czasie = egzekucja ze scieciem glowy.
  Pierwsze 0,6 s jest bezpieczne (kombinacja ciosow na arenie nie zabije przeciwnika).
- Finishery: czesc zabojczych ciosow zamienia sie w scenke z zwolnionym tempem i scieciem.
- Ragdolle: zabici (ludzie, orkowie, szkielety) padaja fizycznie w strone ciosu zamiast animacji smierci;
  po spoczynku cialo zostaje w tej pozie (po wczytaniu gry lezy zwykla poza smierci w tym samym miejscu).
  Ludzi, ktorych gra tylko powala, dotyczy to wedlug KnockoutFinisher (domyslnie tylko trwalych wrogow).
- Blok (parada) bohatera i NPC: w miejscu zetkniecia broni iskry z blyskiem (metal o metal) albo drzazgi (palki,
  buzdygany, kostury - material drewno), do tego krotki odrzut broni, rak i tulowia u blokujacego i u atakujacego
  (ok. 0,2 s z malym odbiciem, bez nowych animacji). Odskok parady i uniki/przewroty W3 sa bez efektu, kontra W3 z.

Ustawienia (Gothic.ini, sekcja [ZDISMEMBER], dopisywana przy pierwszym starcie)
-------------------------------------------------------------------------------
Enabled=1                  wlacznik
DismemberChancePlayer=100  szansa na odciecie przy zabiciu przez gracza (%)
DismemberChanceNpc=100     to samo w walkach NPC z NPC (%)
FinisherChance=30          szansa na finisher przy zabojczym ciosie gracza (%)
KnockoutFinisher=1         finisher (z ta sama szansa) przy ciosie, ktory powalilby czlowieka:
                           0 = nigdy (powaleni klecza, egzekucja tylko atakiem w czasie oszolomienia)
                           1 = tylko trwale wrodzy (arena, bojki i questowe powalenia bez zmian)
                           2 = wszyscy poza przyjaciolmi (uwaga: moze zabic NPC potrzebnych w questach)
DazeEnabled=1              oszolomienie powalonych ludzi
DazeSeconds=5              czas kleczenia (s)
DazeGraceSeconds=0.6       czas bez egzekucji po powaleniu (s)
HeroInvulnerable=1         bohater nietykalny w trakcie finishera
FinisherEarlyRelease=1     sterowanie wraca, gdy w animacji finishera konczy sie ruch (0 = dopiero na jej koncu)
FinisherRecoverBlend=0.3   czas przejscia z finishera do postawy walki (s)
FinisherSlowMo=2           spowolnienie czasu wokol ciosu dobijajacego: 0 wyl., 1 lekkie (polowa), 2 pelne (jak w finishers.ini)
FinisherSlowMoLastEnemy=0  1 = spowolnienie tylko, gdy w promieniu 20 m nie walczy z bohaterem inny wrog
AivarIndex=95              zmienna NPC na zapis odcietych czesci (zmien, jesli mod jej uzywa; -1 = bez zapisu, odciecia do
                           konca sesji). Przy wczytaniu swiata log podaje aivary bez nazwy AIV_* i zerowe u wszystkich NPC;
                           gdy aivar ma nazwe w skryptach albo zapisuje go mod (zywi z maska, wartosci spoza maski), pamiec
                           sama sie wylacza. New Balance: brak wolnego (81-87 bez nazw uzywa UNION_ABI.DLL) -> -1
MaxGibs=24                 maksymalna liczba odcietych czesci w swiecie
GibLifetimeSeconds=180     czas zycia odcietej czesci (s)
GibDespawnDistance=4000    odleglosc od gracza, po ktorej czesc znika (cm)
MaxVictimHeight=230        wyzsze stwory (troll, smok) nie dostaja finishera (cm)
FinisherStaggerRadius=800  walczacy w tym promieniu zataczaja sie i czekaja na koniec finishera (cm, 0 = wylacz)
MovesEnabled=1             uniki i przewroty (wymagaja paczki z ruchami, patrz nizej)
DodgeKey=47                klawisz uniku, kod DirectInput (47 = V)
RollKey=56                 klawisz przewrotu, kod DirectInput (56 = lewy Alt; w walce plugin przejmuje go ze skoku)
DodgeKeyCombatOnly=1       w walce klawisz uniku nie dochodzi do gry ani innych wtyczek (np. C = receptury New Balance);
                           poza walka dziala jak zwykle (0 = gra widzi go zawsze)
DoubleTapDodge=0           unik dwuklikiem kierunku W/S/A/D (Q/E): 0 wyl., 1 z dobyta bronia, 2 z bronia i Shiftem
DoubleTapMove=0            dwuklik robi: 0 unik, 1 przewrot
DoubleTapWindow=0.25       drugie wcisniecie najwyzej tyle s po pierwszym (0.1 - 0.6)
DodgeWithAction=1          unik / przewrot takze z trzymanym atakiem (sterowanie Gothica 1: postawa ataku); przy
                           trzymanym klawiszu uniku i ataku gra nie widzi kierunku, wiec nie zaczyna ciosu w bok
JumpAttackWindow=0.3       atak najwyzej tyle s po klawiszu skoku w walce = atak z wyskoku (Alt+W: co najmniej 0,4 s).
                           Atak musi trafic w odbicie skoku (ok. 0,4 s, do 110 cm) - pozniej bohater juz leci i skok
                           zostaje (w logu "za pozno"); dluzsze okno pomaga wolniejszemu klikaniu, nie atakowi z powietrza
Traversal=1                przeskoki i wspinanie w biegu W3: klawisz skoku w biegu przed przeszkoda (paczka prywatna)
TraversalCombat=1          to samo z dobyta bronia / piesciami (0 = w walce Alt jak dotad: przewrot, atak z wyskoku)
TraversalDebug=0           1 = debug wizualny przeskokow od startu (jak zdism trav show, opis nizej)
TraversalAirGrab=1         chwyt krawedzi w locie (skok gry, spadanie, W trzymane) - zawis i podciagniecie Gothica (0 = wylacz)
TraversalSheathe=1         przeskok / wspiecie z dobyta bronia (miecz, sztylet, piesci): bron chowana od razu na starcie (bez
                           animacji chowania, od razu na pasie / plecach), ruch torem i animacjami bez broni jak w W3; z bronia
                           tez z miejsca (Alt z W, bez S i bokow). Pochodnia / tarcza / para NB w lewej rece - jak dotad (0 = wyl.)
TraversalRedraw=1          po wyladowaniu bron schowana przez TraversalSheathe od razu z powrotem w rece (0 = zostaje schowana)
TraversalHint=3            podpowiedz na ekranie ("Alt - przeskocz" / "Alt - wespnij sie"), gdy bohater biegnie na przeszkode,
                           ktora klawisz skoku teraz by przeskoczyl - tyle pierwszych razy (0 = wylacz); najwyzej co 20 s
TraversalHintShown=0       licznik pokazanych podpowiedzi (plugin go zwieksza; 0 = podpowiedzi od nowa)
                           Jak wywolac przeskok: biec (W) prosto na przeszkode 30-250 cm (plotek, skrzynia, murek) i wcisnac
                           klawisz skoku (Alt) 1-4 m przed nia - plugin sam dobiera moment startu; z miejsca: Alt tuz przy
                           przeszkodzie (do 1,3 m; z bronia przy TraversalSheathe=1 z W)
JumpKeepMomentum=1         skok gry z biegu / sprintu zachowuje pozioma predkosc odbicia (do 850 cm/s; 0 = jak w grze)
Counters=1                 kontry: blok w trakcie zamachu przeciwnika = odbicie i riposta (bron 1H/2H)
CounterRepelChance=40      % kontr jako pchniecie barkiem / kopniak (przeciwnik zatacza sie do tylu, bez obrazen)
CounterOnBackstep=0        1 = odskok w tyl z bronia (T_xPARADEJUMPB) w zamachu wroga tez daje kontre (0 = zwykly odskok)
CounterWindow=0            blok najwyzej tyle s przed trafieniem wroga daje kontre (0 = caly zamach; menu: 0,2-0,7 s)
RapierThrustOnly=1         rapierem tylko finishery pchnieciem przez klatke i bez odcinania
RapierWeapons=RAPIER,DEGEN fragmenty nazwy instancji / modelu broni uznawanej za rapier (tez styl rapierowy zEBA)
BluntWeapons=KEULE,...     fragmenty nazwy instancji / modelu broni obuchowej (obuch: bez odciec i bez finisherow)
W3Locomotion=1             chod i bieg Geralta (bez broni zawsze, z mieczem w stylu Wiedzmina), tempo jak w Gothicu
W3Sprint=1                 w sprincie gry / zEBA animacja sprintu Geralta (0 = animacje sprintu zEBA zostaja)
W3Style=1                  styl walki Wiedzmina sam od progu talentu (paczka prywatna)
W3StyleMinTalent=60        prog talentu w % (osobno 1H i 2H)
W3AttackAssist=1           ciosy W3: doskok do celu - w zamachu obrot do wroga przed soba i dosuniecie do zasiegu
W3AttackReach=40           ciosy W3: premia zasiegu w tescie trafienia gry (cm, 0 = jak w grze)
W3AssistMargin=25          doskok: w chwili ciosu bohater staje o tyle cm blizej niz zasieg walki (do bboxa celu)
W3AssistMaxPath=130        doskok: najdluzsza droga na jedno ciecie (cm)
W3AssistSpeed=450          doskok: najwieksza predkosc (cm/s)
W3AssistCone=45            doskok: wrog najwyzej tyle stopni od przodu bohatera
StyleKey=22                klawisz debugowego cyklu stylow (22 = U, z Shiftem wstecz; 0 = tylko konsola)
LocoKey=24                 klawisz cyklu zestawow ruchu do testow (24 = O, z Shiftem wstecz; 0 = tylko konsola)
LocoMode=0                 zestaw ruchu: 0 AUTO (W3Locomotion/W3Sprint), 1 Gothic/zEBA, 2 W3 trucht, 3 W3 trucht+sprint,
                           4 podglad sprintu W3 (klawisz LocoKey i zdism loco zapisuja tu wybrany zestaw)
WeaponSwap=0               zamiana animacji walki bohatera: 0 wyl., 1 bron 1H animacjami 2H, 2 bron 2H animacjami 1H,
                           3 obie (obrazenia, talent, zasieg i tryb walki z prawdziwej broni; dobycie broni natywne)
WeaponSwapLast=3           ostatni wlaczony wariant (klawisz zamiany go przywraca)
WeaponSwapLevel=0          poziom nakladki talentu przy zamianie: 0 talent broni w rece (miecz 1H z talentem 2 gra
                           HUMANS_2HST2), 1 talent drugiego typu (nakladki jak w grze)
WeaponSwapPerWeapon=0      1 = stan zamiany osobno dla kazdej broni (lista WeaponSwapWeapons, klawisz dopisuje /
                           wypisuje bron w reku); 0 = jeden stan dla wszystkich
WeaponSwapWeapons=         instancje broni z wlaczona zamiana (np. ITMW_1H_SWORD_01,ITMW_2H_AXE_L_01)
WeaponSwapKey=22           klawisz zamiany (22 = U; 0 = tylko konsola zdism swap)
WeaponSwapMods=3           modyfikatory klawisza zamiany: 1 Ctrl + 2 Shift + 4 Alt (3 = Ctrl+Shift+U; samo U = styl)
                           Bronie parowe New Balance (para *_RIGHT / *_LEFT, zmienne skryptu RH_READY_2X2 / LH_READY_2X2):
                           zawsze natywne animacje NB (HUMANS_1X2ST3 / HUMANS_2X2ST3) - bez zamiany, stylu W3 / talentu
                           nad nimi, dobycie i polozenie broni natywne; klawisz stylu przy parze tylko informuje, a wybor
                           1H / 2H wraca po odlozeniu pary (wpis w logu "Bron parowa NB: ...")
LootArmed=1                podnoszenie z dobyta bronia: atak bez walczacych wrogow podnosi przedmiot przed bohaterem
LootRange=150              przedmiot najwyzej tyle cm od bohatera (w poziomie, +-45 st. od przodu, 50-400)
LootEnemyRadius=1000       walczacy wrog blizej niz tyle cm = klawisz zwyczajnie atakuje (0 = bez sprawdzania wrogow)
LootHint=1                 nazwa przedmiotu nad nim, gdy atak by go teraz podniosl
LootArmedDoors=2           drzwi z bronia w rece: 0 wyl., 1 tylko bez zamka i skryptu, 2 tez zamkniete / ze skryptem
                           (schowanie broni, zwykle otwarcie gry, dobycie)
LootArmedChests=1          skrzynie: schowanie broni, zwykle otwarcie gry (zamek, wytrych, kradziez), dobycie
LootArmedCorpses=1         przeszukiwanie martwych i nieprzytomnych (okno od razu; gra chowa bron przy oknie)
LootArmedRedraw=1          dobycie broni po oknie / interakcji, gdy bron zostala schowana (nie po rozmowie)
Meditate=1                 medytacja Wiedzmina (opis nizej; wymaga nakladki HUMANS_ZDF_W3_MEDITATE)
MeditateKey=28             klawisz medytacji (28 = Enter; zaczyna medytacje tylko, gdy mozna - poza tym zostaje grze)
MeditateKeyAlt=51          drugi klawisz (51 = przecinek) z modyfikatorami MeditateKeyAltMods - kombinacje przejmuje plugin
MeditateKeyAltMods=1       modyfikatory drugiego klawisza: 1 Ctrl + 2 Shift + 4 Alt (1 = Ctrl + przecinek)
MeditateSpeed=20           tempo czasu w czekaniu (5-30; zmieniane tez w oknie medytacji)
MeditateRegen=1            regeneracja w czekaniu: 0 brak, 1 stopniowa, 2 jak sen (zapamietywana z okna medytacji)
MeditateRegenHours=8       stopniowa: pelne zycie i mana po tylu godzinach gry
MeditateSleepHours=6       jak sen: pelne zycie i mana po co najmniej tylu godzinach czekania (krocej - nic)
PathMenu=1                 New Balance: zakladki Gildia / Sciezki w medytacji, klawisz okna i pilnowanie wylaczen (opis nizej)
PathMenuKey=26             klawisz okna gildii i sciezek bez klekania (26 = '['; 0 = brak); Steam / Kroniki - klawisz dla gry
PathMenuKeyMods=0          modyfikatory klawisza okna: 1 Ctrl + 2 Shift + 4 Alt
PathDialogGuard=1          w rozmowach wylaczone umiejetnosci wracaja na czas rozmowy (nauczyciel nie sprzeda drugi raz)
MenuMouse=1                mysz w oknach pluginu (medytacja, gildia i sciezki): kursor, LPM, kolko, PPM
MenuMouseSpeed=1           piksele kursora na jednostke myszy (czulosc myszy z opcji gry juz wliczona)
BackSwords=1               miecze na plecach: dobywanie zza plecow animacjami Geralta (2H i 1H z Back1H), drugi miecz
                           (opis nizej; wymaga prywatnej paczki zDismember_W3.vdf z nakladkami HUMANS_ZDF_DRAW_*)
Back1H=1                   bron 1H: 0 przy pasie, 1 miecze (ostrze) na plecach, 2 kazda bron 1H na plecach
BackDrawSpeed=100          tempo dobycia zza plecow (%, 25-400; 100 = ok. 0,64 s, chwyt rekojesci 0,32 s)
BackSheatheSpeed=100       tempo chowania na plecy (%, 100 = ok. 0,68 s)
BeltDrawSpeed=150          tempo natywnego dobycia / chowania 1H przy pasie (%, 100 = animacja z gry / zEBA bez zmian)
SecondSword=1              drugi miecz na plecach i zamiana klawiszem
SecondSwordKey=34          klawisz drugiego miecza (34 = G; 0 = tylko konsola): w ekwipunku wybor zaznaczonej broni,
                           poza nim zamiana z zalozona bronia (z dobyta bronia: chowanie, zalozenie, dobycie)
SecondSwordKeyMods=0       modyfikatory klawisza: 1 Ctrl + 2 Shift + 4 Alt
SecondWithRanged=0         przy zalozonym luku / kuszy drugi miecz: 0 ukryty, 1 zamiast luku (luk / kusza ukryte)
BackAdjustA=0 0 0 0 0 0    przesuniecie miecza na plecach w miejscu A (stal): x y z cm, rx ry rz st. w ukladzie miecza
BackAdjustB=0 0 0 0 0 0    to samo dla miejsca B (srebro); na stale - make_draw.py (przelozenie w dloni bez skoku)
Crossbow=1                 szybki strzal z kuszy spod F (opis nizej; slot kuszy wybiera zQuickSign: Shift + kolko za
                           ostatnim Znakiem; wymaga prywatnej paczki zDismember_W3.vdf z HUMANS_ZDF_XBOW)
XbowSpeed=100              tempo animacji i chwil strzalu (%, 25-400; 100 = jak W3: belt ok. 0,5 s po wcisnieciu F)
XbowCooldownMs=1320,1200,960  przeladowanie (ms) wedlug talentu kuszy 0 / 1 / 2 (jak natywne T_CBOWRELOAD)
XbowAutoRange=3000         zasieg celu automatycznego przy stuknieciu F (cm)
XbowAutoCone=40            pol kata stozka kamery, w ktorym szukany jest wrog (st.); fokus wygrywa, gdy jest przed bohaterem
XbowMoveCone=100           w ruchu cel najwyzej tyle st. od kierunku biegu (inaczej strzal przed siebie)
XbowAutoNeutral=0          1 = cel automatyczny takze nie-wrogi (trafienie neutralnego to przestepstwo, jak zwykly strzal)
XbowWithShield=1           strzal z tarcza New Balance (1) albo odmowa (0)
XbowWithDual=1             strzal z bronia parowa New Balance - lewa bron ukryta na czas strzalu (1) albo odmowa (0)
XbowWithTorch=0            pochodnia w lewej rece: 0 = odmowa "Najpierw schowaj pochodnie", 1 = strzal (swiatlo zostaje)
XbowHitCancel=1            trafienie bohatera przed beltem przerywa strzal (bez beltu i przeladowania, jak w W3)
XbowHandScale=100          wielkosc kuszy w lewej dloni (%, 30-200; kusza Geralta jest mniejsza niz gothicka)
XbowHandAdjust=0 0 0 0 0 0 przesuniecie kuszy w dloni: x y z cm, rx ry rz st. w ukladzie kuszy, wokol chwytu (GripCm)
XbowMuzzleGripCm=10        wylot beltu tyle cm przed chwytem kuszy (dlonia); tor zawsze z wylotu w punkt celu
                           (stary klucz XbowMuzzleCm nieuzywany)
XbowBoltSpeed=3600         predkosc beltu (cm/s; silnik 2000, New Balance 3600); tor prosty, bez grawitacji jak w silniku
XbowAimSlowmo=100          (na przyszlosc) % tempa gry w celowaniu - zadziala z modulem czasu menu kolowego
Ragdoll=1                  ragdolle zwlok
RagdollChance=100          szansa (%)
RagdollImpulse=180         sila pchniecia ciala po ciosie (cm/s)
RagdollMaxActive=4         ile zwlok symuluje sie naraz (starsze zamarzaja)
ParryFx=1                  udany blok: iskry (metal o metal) / drzazgi (drewno) w miejscu zetkniecia broni
ParryRecoil=1              udany blok: odrzut broni i rak u obu walczacych
ParryRecoilStrength=5      sila odrzutu: kat wezla w szczycie (stopnie, 0-20; bron odchyla sie o ok. 3x tyle)
ParryRecoilTime=0.18       czas odrzutu blokujacego (s, 0,08-0,4; atakujacy x1,15 i x1,3 mocniej)
ParryWoodWeapons=STAFF,... fragmenty nazwy instancji / modelu broni drewnianej mimo materialu (mody); w G2 drewno
                           rozpoznaje material broni (C_Item.material = MAT_WOOD)
PartWeightsSwing=50,35,15  wagi: glowa, reka, noga
PartWeightsThrust=20,0,0   wagi przy pchnieciu
Debug=0                    szczegolowy log w zDismember.log
Profiler=1                 lekki profiler: przycinki do logu, podsumowanie na zadanie (zdism prof); 0 = wylaczony
ProfSpikeMs=25             przycinka = klatka dluzsza niz max(tyle ms, 2 x mediana ostatnich 120 klatek)
ProfSummarySeconds=0       podsumowanie profilera do logu co tyle sekund (0 = tylko na zadanie)
ProfSampler=1              probkowanie watku gry (przy Profiler=1): po kazdej przycince linia "Prof: probki" - gdzie
                           (modul / funkcja Gothic2.exe) szedl czas klatki; tabele: zdism prof samples; 0 = wylaczone
ProfSampleMs=1             odstep probek w ms (realnie ok. 1,4 ms; przy koszcie probki > 100 us samo przechodzi na 5)
PreloadOverlays=1          nakladki bohatera (style, ruch W3, uniki, przeskoki, finishery, gesty Znakow) wczytane przy
                           wczytaniu swiata i trzymane w pamieci - zalozenie w grze nie czyta MSB/MAN z dysku (bez
                           przycinek 5-12 ms); kosztem ok. sekundy przy pierwszym wczytaniu i kilkunastu MB; 0 = jak dawniej
Signs=1                    Znaki Wiedzmina (runy Aard, Igni, Axii, Yrden, Quen - opis nizej); 0 = runy znakow nic nie robia
SignSpellIds=90,91,92,93,94  sloty czarow znakow (Aard, Igni, Axii, Yrden, Quen) - jak SPL_Aard..SPL_Quen w skryptach
SignManaCost=10,15,15,20,15  koszt many kazdego znaku
SignAniLetters=WND,FBT,SLE,FEA,HEA  gest rzutu w trybie magii (litery animacji Gothica: piesc wiatru, ognista strzala,
                           sen, strach, leczenie)
SignCooldown=1             odnowienie kazdego znaku (s)
SignHitNeutral=0           Aard / Igni / pekniecie Quen tez w neutralnych (1); przyjaciol i druzyny nigdy
AardDamage=15  AardRange=600  AardCone=50  AardPush=150  AardFlyMaxProt=160  AardFlyMaxHeight=230
                           obrazenia od lotu (DAM_FLY), zasieg (cm), polowa kata stozka (st.), odsuniecie nielatajacych
                           (cm); leca cele z ochrona przed lotem do AardFlyMaxProt (orkowie 130-160; -1 = nikt) i nie
                           wyzsze niz AardFlyMaxHeight cm, reszta (troll, golem, smok) sie zatacza
AardFlyCheck=0.25  AardFlyMinMove=50  AardFallbackPush=250
                           po tylu s sprawdzenie, czy lekki cel naprawde polecial (co najmniej AardFlyMinMove cm od
                           bohatera albo nadal w powietrzu); jesli nie - potkniecie i odsuniecie o AardFallbackPush cm
IgniDamage=40  IgniRange=650  IgniCone=35  IgniBurnTime=3  IgniBurnDps=5  IgniReactTime=1.5
                           obrazenia od ognia, zasieg, polowa kata; plonienie silnika (s, obrazenia/s); reakcja palenia
                           ludzi (s, 0 = bez)
AxiiStunTime=4  AxiiRange=800  AxiiCone=30  AxiiFinisher=1  AxiiStunAnis=PRAY,FEAR1,FEAR2,FEAR3
                           oszolomienie w walce (s), zasieg i polowa kata wyboru celu; cios w kleczacego = egzekucja;
                           warianty oszolomienia ludzi (losowo, bez powtorki pod rzad): PRAY kleczenie, FEAR1-3 skulenie
                           ze strachu, WOUNDED / WOUNDEDB powalenie (dlugie: 3-5 s w dol, 8-10 s wstawania - tylko recznie)
YrdenRadius=500  YrdenTime=15  YrdenSlow=0.65  YrdenArmor=30
                           promien strefy (cm), czas (s), tempo wrogow (x), ochrona wrogow (-%)
QuenTime=30  QuenRadius=250  QuenDamage=10  QuenPush=120
                           czas tarczy (s); pekniecie: promien odrzutu (cm), obrazenia (magia), odsuniecie (cm)
SignPowerPerMana=0.001  SignPowerMax=1
                           moc znakow z maks. many bohatera: P = 1 + min(SignPowerMax, mana max x SignPowerPerMana)
                           (100 many = x1,1, 500 = x1,5, 1000+ = x2); koszty many bez zmian
YrdenSlowMin=0.40  YrdenArmorMax=50
                           Yrden przy mocy x2: tempo wrogow i oslabienie ochrony (-%); miedzy x1 a x2 liniowo
SignAltHold=0.5            przytrzymanie rzutu dluzej niz tyle s = tryb alternatywny (0 = bez trybow, znak od razu;
                           do rundy 5 domyslnie 0.35 - zapisana w Gothic.ini wartosc zostaje, zmiana w menu)
AardAltCost=20  AardAltRadius=400  AardAltDamage=0.8
                           Aard - fala wokol: koszt many, promien (cm), mnoznik obrazen Aarda
IgniStreamDps=30  IgniStreamManaPerSec=12  IgniStreamCone=20  IgniStreamRange=700  IgniStreamTick=0.25
                           Igni - strumien: obrazenia ogniem na sekunde, mana na sekunde, polowa kata, zasieg (cm), takt (s)
YrdenAltCost=25  YrdenTrapRadius=300  YrdenTrapTime=30  YrdenTrapCharges=3  YrdenTrapDamage=35  YrdenTrapInterval=0.8
                           Yrden - pulapka: koszt, promien glifu (cm), czas (s), ladunki, obrazenia pocisku (magia),
                           odstep pociskow w tego samego wroga (s)
QuenActiveManaPerSec=10  QuenActiveHeal=0.5
                           Quen - aktywna tarcza: mana na sekunde, czesc pochlonietych obrazen oddawana jako zycie
SignChannelMoveScale=0.6  SignChannelFrame=10
                           podtrzymanie (strumien Igni, aktywna tarcza Quen): tempo bohatera (ruch i animacje, x) i klatka
                           gestu znaku W3 trzymana jako poza (wyciagnieta dlon; gest ma 17 klatek, pchniecie na 8.)
SignAltInPlace=1           tryb alternatywny Aard / Yrden spod F (zQuickSign gra gest W3 "w ziemie" calym cialem): bohater
                           stoi w miejscu do konca gestu (blokada ruchu ok. 1,1 s), fala / pulapka w chwili dotkniecia
                           ziemi (0,44 s po starcie gestu - czasy z zQuickSign.ini SignAltCastMs / SignAltGestureMs);
                           0 = jak dawniej (skutek od razu, bez blokady)
AxiiAltCost=25  AxiiPuppetTime=15  AxiiPuppetRange=1500  AxiiPuppetEndStun=1.5
                           Axii - marionetka: koszt, czas (s), zasieg szukania wrogow marionetki (cm), oszolomienie po
                           koncu marionetki (s, 0 = od razu wraca do walki z bohaterem)
AxiiTranceTime=20          Axii poza walka: trans czlowieka po niepamieci (s x moc; 0 = sama niepamiec, jak w rundzie 4)
SignFires=1                Aard gasi, Igni (i strumien) podpala ogniska, piece, paleniska i pochodnie w uchwytach w stozku
SignAimRange=1500          celowanie (Gothic Free Aim / celownik F zQuickSign): najdalszy punkt kregu i pulapki Yrden (cm)
SignDefaultsRev=5          numer zmian domyslnych (runda 5: IgniRange 450 -> 650, IgniStreamRange 500 -> 700 - stara
                           wartosc domyslna w ini zamieniana raz); nie ruszaj
MenuSg*                    pozycje stron strojenia znakow w menu - jak Menu*
Menu*                    pozycje menu w grze (nizej) - plugin sam je uzgadnia z kluczami powyzej, nie ruszaj

Menu w grze
-----------
Opcje -> Union & Pluginy -> zDismember: cztery strony - "Odcinanie i finishery", "Walka i ruch Wiedzmina" oraz dwie
strony strojenia znakow: "Znaki: zasieg i sila" (zasieg i obrazenia Aard, zasieg stozka i strumienia, obrazenia,
palenie na sekunde i czas palenia Igni, oszolomienie i marionetka Axii, promien, czas, spowolnienie i oslabienie Yrden)
i "Znaki: moc, czas i koszty" (czas tarczy i pekniecie Quen, moc z maks. many i jej limit - od mocy zaleza tez
trafienia Quen, przytrzymanie trybu alternatywnego, koszty many piecu znakow i trzech trybow alternatywnych). Wartosci
to listy wyboru; wartosc z Gothic.ini spoza listy pokazuje najblizsza pozycje i zostaje, dopoki jej nie przestawisz.
Strona "Walka i ruch Wiedzmina"
(doskok do celu, styl, prog talentu, chod i bieg, animacja sprintu, uniki, klawisze uniku i przewrotu, kontry - osobna
strona: kontra przy bloku, kontra z odskoku, okno kontry, bark / kopniak; zestaw ruchu do
testow, przeskok i wspinanie: wyl. / poza walka / tez w walce, Znaki Wiedzmina: wyl. / wl., podnoszenie z bronia:
wyl. / wl. / wl. + podpowiedz). Zmiany dzialaja po
wyjsciu z menu. Menu wymaga zParserExtendera (jest w Unionie); paczka niesie wspolny zUnionMenu.d.

Konsola (tryb Marvina)
----------------------
zdism info          stan NPC w fokusie
zdism cut HEAD      odetnij czesc (HEAD, ARM_L, ARM_R, UPPERARM_L, UPPERARM_R, LEG_L, LEG_R, THIGH_L, THIGH_R)
zdism uncut         przywroc odciete czesci NPC w fokusie (jak cofniecie czasu: sciskanie, odlamki, krew z kikuta;
                    zwloki zostaja zwlokami)
zdism daze          powal NPC jak gracz (test oszolomienia)
zdism fin <ID>      wymus finisher na NPC (lista: zdism list)
zdism moves         lista unikow i przewrotow
zdism move <ID>     wymus ruch (DODGE_L/R/B/F, ROLL_L/R/B/F, COUNTER_L/R) - test bez klawiszy
zdism style         nastepny styl walki dla broni w reku (prev / auto / list)
zdism loco          nastepny zestaw ruchu bohatera (prev / auto / list / 0-4); info = stan ruchu na ekranie,
                    watch = stan ruchu na ekranie co 0,8 s (wlacz/wylacz)
zdism trav          przeskok / wspiecie: co plugin widzi przed bohaterem (teren, rodzaj, wysokosc gory od podstawy, sciana,
                    kat, glebokosc / grubosc, ziemia za przeszkoda), wybrany klip i decyzja (start / czeka / za
                    daleko / za blisko) oraz czy bohater "biegnie" (W, predkosc); wpis tez w zDismember.log;
                    zdism trav go = wykonaj teraz (bez warunku biegu), zdism trav list = klipy;
                    zdism trav show / hide = debug wizualny (TraversalDebug), zdism trav kropki / linie = linie
                    rysowane kropkami (gdy linie DrawLine nie sa widoczne) albo z powrotem liniami
zdism swap          zamiana animacji 1H <-> 2H: stan (wariant, bron, tablica, nakladki talentu); toggle = jak
                    klawisz, 0-3 = wariant, reload = przebuduj animacje bohatera
zdism ragdoll       wlacz/wylacz ragdolle
zdism parry         blok bez walki: NPC w fokusie uderza, bohater blokuje (iskry/drzazgi + odrzut);
                    zdism parry hit = bohater uderza, NPC blokuje
zdism reload        przeladuj ustawienia, finishery i ruchy
zdism prof          podsumowanie profilera od resetu (krotko w konsoli, tabela sekcji w zDismember.log);
                    zdism prof reset = statystyki od nowa, zdism prof off / on = wylacz / wlacz do restartu;
                    zdism prof samples = tabele probek (przycinki vs klatki normalne), zdism prof samples reset
zdism sign <znak>   Znak Wiedzmina bez runy (aard, igni, axii, yrden, quen) - do testow: bez many i odnowienia, bez
                    gestu i efektu dloni; Axii bierze NPC z fokusu albo najlepszego w stozku;
                    zdism sign <znak> alt = tryb alternatywny (strumien Igni i aktywna tarcza Quen trwaja 3 s)
zdism sign info     stan znakow: moc, tarcza Quen (trafienia), strefa i pulapka Yrden (ladunki, spowolnieni),
                    zatrzymani (Axii / Igni), podtrzymanie, marionetka, odnowienia
zdism fires [m]     ognie w promieniu (domyslnie 10 m): klasa, nazwa, wizual, widoczny / ukryty, uchwyt, grupa (dodatki,
                    swiatla dyn. / statyczne, dzwieki) i czy Aard / Igni zadziala; pelna lista w zDismember.log
zdism loot          podnoszenie z bronia: przedmiot przed bohaterem (odleglosc, kat), walczacy wrog w poblizu i czy atak
                    teraz by podniosl (albo dlaczego nie); cel interakcji (drzwi, skrzynia, zwloki) z planem i opisem
                    (zamek, skrypty, lup), pominiety cel, opcje LootArmed*, faza trwajacej interakcji; wpis tez w logu
zdism meditate      medytacja: start po zamknieciu konsoli (albo powod odmowy w logu); info = stan, godzina, tempo,
                    pliki nakladki, dar aktywny; stop = wstan
zdism paths         New Balance: gildia, znaczniki sciezek (stan, slad, zapamietany), ZDF_PATHS_OFF, wylaczone
                    umiejetnosci, ochrona rozmow, blokady; skills = wyuczone umiejetnosci; open = okno po zamknieciu konsoli
zdism back          miecze na plecach: plan (gdzie wisi bron, wariant dobycia 1H / 2H, drugi miecz), tempo, zamiana;
                    second [INSTANCJA|clear|auto] = drugi miecz, swap = zamiana po zamknieciu konsoli,
                    adjust a|b x y z [rx ry rz] = przesuniecie miecza na plecach (zapis w Gothic.ini)
zdism xbow          szybki strzal z kuszy: stan (kusza, belty, talent, szansa, slot pod F, faza, gotowosc, przeladowanie,
                    pozyczone wpisy animacji); fire = sam belt bez animacji (test pocisku), shoot = stukniecie, aim =
                    celowanie 1,5 s i strzal w punkt kamery (po zamknieciu konsoli); anis = test pozyczania wpisu
                    T_FOOD_RANDOM_1 (anis info - lista), reset = strzal przerwany, przeladowanie od nowa

Podnoszenie przedmiotow z dobyta bronia (jak w Wiedzminie 3)
------------------------------------------------------------
Z bronia biala albo piesciami w rece klawisz ataku (akcja: Ctrl / lewy przycisk myszy) podnosi przedmiot lezacy przed
bohaterem (do LootRange cm, +-45 st., widoczny - sciana go nie zaslania), jesli w promieniu LootEnemyRadius nikt nie
walczy (wrog celujacy w bohatera, wrogi NPC z dobyta bronia albo cel bohatera). Wtedy wcisniecie nie daje ciosu (nic nie
zostaje w kolejce), a z wrogiem w poblizu klawisz atakuje jak zawsze. Bohater stoi albo idzie (w biegu najpierw staje),
nie w trakcie ciosu, uniku, przeskoku, finishera, skoku, sprintu ani w wodzie; okna (ekwipunek, dialog, konsola) zamkniete.
Animacja: natywne podnoszenie Gothica (T_STAND_2_IGET / S_IGET / T_IGET_2_STAND, wysokosc wg przedmiotu) z bronia
w dloni, bohater obraca sie do przedmiotu; potem wraca do postawy broni stylu (w stylu Wiedzmina - garda W3). Przedmiot
trafia do ekwipunku w chwili chwytu dokladnie jak przy zwyklym podnoszeniu (ten sam komunikat gry: skrypty, stosy,
kradziez - wlasciciel moze zauwazyc). Pomijane: plonaca pochodnia (gra wklada ja do lewej reki), przedmioty w gniazdach
(oltarze), lecace / spadajace i oznaczone przez gre jako nie do fokusu.
Podpowiedz (LootHint=1): nazwa przedmiotu nad nim, gdy atak teraz by go podniosl.
Debug=1: w zDismember.log kazde podniesienie ("Podnoszenie: start ...", "... animacja podnoszenia po X s",
"... w ekwipunku po X s (w ekwipunku A -> B)") i kazda odmowa z powodem ("Podnoszenie: odmowa (...) - walczacy wrog
w poblizu: <NPC> <cm> ..."); odmowy bez przedmiotu przed bohaterem tylko poza walka (w walce bez spamu).

Drzwi, skrzynie i przeszukiwanie cial z dobyta bronia (LootInteract.cpp, Core\Interact.*)
------------------------------------------------------------------------------------------
Ten sam klawisz ataku i te same warunki co podnoszenie (bron biala / piesci, nikt nie walczy w LootEnemyRadius, bohater
stoi albo idzie, okna zamkniete) dzialaja tez na cel przed bohaterem: drzwi, skrzynie, martwych i nieprzytomnych NPC.
Wybor: najblizszy punkt obrysu do LootRange cm, +-45 st., widoczny; przedmiot i cel interakcji - wygrywa blizszy / lepiej
na osi (przy remisie przedmiot). Wszystko przez zwykle funkcje gry (jak oCAIHuman::StandActions), wiec kradziez (wlasciciel
skrzyni widzi - AssessUseMob od pierwszej klatki), zamki, wytrych, skrypty MOB-ow i komunikaty dzialaja jak zawsze.
- Drzwi bez zamka i skryptu: otwarcie / zamkniecie zwykla interakcja gry Z BRONIA W RECE (bez chowania). Po animacji
  postawa broni (silnik po T_..._2_STAND gra S_RUN - plugin dopilnowuje postawy broni).
- Drzwi zamkniete na klucz / wytrych albo ze skryptem (conditionFunc, onStateFunc, przedmiot): przy LootArmedDoors=2
  schowanie broni, zwykla interakcja gry (klucz, minigra wytrycha, skrypt), potem dobycie; przy LootArmedDoors=1 atak
  zostaje grze.
- Skrzynie: schowanie broni, zwykle otwarcie gry (zamek, wytrych, kradziez), po zamknieciu okna dobycie. Silnik i tak chowa
  bron przy kazdym oknie ekwipunku (oCNpc::OpenInventory wysyla EV_REMOVEWEAPON), a chowanie w trakcie animacji skrzyni
  to niezbadana sciezka - dlatego najpierw schowanie.
- Zwloki i nieprzytomni: okno przeszukania od razu (jak w grze, bez animacji); gra chowa bron przy otwarciu okna, po
  zamknieciu plugin dobywa ja z powrotem. Ciala bez lupu (tylko zalozone rzeczy) sa pomijane - atak zostaje grze.
- Zamek bez klucza i bez wytrycha (albo bez talentu): tylko komunikat gry ("brak klucza", T_DONTKNOW), bron zostaje w rece.
- Ponowne dobycie (LootArmedRedraw=1): tym samym trybem (piesci / 1H / 2H), gdy okna zamkniete, kolejka pusta i bohater
  stoi / idzie; bez dobycia, gdy w trakcie byla rozmowa (np. wlasciciel skrzyni) albo gracz dobyl bron sam.
Opcje (Gothic.ini [ZDISMEMBER], menu "Interakcje z bronia w reku" - numer pozycji = wartosc): LootArmedDoors (0 wyl.,
1 tylko bez zamka i skryptu, 2 wszystkie - domyslnie), LootArmedChests, LootArmedCorpses, LootArmedRedraw (0/1, domyslnie
1). LootArmed=0 (MenuLoot "wyl.") wylacza tez drzwi, skrzynie i ciala. Podpowiedz (LootHint): nazwa celu nad nim,
"(zamkniete)" przy zamku bez klucza.
Log (zawsze): "Interakcja z bronia: <drzwi|skrzynia|zwloki> '<nazwa>' <cm> (wynik): <plan> [<vob / schemat: zamek,
skrypty, stan>]", potem etapy ("chowanie broni", "start interakcji gry (otwieranie|zamykanie, stan s/n)", "przeszukanie:
okno otwarte, gra chowa bron", "dobycie broni"), na koniec "koniec po X s: <powod>". Odmowy z celem przed bohaterem
("odmowa (...) - <powod>; cel: ..."; przy walczacym wrogu tylko z Debug=1) i cele pominiete ("cel pominiety - ...:
zamek / skrypt przy LootArmedDoors=1, bez lupu, za daleko od miejsca interakcji, zasloniety"). Konsola: zdism loot
(przedmiot, cel interakcji z planem, pominiety cel, opcje, faza przebiegu).

Medytacja (jak w Wiedzminie 3)
------------------------------
Enter (MeditateKey) albo Ctrl + przecinek (MeditateKeyAlt) poza walka: bohater chowa bron, kleka i otwiera sie okno:
Czekaj 1-24 godz., Czekaj do (rano 6:00, poludnie 12:00, wieczor 18:00, polnoc 0:00), Tempo x5-x30, Regeneracja (brak /
stopniowa / jak sen), Dar aktywny (tylko New Balance z co najmniej dwoma patronatami: auto / Innos / Adanos / Beliar -
karma_skill_learned[90]) i Wstan. Strzalki (tez W/S/A/D) wybieraja i zmieniaja, Enter wykonuje, Esc wstaje; w trakcie
okna gra nie widzi klawiszy (Esc nie otwiera menu gry, bohater sie nie rusza). Czekanie przyspiesza czas (factorMotion =
tempo, a reszte tempa - silnik tnie klatke do 100 ms - plugin doklada do zegara gry); bohater kleczy w normalnym tempie.
Koniec czekania (godzina, Esc / Enter) = z powrotem okno; walczacy wrog w poblizu = bohater wstaje; trafienie, dialog,
smierc = koniec od razu. Nie da sie zaczac: w walce (wrog w promieniu 15 m), w wodzie, w skoku / upadku, na wierzchowcu,
w ekwipunku, dialogu, dokumencie. Nakladka HUMANS_ZDF_W3_MEDITATE jest zakladana tylko na czas medytacji (zapis bez niej).
Nazwy nakladki i animacji mozna nadpisac w Gothic.ini (klucze nie sa dopisywane): MeditateOverlay, MeditateAniStart,
MeditateAniLoop, MeditateAniStop. Wpisy w zDismember.log: "Medytacja: start", "okno", "czekanie", "koniec czekania",
"wstawanie", "koniec", "odmowa".

Gildia i sciezki (tylko New Balance)
------------------------------------
W oknie medytacji zakladki Medytacja / Gildia / Sciezki (Q / E albo Tab), to samo okno bez klekania pod PathMenuKey ('[')
poza walka. Wykrywanie po symbolach parsera (skrypty NB + System\Autorun\zDismember_NB_Paths.d z paczki --nb) - na Steamie
i w Kronikach zakladek nie ma, a klawisz zostaje grze. Mapa zmiennych: [dokument projektowy - niepublikowany].
- Gildia: wybor jednej ze zdobytych (slady VALUE_G* fake guilds; NONE zawsze) - pole gildii bohatera do nastepnej zmiany
  zbroi [G] (VALUE_HERO_GUILD bez zmian, hak bandytow zdejmowany). Nigdy 9, 10, 11, 13, >= 20.
- Sciezki: wampir (z typem), zwiadowca, lowca demonow, mroczny rycerz, wojownik Adanosa, czarnoksieznik, przywolywacz,
  szaman, druid, truciciel - tylko zdobyte; lewo / prawo wlacza / wylacza znacznik (zapamietany), Enter = umiejetnosci
  sciezki (wyuczone; trwale premie z nauki - "trwala", bez przelacznika). Stan menu w zmiennych zapisu ZDF_* (opis w
  Core\PathMenu.h). Zbroja / szata wlaczajaca wylaczona sciezke - znacznik znow 0 (co 250 ms).
- Blokady z komunikatem: przemiana, rozmowa / przerywnik, joint (gildia 16 i sciezki TPL), Swiete Swiatlo (szaman), dar
  krwi Beliara (typ wampira), Wyspa Starozytnych (gildia bez statusu wody, wylaczenie AW), szaman <-> mroczny rycerz,
  umiejetnosci DK tylko przy wylaczonej sciezce DK, chowaniec tylko odwolany.
- Ochrona rozmow (PathDialogGuard): od startu rozmowy (hak oCInformationManager::SetNpc) do konca wylaczone umiejetnosci
  maja zapamietane wartosci; po rozmowie znow 0 (wyzszy poziom kupiony w rozmowie - zapamietany). "Czarna magia (nowe
  dialogi)" wraca tylko u Xardasa.
Log: "Sciezki: ..." (kazde przelaczenie: zmienne z wartosciami przed i po, odmowy z powodem, pilnowanie, ochrona rozmow).
- Sciezki niezdobyte (bez znacznika i sladu) na dole listy "- Do zdobycia -": szare, bez przelacznika, w opisie
  u kogo / jak je zdobyc (rozmowa przyjecia z dekompilacji NB) i blokady nauki; Enter / LPM = podglad umiejetnosci.
- "- Klasy gildii -": umiejetnosci gildii glownych - Mag Ognia (Parlan, spalanie), Mag Wody (Nefarius, mana i eliksiry),
  Paladyn (Albrecht), Guru (Baal Namib), Straznik Swiatynny (Gor Na Toth), Najemnik / Lowca smokow (Torlof). Czesci
  liczone w DLL dzialaja w kazdej gildii z lata patch_guildskills (build_union_abi_fix.py, 22.10; Straznik -
  patch_tplskills), czesci skryptowe NB tylko przy gildii (orki paladyna i Szczescie najemnika - RX_WEAPONDAMAGE,
  regeneracja i eliksiry Maga Wody, Skuteczne mikstury). Spokoj Guru od 22.10 w kazdej gildii: DLL z lata, skryptowe
  RX_APPLYCURSE nadpisuje system\Autorun\guildskills_isway.d (bez wymogu GIL_GUR). Plugin szuka laty w pamieci
  UNION_ABI.DLL (log "Sciezki: lata patch_guildskills ..."). Stan: "aktywna" (obecna gildia), "aktywna (latka)" (wszystko
  dziala), "czesciowo (latka)", "inna gildia" (szara - nic nie dziala), "nie zdobyta" (bez sladu VALUE_G* i nie obecna
  gildia). Enter = lista umiejetnosci z przelacznikami (ten sam model co sciezki: ZDF_SKILL_*, ochrona rozmow); opis
  kazdej zaczyna sie od tego, czy dziala w obecnej gildii. Bez przelacznika: Potega Guru (+3 do limitu nauki zaklec na stale, flagi nikt nie czyta) i wybor
  linii run Straznika (stan rozmowy Gor Na Totha). Straz (MIL), nowicjusz, Bractwo (SEK), bandyci i piraci nie maja
  wlasnych umiejetnosci (tarcze, regeneracja Vatrasa, luk - umiejetnosci ogolne, poza menu).
- Lista umiejetnosci sciezki pokazuje wszystkie: wyuczone z przelacznikiem, trwale i niewyuczone na szaro (wybieralne -
  pod lista efekt, koszt kolejnych poziomow, wymagania, nauczyciel). Koszt liczony w grze tymi samymi funkcjami NB co
  rozmowa nauki (tryb gry: Arena Koszmaru x0,7, SNC, FullSNC), dane stale z Core\PathSkillInfo.cpp (generator:
  [notatki lokalne] z dekompilacji NB 18.10, opisy efektow w effects.py).

Mysz w oknach (MenuMouse=1)
---------------------------
Gdy okno z lista jest na ekranie (medytacja w menu, okno gildii i sciezek): kursor (strzalka LO.TGA), najechanie
zaznacza wiersz, LPM = Enter / przelacznik / klik w strzalke "<" ">" wartosci / zakladke, kolko nad wartoscia zmienia
ja, gdzie indziej przewija, PPM = wstecz (podlista, zakladka Gildia / Sciezki -> Medytacja, okno pod [ - zamkniecie;
na zakladce medytacji nic). Ruch myszy nie obraca wtedy kamery ani bohatera (delty silnika zerowane w haku
ProcessInputEvents); w medytacji PPM przytrzymany z ruchem = obrot kamery orbitalnej (kursor stoi). W czekaniu,
klekaniu i wstawaniu mysz obraca kamere jak dotad.

Znaki Wiedzmina (Aard, Igni, Axii, Yrden, Quen)
-----------------------------------------------
Runy bez kregu jak paladynskie, na razie tylko z konsoli: insert ItRu_Aard (ItRu_Igni, ItRu_Axii, ItRu_Yrden,
ItRu_Quen). Czary, runy i efekty sa w skryptach (System\Autorun\zDismember_Signs*.d), cala rozgrywka w pluginie.
Sterowanie: runa w rece (tryb magii) i klawisz rzutu albo szybki Znak z mieczem w rece (zQuickSign, klawisz F).
Krotkie wcisniecie (do SignAltHold = 0,5 s) = znak zwykly, wylatuje przy puszczeniu klawisza; przytrzymanie dluzej =
tryb alternatywny (nizej). Brak many albo odnowienie (1 s) = krotki komunikat, mana zostaje. Znaki rzuca tylko bohater.
Wartosci ponizej sa dla mocy x1 - moc rosnie z maks. mana (SignPowerPerMana, opis nizej).
- Aard: stozek przed bohaterem (+-50 st., 6 m), obrazenia od lotu 15. Ludzie, orkowie i lzejsze stwory leca do tylu
  jak po Piesci Wiatru (silnik, ok. 3 m); ciezkie i odporne (ochrona przed lotem ponad 160 albo IMMUNE, model ponad
  2,3 m: troll, golem, smok) tylko sie zataczaja i sa odsuwane. Lot robi silnik; gdy cel po 0,25 s jednak nie
  polecial (np. AI zgasilo lot), plugin daje potkniecie i odsuniecie o 2,5 m. Cel musi byc widoczny: promienie z piersi
  bohatera do kilku punktow celu (niskie stwory na nierownym terenie tez); w logu kazdy pominiety NPC z powodem.
- Igni: stozek +-35 st., 6,5 m, ogien 40 i plonienie gry przez 3 s (5 zycia na sekunde - takt plonienia dostaje dodatek
  rowny ochronie celu przed ogniem, ktora silnik odejmuje od kazdego taktu; IMMUNE i woda - bez plonienia). Ludzie przez 1,5 s miotaja sie w ogniu (stan AI jak przy Deszczu ognia), orkowie i stwory sie
  zataczaja.
- Axii: cel w fokusie (do 8 m) albo najlepszy w stozku +-30 st. Poza walka (cel nie walczy z bohaterem): niepamiec
  jak czar Urok z gry - zapomina przestepstwa (tez licznik miasta / obozu), ostatnia walke i chwilowa zlosc - i (ludzie)
  trans na AxiiTranceTime x moc (20 s przy x1): stoi z rekami skrzyzowanymi, nie widzi bohatera - nie zagaduje go, nie
  zatrzymuje (straz przy bramie gornego miasta, straz w porcie, strazniki przejsc), nie reaguje na skradanie i dobyta
  bron, rozmowy z nim tez nie zaczniesz. Trafienie przerywa trans (zwykla reakcja na cios). Po transie wraca do rutyny
  bez wrogosci i nie pamieta, ze bohater przeszedl. Na ekranie "Trans: <imie> (N s)". W walce:
  oszolomienie 4 s jako stan AI Gothica (jak Lodowy blok / Sen - atak przerwany czysto, bez szarpania): ludzie losowo
  klecza albo kula sie ze strachu (AxiiStunAnis), orkowie i stwory zataczaja sie i stoja; kazde trafienie konczy
  oszolomienie, po nim cel wraca do walki. Cios bronia sieczna bohatera w KLECZACEGO = egzekucja (finisher W3 jak przy
  oszolomieniu, AxiiFinisher=1) - tylko kleczenie ma poze, z ktorej startuja finishery oszolomionego.
- Yrden: krag 5 m w miejscu rzutu na 15 s. Wrogowie w kregu sa wolniejsi (tempo x0,65 - ruch i ataki) i maja o 30%
  mniej ochrony (ostrze, obuch, przebicie, ogien, magia). Po wyjsciu z kregu, po jego koncu, przy zapisie i zmianie
  swiata wartosci wracaja dokladnie. Ponowny rzut przenosi krag. Bohater, sojusznicy i neutralni bez zmian.
- Quen: tarcza na 30 s pochlania w calosci nastepne trafienie (cios, strzala, czar) albo obrazenia od upadku. Wtedy
  peka: wrogowie do 2,5 m dostaja 10 obrazen magicznych, zataczaja sie i sa odsuwani. Ponowny rzut odnawia tarcze.
  Takty palenia nie zuzywaja tarczy. Efekt tarczy jest pilnowany co klatke: bez tarczy - zatrzymany (razem z bankami
  i orbitami), z tarcza - zawsze przy bohaterze (oderwany albo zgaszony - od nowa).

Moc znakow (maks. mana): P = 1 + min(SignPowerMax, mana max x SignPowerPerMana) - domyslnie 100 many = x1,1,
500 = x1,5, 1000 i wiecej = x2. Moc mnozy: obrazenia wszystkich znakow i trybow (tez plonienie i pekniecie Quen),
czasy (oszolomienie Axii, marionetka, plonienie Igni, krag i pulapka Yrden, tarcza Quen), odsuniecie Aard i prog
"ciezkich" (AardFlyMaxProt x P - przy mocy x2 leca tez ciezsi). Quen pochlania 1 trafienie (P < 1,5), 2 (od 1,5),
3 (od 2,0). Yrden: tempo wrogow od 0,65 (x1) do YrdenSlowMin 0,40 (x2), oslabienie ochrony od 30% do YrdenArmorMax 50%.
Koszty many bez zmian. Kazdy rzut w logu (Debug=1): "moc x1.50 (mana max 500)".

Tryby alternatywne (przytrzymanie rzutu dluzej niz SignAltHold = 0,5 s)
Do decyzji czar czeka (status SPL_STATUS_CANINVEST_NO_MANADEC - bez zdejmowania many). Tryby jednorazowe odpalaja
po SignAltHold (puszczenie juz nic nie robi), podtrzymywane trwaja, dopoki klawisz jest wcisniety i starcza many (puszczenie,
brak many albo przerwanie czaru = koniec).
Ruch w podtrzymaniu (strumien Igni, aktywna tarcza Quen): bohater chodzi i biega jak zwykle, tylko wolniej
(SignChannelMoveScale x0,6 - tempo modelu). Z runa w rece plugin pomija na ten czas obsluge klawiszy magii
(oCAIHuman::MagicMode - to ona przy trzymanej akcji blokowala ruch) i sam czyta klawisz akcji; z mieczem pod F
(zQuickSign) ruch dziala zawsze. Poza: gest Znaku Geralta z nakladki HUMANS_ZDF_SIGNS (ta sama co gesty zQuickSign) -
tylko tulow i rece, warstwa 21, zatrzymany na klatce z wyciagnieta dlonia (SignChannelFrame), nogi z normalnego ruchu;
po puszczeniu gest dogrywa sie i wygasza. Nakladke zaklada zQuickSign na postoju - bez niej podtrzymanie bez pozy.
Gest "w ziemie" (23.10, Aard i Yrden spod F): zQuickSign gra gest W3 man_geralt_aard_ground / yrden_ground (z mieczem
man_ger_sword_*_ground_rp_sword przy sztylecie i 1H, _lp_sword przy 2H) calym cialem - zamach, przyklek, lewa dlon
w ziemie, wstanie (1,12 s). Bohater stoi na ten czas w miejscu (SetMovLock - zakladana tylko wolna, zdejmowana tylko
wlasna; finisher ja przejmuje), fala / pulapka w chwili dotkniecia ziemi (eksport zQuickSign ZQS_SignGesture). Log Debug:
"Znak X: tryb alternatywny w miejscu - skutek za ... s, bohater w miejscu ... s", "Znak X: bohater w miejscu - koniec".
Pozostale Znaki (Igni, Quen, Axii) w trybie alternatywnym bez zmian.
Koniec strumienia Igni / aktywnej tarczy Quen (23.10): klip W3 man_geralt_ignii_stop / quen_stop (z mieczem
man_ger_sword_*_stop_rp_sword) - opuszczenie reki z pozy trzymania, tylko tulow i rece (nakladka HUMANS_ZDF_SIGNS,
nazwy T_LGUARD_STRETCH / T_LGUARD_CHANGELEG / T_LGUARD_SCRATCH / T_DANCE_09), zamiast dogrywania gestu od klatki
trzymania. Log Debug: "Znak X: koniec podtrzymania - klip W3 ...". Bez klipu w paczce W3 - jak dawniej.
Tryb zwykly spod F (zQuickSign 23.10, zQuickSign.ini SignCastSync=1): skutek Znaku z koncem pchniecia dloni gestu
(SignCastSyncMs=360,400,360,360,320 - Aard, Igni, Quen, Yrden, Axii, z klatek animacji W3), mana i odnowienie w tej samej
chwili; gest przerwany wczesniej (trafienie wroga, unik / przewrot / przeskok / finisher - spoznienie > SignSyncLateMs=300,
ekwipunek, zmiana runy) = bez skutku i bez kosztu, jak w W3. SignCastSync=0 = skutek przy puszczeniu F jak dawniej.
- Aard - fala wokol (AardAltCost 20 many): 360 st., promien 4 m, obrazenia x0,8 Aarda, lot / potkniecie i odsuniecie
  jak przy Aardzie.
- Igni - strumien ognia (podtrzymywany, 12 many/s): waski stozek +-20 st., 7 m, co 0,25 s ogien (30 na sekunde) i
  podpalenie (plonienie jak przy Igni), ludzie miotaja sie w ogniu. Poza: gest Igni W3 (dlon wyciagnieta do przodu),
  strumien obraca sie z bohaterem.
- Yrden - pulapka (YrdenAltCost 25 many): glif 3 m w miejscu rzutu na 30 s z 3 kulami (ladunkami). Wrog w glifie jest
  spowolniony jak w kregu Yrden; kula najblizsza wroga gasnie i wystrzeliwuje pocisk (35 obrazen magicznych, trafienie
  w chwili dolotu), w tego samego wroga najwyzej co 0,8 s. Pulapka znika po zuzyciu ladunkow albo czasu; ponowny rzut
  ja przenosi. Krag i pulapka moga dzialac naraz (spowolnienie liczy sie raz).
- Quen - aktywna tarcza (podtrzymywana, 10 many/s): pochlania wszystkie trafienia i upadek, polowe pochlonietych
  obrazen oddaje jako zycie (QuenActiveHeal; cios wrecz szacowany: bron + sila albo sila potwora, minus ochrona
  bohatera); bohater chodzi wolniej i jego ciosy nie dzialaja. Poza: gest Quen W3 (wyciagnieta dlon). Po puszczeniu,
  jesli tarcza cos pochlonela: pekniecie jak przy Quen (obrazenia, potkniecie i odsuniecie wrogow do 2,5 m).
- Axii - marionetka (AxiiAltCost 25 many): wrog walczacy z bohaterem - czlowiek, ork albo potwor - przez 15 s walczy po
  jego stronie jak przywolaniec: atakuje najblizszego wroga bohatera do 15 m (ludzie jak w walce ZS_Attack, orkowie i
  potwory ZS_MM_Attack), a bez wrogow idzie do bohatera i stoi przy nim (nie atakuje go; ciosy bohatera ignoruje). Na
  ekranie "Marionetka: <imie> (15 s)". Po czasie krotkie oszolomienie (AxiiPuppetEndStun 1,5 s) i powrot do walki z
  bohaterem. Postawa wobec bohatera i znacznik druzyny wracaja dokladnie: po czasie, przy smierci / powaleniu, przed
  zapisem (po zapisie znow marionetka), przy wczytaniu. Poza walka - zwykla niepamiec.
Zapis gry: przed zapisem plugin cofa Yrden (tempo i ochrone NPC), konczy tryb podtrzymywany (tempo bohatera wraca),
cofa postawe marionetki i konczy jej stan chodzenia za bohaterem, zdejmuje efekty tarczy, kregu i pulapki i konczy stany
oszolomienia / palenia (nazwa stanu znaku nie trafia do zapisu), po zapisie przywraca krag, pulapke (z pozostalymi
kulami), tarcze i marionetke (jej efekt trwa przez zapis - nie trafia do pliku). Wczytanie, nowa gra i zmiana swiata
koncza wszystkie znaki.
Ogien: Aard gasi, Igni (i co ok. 1 s strumien) podpala ogniska, piece, paleniska i pochodnie w uchwytach (moby
oCMobFire) w stozku znaku, w zasiegu wzroku - tymi samymi funkcjami, ktorymi gra zapala je ogniem i gasi skryptem
(OnTrigger / OnUntrigger). Zgaszone zostaje zgaszone przy zapisie, po wczytaniu ogien wraca do stanu ze swiata (stanu
ognia gra nie zapisuje - tak samo jak przy przelaczeniu skryptem). Pochodnie w rece NPC - bez zmian. SignFires=0 - wylaczone.
Ognie z efektu (wiekszosc ognisk, pochodni sciennych, kominkow i lamp w swiatach G2 - drzewa vobow z ZEN-a, nie moby):
plomien = vob z czasteczkami FIRE*.PFX / TORCH.PFX albo animowanym decalem FIRE_COMPLETE_A0. Aard ukrywa plomien z jego
grupa (dym, iskry, poswiata ZFLARE, voby uchwytu i luzne w 2 m), wylacza dynamiczne swiatlo i wycisza dzwiek ognia
(FIRE_MEDIUM, TORCH_BURN); swiatla statycznego (wypalonego w lightmapie) nie da sie zgasic - zostaje (log). Igni przywraca
to, co plugin zgasil (oryginaly na liscie, czyszczonej przy wczytaniu / zmianie swiata). Zgaszony ogien z efektu trafia
do zapisu (ukryty wizual) - po wczytaniu Igni zapala go bez pamieci: pokazuje plomien z ukrytymi dodatkami, wlacza
wylaczone swiatla dynamiczne i dzwieki ognia grupy. Efekty czarow, broni, palacych sie postaci i lawy - nigdy.
Debug=1: gdy znak nic nie zmienil, log wypisuje do 5 najblizszych vobow PFX / decali / mobow w zasiegu z powodem.
Celowanie (Gothic Free Aim i celownik F zQuickSign): z GFA w trybie magii Aard, Igni, Axii i Yrden celuja celownikiem GFA (czary
spelniaja warunki GFA: canTurnDuringInvest, canChangeTargetDuringInvest, TARGET_COLLECT_FOCUS_FALLBACK_NONE) - stozek
Aard / Igni idzie w kierunku celownika, Axii bierze NPC pod celownikiem (bez niego najlepszego w stozku wokol
kierunku celownika), krag i pulapka Yrden staja w punkcie celownika na ziemi (najdalej SignAimRange). Kierunek i punkt
licza sie jak w GFA: promien z kamery przez srodek ekranu. Strumien Igni z runa przy GFA obraca bohatera za
celownikiem. Z mieczem pod F (zQuickSign): celownik od wcisniecia F (opis przy zQuickSign nizej), strumien Igni idzie
za celownikiem (bohater obraca sie za kamera). Bez GFA i z AimOnPress=0 - jak dotad (przod bohatera, krag przy bohaterze).
zQuickSign: gesty znakow wybiera po id czaru - w system\zQuickSign.ini dopisz do list SignIgni 91, SignQuen 94,
SignYrden 93 i SignAxii 92 (Aard 90 to gest domyslny). Bez tego szybki Znak dziala, ale gest bywa nie ten.
Debug=1: kazdy rzut w zDismember.log - "Znak X: Cast" (mana, cel czaru), cele z odlegloscia i katem, HP przed / po,
lot / potkniecie, palenie (czas i tempo), oszolomienie / niepamiec, wejscia i wyjscia z kregu Yrden (tempo i ochrona),
pekniecie Quen; odmowy rzutu (mana / odnowienie) i brakujace efekty wizualne ("Znaki: brak efektu ..."). Tryby
alternatywne: "przytrzymanie X s - tryb alternatywny" / "puszczony po X s - tryb zwykly", takty strumienia Igni, pociski
i trafienia pulapki, pochloniecia aktywnej tarczy (HP przed / po), cele i koniec marionetki (przywrocona postawa).
Runda 5: kazdy takt palenia ("Igni: takt palenia X - N obrazen ognia (na takt, ochrona, skutecznie), HP a -> b"),
celowanie przy rzucie ("celowanie: Gothic Free Aim / celownik zQuickSign (F)", "kierunek z celownika", "srodek z celownika"), trans
Axii, zapalone / zgaszone ognie, zmiany z menu strojenia ("Menu: AardRange = 700").

Znaki Wiedzmina - runy i efekty
-------------------------------
Konsola Marvina: insert ItRu_Aard (ItRu_Igni, ItRu_Axii, ItRu_Yrden, ItRu_Quen). Runy na modelach run z gry: Piesc
Wiatru (Aard), Burza Ognia (Igni), Sen (Axii), Strach (Yrden), paladynska runa leczenia (Quen), z poswiata w kolorze
znaku; w opisie koszt many i wartosci domyslne z ini. Skrypty zParserExtendera w paczce (System\Autorun, UTF-8):
  zDismember_Signs.d     Parser=Game  stale SPL_Aard..SPL_Quen = 90..94, czary Spell_*, puste Spell_Cast_*, runy ItRu_*
  zDismember_SignsVfx.d  Parser=VFX   efekty SPELLFX_* (dlon przy rzucie, fala / stozek / krag / tarcza, trafienia)
  zDismember_SignsPfx.d  Parser=PFX   czastki ZDF_SIGN_* (tylko tekstury gry)
  zDismember_SignsSfx.d  Parser=SFX   dzwieki ZDF_SIGN_* (tylko pliki .wav gry)
Wyglad: Aard - bladoblekitne pierscienie powietrza i kurz w stozku; Igni - strumien ognia z iskrami, zarem i dymem;
Axii - fioletowo-zielone spirale i krazace gwiazdki nad glowa celu (poza walka: rozblysk mgly); Yrden - fioletowy glif
5 m na ziemi, pierscien czastek, iskry run biegnace po obwodzie, fale; Quen - bursztynowa banka z krazacymi zlotymi
iskrami, przy peknieciu zloty wybuch, odlamki i fala po ziemi. Efekty nie koliduja i nie zadaja obrazen (robi to plugin).
Petle (Axii nad glowa, krag Yrden, nici przy wrogu w kregu, tarcza Quen) gasi plugin; bezpiecznik w skrypcie
(emFXLifeSpan: 8 / 30 / 30 / 60 s) gasi je sam tylko wtedy, gdy plugin przestanie je prowadzic (opis nizej).
Efekty trybow alternatywnych: SPELLFX_AARD_SWEEP (fala wokol, jednorazowy), SPELLFX_IGNI_STREAM (strumien, petla do
20 s, obraca sie z bohaterem), SPELLFX_YRDEN_TRAP (sam glif 3 m, petla do 60 s) + SPELLFX_YRDEN_TRAP_ORB (jedna kula na
ladunek, petla do 60 s; plugin stawia kule na okregu 1,1 m, 0,9 m nad glifem, pierwsza przed bohaterem),
SPELLFX_YRDEN_BOLT (pocisk z kuli do wroga, ok. 26 m/s), SPELLFX_QUEN_ACTIVE (aktywna tarcza, petla do 30 s),
SPELLFX_QUEN_ABSORB (rozblysk pochloniecia), SPELLFX_AXII_PUPPET (nici nad marionetka, petla do 30 s). Petle
prowadzone przez plugin (krag, pulapka i kule, tarcza, nici Axii, efekt celu Axii, strumien, aktywna tarcza) maja
licznik bezpiecznika skryptu (emFXLifeSpan) zerowany co klatke - trwaja tyle, ile znak (moc x2, zapas many), bez
tworzenia od nowa (nowy efekt gralby od nowa dzwiek startu); bezpiecznik dziala dalej, gdyby plugin przestal je
prowadzic. Efekt dloni czaru (SPELLFX_<ZNAK> z dziecmi) plugin gasi 3 s po rzucie - zaden dzwiek rzutu nie gra w petli.
Brak instancji efektu nie psuje znaku (raz w logu "Znaki: brak efektu ...").
Sprawdzenie bez gry: py -3.12 -I anims\pipeline\check_signs_scripts.py - kompilacja gdc ze zrodlami G2 Addon, symbole
w .DAT, tekstury, dzwieki, modele run i odwolania miedzy skryptami.

Profiler i log (szukanie przycinek)
-----------------------------------
Klatka = odstep miedzy kolejnymi Game_Loop (czas calej klatki gry). Klatka dluzsza niz max(ProfSpikeMs, 2 x mediana)
daje w zDismember.log jedna linie: czas klatki, mediana, czas pluginu i najdrozsze sekcje (czas wlasny, liczba
wywolan), udzial innych pluginow z profilerem (zTimeRewind - wspolna tablica w pamieci procesu) i reszta gry, na koncu
werdykt ("przyczyna: zDismember", "czesciowo ...", "poza pluginem/pluginami"). Nadwyzka = klatka - mediana (bez
pelnej mediany - pierwsze klatki po wczytaniu, "mediana -" - cala klatka); przyczyna = plugin z co najmniej polowa
nadwyzki, czesciowo = pluginy razem co najmniej 1/4, plugin ponizej 2 ms nigdy nie jest przyczyna. Klatki > 1 s, w ktorych pluginy
zajely malo, to "przerwa" (alt-tab, okno systemowe) - poza statystykami. Wczytanie, zapis i menu nie sa klatkami.
Przyklad:
  Prof: przycinka 48.3 ms (kl. 5120, mediana 16.7, prog 33.4) | zDismember 31.2 ms: Traversal.Probe 28.9 ms (3x),
  Log 1.4 ms (6x) | zTimeRewind 0.4 ms (Recorder.Snap) | reszta gry 16.7 ms - przyczyna: zDismember
Sekcje: Loop, Moves (Moves.Start, Moves.Step), Traversal (Traversal.Buffer, Traversal.Probe, Traversal.Decide),
Traversal.Debug (debug wizualny przeskokow, w nim Traversal.Probe co 0,1 s),
Styles (Styles.Apply, Styles.Motion), Ovl.Apply / Ovl.Remove (zalozenie / zdjecie nakladki w silniku - pod Styles.Apply,
Moves.Start, przeskokami, finisherem), Preload (PreloadOverlays po zdism reload), Director (Director.Start), Daze, Gibs, Corpses, Blood, Parry (Parry.Block),
Collect, Anis.Hook (hak AdvanceAnis na kazdy model: Anis.Ragdoll, Anis.Recoil, Anis.Squash), Input.Keys (hak
klawiszy, w nim Loot.Keys), Loot (Loot.Start, Loot.Run, Loot.Scan, Loot.Hint), Hit.Before, Hit.Record, Die.Before, Die.After, Cut, Knockout, AttackFinish, Log (zapis logu).
Kod profilera: plugins\_shared\ZProf.h + ZProfCore.h (wspolny z zTimeRewind, testy w tests\test_core.cpp).
Log jest buforowany (plugins\_shared\ZLogFile.h): plik otwarty raz, zapis raz na klatke, przy wyjsciu i przy awarii;
usuniety w trakcie gry plik zaklada sie od nowa w ciagu sekundy.

Profiler probkujacy (ProfSampler=1; plugins\_shared\ZSampler.h + ZSampleCore.h): osobny watek co ok. 1 ms wstrzymuje
watek gry na kilkanascie-kilkadziesiat us, czyta EIP i kopie stosu, wznawia go i dopiero potem szuka adresow powrotu.
Po linii przycinki druga linia z udzialem probek tej klatki: modul (top 5), w nawiasie funkcje (Gothic2.exe: tabela
data\System\zDismember\g2a_funcs.txt z Union SDK Names_G2A.hpp - tools\gen_g2a_funcs.py; inne DLL: najblizszy eksport),
dla modulow spoza exe "z <funkcja exe ze stosu>" i "przez <pierwszy obcy modul na stosie>". Przyklad:
  Prof: probki 73.2 ms (52 probek): ntdll.dll 60% [NtWaitForSingleObject 55%; z oCGame::Render 58%; przez ddraw.dll 58%],
  Gothic2.exe 30% [zCResourceManager::CacheIn 20%, zCWorld::Render 6%], nvwgf2um.dll 10% [z oCGame::Render 10%]
zdism prof samples: tabele ms na klatke osobno w przycinkach i w klatkach normalnych, posortowane wg nadwyzki
(przycinka - normalna) - na gorze to, co pojawia sie wlasnie w przycinkach. Koszt probki w logu przy starcie
("Prof: probkowanie watku gry ..."). Straz: watek gry wstrzymany > 250 ms = wznowienie i probkowanie wylaczone do restartu.

Paczka prywatna zDismember_W3.vdf (animacje z Wiedzmina 3 - tylko do uzytku wlasnego, nie do wydania)
-----------------------------------------------------------------------------------------------------
Finishery parowane (bohater i ofiara zgrani jak w W3), bron jedno- i dwureczna, ofiara: czlowiek:
  W3_01..W3_10  sciecia glowy (01-04, 07, 08, 10) i pchniecia w klatke bez odciecia (05, 06)
  W3_D01..D08   z DLC: noga (D01, D02), reka (D03 prawa, D04 lewa), ramie z barkiem (D05, D06),
                glowa z wyskoku (D07), szyja (D08)
  W3_01_D, W3_02_D, W3_07_D, W3_D07_D  egzekucje oszolomionego (kleczacego) przeciwnika
Lista w grze: zdism list; wymuszenie: zdism fin <ID>_1H (albo _2H).

Uniki i przewroty (paczka prywatna zDismember_W3.vdf)
------------------------------------------------------
W trybie walki (miecz, bron dwureczna, piesci):
  V + kierunek    unik ok. 3,3 m: Q/E albo A/D w bok, S w tyl, W w przod (sam V = w tyl)
  Alt + Q/E/A/D/S przewrot ok. 5 m w bok albo w tyl; sam Alt = zwykly skok (takze z bronia)
  Alt+W, Alt      Alt+W = skok gry od razu; drugie Alt z W w ciagu 0,4 s, dopoki bohater jest nisko (odbicie
                  T_*RUN*_2_JUMP do 110 cm - cale odbicie, szczyt ok. 82 cm; poza nim do 50 cm nad ziemia; lot
                  S_JUMP / spadanie - za pozno) = skok przerwany i przewrot do przodu z biezacej pozy (vob schodzi
                  na ziemie w 0,12 s, ped skoku przechodzi w rozped przewrotu); pozniej drugie Alt nic nie robi
                  (skok trwa). Tylko w walce (miecz, bron dwureczna, piesci) - bez broni i z magia Alt to skok gry /
                  przeskok. Debug=1: kazde Alt w walce w logu z wygrana sciezka ("Ruchy: Alt w walce (bron X, stan Y)
                  - przejal przeskok / przewrot od razu / skok gry bez okna (powod)"), poza walka W + 2x Alt:
                  "przewrot w przod tylko w walce". Okno liczone ze stanu bohatera w chwili wcisniecia (silnik
                  w tej samej klatce startuje skok - dawniej okno sie nie otwieralo, log: "okno wg stanu z chwili
                  wcisniecia")
  Alt+S           przewrot w tyl (wlasny klip W3 - Geralt laduje przodem do wroga); przy szybkim ruchu w strone
                  kamery jej wygladzanie jest na czas ruchu krotsze (kamera nadaza)
                  (w biegu z W obrot A/D i mysz nie licza sie jako bok - bokiem jest wtedy Q/E)
                  Unik (klawisz uniku) bierze bok i z obrotu A/D, i z kroku w bok Q/E - takze z W
                  (W + A + unik = unik w lewo); zasada "W + obrot to nie bok" dotyczy tylko przewrotow spod Alt
Ruch idzie dokladnie pod stopami animacji (bez slizgu), z krokami i dzwiekiem; na koncu plynne przejscie do postawy
aktualnego stylu (w stylu Wiedzmina - do gardy W3).
Uniki i przewroty ida po zboczach jak chod (do ok. 50 st.), wzdluz scian sie slizgaja; gdy utkna - koncza sie plynnie.
Wrog w celowniku: unik / przewrot w bok krazy wokol niego, w tyl odskakuje, bohater caly czas patrzy na wroga; w przod
  idzie prosto (flankowanie, zajscie od tylu). Postacie nie blokuja uniku ani przewrotu (koniec wypychany obok ciala).
Mysz i klawisze obrotu w trakcie uniku / przewrotu obracaja bohatera i reszte toru (z celem tylko lekka korekta).
Blok (parada) w koncowce uniku, przewrotu, kontry albo ataku z wypadem przerywa ruch parada stylu.
Wcisniecie w trakcie ruchu jest zapamietane na 0,25 s (uniki mozna laczyc). Plugin ostrzega w zDismember.log,
gdy klawisz uniku/przewrotu jest tez przypisany do akcji gry w Gothic.ini [KEYS].
Na poczatku ruchu bohater jest nietykalny (okno w moves_w3.ini: iframes), sciany, NPC i urwiska zatrzymuja ruch.

Kontry (jak w W3, z mieczem albo bronia dwureczna, w kazdym stylu):
  blok (klawisz parady z gry) w chwili, gdy przeciwnik przed toba (do 3 m) bierze zamach na ciebie
  = Geralt odbija cios i od razu tnie (riposta po 0,6 s, wypad ok. 85 cm). Do riposty bohater jest nietykalny.
  Strona: A/Q = odbicie z lewej i ciecie w lewo, D/E = w prawo, bez kierunku losowo. Blok bez zamachu
  przeciwnika = zwykla parada. Odskok w tyl (T_xPARADEJUMPB) to zwykly odskok, chyba ze CounterOnBackstep=1.
  CounterWindow > 0: blok wczesniej niz tyle s przed trafieniem wroga = zwykla parada (log Debug=1: "za wczesnie
  na kontre"). Riposta to normalny cios bronia (obrazenia, ochrona, krew, odcinanie przy smierci).
  Czesc kontr (CounterRepelChance) to repel z W3: pchniecie barkiem (2 na 3) albo kopniak - przeciwnik traci atak,
  zatacza sie i odlatuje do tylu (bark 40 cm, kopniak 70 cm), otwarty na cios; obrazenia obuchowe: bark 50%,
  kopniak 35% z (obrazenia broni + sila). Uwaga: NPC z flaga niesmiertelnosci (np. Greg) nie traca HP od niczego -
  log podaje "niesmiertelny: TAK".
Atak z wyskoku (W3, bron 1H/2H w walce): Alt (sam albo z W - skok gry od razu) i w ciagu 0,3 s (z W 0,4 s) atak,
  dopoki bohater jest nisko = skok przerwany, wypad z wyskokiem i ciecie (Alt i atak naraz - od razu, bez skoku)
  w cel (wrog w celowniku albo najblizszy przed toba do ok. 5 m), obrazenia jak zwykly cios. Po 3 warianty na bron,
  losowane (z celem - te, ktorych wypad pasuje do odleglosci): 1H Rend / wyskok z obrotem / dlugi wypad,
  2H Rend / ciecie znad glowy / obrot z ciezkim cieciem. Zwykly skok startuje bez opoznienia (dawniej 0,22-0,3 s
  czekania na drugie Alt / atak). Debug=1: "Ruchy: skok od razu", "drugi Alt / atak po X s, wysokosc Y cm (animacje)
  -> przewrot w przod / atak z wyskoku" albo "- za pozno, skok gry trwa". Konsola: zdism move JUMPATK_F (F2, F3,
  JUMPATK2H_F...).
Atak w biegu (styl W3, bron 1H/2H): bieg do przodu i atak jak w grze - plugin przejmuje go jako ruch Geralta calym
  cialem (wypad, obrot do celu, cios, koniec w gardzie W3); wariant z daleka albo z bliska wg odleglosci celu
  (1H: rozbieg z cieciem 3 m / szybkie ciecie 1,4 m; 2H: mocny wypad 3,75 m / mocne ciecie 1,7 m). Poza stylem W3
  zostaje zwykly atak w biegu gry / zEBA.
  Kontra rusza tylko, gdy przeciwnik jest w stozku 60 st. przed toba i da sie go dosiegnac; wypad dopasowuje sie do
  odleglosci, a bohater dokreca sie do wroga az do ciosu. Za daleko = zwykla parada.
Styl walki Wiedzmina (od 60% talentu, osobno dla broni 1H i 2H):
  1H = szybkie ciecia Geralta (atak w lewo / w prawo + kombinacja 4 ciec do przodu)
  garda 1H = 1. klatka ciecia Geralta (postawa rp); garda 2H = petla gardy parady Geralta (miecz uniesiony przed soba)
  2H = mocne ciecia z rozmachem (atak w lewo / w prawo + kombinacja 6 ciec); zamach przyspieszony do tempa 1H:
       cios 0,48 s po ataku (1H 0,40 s), w kombinacji co 1,0 s (1H 0,92 s, natywne 2H mistrza 1,6-1,9 s)
  atak w biegu = ruch Geralta calym cialem (patrz "Atak w biegu" wyzej)
  blok = parady Geralta (z gornej lewej, z gornej prawej, od dolu - kolejne bloki po sobie), z postawy i do postawy
Ponizej progu walczysz stylem z gry / zExtendedBattleAnimations. Klawisz U (StyleKey) przelacza debugowo kazdy
dostepny styl dla broni w reku: natywne style Gothica, wszystkie style zEBA, Wiedzmin; kolejne U po ostatnim = AUTO.
Zasieg ciosow W3 (W3AttackAssist, W3AttackReach): gra sprawdza trafienie raz, w klatce ciosu - wrog musi byc
  w zasiegu walki (od bohatera do bboxa wroga: 60 cm + zasieg broni, np. kilof 60) i w kacie 30 st. przed
  bohaterem. Ciecie W3 dochodzi do ciosu po 0,40 s (1H) / 0,48 s (2H), natywne po 0,08-0,20 s, wiec wilk czy
  scierwojad zdazy odskoczyc. Dlatego jak w W3: w zamachu Geralt obraca sie do wroga przed soba (do 45 st.) i doskakuje
  tak, by w chwili ciosu stac 25 cm w zasiegu (do 1,3 m na ciecie, z kolizja, po terenie; blizej = bez ruchu, wrog
  wchodzacy w bohatera = lekko w tyl). Bez fokusu cios idzie w tego wroga. Do tego test trafienia ciosu W3 ma
  +40 cm zasiegu. Debug=1: przy kazdym ciosie bohatera wpis "Cios W3 #n ..." (cel, odleglosc na starcie i w chwili
  ciosu, doskok, kat, TRAFIL / pudlo z powodem, czy pomogla premia); ciosy innych stylow jako "Cios gry" - do
  porownania. Menu: "Doskok do celu" (pierwszy wiersz strony Wiedzmina); premia tylko w Gothic.ini.
Chod, bieg i sprint Geralta (W3Locomotion, W3Sprint):
  petle W3 zmieszane z cyklu wolnego i szybkiego jak w W3, w tempie Gothica (bieg 3,45 m/s, chod 1,4 m/s) -
  stopy nie slizgaja sie; przejscia z postawy i do postawy krokami. Z mieczem tylko w stylu Wiedzmina.
  Sprint: klawisz, wytrzymalosc i tempo zostaja z gry / zEBA, plugin podmienia tylko animacje (bez broni 6,9 m/s,
  z mieczem 4,7 m/s jak sprint zEBA). W3Sprint=0 - podczas sprintu zostaja animacje zEBA. Nakladki sprintu W3 leza
  na modelu na stale (zakladane na postoju); przy starcie sprintu zEBA plugin tylko przeklada je na wierzch.
  Tempo: zEBA (i Ninja Sprint, mikstura szybkosci) przyspiesza wylacznie nakladka - predkoscia korzenia jej petli
  biegu. Petla W3 gra wiec z fps przeskalowanym tak, by predkosc korzenia byla taka jak w petli sprintu gry
  (0,5..2x, inaczej tempo wlasne) - bez podwojnego przyspieszenia. Gdy sprint gry nie ma petli dla broni w reku
  (mikstura: tylko bez broni), sprint W3 z ta bronia zostaje pod spodem.
  Sprint w Steam G2 tej instalacji: dziala zEBA (lewy Shift, HUMANS_SPRINT_REMASTER). Ninja Sprint (Sprint.vdf
  z warsztatu) jest wczytany, ale przy Perma=1 i Ninja_Sprint_UseKey=0000 nie sprintuje; [NATURALSPRINT] w ini to
  pozostalosc po Nostalgic Edition. Log przy starcie: "Sprint w grze: ..."; kolizja dwoch modow na jednym klawiszu
  albo dwie nakladki sprintu naraz = "Sprint: UWAGA ..." z podpowiedzia, co wylaczyc.
Zestawy ruchu do testow (klawisz O = LocoKey, Shift+O wstecz, konsola zdism loco, menu "Zestaw ruchu (test)"):
  AUTO (wg W3Locomotion/W3Sprint) -> Gothic / zEBA (bez nakladek ruchu W3) -> W3 trucht i chod (sprint gry)
  -> W3 trucht + sprint W3 w sprincie gry -> W3 sprint jako zwykly bieg (podglad petli sprintu) -> AUTO.
  Po kazdej zmianie (i co 0,8 s po zdism loco watch) napis na ekranie: zestaw, z jakiej nakladki gra teraz petla
  S_RUNL i petla broni w reku (np. S_1HRUNL: W3 trucht, Gothic, sprint zEBA), jaki sprint gry jest na modelu i tempo
  sprintu W3 (x1,00 = jak sprint gry).
Przeskoki i wspinanie w biegu (parkour W3, Traversal=1; System\zDismember\traversal_w3.ini, nakladki
HUMANS_ZDF_MV_TRAV_*):
  bieg do przodu (W) + klawisz skoku gry (Alt) przed przeszkoda = Geralt sie na nia wspina albo ja przeskakuje.
  Wspiecie: gora 22-355 cm nad podstawa sciany (klipy climb_run 30/100/150/200/250 cm; 250-355 cm to klip 250
    z wyzszym odbiciem - W3 nie ma wspiecia z biegu wyzej), plaska, z miejscem na postac (ok. 1,85 m nad gora i za
    krawedzia) i dosc gleboka (30 cm: ok. 0,7 m - bohater biegnie dalej po niej; wyzsze 0,4-0,8 m). Gora moze byc do
    80 cm za sciana za zaokraglona / pochyla warga (nasyp: do ok. 50 st.) - bohater staje na plaskiej gorze. Na koncu
    stoi na gorze; z W trzymanym od chwili wstania - prosto w bieg.
  Przeskok: cienka gora (do 0,6-1 m wg wysokosci; ploty z okraglych i cienkich zerdzi tez), za nia ziemia od 1,2 m
    nizej niz podstawa do 25 cm pod gora (parapet przed wyzszym tarasem) - klipy vault_run z ladowaniem w bieg; z W
    biegnie dalej, bez W zatrzymuje sie.
    Platforma bez pasujacego klipu wspiecia, ale cienka i z ziemia za nia - przeskok.
  Szukanie sciany idzie po terenie (krok 50 cm: ziemia lokalna i promienie co 10 cm od 10 do 110 cm nad nia) - stok
  przed murem, stopnie i drobne kamienie nie myla; promien w dol trafiajacy wierzch zerdzi / slupka (skok terenu
  i znow nizej) to przeszkoda, nie teren; wysokosc liczona od ziemi u podstawy sciany, rozbieg toru idzie po terenie.
  W buforze szukanie co klatke tylko w okolicy znanej sciany, pelne przed samym startem.
  Gzyms / wystajace kamienie pod krawedzia nie psuja wykrycia; profil sprawdza do 4 m (log podaje prawdziwa gore).
  Klip wg wysokosci (kubelki: do 65 cm = 30, do 125 = 100, do 175 = 150, do 225 = 200, wyzej = 250) i nogi podpartej
  w biegu (lf / rf jak w W3); wysokosc toru dopasowana do prawdziwej gory w chwili odbicia, rozbieg do odleglosci
  sciany. Wczesne wcisniecie (bufor): sciana w zasiegu (predkosc x 1 s + rozbieg klipu, do 8 m - takze w sprincie)
  = Alt jest polykany, bohater biegnie dalej i ruch rusza w oknie startu klipu (do 1,2 s); puszczone W, zwolnienie,
  skret ponad ok. 35 st., za dlugo albo przeszkoda okazuje sie zla = skok gry w tej chwili (wg zasady nizej). Sciana
  pod katem do ok. 35 st. - bohater obraca sie do niej. Murek przy innej przeszkodzie (jeden bok gory zasloniety)
  wystarcza, gdy gora jest gleboka na co najmniej 60 cm. W walce (TraversalCombat=1) przeskok ma pierwszenstwo przed
  przewrotem Alt+W, Alt i atakiem z wyskoku tylko wtedy, gdy przed bohaterem jest pasujaca przeszkoda. Na czas ruchu
  bohater nie ma kolizji (vob prowadzony po torze; promien sprawdza droge - gdy cos weszlo na tor, ruch sie przerywa
  i bohater spada jak w grze). Debug=1: kazde wcisniecie Alt w logu (stan bohatera, teren, sciana, profil, gora,
  decyzja; bufor - start i koniec czekania).
  Z miejsca / z wolnego chodu (bez broni): Alt przed sciana do 1,3 m (szukanie do 2,2 m) = wspiecie albo przeskok W3
    z klipow climb_idle / vault_idle (30, 100, 150, 200, 250 i prawdziwe 300 cm - klip o najblizszej wysokosci, do
    355 cm); tor dochodzi do sciany (klipy W3 zakladaja 26-57 cm od niej) i obraca bohatera do niej. Gdy wykrywanie nic
    nie znajdzie albo sciana dalej - zwykly skok gry (zawis i podciaganie Gothica). Debug: "WSPIECIE Z MIEJSCA 250 ->
    CLIMB_IDLE_250". Dane: anims\pipeline\make_traversal.py (klipy z man_geralt_exploration.w2anims, zrzuty
    w anims\work\w3trav; --report = sama analiza; sekcje CLIMB_IDLE_<h> / VAULT_IDLE_<h> ze start=IDLE).
  Wykrywanie: roslinnosc (wizual voba / tekstura z alfa o nazwie drzewa, krzaka, bluszczu, lisci) nie jest sciana ani
  gora - promien idzie dalej; wysoka przeszkoda (ponad 60 cm) musi miec sciane co najmniej z jednej strony +-32 cm
  od osi (pien, slup - odrzucone); obiekt okragly (snopek, beczka - boki dalej niz plaszczyzna z normalnej) - bez
  obrotu do normalnej; koniec ruchu musi miec podparcie (ziemia / gora pod punktem konca w kierunku po obrocie) -
  inaczej przeskok albo nic; skarpa o poszarpanej scianie bez ziemi za nia to sciana, nie uskok. Za krawedzia moze
  byc pochyla powierzchnia do chodzenia (dach, stok do ok. 45 st.): gora = krawedz, koniec wspiecia na wysokosci
  dachu w punkcie konca (do 90 cm wyzej / nizej, podparcie sprawdzane); "sciana za krawedzia" tylko przy pionowej /
  stromej przeszkodzie na wysokosci ciala (log: "gora ... pochyla - dach / stok", "glebokosc ... (pochyla, ...)").
  Skok gry po odmowie (bez pasujacego klipu, za blisko / za daleko, brak gory): skok w biegu Gothica to stala
  animacja T_RUNL_2_JUMP (ok. 2,9 m, szczyt ok. 80 cm; z akrobatyka ok. 4,7 m, ok. 40 cm) - nigdy nie chwyta krawedzi,
  a sciana w locie = odbicie do tylu; silnik sam go anuluje, gdy jego promien (ze srodka postaci, 4 m do przodu,
  ok. 37 st. w gore) trafia sciane. Dlatego plugin: przepuszcza skok gry, gdy przed bohaterem nie ma sciany, skok
  wyladuje przed nia albo przejdzie nad przeszkoda (stopy w locie ponad krawedzia, niska do 50 cm - postac na nia
  wejdzie) albo gdy petla biegu juz nie gra (zatrzymywanie - silnik zrobi skok z miejsca); przy scianie z krawedzia
  w zasiegu podciagania Gothica (do 6 m) - bohater biegnie dalej do sciany (dopoki trzyma W) i przy niej (80 cm) robi
  skok z miejsca gry (zawis / wspiecie Gothica, jak Alt na postoju; gdy silnik nie widzi krawedzi - wspiecie /
  przeskok W3 z miejsca, a bez klipu zwykly skok gry klawiszem, bez zawisu); w pozostalych
  razach (krawedz za wysoko) Alt nic nie robi - zamiast odbicia od sciany. Z bronia w reku Alt zawsze dla gry / unikow.
  Start skoku gry w biegu albo spadania potrafi w jednej klatce cofnac bohatera do pozycji sprzed ok. 0,3 s (log:
  -157 / -206 cm) - plugin przywraca pozycje sprzed klatki ("Skok gry: cofniecie o X cm ... - poprawione"); fizyka
  po zderzeniu w locie odbija bohatera do tylu - predkosc w tyl (wzgledem biegu przy odbiciu) przycinana do 50 cm/s
  ("odbicie fizyki - predkosc w tyl ... przycieta"), a dryf w tyl ponad 60 cm w 0,3 s cofany ("odbicie fizyki X cm
  w tyl ... - poprawione"). Niska przeszkoda (ponad 30 cm) tuz przed bohaterem albo stos skrzyn (koniec wspiecia na
  wyzszej skrzyni) - skok gry nie jest juz przepuszczany (wlecialby w nia).
  Runda 9: cofniecia o 2-4 m to ruch korzenia animacji (zCModel::vobTrans) z niespojnych klatek petli biegu - silnik
  przesuwa o niego voba w kazdej klatce. Straznik cofa dokladnie taki skok (ponad 60 cm + 1000 cm/s x czas klatki)
  w dowolna strone, takze na ziemi i po przeskoku ("Skok korzenia animacji: X cm ... - cofniety; animacje: ..."); krok
  w locie nigdy szybciej niz limit pedu 850 cm/s - poprawka cofniecia nie dodaje kroku na kroku, powrot silnika do
  przodu przyciety ("Skok gry: krok X cm w klatce ponad limit pedu ... - przyciety"). Po przeskoku (kolizja byla
  wylaczona) stan kolizji postaci (podloga / sufit) liczony od nowa w nowej pozycji. Debug=1: przez 0,6 s po koncu
  przeskoku co klatke "Po ruchu ...: y, krok, stan, fizyka, kolizja, nad podloga, podloga kolizji, korzen"; kazdy
  inny skok pozycji ponad 150 cm w klatce - "Skok pozycji X cm w klatce bez skoku korzenia"; Alt bez sciany -
  "bez sciany - promienie przed bohaterem: 40 / 90 / 140 cm" (co trafia, w tym voby pomijane filtrem cdDyn).
  Debug=1, runda 10: przez ostatnie 1,2 s przeskoku co klatke "Przeskok <ID> [kl. N, t s]: vob (od startu), silnik od
  naszego celu X cm, BIP01 wzgl. voba, animacje na wezle BIP01 (waga, klatka), kamera <tryb>: pozycja wzgledem
  bohatera, cel kamery" - te same pola w "Po ruchu ..." (0,6 s po koncu).

  Runda 12: wyjscie w bieg (W od chwili exit) dopiero, gdy tor jest na wysokosci konca ruchu ("wyjscie w bieg czeka,
  stopy X cm od wysokosci konca") i ze stopami dosunietymi na gore przed wlaczeniem kolizji - dawniej kolizja
  wypychala bohatera ze skrzyn 1,8-4,2 m do tylu; przez 0,6 s po ruchu skok pozycji bez ruchu korzenia jest cofany
  ("... - przywrocone (okno po naszym ruchu)", wpis podaje tez, sprzed ilu sekund byla pozycja docelowa). Debug=1:
  "Przesuniecie bohatera X cm: <funkcja> z <modul/adres>" - kto ustawil pozycje bohatera (SetPositionWorld /
  SetTrafoObjToWorld / Move / EndMovement z wynikiem kolizji) i stos wywolan. Koniec ruchu (z biegu i z miejsca)
  musi miec miejsce na stojaca postac (nawis / mur nad koncem - "brak miejsca na koncu ruchu"). Na pochylym dachu
  nad inna plaska gora profil idzie po dachu (wyzsza ciagla powierzchnia). Chwyt w locie a skok gry: sciana za
  przepascia (podstawa > 60 cm nizej) z krawedzia w zasiegu dloni (do wzrostu + szczytu lotu + 45 cm) - skok gry
  dozwolony ("chwyt w locie dokonczy skok"); bez chwytu - bez skoku (dawniej dobieg do sciany wpadal w przepasc).
  Spadanie (runda 13, FallForgiveness=1, FallForgiveDrop=40): silnik zaczyna spadac, gdy podloga pod samym srodkiem
  jest nizej niz krok (zMV_STEP_HEIGHT) - szczelina miedzy skrzyniami, dziura przy kamyku. Plugin sprawdza podloge
  na obwodzie ciala (8 punktow, 30 cm) i 30 cm przed bohaterem: gdy cialo ma podparcie najwyzej FallForgiveDrop cm
  nizej (w ruchu: przod i 2 punkty obwodu, bez ruchu: 5 punktow) - bez spadania (najwyzej 0,4 s pod rzad); prawdziwa
  krawedz spada jak w grze. Debug=1: "Spadanie wybaczone: ...", "Spadanie: start - ... obwod ...", "Spadanie:
  ladowanie po X s, spadek Y cm, animacje ...".
  18.10 (log NB): (1) TeleportGuard=1 - silnik (rozwiazanie kolizji w zCVob::EndMovement z DoFrameActivity) potrafi
  przy zwyklym ruchu w klatce przeniesc bohatera o 1,5-4 m do pozycji sprzed 0,3-0,6 s (start skoku w biegu przy
  scianie, start spadania, wlaczenie kolizji po przeskoku); plugin cofa taki wynik do pozycji z poczatku klatki od
  razu w haku, przed renderem (najwyzej 20 klatek pod rzad) - "Teleport kolizji: zadane X cm, kolizja przeniosla o Y cm
  - cofniety ...". (2) Korekty pozycji (straznik lotu, skok korzenia, skok pozycji po ruchu, teleport kolizji, ped,
  wybaczanie spadania) sa wstrzymane, gdy model bohatera jest zamrozony (zTimeRewind cofa czas) albo przez 0,35 s po
  przestawieniu bohatera z zewnatrz (inny DLL, ResetPos, BeamTo, Enable, konsola, mob): "Przestawienie bohatera z
  zewnatrz ...", "Korekty pozycji wstrzymane / wznowione", "Korekta pozycji pominieta - ...". Tempo modelu przy
  zamrozeniu (23.10): szybszy cios po idealnym uniku czeka (koniec ciosu przy timeScale 0 nie zdejmowal x boost, a
  zTimeRewind oddawal je potem na stale), powrot tempa Yrdenu / podtrzymania znaku odlozony do odmrozenia ("Znaki: ...
  - model zamrozony (cofanie czasu), powrot tempa ... po odmrozeniu", "Znaki: ... - model odmrozony, tempo ..."). (3) Straznik lotu
  poprawia tylko skok z biegu (start T_*RUN*_2_JUMP), a poprawke cofnieta przez silnik w nastepnej klatce konczy
  ("poprawka X cm cofnieta w nastepnej klatce - bez dalszych poprawek w tym skoku"). (4) W trakcie animacji przeskoku
  klawisze ruchu (W/S/A/D, boki, skok, chod, skradanie) sa dla gry puszczone (poza wyjsciem w bieg); kolizja wlaczona
  w trakcie toru przez kogos innego jest wylaczana ponownie ("kolizja wlaczona w trakcie ruchu ... - wylaczona
  ponownie" z wywolujacym). Wpisy "Przesuniecie bohatera" maja pole "zrodlo: nasza korekta (powod) / nasz kod /
  zewnetrzne (kto) / silnik".

  Ped skoku (JumpKeepMomentum=1): animacja skoku gry z biegu niesie ok. 370 cm/s, sprint 700-850 - pozioma
  predkosc odbicia jest utrzymana w calym skoku (faza animacji: dosuniecie z kolizja; faza fizyki: predkosc ciala
  sztywnego tylko do brakujacej wartosci - bez dublowania tego, co nada silnik / wtyczka JumpAcrobaticsNoFps), po
  ladowaniu wygasa w 0,2 s; sciana w locie konczy ped. Zasieg skoku w werdykcie i luk na debugu licza z predkoscia
  (sprint 830 cm/s: ok. 6 m zamiast 2,7). Takze z bronia w reku (T_1HRUNL_2_JUMP itd.). Wpisy: "Skok gry: ped X cm/s",
  "Skok gry: ped - dodano X cm".
  Chwyt w locie (TraversalAirGrab=1, bez broni, tylko z W trzymanym i bez S): w skoku gry albo spadaniu krawedz
  znaleziona przez silnik na wysokosci dloni (od 45 cm pod glowa do 45 cm nad nia, co najmniej 110 cm nad stopami) =
  natywny zawis Gothica (jak przy wychodzeniu z wody); dalej podciaganie, z S - puszczenie. Chwyt, gdy dlon jest
  najwyzej 70 cm od krawedzi (albo bohater juz sie do niej nie zbliza); poza S_HANG wchodzi z pozy lotu w 0,22 s,
  a vob co klatke tak, by dlonie biezacej pozy byly na krawedzi (dochodzenie ease-out od pozycji chwytu) - dlonie na
  krawedzi od chwytu, bez opadania w polowie; do konca dosuwania (0,3 s) zawis trzymany, potem podciaganie Gothica.
  Debug=1: wpisy "chwyt w locie: krawedz X cm ..., dlon Y cm", "dosuniety w X s - vob / dlon wzgledem krawedzi"
  i co klatke przez 2 s "Chwyt [kl. N, t s]: vob y, dlon y wzgl. krawedzi, warstwa 1, ruch, cialo, fizyka".
  Debug=1: wpis z werdyktem
  ("skok gry: ...", "bez skoku: ...", "skok z miejsca: ..." - krawedz, stopy w locie przy scianie, promien silnika,
  zasieg), parametry skoku z modelu (zasieg z animacji, promien, GetJumpUpHeight) i sledzenie: pozycja wzgledem
  wcisniecia po 0,1 / 0,3 / 0,6 / 1,0 s, stan ruchu, fizyka, animacje warstwy 1, najwiekszy krok w tyl, krawedz
  znaleziona przez silnik.
  Debug wizualny (zdism trav show / hide, TraversalDebug=1): co 0,1 s pelne wykrywanie przed bohaterem i rysunek
  w swiecie; w lewym gornym rogu linia stanu (predkosc, bieg / sprint, zasieg szukania, bufor: czeka / ruch),
  decyzja "teraz: ..." (co zrobilby Alt w tej chwili) i co zrobi skok gry. Wcisniecie Alt zamraza obraz decyzji na
  5 s ("Alt: <decyzja>", ta sama linia w logu "Przeskok debug: Alt: ..."). Legenda:
    turkusowy krzyzyk i strzalka - bohater (ziemia pod nim) i kierunek biegu
    biale kropki i szara linia - probki terenu po drodze do sciany
    czerwone linie - promienie trafione (do punktu trafienia), zielone - wolne (szukanie sciany z ostatniego kroku,
      skok terenu, nad gora i za sciana); turkusowe + to wierzch zerdzi / ziemia za nia
    fioletowa pionowa linia - sciana (od podstawy do gory) z napisem "SCIANA <cm>, kat <st.>", pozioma i + -
      podstawa ("podstawa +X")
    x / . na scianie - profil nad podstawa (x zablokowany, . wolny)
    zolta podwojna linia (szerokosc barkow) - gorna krawedz, tu chwyta: "GORA <wys. nad podstawa> (podstawa +X)";
      zolte ^ - trafienia gory, zolte o - glebokosc gory, pomaranczowe o - spadek za gora
    turkusowe s - boki gory; pomaranczowa | - grubosc ("grubosc X"); jasnoniebieskie v - ziemia za przeszkoda
      ("ZIEMIA ZA +X")
    niebieskie okno na ziemi - okno startu klipu (od 1,2 do 0,55 sciany klipu), > - start z czekania
    pomaranczowe K - koniec ruchu (gdzie bohater stanie, po obrocie do sciany), napis "KONIEC"; pomaranczowa kreska
      w dol w punkcie konca - promien podparcia (czerwona = brak podparcia - ruch odrzucony)
    czerwone / zielone linie z boku na wysokosci sciany - szerokosc (+-32 cm: trafia / obok); "okragla" przy SCIANA -
      obiekt okragly, bez obrotu
    "WSPIECIE Z MIEJSCA 250 -> CLIMB_IDLE_250" - z miejsca (okno startu: 30-130 cm od sciany)
    biala decyzja nad sciana: "WSPIECIE 150 -> CLIMB_150_RF", "PRZESKOK 100 -> VAULT_100_LF", "BRAK: <powod>"
    szara linia od srodka postaci - promien silnika skoku w biegu (czerwona do sciany = silnik anuluje skok);
      luk lotu skoku gry: zielony = przepuszczony, rozowy = wstrzymany (X - uderzylby w sciane), zloty = skok
      z miejsca (zawis / wspiecie Gothica)
    w rogu: "bufor: dobieg do krawedzi X cm" - bieg do sciany przed skokiem z miejsca gry; zloty napis "chwyt w locie:
      krawedz X cm" - ostatni chwyt w locie (3 s)
  Linie rysuje zrenderer->DrawLine (rzut kamery jak napisy); gdy pod rendererem ich nie widac - zdism trav kropki
  (linie z kropek - napisy zCView sa widoczne zawsze, takze pod GD3D11).
Gesty Znakow w zQuickSign (osobny plugin, klawisz F): gest Geralta tylko na tulow i rece (nogi biegna dalej),
  zalezny od czaru - ogien = Igni, leczenie / swiatlo / teleport / przemiana = Quen, przywolania i obszarowe = Yrden,
  sen / strach / urok = Axii, reszta = Aard; czar wylatuje w chwili pchniecia dloni (0,32 s, Axii 0,2 s).
  Na postoju gest calym cialem Geralta (stopy w miejscu, dlon do przodu niezaleznie od postawy pod spodem,
  nakladka HUMANS_ZDF_SIGNS_STAND); w ruchu tylko tulow i rece (nogi biegna dalej); ruszenie w trakcie gestu
  przelacza go plynnie na wariant ruchu.
  Ustawienia w system\zQuickSign.ini: SignGestures, SignIgni/SignQuen/SignYrden/SignAxii (id czarow), SignCastMs.
  Gesty siedza w jednej nakladce HUMANS_ZDF_SIGNS (warstwa 21) zakladanej na postoju (podmienia losowe animacje
  bohatera przy krzesle, pile, naprawie, piecu i jedzeniu - zalozenie i zdjecie nakladki zatrzymuje w silniku
  wszystkie animacje postaci). Rzut nie zatrzymuje skoku ani sprintu.
  Celowanie od wcisniecia F (zQuickSign.ini AimOnPress=1, domyslnie; zastepuje AimWithMouse z rundy 5 - bez LPM):
  na srodku ekranu celownik czarow (tekstura Gothic Free Aim AimReticle=GFA_RETICLECIRCLECROSS.TGA, rozmiar z Gothic.ini
  [GFA] reticleSizePx), bohater obraca sie za kamera, klawisz akcji (LPM) nie atakuje. Puszczenie F = rzut w punkt
  celownika: zwykle czary leca do NPC pod celownikiem (gdy to dobry cel czaru) albo w trafiony punkt, czar natychmiastowy
  czeka w rece do puszczenia F (szybkie stukniecie tez leci w punkt celownika, bohater obraca sie po rzucie); znaki
  zDismember biora kierunek, cel Axii i punkt Yrden z tego samego promienia (eksport ZQS_AimActive). Celownik tylko dla
  czarow z kierunkiem (runy celujace w fokus, Aard / Igni / Axii / Yrden); Quen, leczenie, swiatlo, przywolania - bez
  celownika, ale tez rzut przy puszczeniu F. Znaki: przytrzymanie F dluzej niz SignAltHold (0,5 s) = tryb alternatywny;
  pod celownikiem pasek ladowania od wcisniecia F do progu, w chwili przejscia w tryb alternatywny celownik i pasek
  robia sie pomaranczowe (stan z eksportu zDismember ZDISM_SignHoldState). Fala Aard wokol i aktywna tarcza Quen - celownik
  i pasek znikaja; strumien Igni i pulapka Yrden (w punkcie celownika) dalej celuja. AimOnPress=0 - jak przed runda 5
  (rzut przy wcisnieciu, cel z fokusu, przytrzymanie = tryb alternatywny znaku, bez celownika).
Bron a odcinanie: tna tylko bronie z obrazeniami od ostrza; obuch (typ BLUNT albo BluntWeapons, np. maczugi, mloty,
  Inkwizytor, Kostur sedziego) nie odcina i nie ma finisherow. Rapiery (RapierWeapons, RapierThrustOnly): finishery
  tylko pchnieciem przez klatke (W3_05, W3_06), bez odcinania. Oszolomiony (kleczacy) przeciwnik z rapierem:
  egzekucja W3_05_D / W3_06_D - ofiara wstaje, Geralt przebija ja przez klatke jak w W3 (z innymi bronmi tnacymi
  te pchniecia sa w puli obok sciec).
Kody klawiszy DirectInput: 42 lewy Shift, 29 lewy Ctrl, 56 lewy Alt, 57 spacja, 47 V, 45 X, 44 Z, 46 C.

Tor testowy (zdism trav tor)
----------------------------
  zdism trav tor [m]   stawia przed bohaterem (w kierunku patrzenia, start m metrow dalej - domyslnie 2) tor
                       przeszkod z modeli gry w 4 rzedach co 5,5 m, ok. 57 m dlugosci, 6 m rozbiegu przed kazda.
                       Od lewej: stopnie 30, 60, 100, 150 i stopnie 200, 250, 300, 350 cm (blok z plyt 100x30,
                       skrzyn portowych 120x100, muru 350x300 i poklad z plyt za nim - gora 6 m gleboka), rodzaje:
                       cienki murek 107 cm (23 cm gruby), plot z zerdzi (45 / 90 cm, slupki 105), beczka (okragla),
                       waski slup 40 cm (ma byc odrzucony), nawis 35 cm za krawedzia skrzyn 100 (ma byc odmowa), stos
                       skrzyn z portu (nierowna gora), skoki: dach 30 i 45 st. za krawedzia 200 cm, przepasc 250 cm
                       (platformy 100 po 6 m), chwyt w locie (platforma 100 z 8 m rozbiegu, przepasc 250, mur
                       z krawedzia 200 nad platforma i poklad 4 m na nim). Wysokosci z bboxow modeli (napis przy
                       przeszkodzie: nazwa i wysokosc, do 35 m od bohatera); pelna lista z odleglosciami, modelami
                       i oczekiwanym wynikiem w zDismember.log ("Tor przeszkod: ...").
  zdism trav tor usun  usuwa tor (sam znika tez przy wczytaniu gry i zmianie swiata).
  Komenda sprawdza pas: nierowny teren (odchylka > 150 cm albo skok > 60 cm miedzy probkami co 2 m) i cos w pasie
  (promienie na 50 / 150 cm) - ostrzezenie w konsoli i lista w logu; przeszkody stoja na ziemi kazda z osobna.
  Przeszkody to zwykle voby z kolizja (bohater, NPC i wykrywanie przeskokow je widza), nie trafiaja do zapisu gry
  (dontWriteIntoArchive). Stawiac w pustym miejscu - NPC na swoich trasach moga sie o nie zaczepiac.


Miecze na plecach (jak u Geralta)
---------------------------------
Bron 2H (i 1H przy Back1H) wisi na plecach w miejscu pochwy Geralta i jest dobywana / chowana animacjami z Wiedzmina 3
(przyspieszonymi), takze w biegu. Bron 1H przy pasie - animacja z gry w tempie BeltDrawSpeed. Drugi miecz: w ekwipunku
zaznacz bron biala i nacisnij G - wisi na plecach obok pierwszej (w miejscu luku); G poza ekwipunkiem zamienia bronie
(z dobyta bronia: schowanie, zalozenie, dobycie), zalozenie drugiego miecza z ekwipunku tez zamienia je miejscami.
Sloty broni gry sie nie zmieniaja (zapis, skrypty, inne mody widza bron jak zawsze) - plugin tylko przestawia wezly
ZS_SWORD / ZS_LONGSWORD / ZS_BOW modelu bohatera. Drugi miecz zapisuje sie z gra (System\Autorun\zDismember_Back.d).
Log: "Miecze na plecach: ..." w zDismember.log. Animacje: anims\pipeline\make_draw.py (tylko prywatnie).
Bronie parowe New Balance: polozenie i dobycie natywne NB (animacje 1x2 / 2x2 dobywaja obie bronie), zamiana z drugim
mieczem odrzucana (zmiana pary w ekwipunku). Straznik (23.10): przestawiony wezel sprawdzany wzgledem BIP01 SPINE2 po
prawdziwej hierarchii modelu - niezgodny (inny szkielet / przebudowany model) = natywny slot w tej klatce i wpis
"STRAZNIK". Diagnostyka co 10 klatek: "UWAGA ... w renderze N cm od BIP01 SPINE2" (miecz poza plecami w macierzach
renderu) i "lewa dlon ... bez kluczy animacji" (przedmiot w ZS_LEFTHAND, ktorego animacja nie ustawia - np. lewa bron
pary NB grana animacjami innymi niz NB).

Szybki strzal z kuszy (jak u Geralta)
-------------------------------------
Strzal z zalozonej kuszy bez przechodzenia w tryb walki dystansowej: z mieczem w prawej rece albo bez broni, takze
w biegu. Kusze wybiera sie pod F w zQuickSign (Shift + kolko myszy - "Kusza: <nazwa> (belty: N)" za ostatnim Znakiem;
Shift + 4..0 wraca do runy). Stukniecie F = bohater siega po kusze z plecow (lewa dlon), na postoju obraca sie do celu
(fokus albo najlepszy wrog w stozku kamery, XbowAuto*), belt leci ok. 0,5 s po wcisnieciu, kusza wraca na plecy.
Przytrzymanie F (zQuickSign XbowHoldMs, 0,3 s) = celownik i petla celowania, bohater obraca sie za kamera, puszczenie =
strzal w punkt celownika (albo w NPC pod nim), PPM = anulowanie bez strzalu. Belt to zwykly pocisk silnika (oCAIArrow) -
obrazenia z kuszy i beltu (CopyDamage), szansa z talentu kuszy, trafienie, reakcje skryptow, przestepstwa i XP jak przy
zwyklym strzale: na czas startu pocisku i trafienia tryb broni bohatera udaje kusze (fmode 6, kusza w slocie prawej
reki), miecz zostaje w dloni. Przeladowanie wedlug talentu (XbowCooldownMs), osobne od odnowienia Znakow.
Przerwanie przed beltem (trafienie, unik / przewrot / przeskok / finisher, ekwipunek, zmiana broni, dialog, PPM) = bez
strzalu, bez zuzycia beltu i bez przeladowania; po belcie unik i reszta przerywaja tylko animacje. Odmowy z komunikatem:
brak kuszy / beltow, luk, pochodnia, tryb 5-7, woda, przeladowanie. Animacje: klipy kuszy Geralta (aim_start_rp,
aim_idle_rp, shoot_additive_center + aim_end_rp; anims\pipeline\make_xbow.py, tylko prywatnie) w dwoch wariantach
(postoj - cale cialo, ruch - tulow i rece), dlon chwyta kusze na plecach za przod (blisko luku, srodek plecow - tam,
dokad siega dlon W3; IK ramienia tylko domyka tor) i trzyma ja za ten chwyt, kolba przy celowaniu pod ramieniem. PPM
w celowaniu = odlozenie kuszy animacja (bez strzalu). Belt: tor ustawiany przez plugin od razu z wylotu (bez
oCAIArrow::SetupAIVob - free-aim New Balance stawial belt gdzie indziej). Nazwy animacji
sa pozyczane na czas strzalu od gestow Znakow (zero nowych nazw; wpisy wracaja po strzale, przy zapisie i wczytaniu).
Kusza na czas strzalu jest w lewej dloni (wezel ZS_LEFTHAND), sloty gry sie nie zmieniaja; drugi miecz z "mieczy na
plecach" i kusza nie gryza sie (rozne wezly). Wybor slotu zapisuje sie z gra (System\Autorun\zDismember_Xbow.d,
ZDF_XBOW_SLOT). Log: "Kusza: ..." w zDismember.log (strzal, belt, tor, trafienie z trybem napastnika, raport po 2 s).
Spec: [dokument projektowy - niepublikowany].

Menu kolowe szybkiego wyboru (Tab)
----------------------------------
Przytrzymanie Tab (WheelKey=15) dluzej niz WheelHoldMs (250 ms) otwiera kolo jak w W3: czas zwalnia do WheelSlowmo
(15 %), kamera i bohater stoja (gra nie widzi klawiszy logicznych, ruch myszy nie trafia do kamery), kierunek myszy od
srodka wybiera pole (martwa strefa WheelDeadZone, czulosc WheelMouseSens), rolka zmienia strone, puszczenie Tab
zatwierdza (LPM tez przy WheelClickConfirm=1), PPM / Esc anuluje. Krotkie stukniecie Tab = ekwipunek: Tab wraca do
bufora klawiszy gry po puszczeniu (opoznienie rowne dlugosci stukniecia; Backspace otwiera od razu), wiec blokady NB / KM
i Hedin dzialaja jak dotad. Zawartosc: wszystkie czary ksiegi bohatera (runy, zwoje x N, Znaki W3 w kolorze znaku) w
kolejnosci slotow, 7 na strone + kusza przypieta na dole kazdej strony (bez kuszy 8 czarow); [F] = aktywna pozycja.
Srodek: nazwa, rodzaj, linie opisu przedmiotu (koszt many), krag, odnowienie; kusza: belty, przeladowanie. Wybor tylko
ustawia pozycje pod F (zQuickSign ZQS_Select, jak Shift + kolko), F rzuca / strzela jak dotad. Kolo nie otwiera sie przy
otwartym oknie gry (konsola, ekwipunek, dialog, dokument, pauza), oknie zDismember (medytacja, sciezki), nowym oknie
innego pluginu (callback wejscia przed gra spoza linii bazowej z wczytania - np. okienko paska czarow NB), przerywniku,
mobie, finiszerze / przeskoku / uniku, szybkim rzucie i strzale z kuszy, cofaniu czasu (eksport ZTR_RewindActive, gdy
jest; zapas: tryb magii z trzymana akcja) - wtedy Tab idzie od razu do gry. Czas: hak zCWorld::AdvanceClock - tempo
mnozone przez factorMotion innych (NB, konsola, finisher), czas klatki z ulamkiem (bez zamrazania swiata przy wysokim fps),
przywracany przy zapisie, wczytaniu, pauzie, dialogu i smierci. Ten sam modul: XbowAimSlowmo (% tempa w petli celowania
z kuszy, 100 = wylaczone). Menu Union: strona "Kolo wyboru" (wiersz 12 strony glownej). Konsola: zdism wheel [info|open|
close]. Log: "Kolo: ..." (otwarte / zatwierdzone / anulowane / odrzucone / przerwane / odmowa / Tab stukniecie, statystyki
czasu przy zamknieciu). Spec: [dokument projektowy - niepublikowany].

Od 23.10 wieczorem klawisz kola to Caps Lock (WheelKey=58; Tab wrocil w calosci do gry - szybki teleport NB 3 + Tab).
Silnik czyta przelacznik Caps Lock Windows jako chod (oCAIHuman::PC_SlowMove 0x69A350, GetKeyboardState) i sam go
wylacza (SetCapsLock 0x694FE0, PC_Sneak 0x69A790 - keybd_event). Przy kole pod Caps Lock (WheelCapsPatch=1) dwa skoki w
pamieci (0x69A39B, 0x69A7DA: 74 -> EB) - chod tylko z GAME_SLOW, Caps Lock gra nie widzi; stukniecie Caps Lock =
przelacznik chodu pluginu (WheelTapWalk=1, GetState(GAME_SLOW) = 1), po kazdym puszczeniu przelacznik Windows wraca do
stanu sprzed wcisniecia (WheelCapsRestore=1, syntetyczne wcisniecie polykane). Wcisniecie: bufor gry albo zapas z
GetAsyncKeyState (log "bez zdarzenia w buforze gry"). Klawisz wspolny z unikiem / przewrotem (Kroniki: DodgeKey=58) -
w walce dla ruchow, kolo poza walka.

Wlasne finishery
----------------
Narzedzia: anims\pipeline\make_finisher.py (FBX z Mixamo -> animacja Gothica -> wpis w finishers.ini).
Animacje Mixamo: pobieranie w formacie FBX Binary, 30 fps, keyframe reduction: none.

Autor: LukePizon2104. Animacje finisherow: Mixamo (Adobe).
