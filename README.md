## PROJEKT: ZDALNE ZAMYKANIE SYSTEMÓW OPERACYJNYCH

## Opis

Program składa się z części Klienta oraz Serwera. Umożliwia zarządzanie Klientami i Administratorami, a także komunikację pomiędzy nimi.


## Klient

### Struktura zapytania:

- `client_id`: Unikalny identyfikator Klienta.
- `action`: Akcja którą Klient chce wykonać.
- `action_1_client_id` and `action_2_client_id`: Dodatkowe identyfikatory które mogą być wymagane w zależności od akcji.

### Funkcje Klienta:

- `sendRequest(int sockfd, const Request& request)`: Tworzy zapytanie i wysyła je do Serwera przez socket.
- `receiveThread(void* arg)`: Odbiera wiadomości z Serwera w oddzielnych wątkach i reaguje na nie.

### Role użytkowników:

- **Client**: Domyślna rola Klienta.
    - Może wyłączać innych Klientów (pod warunkiem że posiada uprawnienia).
  
- **Administrator**: Domyślnie ta rola jest przypisana do Klienta z identyfikatorem `client_id=1`.
    - Może dodawać nowego Administratora.
    - Może przyznawać Klientom uprawnienia do wyłączania innych Klientów.
    - Może wyświetlać listę wszystkich Administratorów.
    - MOże wyłączać innych Klientów (pod warunkiem że posiada uprawnienia).

### Menu:

- Dodawanie nowego Administratora.
- Nadawanie uprawnień Klientowi.
- Wyświetlanie aktywnych Klientów.
- Wyświetlanie wszystkich Administratorów.
- Wyjście z aplikacji.

### Komunikacja:

- Klient jest podłączony do Serwera poprzez socket.
- Klient wysyła zapytania do Serwera używając struktury zapytania (patrz punkt Struktura zapytania).

## Serwer

### Struktura zapytania:

- Podobnie jak dla Klienta, Struktura zapytania jest używana do przesyłania informacji pomiędzy Klientem a Serwerem.

### Funkcje Serwera:

- `parseRequest(const char* buffer)`: Konwertuje zapytanie Klienta z bufora na odpowiednią Strukturę zapytania.
- `deleteClientFromActiveClientsRegistry`: Usuwa Klienta z listy aktywnych klientów.
- `addAdminId`: Dodaje nowego Administratora do listy adminisratorów.
- `addPermissionForUser`: Nadaje uprawnienia Klientowi.
- `shutdownClient`: Wysyła polecenie wyłączenia Klienta i obsługuje odpowiedzi.

### Uwierzytelnianie:

- Serwer weryfikuje uprawnienia Klienta przed wykonaniem poszczególnych akcji (np. dodaniem nowego Administratora, nadaniem uprawnień).

### Komunikaty i Logi:

- Serwer generuje kolorowe komunikaty które pomagają śledzić i rozumieć wykonywane akcje.
- Serwer informuje Klienta o występujących błędach.

### Wielowątkowość:

- Serwer obsługuje wielu Klientów równocześnie, każdego w osobnym wątku.
- Wątki są tworzone i zarządzane dynamicznie.

## Podsumowanie:
Program zapewnia prosty system do zarządzania wieloma Klientami i Administratorami oraz komunikowania się między nimi równocześnie z wykorzystaniem Serwera.

## Uruchamianie:

Aby skompilować projekt, należy użyć poniższego polecenia:

```bash
g++ server.cpp -o run-server
g++ client.cpp -o run-client
```
Aby uruchomić Serwer:
```bash
./run-server
```
Aby uruchomić klienta z konkretnym identyfikatorem:
```bash
./run-client <client_id>
```
Domyślnie Klient z identyfikatorem id=1 posiada uprawnienia Administratora.

