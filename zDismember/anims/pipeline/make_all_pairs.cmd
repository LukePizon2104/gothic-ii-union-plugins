@echo off
rem Wszystkie finishery parowane z Wiedzmina 3 -> data_w3 (TYLKO UZYTEK PRYWATNY, animacje CD Projekt RED).
rem Cel i okno ciosu ustalone z analizy kontaktu ostrza (szyja = sciecie, klatka/brzuch = pchniecie bez odciecia).
rem --dazed-from: dodatkowy wariant egzekucji oszolomionego (_D) od klatki, w ktorej ofiara juz kleczy.
rem Pominiete: man_finisher_sword_v2_rp_remaster (animacje bohatera 3,3 s i ofiary 4,7 s sie nie skladaja) i
rem man_finisher_sword_v3_lp_remaster (ostrze mija lezaca ofiare o ponad 30 cm). Okna i cele z osia ostrza +Z (r_weapon).
setlocal
if not defined PY set "PY=py -3"
cd /d "%~dp0"
%PY% -I make_pair.py W3_01 man_finisher_01_rp --target neck --dazed-from 20 || exit /b 1
%PY% -I make_pair.py W3_02 man_finisher_02_lp --target neck --window 20,35 --dazed-from 21 || exit /b 1
rem W3_03/04 to pchniecia w klatke (ostrze wbite od kl. ok. 16 / 12 do 38 / 40): cios = wyciagniecie ostrza obok szyi,
rem bez odciecia, krew z rany w klatce (21.10: wczesniej cut=HEAD - pchniecie konczylo sie scieciem glowy)
%PY% -I make_pair.py W3_03 man_finisher_03_rp --target neck --window 25,42 --cut NONE --wound "BIP01 SPINE2" || exit /b 1
%PY% -I make_pair.py W3_04 man_finisher_04_lp --target neck --window 30,44 --cut NONE --wound "BIP01 SPINE2" || exit /b 1
rem --dazed-rise: pchniecia W3_05/06 nie maja kleczenia - ofiara wstaje z S_PRAY, bohater z postawy walki wchodzi w zamach W3
%PY% -I make_pair.py W3_05 man_finisher_05_rp --target chest --window 16,22 --dazed-rise || exit /b 1
%PY% -I make_pair.py W3_06 man_finisher_06_lp --target chest --window 16,21 --dazed-rise || exit /b 1
%PY% -I make_pair.py W3_07 man_finisher_07_lp --target neck --window 30,40 --dazed-from 25 || exit /b 1
rem W3_08: pchniecie w gardlo po jelec (ostrze w szyi od kl. 16) - bez odciecia, krew z szyi
%PY% -I make_pair.py W3_08 man_finisher_08_lp --target neck --window 13,18 --cut NONE --wound "BIP01 NECK" || exit /b 1
%PY% -I make_pair.py W3_10 man_finisher_aard_01_rp_remaster --target neck --window 30,45 || exit /b 1
rem DLC (dlc16): finishery z odcinaniem - nogi, reka, tulow (=reka z barkiem), glowa, szyja
%PY% -I make_pair.py W3_D01 man_finisher_dlc_legs_lp --set dlc16 --target leg_r --window 11,15 || exit /b 1
%PY% -I make_pair.py W3_D02 man_finisher_dlc_legs_rp --set dlc16 --target leg_r --window 8,16 || exit /b 1
%PY% -I make_pair.py W3_D03 man_finisher_dlc_arm_lp --set dlc16 --target arm_r --window 20,30 || exit /b 1
%PY% -I make_pair.py W3_D04 man_finisher_dlc_arm_rp --set dlc16 --target arm_l --window 20,30 || exit /b 1
%PY% -I make_pair.py W3_D05 man_finisher_dlc_torso_lp --set dlc16 --target arm_r --window 12,18 || exit /b 1
%PY% -I make_pair.py W3_D06 man_finisher_dlc_torso_rp --set dlc16 --target arm_l --window 12,18 || exit /b 1
%PY% -I make_pair.py W3_D07 man_finisher_dlc_head_rp --set dlc16 --target head --window 33,42 --dazed-from 25 || exit /b 1
%PY% -I make_pair.py W3_D08 man_finisher_dlc_neck_rp --set dlc16 --target neck --window 18,28 || exit /b 1
