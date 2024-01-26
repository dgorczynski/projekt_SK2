#include <iostream>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>  //memset
#include <mutex>
#include <unordered_map>
#include <algorithm> 

using namespace std;
namespace Colors {
    const std::string RESET = "\033[0m";
    const std::string RED = "\033[31m"; // something went wrong
    const std::string GREEN = "\033[32m"; //something passed
    const std::string YELLOW = "\033[33m"; // data modification request
    const std::string BLUE = "\033[34m"; //reading data request
}

void logMessage(int colorId, const string& origin, const string& message) { //kolorowanie komunikatów w zależności od kategorii
    string color;
    if (colorId == 1) {
        color = Colors::RED;
    } else if (colorId == 2) {
        color = Colors::GREEN;
    } else if(colorId == 3){
        color = Colors::YELLOW;
    } else if(colorId == 4){
        color = Colors::BLUE;
    }
    std::cout << "[" << color << origin << Colors::RESET << "] " << message << std::endl;
}

struct Request {
    int client_id;
    string action;
    int action_1_client_id;
    int action_2_client_id;
};

struct ThreadData {
    mutex data_mutex;
    vector<int>& admin_ids; //referencja do wektora przechowującego identyfikatory administartorów, dostęp kontrolowany przez mutex
    unordered_map<int, int>& client_id_sock_map; //referenecja do mapy haszującej, która mapuje id klientów na ich sockety
    unordered_map<int, vector<int>>& client_id_permissions_map; //referencja do mapy haszującej, która mapuje id klientów na wektory uprawnień
    vector<int>& active_clients; //referencja do wektora przechowującego aktywnych klientów
    int client_sock; //zmienna przechowująca gniazdo klienta, dla którego utworzono wątek

    //tworzenie konstruktora do łatwego inicjowania wątków
    ThreadData(vector<int>& admins, unordered_map<int, int>& client_map, unordered_map<int, vector<int>>& permissions, vector<int>& all_clients, int sock)
        : admin_ids(admins), client_id_sock_map(client_map), client_id_permissions_map(permissions), active_clients(all_clients), client_sock(sock) {}
    
    void removeClientIdFromLists(int client_id) {

        //Usuwanie z client_id_permissions_map
        auto permissions_map_it = client_id_permissions_map.find(client_id);
        if (permissions_map_it != client_id_permissions_map.end()) {
            client_id_permissions_map.erase(permissions_map_it);
        }

        // Usuwanie z active_clients
        auto active_clients_it = find(active_clients.begin(), active_clients.end(), client_id);
        if (active_clients_it != active_clients.end()) {
            active_clients.erase(active_clients_it);
        }
    }
};
//parsowania żądania z bufora do struktury typu request
Request parseRequest(const char* buffer) {
    istringstream iss(buffer); // Tworzenie strumienia wejściowego na podstawie bufora
    Request request; // Inicjalizacja obiektu Request, który będzie przechowywał sparsowane dane
     // Parsowanie danych ze strumienia wejściowego do obiektu request
    iss >> request.client_id >> request.action >> request.action_1_client_id >> request.action_2_client_id;
    return request; // Zwracanie sparsowanego obiektu Request
}

void deleteClientFromActiveClientsRegistry(vector<int>& active_clients, int id_to_delete) {
     // Wywołanie funkcji logującej informacje o operacji usuwania klienta
     logMessage(3, "deleteClientFromActiveClientsRegistry", 
        "Delete client with  id: " + to_string(id_to_delete) + " from active client registry");
    
    // Iteracja przez elementy wektora active_clients w poszukiwaniu klienta do usunięcia
    for (auto it = active_clients.begin(); it != active_clients.end(); ++it) {
            // Jeśli znaleziono klienta o identyfikatorze id_to_delete, usuń go z wektora
            if (*it == id_to_delete) {
                active_clients.erase(it);
                break; // Przerwij pętlę po znalezieniu klienta do usunięcia
            }
        }
}

void addAdminId(vector<int>& admin_ids, int new_id) {
    // Dodanie nowego identyfikatora administratora do wektora admin_ids
    admin_ids.push_back(new_id);
    // Wywołanie funkcji logującej informacje o operacji dodawania nowego administratora
    logMessage(2, "addAdminId", " Added admin with Id: " + to_string(new_id));
}

void addPermissionForUser(unordered_map<int, vector<int>>& permissions_map, int client_id_to_add_permission, int client_id_to_shutdown) {
    // Dodanie uprawnienia do zamykania klienta w mapie permissions_map
    permissions_map[client_id_to_add_permission].push_back(client_id_to_shutdown);
    // Wywołanie funkcji logującej informacje o operacji dodawania uprawnienia
    logMessage(2, "addPermissionForUser", " Added permission for client with id: " + to_string(client_id_to_add_permission) + " to shutdown client with id: " + to_string(client_id_to_shutdown));
}

bool shutdownClient(unordered_map<int, int>& client_socket_map, int client_id_to_shutdown) {
    // Inicjalizacja bufora dla otrzymywanych danych
    char buffer[1024] = {0};
   
    // Komenda do wysłania do klienta w celu zgłoszenia prośby o zatrzymanie
    string shutdown_command = "SHUTDOWN";
    
    // Pobranie deskryptora gniazda klienta na podstawie identyfikatora klienta
    int client_to_shutdown_socket_dp = client_socket_map[client_id_to_shutdown];
    
    // Logowanie informacji o wysłaniu prośby o zatrzymanie do klienta
    logMessage(3, "shutdownClient", " Send request to descriptor: " + to_string(client_to_shutdown_socket_dp));

    // Wysłanie komendy SHUTDOWN do klienta
    int result = send(client_to_shutdown_socket_dp, shutdown_command.c_str(), shutdown_command.length(), 0);
    
    // Logowanie wyniku operacji wysyłania
    logMessage(3, "shutdownClient", " Result: " + to_string(result));

    // Odbieranie danych od klienta
    recv(client_to_shutdown_socket_dp, buffer, sizeof(buffer), 0);
    string result_message(buffer);
    
    // Sprawdzenie, czy klient został zatrzymany poprawnie
    if (result_message == "SUCCESS") {
        // Logowanie informacji o udanym zatrzymaniu klienta
        logMessage(2, "shutdownClient", " Client with id: " + to_string(client_id_to_shutdown) + " shutdowned successfully!");
        return true;
    } else {
        // Logowanie informacji o nieudanym zatrzymaniu klienta
        logMessage(1, "shutdownClient", " Client with id: " + to_string(client_id_to_shutdown) + " not shutdowned!");
    }
    return false;
}

bool isAdminIdInVector(const vector<int>& admin_ids, int target_id) {
    // Iteracja przez wektor identyfikatorów administratorów
    for (int id : admin_ids) {
        if (id == target_id) { // Sprawdzenie, czy bieżący identyfikator jest równy docelowemu identyfikatorowi
            return true; // Identyfikator został znaleziony
        }
    }
    return false; // Identyfikator nie został znaleziony w wektorze

}

bool isIdInPermissionVector(unordered_map<int, vector<int>>& permissions_map, int client_id_to_add_permission, int client_id_to_shutdown) {
    // Iteracja przez wektor uprawnień przypisany do określonego klienta
    for (int id : permissions_map[client_id_to_add_permission]) {
        if (id == client_id_to_shutdown) {
            return true; // Identyfikator został znaleziony w wektorze uprawnień
        }
    }
    return false; // Identyfikator nie został znaleziony w wektorze uprawnień
}

void handleAddAdminRequest(int client_sock, const Request& request, ThreadData& thread_data) {
    string response_message;
        logMessage(3, "handleAddAdminRequest", " Add new admin with id: " + to_string(request.client_id) + " by admin with id: " + to_string(request.action_1_client_id));
    lock_guard<mutex> lock(thread_data.data_mutex);
    if (!isAdminIdInVector(thread_data.admin_ids, request.action_1_client_id)) {
            addAdminId(thread_data.admin_ids, request.action_1_client_id);   
            response_message = "ADMIN WITH ID: " + to_string(request.action_1_client_id) + " CREATED";
            send(client_sock, response_message.c_str(), response_message.length(), 0);
    } else {
        response_message = "ADMIN WITH ID: " + to_string(request.action_1_client_id) + " ALREADY EXISTS";

        //we convert string to byte array and send to client
        send(client_sock, response_message.c_str(), response_message.length(), 0);
    }
}

void handleAddPermissionRequest(int client_sock, const Request& request, ThreadData& thread_data) {
    string response_message;
        logMessage(3, "handleAddPermissionRequest", 
        " Add permissions for user: " + to_string(request.action_1_client_id) + " to shutdown user with id: " + to_string(request.action_2_client_id) + " by admin with id: " + to_string(request.client_id));
    // Blokada mutexa, aby uniknąć równoczesnego dostępu do współdzielonych danych
    lock_guard<mutex> lock(thread_data.data_mutex);
    // Sprawdzenie, czy uprawnienie nie zostało już dodane
    if (!isIdInPermissionVector(thread_data.client_id_permissions_map, request.action_1_client_id, request.action_2_client_id)) {
            // Dodanie uprawnienia i przygotowanie wiadomości potwierdzającej
            addPermissionForUser(thread_data.client_id_permissions_map, request.action_1_client_id, request.action_2_client_id);   
            response_message = "Permission added successfully!";
            // Wysłanie wiadomości potwierdzającej do klienta
            send(client_sock, response_message.c_str(), response_message.length(), 0);
    } else {
        // Przygotowanie wiadomości informującej, że klient już ma to uprawnienie
        response_message = "Client already has this permission!";
        logMessage(1, "handleAddPermissionRequest", 
        "Client already has this permission!");
         // Wysłanie wiadomości informacyjnej do klienta
        send(client_sock, response_message.c_str(), response_message.length(), 0);
    }
}

void handleShutdownClientRequest(int client_sock, const Request& request, ThreadData& thread_data) {
    string response_message;
        logMessage(3, "handleShutdownClientRequest", 
        " Start shutdown client with id: " + to_string(request.action_1_client_id) + " by client with id: " + to_string(request.client_id));
    // Blokada mutexa, aby uniknąć równoczesnego dostępu do danych współdzielonych
    lock_guard<mutex> lock(thread_data.data_mutex);
    // Sprawdzenie, czy klient ma uprawnienie do wyłączenia innego klienta
    if (isIdInPermissionVector(thread_data.client_id_permissions_map, request.client_id, request.action_1_client_id)) {
            // Sprawdzenie, czy proces wyłączania klienta zakończył się powodzeniem
            if(shutdownClient(thread_data.client_id_sock_map, request.action_1_client_id)) {
                // Usunięcie klienta z listy aktywnych klientów
                deleteClientFromActiveClientsRegistry(thread_data.active_clients, request.action_1_client_id);
                response_message = "Client shutdowned successfully!";
            } else {
                response_message = "There was some error with shutdown!";
            }         
    } else {
        response_message = "You don't have permission to shutdown this client!";
        
    }
    // Wysłanie odpowiedzi do klienta
    send(client_sock, response_message.c_str(), response_message.length(), 0);
}

void handleShowAllAdminsRequest(int client_sock, const Request& request, ThreadData& thread_data) {
    string response_message = "Existing admin ids: [";
    logMessage(4, "handleShowAllAdminsRequest", "Show all admins for client with id: " + to_string(request.client_id));
    // Blokada mutexa, aby uniknąć równoczesnego dostępu do danych współdzielonych
    lock_guard<mutex> lock(thread_data.data_mutex);
    // Iteracja przez wektor admin_ids i dodanie każdego identyfikatora admina do wiadomości odpowiedzi
    for(int id : thread_data.admin_ids) {
        response_message = response_message + to_string(id) + ", ";
    }
    // Usunięcie ostatnich dwóch znaków (", ") z wiadomości odpowiedzi
    response_message = response_message.substr(0, response_message.size() - 2) + "]";
    // Wysłanie odpowiedzi do klienta
    send(client_sock, response_message.c_str(), response_message.length(), 0);
    }

void handleShowAllActiveClientsRequest(int client_sock, const Request& request, ThreadData& thread_data) {
    string response_message = "Active client ids: [";
    logMessage(4, "handleShowAllActiveClientsRequest", "Show all active clients for client with id: " + to_string(request.client_id));
    // Blokada mutexa, aby uniknąć równoczesnego dostępu do danych współdzielonych
    lock_guard<mutex> lock(thread_data.data_mutex);
    // Iteracja przez wektor active_clients i dodanie każdego identyfikatora klienta do wiadomości odpowiedzi
    for(int id : thread_data.active_clients) {
        response_message = response_message + to_string(id) + ", ";
    }
    // Usunięcie ostatnich dwóch znaków (", ") z wiadomości odpowiedzi
    response_message = response_message.substr(0, response_message.size() - 2) + "]";
    // Wysłanie odpowiedzi do klienta
    send(client_sock, response_message.c_str(), response_message.length(), 0);
    }

void handleExitClientRequest(int client_sock, const Request& request, ThreadData& thread_data) {
    logMessage(4, "handleExitClientRequest", "Client with id: " + to_string(request.client_id) + " exited");
    // Blokada mutexa, aby uniknąć równoczesnego dostępu do danych współdzielonych
    lock_guard<mutex> lock(thread_data.data_mutex);
    {
         // Iteracja przez wektor active_clients i usunięcie identyfikatora klienta, który wysłał żądanie
        for (auto it = thread_data.active_clients.begin(); it != thread_data.active_clients.end(); ++it) {
            if (*it == request.client_id) {
                thread_data.active_clients.erase(it);
                break; 
            }
        }
    }
    }

void handleRegisterActiveClient(int client_sock, const Request& request, ThreadData& thread_data) {
    string response_message = "Connected successfully!";
    unordered_map<int, int>& client_id_sock_map = thread_data.client_id_sock_map;
    logMessage(4, "handleRegisterActiveClient", "Active client with id: " + to_string(request.client_id) + " registered");
    // Blokada mutexa, aby uniknąć równoczesnego dostępu do danych współdzielonych
    lock_guard<mutex> lock(thread_data.data_mutex);
    {
        // Dodanie identyfikatora klienta do wektora active_clients
        thread_data.active_clients.push_back(request.client_id);
    }
        // Wysłanie komunikatu potwierdzającego połączenie z serwerem
        send(client_sock, response_message.c_str(), response_message.length(), 0);
    }

bool authenticateAdmin(int client_sock, const Request& request, ThreadData& thread_data) {
    if(isAdminIdInVector(thread_data.admin_ids, request.client_id)) { 
        // Logowanie pomyślnego uwierzytelnienia administratora
        logMessage(2, "authenticateAdmin", "User with Id: " + to_string(request.client_id) + " authenticated");
        return true;
        } else {
            // Logowanie nieudanego uwierzytelnienia administratora
            logMessage(1, "authenticateAdmin", " User with Id: " + to_string(request.client_id) + " not authenticated");
            return false;
        }
}

void* checkClientConnections(void* arg) {
    ThreadData* thread_data = (ThreadData*)arg;

    while (true) {
        sleep(5);  // Odczekanie 5 sekund przed kolejnym sprawdzeniem połączeń
        {
            // Blokada wątku dostępu do wspólnych danych
            lock_guard<mutex> lock(thread_data->data_mutex);
            // Iteracja po mapie klientów
            auto it = thread_data->client_id_sock_map.begin();
            while (it != thread_data->client_id_sock_map.end()) {
                int client_id = it->first;
                int client_sock = it->second;

                char buffer[1];
                // Próba odbioru 1 bajta bez oczekiwania
                int result = recv(client_sock, buffer, 1, MSG_DONTWAIT);

                if (result == 0) {
                    // Klient rozłączony
                    if (result == 0) {
                        logMessage(1, "checkClientConnections", "Client with id: " + to_string(client_id) + " disconnected ");
                    } else {
                        // Inny błąd podczas próby odbioru danych
                        perror("Recv failed");
                    }
                    thread_data->removeClientIdFromLists(client_id);
                    it = thread_data->client_id_sock_map.erase(it);
                    continue;
                }
                ++it;
            }
        }
    }
}

// Pętla klienta
void* clientHandler(void* arg) {
    //Dane wątku
    ThreadData* thread_data = (ThreadData*)arg;
    int client_sock = thread_data->client_sock;
    unordered_map<int, int>& client_id_sock_map = thread_data->client_id_sock_map;

    string response_message;
    char buffer[1024] = {0};

    bool client_id_added_to_map = false;
    while (true) {
        // Odbiór danych od klienta
        recv(client_sock, buffer, sizeof(buffer), 0);
        Request request = parseRequest(buffer);

        // Dodanie identyfikatora klienta do mapy, jeśli jeszcze nie dodano
        if(!client_id_added_to_map) {
            lock_guard<mutex> lock(thread_data->data_mutex);
            client_id_sock_map[request.client_id] = client_sock;
            client_id_added_to_map = true;
        }
        // Obsługa różnych rodzajów żądań od klienta
        if(request.action == "ADD_ADMIN_ID") {    
                if(authenticateAdmin(client_sock, request, *thread_data)) {
                    handleAddAdminRequest(client_sock, request, *thread_data); 
                } else {
                     response_message = "You have not permission to add new admin id";
                    send(client_sock, response_message.c_str(), response_message.length(), 0);

                }
            } else if (request.action == "SHOW_ALL_ADMINS")
            {
                if (authenticateAdmin(client_sock, request, *thread_data)) {
                    handleShowAllAdminsRequest(client_sock, request, *thread_data);
                } else {
                    string response_message = "You have not permission to show all admins";
                    send(client_sock, response_message.c_str(), response_message.length(), 0);
                }
            } else if (request.action == "REGISTER")
            {
                handleRegisterActiveClient(client_sock, request, *thread_data);
            } else if (request.action == "ADD_PERMISSION")
            {
                if(authenticateAdmin(client_sock, request, *thread_data)) {
                    handleAddPermissionRequest(client_sock, request, *thread_data); 
                } else {
                     response_message = "You have not permission to add shutdown permissions for clients";
                    send(client_sock, response_message.c_str(), response_message.length(), 0);
                }
            } else if (request.action == "SHUTDOWN_CLIENT")
            {
                handleShutdownClientRequest(client_sock, request, *thread_data); 
            } else if (request.action == "SHOW_ALL_ACTIVE_CLIENTS")
            {
                if (authenticateAdmin(client_sock, request, *thread_data)) {
                    handleShowAllActiveClientsRequest(client_sock, request, *thread_data); 
                } else {
                    string response_message = "You have not permission to show all active clients";
                    send(client_sock, response_message.c_str(), response_message.length(), 0);
                }
            } else if (request.action == "EXIT")
            {
                handleExitClientRequest(client_sock, request, *thread_data);
            }
        // Wyczyszczenie bufora
        memset(buffer, 0, sizeof(buffer));
    }
}

int main() {
    // Tworzenie gniazda serwera
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Error creating socket");
        return 1;
    }
    // Konfiguracja adresu serwera
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8881);
    // Przypisanie adresu do gniazda serwera
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        return 1;
    }
    // Nasłuchiwanie na gnieździe serwera
    if (listen(server_sock, 10) == -1) {
        perror("Listen failed");
        return 1;
    }

    cout << "[MAIN] Server listening on port 8888..." << std::endl;

    vector<pthread_t> thread_ids;
    vector<int> admin_ids;
    vector<int> active_clients;
    unordered_map<int, int> client_id_sock_map;
    unordered_map<int, vector<int>> client_id_permissions_map;
    admin_ids.push_back(1); // Początkowo mamy jednego administratora o identyfikatorze 1

    while (true) {
        // Akceptowanie nowego połączenia od klienta
        int client_sock = accept(server_sock, nullptr, nullptr);
        cout << "[MAIN] New client connected + sock:  \n";
        cout << client_sock;
        if (client_sock == -1) {
            perror("Accept failed");
            continue;
        }
        // Tworzenie danych dla wątku
        ThreadData* thread_data = new ThreadData {
            admin_ids, 
            client_id_sock_map, 
            client_id_permissions_map, 
            active_clients, 
            client_sock
            };
        // Tworzenie wątku do monitorowania połączeń
        pthread_t check_connections_thread;
        if (pthread_create(&check_connections_thread, NULL, checkClientConnections, (void*)thread_data) != 0) {
            perror("Failed to create check connections thread");
            close(server_sock);
            return 1;
        }
        thread_ids.push_back(check_connections_thread);
        pthread_detach(check_connections_thread);

        // Tworzenie wątku obsługującego klienta
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, clientHandler, (void*)thread_data) != 0) {
            perror("Failed to create thread");
            close(client_sock);
            delete thread_data;
            continue;
        }

        thread_ids.push_back(thread_id);
        pthread_detach(thread_id);
    }

    // Oczekiwanie na zakończenie wszystkich wątków
    for (pthread_t thread_id : thread_ids) {
        pthread_join(thread_id, NULL);
    }
    close(server_sock); // Zamknięcie gniazda serwera
    return 0;
}
