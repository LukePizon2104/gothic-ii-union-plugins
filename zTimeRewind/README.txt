zTimeRewind - runa "Piaski Czasu" (Gothic II NotR, Union)

Trzymaj akcje z runa w rece: swiat cofa sie na twoich oczach (do 30 s). Puszczasz - gra toczy sie dalej.
RewindScope=1 (domyslnie): cofa sie wszystko jak w PoP - swiat i stan skryptow (dialogi, questy, dziennik,
przestepstwa) z migawek co StateInterval; mana wydana na rune zostaje wydana. RewindScope=0: tylko swiat fizyczny.
Ratunek przed smiercia (AutoRewindOnDeath): z runa w plecaku i mana na start smierc bohatera zamienia sie w samo
cofniecie o AutoRewindSeconds.
Zwrot zuzytych rzeczy (Restore*): zwoje, mikstury, jedzenie i strzaly/belty zuzyte po chwili docelowej wracaja do
plecaka (HP z nagrania, przyrost many z oddanej mikstury jest zabierany). Rejestr z hakow sciezek zuzycia silnika
(rzut czaru, zniszczenie przedmiotu w rece po uzyciu, natychmiastowe uzycie, strzal) - handel, dawanie, wyrzucenie,
kradziez i skrypty nie sa zuzyciem, wiec nic nie wraca podwojnie. Bez zwrotu: zuzycie w rozmowie / cutscence / przy
mobie (gotowanie), efekt trwaly (atrybuty max, sila, zrecznosc, ochrona, doswiadczenie, nakladka animacji jak
mikstura szybkosci, przyzwana postac), strzala podniesiona / wbita / zniszczona, ponad ubytek netto od chwili
docelowej (zdobyte pozniej tej samej instancji). Kod: Consume.cpp + Core\Consume.*; konsola tr inv.
Zwoj z ksiegi mierzony takze wprost (amount przedmiotu ksiegi w Spell_Cast), liczniki obejmuja ksiege czarow i spakowany
plecak (packString). Zwrot many (RestoreMana): mana wydana na czary po chwili docelowej (pomiar many bohatera wokol
Spell_Invest / SetReleaseStatus / Spell_Cast, bez runy cofania i bez strumieni NB) wraca do max many; przyrost many
z oddanych mikstur odchodzi w tym samym rachunku. Log: "Mana: zwrot wydanej na czary +N ...".
Odciecia zDismember (22.10): maska odcietych czesci postaci (glowa, rece, nogi) jest w migawkach (tor zmian, Cuts.cpp
+ Core\Cuts.h); w przewijaniu czesci wracaja, gdy kursor minie chwile odciecia (ragdoll zwlok znika, gdy minie smierc),
przy Commit postac wstaje cala - zDismember zdejmuje sciskanie kosci, odlamki, krew z kikutow, ragdoll, nakladki ofiary
finishera / oszolomienia (przed ResetPos). Kanal: pamiec nazwana procesu plugins\_shared\ZCutLink.h (zDismember publikuje
funkcje, zTimeRewind je wola); bez zDismember nic sie nie zmienia. Log: "Odciecia: migawka / podglad / przywrocenie ...",
w zDismember.log "Cofniecie czasu (zTimeRewind ...): ...". Nieprzytomny w chwili docelowej, a teraz martwy zostaje
martwy - z odcieciami.

Paczka: Data\zTimeRewind.vdf
  System\Autorun\zTimeRewind.dll
  System\Autorun\zTimeRewind_Rune.d   (runa i czar, zParserExtender, cp1250)
  System\Autorun\zTimeRewind_Menu.d   (menu "Union & Pluginy", UTF-8)
  System\Autorun\Menu\zUnionMenu.d   (wspolna baza menu Union - ta sama co w zDismember)
Menu: Opcje -> Union & Pluginy -> zTimeRewind (zmiany dzialaja po wyjsciu z menu).

Runa: konsola (tryb Marvina) -> insert ItRu_TimeRewind
Konsola: tr stats | tr rewind <s> | tr cfg | tr prof [reset|on|off] | tr combat (relacje postaci z fokusu) | tr inv (zuzyte przedmioty w buforze)

Gothic.ini [ZTIMEREWIND]:
  BufferSeconds=30      ile sekund wstecz
  SampleInterval=0.1    odstep nagrywania
  RewindSpeed=3.0       predkosc przewijania
  ManaStart=10          koszt rzucenia
  ManaPerSecond=5       mana za sekunde cofniecia
  MinRewindSeconds=3    krotkie klikniecie cofa co najmniej tyle
  SpellID=99            slot czaru 0..255 (rowny SPL_TimeRewind w zTimeRewind_Rune.d; przy niezgodnosci wygrywa skrypt)
  PuppetAnims=1         0 zastygla poza, 1/2 animacje od tylu
  CastFx=SPELLFX_TELEPORT
  HardResetDistance=50  od ilu cm przesuniecia postac dostaje AI od nowa
  RewindScope=1         1 wszystko (swiat + stan skryptow), 0 tylko swiat fizyczny
  StateInterval=1       odstep migawek stanu (s)
  KeepGlobals=ANIACCELERATION,RUNACCELERATION,ADDITIONALACCELERATION,ACCELERATIONACTIVE,ACCELERATIONRATIO,RX_STAMINAZEROSLOW,SPRINT_ACTIVE,RX_SPEEDPOTION
                        zmienne skryptow poza cofaniem (przecinki, '*' na koncu = przedrostek). NB trzyma tempo
                        bohatera w zmiennych skryptu, ale dodaje / odejmuje je z DLL (UNION_ABI, lista efektow poza
                        skryptami) - cofniecie samej zmiennej zostawialo bohatera szybszego / wolniejszego na stale
                        (23.10). Log: "State: KeepGlobals bez cofania ...", "State: cofniete zmienne (N): ...".
                        Tempo modelu marionetek: po cofnieciu wraca tylko nasze zamrozenie (0); wartosc wpisana w trakcie
                        przez inny modul zostaje. Log: "Rewind: tempo bohatera - start / przy oddaniu / 1 s po oddaniu".
  AutoRewindOnDeath=1   ratunek przed smiercia (wymaga runy w plecaku i many na start)
  AutoRewindSeconds=10  o ile cofa ratunek
  RestoreCombat=1       relacje i walka: attitude/tmp_attitude, npcType, enemy, stan AI i aivary walki zawsze;
                        przy RewindScope=1 takze aivary przestepstw, newsy i postawy gildii
  CombatStates=ZS_ATTACK,...   stany AI walki/reakcji - postac w takim stanie, a wtedy w innym, wraca do rutyny
  CombatAivars=AIV_ATTACKREASON,...   aivary walki (nazwy stalych ze skryptow; indeksy z parsera, Steam i NB)
  CrimeAivars=AIV_NPCSAWPLAYERCOMMIT,...   aivary przestepstw i skutkow walki (tylko RewindScope=1)
  RestoreScrolls=1      zwrot zwojow zuzytych po chwili docelowej cofniecia
  RestorePotions=1      zwrot mikstur (przyrost many z oddanej mikstury zabierany, HP i tak z nagrania)
  RestoreFood=1         zwrot jedzenia
  RestoreAmmo=1         zwrot strzal / beltow (lezaca w swiecie strzala znika, wystrzelona w locie wraca)
                        menu: "Zwrot zuzytych rzeczy" wyl. / zwoje / wszystko / z ini (reczny zestaw Restore*)
  RestoreMana=1         zwrot many wydanej na czary po chwili docelowej (do max many; koszt runy cofania zostaje)
  Menu*=                pozycje menu Union (plugin przelicza je na wartosci po wyjsciu z menu)
  Debug=0
  Profiler=1            lekki profiler: przycinki do logu, podsumowanie na zadanie (tr prof); 0 = wylaczony
  ProfSpikeMs=25        przycinka = klatka dluzsza niz max(tyle ms, 2 x mediana ostatnich 120 klatek)
  ProfSummarySeconds=0  podsumowanie profilera do logu co tyle sekund (0 = tylko na zadanie)

Profiler (szukanie przycinek): przycinka -> jedna linia "Prof: przycinka ..." w zTimeRewind.log (czas klatki,
mediana, czas pluginu, najdrozsze sekcje, udzial zDismember i reszty gry, werdykt). tr prof = podsumowanie od
resetu (tabela sekcji w logu), tr prof reset / off / on. Sekcje: Loop, Recorder (Recorder.Snap, Recorder.Trim,
StateSnap.Capture z .Globals/.Log/.Told, StateSnap.Relist), Rewind (Rewind.Begin, Rewind.Frame, Rewind.Commit,
StateSnap.Apply, Mobs.Revert, Consume.Restore), Consume.Probe, Freeze.Pose, Spell, Spell.Hook, Spike, Mobs.Hook, Fx.Render, Log.
Kod: plugins\_shared\ZProf.h (wspolny z zDismember).
Nagrywanie: postacie zdespawnowane (zostaja na liscie NPC swiata, ale poza swiatem) nie sa probkowane - ich model
zwolniony przez silnik nie jest odbudowywany (oCNpc::GetModel = UnShrink, 10-20 ms na postac). Log: "Recorder:
pierwsza probka" po wczytaniu, "Recorder: wolna probka" gdy tick > 2 ms; tr stats pokazuje ostatni tick.
Lista zmiennych zapisu (pierwsza migawka stanu po wczytaniu) budowana porcjami w kolejnych tickach.
Log jest buforowany (plugins\_shared\ZLogFile.h): plik otwarty raz, zapis raz na klatke, przy wyjsciu i przy awarii.

Budowanie: build.cmd (gra zamknieta) = VDF w Data\ + usuniecie luznych plikow; build.cmd novdf = luzne pliki
w system\autorun do szybkich testow (VDF z Data\ usuwany). Skladanie paczki: build_vdf.py [--install].
Testy logiki: tests\build_tests.cmd (w tym wspolny _shared\ZSpellSlotsCore.h)

New Balance (GOTHIC2_NB_DIR): build_vdf.py --nb [--dll ...] [--out ...] -> zTimeRewind.vdf z DLL jako
System\Autorun\NB_UNION_PLUGIN_DIFF_02.DLL (biala lista UNION_ABI), runa z SPL_TimeRewind = 201, bez
Menu\zUnionMenu.d (baza menu jest w Union.vdf). Gothic.ini NB: [ZTIMEREWIND] SpellID=201. NB ma 196 czarow
(ID 0..195, 99 = UnlockChest), wiec plugin wydluza tablice SPELLFXINSTANCENAMES / SPELLFXANILETTERS / TXT_SPELLS
(_shared\ZSpellSlots.h, tylko w pamieci). Stan trzymany w DLL NB (karma, timery, ognik) sie nie cofa.
