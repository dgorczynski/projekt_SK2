#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

//Struktura reprezentująca żądanie do serwera
struct Request {
    int client_id;
    string action;
    int action_1_client_id;
    int action_2_client_id;
};

//Funkcja wysyłająca żądanie do serwera
void sendRequest(int sockfd, const Request& request) {
    // Konwertowanie struktury Request na string i wysyłanie danych przez socket
    string request_str = to_string(request.client_id) + " " + request.action + " " + to_string(request.action_1_client_id) + " " + to_string(request.action_2_client_id);
    send(sockfd, request_str.c_str(), request_str.length(), 0);
}

// Wątek odpowiedzialny za odbieranie danych z serwera
void* receiveThread(void* arg) {
    int sockfd = *(int*)arg; //deskryptor gniazda
    char buffer[1024] = {0}; //bufor do odbierania danych z serwera
    ssize_t bytes_received; //liczba otrzymanych bajtów

    while (true) {
        // Odbieranie danych z serwera
        bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0); //zapisanie odebranych danych do bufora
        if (bytes_received == -1) { //sprawdzenie czy wystąpił błąd przy odbiorze danych z serwera
            perror("Error receiving data");
            close(sockfd); //zamknięcie gniazda
            pthread_exit(NULL); //zakończenie bieżacego wątku
        }
        string server_response(buffer); //tworzenie obiektu string na podstawie danych otrzymanych z serwera
        // Obsługa specjalnej komendy "SHUTDOWN" - próba zatrzymania aplikacji klienta
        if(server_response == "SHUTDOWN") { //sprawdzanie czy otrzymane polecenie to próba zatrzymania aplikacji klienta
            string accept_shutdown = "SUCCESS";
            send(sockfd, accept_shutdown.c_str(), accept_shutdown.length(), 0);
            sleep(1);
            system("kill -9 $(ps -o ppid= -p $$)"); //zakończenie procesu o id równym procesowi klienta
            // system("shutdown -P now");
        }

        if (bytes_received > 0) { //sprawdzanie czy otrzymano jakiekolwiek dane z serwera
            buffer[bytes_received] = '\0'; //ustawienie wartości null na końcu bufora
            cout << "Server response: " << buffer << endl; //wyświetlenie zawartości bufora na ekran
            memset(buffer, 0, sizeof(buffer)); //czyszczenie bufora
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) { //sprawdzanie czy program został uruchomiony z odpowiednią liczbą argumentów
        cerr << "Usage: " << argv[0] << " <client_id>" << endl;
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //tworzenie gniazda ipv4, typu strumieniowego
    if (sockfd == -1) {
        perror("Error creating socket");
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; //użycie ipv4
    server_addr.sin_port = htons(8881); //wybór portu serwera
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr)); //konwersja adresu na postać binarną

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) { //nawiązywanie połączenia z serwerem
        perror("Connection failed");
        close(sockfd);
        return 1;
    }

    int client_id = stoi(argv[1]); //przypisanie id klienta podanego jako parametr wywołania programu do zmiennej
    string action; //tworzenie zmiennej do przechowywania wyboru akcji którą klient chce wykonać
    char buffer[1024] = {0}; //inicjowanie bufora i zerowanie go

    // Tworzenie wątku do nasłuchiwania poleceń serwera
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receiveThread, (void*)&sockfd) != 0) { //tworzenie nowego wątku
        perror("Failed to create receive thread");
        close(sockfd);
        return 1;
    }

    // Rejestracja klienta po nawiązaniu połączenia
    Request request{client_id, "REGISTER", -1, -1};
    sendRequest(sockfd, request);
    sleep(1);
    while (true) {
        
        cout << "\nSELECT ACTION:\n" << //wyświetlanie menu dostępnych akcji
        "  -FOR ADMIN:\n      [1.] Add new admin ID\n      [2.] Add permission for client\n      [3.] Show active clients\n      [4.] Show all admins\n" <<
        "  -FOR CLIENTS:\n      [5.] Shutdown client\n" <<
        "  [6.] Exit application\n";
        getline(cin, action);

        if (action == "exit") break;
        
        // DODAJ NOWEGO ADMINA
        if (action == "1") 
        {
            string new_admin_id_str;
            cout << "Enter new admin ID: ";
            getline(cin, new_admin_id_str); //pobranie id
            try {
                action = "ADD_ADMIN_ID";
                int new_admin_id = stoi(new_admin_id_str); //konwersja wprowadzonego id na liczbę całkowitą i przypisanie jej do zmiennej
                Request request{client_id, action, new_admin_id, -1}; //tworzenie struktury żądania dodania nowego administratora
                sendRequest(sockfd, request); //wysyłanie żądania do serwera
            } catch (const invalid_argument& e) {
                cerr << "Invalid input. Please enter a valid integer." << endl;
            } catch (const out_of_range& e) {
                cerr << "Input out of range for integer." << endl;
            }
        
        // DODAJ UPRAWNIENIE
        } else if (action == "2")
        {
            string client_id_to_add_permission;
            cout << "Add permission for client with id: ";
            getline(cin, client_id_to_add_permission);

            string client_id_to_be_shutdowned;
            cout << "To shutdown client with id: ";
            getline(cin, client_id_to_be_shutdowned);

            try {
                action = "ADD_PERMISSION";
                int clinet_id_permission = stoi(client_id_to_add_permission);
                int clinet_id_shutdown = stoi(client_id_to_be_shutdowned);
                Request request{client_id, action, clinet_id_permission, clinet_id_shutdown};
                sendRequest(sockfd, request);
            } catch (const invalid_argument& e) {
                cerr << "Invalid input. Please enter a valid integer." << endl;
            } catch (const out_of_range& e) {
                cerr << "Input out of range for integer." << endl;
            }
        // POKAŻ WSZYSTKICH AKTYWNYCH KLIENTÓW
        } else if (action == "3")
        {
            try {
                action = "SHOW_ALL_ACTIVE_CLIENTS";
                Request request{client_id, action, -1, -1};
                sendRequest(sockfd, request);
            } catch (const invalid_argument& e) {
                cerr << "Invalid input. Please enter a valid integer." << endl;
            } catch (const out_of_range& e) {
                cerr << "Input out of range for integer." << endl;
            }
        // POKAŻ WSZYSTKICH ADMINÓW 
        }else if (action == "4")
        {
            try {
                action = "SHOW_ALL_ADMINS";
                Request request{client_id, action, -1, -1};
                sendRequest(sockfd, request);
            } catch (const invalid_argument& e) {
                cerr << "Invalid input. Please enter a valid integer." << endl;
            } catch (const out_of_range& e) {
                cerr << "Input out of range for integer." << endl;
            }
        // WYŁĄCZ KLIENTA
        } else if (action == "5")
        {
            string client_id_to_shutdown;
            cout << "Client id to shutdown: ";
            getline(cin, client_id_to_shutdown);
            try {
                action = "SHUTDOWN_CLIENT";
                int clinet_id_to_shutdown = stoi(client_id_to_shutdown);
                Request request{client_id, action, clinet_id_to_shutdown, -1};
                sendRequest(sockfd, request);
            } catch (const invalid_argument& e) {
                cerr << "Invalid input. Please enter a valid integer." << endl;
            } catch (const out_of_range& e) {
                cerr << "Input out of range for integer." << endl;
            }

        // WYJŚCIE Z APLIKACJI
        } else if (action == "6")
        {
            try {
                action = "EXIT";
                Request request{client_id, action, -1, -1};
                sendRequest(sockfd, request);
                exit(0);
            } catch (const invalid_argument& e) {
                cerr << "Invalid input. Please enter a valid integer." << endl;
            } catch (const out_of_range& e) {
                cerr << "Input out of range for integer." << endl;
            }
        }
        sleep(1);
    }

    close(sockfd);
    return 0;
}
