# Temat projektu

W hali odpraw czekają osoby z bagażem podręcznym i próbują wejść na prom, który może pomieścić `P` pasażerów, dla których ustalony dopuszczalny ciężar bagażu podręcznego wynosi `Mp`.
Z uwagi na wymogi bezpieczeństwa ustalono następujące zasady:

- Najpierw odbywa się odprawa biletowo-bagażowa. Pasażer, którego bagaż waży więcej niż dopuszczalny limit `Mp`, zostaje wycofany do hali odpraw;
- Wejście na prom możliwe będzie tylko po przejściu drobiazgowej kontroli, mającej zapobiec
  wnoszeniu przedmiotów niebezpiecznych.
- Kontrola przy wejściu jest przeprowadzana równolegle na 3 stanowiskach, na każdym z nich
  mogą znajdować się równocześnie maksymalnie 2 osoby.
- Jeśli kontrolowana jest więcej niż 1 osoba równocześnie na stanowisku, to należy
  zagwarantować by były to osoby tej samej płci.
- Pasażer oczekujący na kontrolę może przepuścić w kolejce maksymalnie 3 innych
  pasażerów. Dłuższe czekanie wywołuje jego frustrację i agresywne zachowanie, którego
  należy unikać za wszelką cenę.
- Po przejściu przez kontrolę bezpieczeństwa pasażerowie oczekują na wejście na pokład
  promu w poczekalni.
- W odpowiednim czasie obsługa (kapitan portu) poinformuje o gotowości przyjęcia pasażerów
  na pokład promu.
- Pasażerowie są wpuszczani na pokład poprzez trap o pojemności `K` (`K`<`P`).
- Istnieje pewna liczba osób (VIP) posiadająca bilety, które umożliwiają wejście na pokład
  samolotu z pominięciem kolejki oczekujących (ale nie mogą ominąć trapu).

Prom wyrusza w podróż co określoną ilość czasu `T1` (np.: pół godziny). Przed rozpoczęciem podróży kapitan promu musi dopilnować aby na trapie nie było żadnego wchodzącego pasażera. Jednocześnie musi dopilnować by liczba pasażerów na promie nie przekroczyła `P`. Ponadto prom może wypłynąć przed czasem `T1` w momencie otrzymania polecenia (_sygnał1_) od kapitana portu.

Po odpłynięciu promu na jego miejsce pojawia się natychmiast (jeżeli jest dostępny) nowy prom o takiej samej pojemności `P` jak poprzedni, ale z innym dopuszczalnym ciężarem bagażu podręcznego `Mp`. Łączna liczba promów wynosi `N`, każdy o pojemności `P`.

Promy przewożą pasażerów do miejsca docelowego i po czasie `Ti` wracają do portu. Po otrzymaniu od kapitana portu polecenia (_sygnał 2_) pasażerowie nie mogą wsiąść do żadnego promu - nie mogą wejść na odprawę biletowo-bagażową. Promy kończą pracę po przewiezieniu wszystkich pasażerów.

Napisz programy symulujące działanie kapitana portu, kapitana promu i pasażerów. Raport z przebiegu symulacji zapisać w pliku (plikach) tekstowym.

# Testy

- pasażerowie o losowych płciach i losowym bagażem, przychodzący na odprawę w miarę równomiernie (przypadek 'realistyczny')
- pasażerowie z nadwyżką mężczyzn/kobiet (test stanowisk kontrolnych),
- duża ilośc pasażerów czekających na odprawę (test czy puszcza max 3 osoby)
- duża ilość VIPów (czy są wpuszczani poza kolejką i czy ich duża ilość psuje działanie symulacji)
- różne pojemności `P`, ilości `N` i czasy `T1` promów (jak zachowuje się sytuacja kiedy pasażerów jest dużo mniej/więcej niż pojemność promów, promów jest mało itd.)

# Repozytorium

https://github.com/Zachaly/projekt_so

# Uruchomienie
Skrypt `compile.sh` służy do kompilacji wszystkich potrzebnych programów, należy uważać z otwieraniem i zapisywaniem tego pliku na Windowsach, nadpisze to końce linii, przez co nazwy plików wynikowych zostaną zniekształcone