# SkateTune

Autotune efekt (pitch korekcia s nastaviteľným Key/Scale/Retune Speed), ktorý
pri každej väčšej a rýchlej korekcii tónu ("retune speed peak") pustí na
obrazovke nový skejt trik - meno triku, skóre, combo counter, aj malý
"retune" meter. Formáty: **VST3** + **Audio Unit** (AU, potrebné pre Logic
Pro - Logic natívne VST3 nepodporuje, len AU).

## Čo je reálne overené a čo nie

Toto som písal v Linuxovom cloud sandboxe, takže macOS/Xcode/Logic Pro
odtiaľto fyzicky nemám. Čo som ale overil naozaj, nie len "malo by to
fungovať":

- **DSP jadro** (pitch tracking, kvantizácia do stupnice, retune speed
  smoothing, pitch shifter, detekcia "trikového" peaku) - 16 automatických
  testov (`tests/dsp_standalone_test.cpp`), vrátane testu, ktorý odhalil a
  opravil reálny bug (retune speed bol 40x pomalší, než mal byť, kým som to
  nenašiel).
- **Samotný .vst3 plugin** - skompiloval sa čisto (0 warningov) a spustil
  som ho cez skutočné JUCE VST3 hostovanie (presne tou istou cestou, akou ho
  načíta DAW): parametre sa ukázali správne (Key/Scale/Retune
  Speed/Mix/Output Gain), 2.4s testovacieho audia prešlo bez NaN/Inf/klipnutia.
- **AU build a vizuál GUI** - toto nespustím na Linuxe (AU ide skompilovať
  len na macOS; obrazovku pluginu som sa pokúsil odfotiť aj cez virtuálny
  X server, ale VST3 GUI hosting protokol to vyžaduje viac než môj testovací
  harness robí, takže screenshot sa mi spraviť nepodarilo). Kód kreslenia
  používa len základné, veľmi bežné JUCE Graphics volania, takže by to malo
  vyzerať tak, ako je navrhnuté - ale prvé skutočné vizuálne overenie bude až
  v Logic Pro / inom DAW na Macu.

## Krok za krokom: dostať z tohto hotový .pkg installer (bez Macu, bez Xcode)

1. Na [github.com](https://github.com) si vytvor (alebo prihlás sa do)
   účet, a vytvor nové **prázdne** repo (napr. `skatetune`), bez README.
2. Na stránke repa klikni **"uploading an existing file"** (alebo "Add
   file" → "Upload files") a pretiahni tam **celý obsah** tohto priečinka
   (zachová sa aj štruktúra podpriečinkov pri drag&drop celého priečinka).
   Commitni rovno do `main`.
3. Nahratie automaticky spustí build (priečinok `.github/workflows`). Ak
   nie, choď do záložky **Actions** v repe a spusti workflow "Build macOS
   installer (VST3 + AU)" tlačidlom **Run workflow**.
4. Počkaj ~10-15 minút (prvý build sťahuje a kompiluje celý JUCE framework
   od nuly). Keď job dobehne so zeleným ✔, klikni naň a dole v sekcii
   **Artifacts** stiahni `SkateTune-macOS.zip`.
5. V tom zipe nájdeš:
   - `SkateTune-Installer-1.0.0.pkg` - dvojklikový installer (toto pošli
     kamošovi).
   - `SkateTune-VST3-raw.zip` / `SkateTune-AU-raw.zip` - surové bundly pre
     ručnú inštaláciu (drag&drop do priečinkov nižšie), ak by installer
     robil problémy.

## Inštalácia na Macu (pre kamoša)

**Cez installer (odporúčané):** rozbaliť, dvojklik na
`SkateTune-Installer-1.0.0.pkg`, preklikať sprievodcu. Nepýta admin heslo -
inštaluje len do jeho vlastného účtu:
- `~/Library/Audio/Plug-Ins/VST3/SkateTune.vst3`
- `~/Library/Audio/Plug-Ins/Components/SkateTune.component`

**Ak macOS ukáže "nemožno overiť vývojára"** (installer nie je podpísaný
Apple certifikátom - bežné pri nezávislých pluginoch): System Settings →
Privacy & Security → dole sa objaví hláška o blokovanom súbore → **Open
Anyway** → potvrdiť ešte raz. Alebo rýchlejšie, v Termináli pred otvorením:
```
xattr -dr com.apple.quarantine ~/Downloads/SkateTune-Installer-1.0.0.pkg
```

Po inštalácii reštartovať Logic Pro (ak bol otvorený).

## Používanie

Vlož ako Audio FX na stopu s hlasom/nástrojom v Logic Pro (AU) alebo
hociktorom VST3 hostiteľovi. Nastav **Key** + **Scale**, **Retune Speed**
(vyššie = rýchlejší/"robotickejší" snap), **Mix**, **Output Gain**. Skejťák
na obrazovke dostane nový trik vždy, keď efekt musí poriadne a rýchlo
"strhnúť" tón do stupnice.

## O JUCE frameworku (licencia)

Tento projekt je postavený na [JUCE](https://juce.com) (9.0.5), ktorý je
dual-licencovaný: **AGPLv3** (zdarma, ak je tvoj zdrojový kód tiež
verejný/zdieľaný pod AGPLv3 - presne tento prípad, zdieľanie s kamošom na
testovanie s otvoreným zdrojom) alebo platená komerčná JUCE licencia
(potrebná len keby si toto niekedy chcel predávať ako zatvorený komerčný
produkt). Pre toto hobby/zdieľané použitie netreba nič platiť. Viac:
https://juce.com/legal/juce-9-licence/

## Štruktúra projektu

```
Source/               - plugin (DSP + GUI)
Source/dsp/           - pitch tracking, kvantizácia, shifter, trick detektor (bez JUCE, čisté C++)
Source/gui/           - skejťák, triky, HUD
tests/                - automatické testy DSP jadra (+ voliteľný dev host test)
installer/            - skript, čo balí .pkg
.github/workflows/    - CI, čo to celé zbuildí na macOS a vyrobí installer
```
