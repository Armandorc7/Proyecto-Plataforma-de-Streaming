
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <future>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <memory>
#include <queue>

using namespace std;


struct Pelicula {
    string id;
    string titulo;
    string sinopsis;
    string tagsRaw;
    string split;
    string source;
};

// 1. TEMA 5.6: ESTRUCTURAS FUNDAMENTALES - ÁRBOL DE SUFIJOS (Suffix Trie)

struct TrieNode {
    unordered_map<char, TrieNode*> hijos; // Hash map para O(1) al buscar la siguiente letra
    unordered_set<string> idsPeliculas;   // Set para evitar IDs duplicados

    ~TrieNode() {
        for (auto& par : hijos) delete par.second;
    }
};

//
class SuffixTrie {
private:
    TrieNode* raiz;

public:
    SuffixTrie() : raiz(new TrieNode()) {}
    ~SuffixTrie() { delete raiz; }

    // Inserción - Complejidad: O(L^2) por palabra, donde L es su longitud.
    void insertarPalabra(const string& palabra, const string& idPelicula) {
        for (size_t i = 0; i < palabra.length(); i++) {
            TrieNode* actual = raiz;
            for (size_t j = i; j < palabra.length(); j++) {
                char letra = palabra[j];
                if (actual->hijos.find(letra) == actual->hijos.end()) {
                    actual->hijos[letra] = new TrieNode();
                }
                actual = actual->hijos[letra];
                actual->idsPeliculas.insert(idPelicula);
            }
        }
    }

    // Búsqueda - Complejidad: O(M) donde M es el tamaño de la consulta.
    unordered_set<string> buscar(const string& query) {
        TrieNode* actual = raiz;
        for (char letra : query) {
            if (actual->hijos.find(letra) == actual->hijos.end()) {
                return unordered_set<string>();
            }
            actual = actual->hijos[letra];
        }
        return actual->idsPeliculas;
    }
};

// 2. TEMA 5.5: PATRONES DE DISEÑO - STRATEGY PATTERN

class IBusquedaStrategy {
public:
    virtual unordered_set<string> buscar(const string& query) = 0;
    virtual ~IBusquedaStrategy() = default;
};

class BusquedaPorTituloStrategy : public IBusquedaStrategy {
private:
    SuffixTrie& indice;
public:
    BusquedaPorTituloStrategy(SuffixTrie& trie) : indice(trie) {}
    unordered_set<string> buscar(const string& query) override { return indice.buscar(query); }
};

class BusquedaGlobalStrategy : public IBusquedaStrategy {
private:
    SuffixTrie& indTitulos;
    SuffixTrie& indTags;
public:
    BusquedaGlobalStrategy(SuffixTrie& titulos, SuffixTrie& tags) : indTitulos(titulos), indTags(tags) {}
    unordered_set<string> buscar(const string& query) override {
        unordered_set<string> res = indTitulos.buscar(query);
        unordered_set<string> tags = indTags.buscar(query);
        res.insert(tags.begin(), tags.end());
        return res;
    }
};

class MotorBusqueda {
private:
    SuffixTrie indiceTitulos;
    SuffixTrie indiceTags;
    unique_ptr<IBusquedaStrategy> estrategiaActual;

    void procesarYAgregar(const string& texto, const string& id, SuffixTrie& trie) {
        string textoLimpio = texto;
        transform(textoLimpio.begin(), textoLimpio.end(), textoLimpio.begin(),
                  [](unsigned char c){ return static_cast<char>(tolower(c)); });
        replace_if(textoLimpio.begin(), textoLimpio.end(),
                   [](unsigned char c){ return ispunct(c); }, ' ');

        stringstream ss(textoLimpio);
        string palabra;
        while (ss >> palabra) {
            trie.insertarPalabra(palabra, id);
        }
    }

public:
    MotorBusqueda() {
        estrategiaActual = make_unique<BusquedaGlobalStrategy>(indiceTitulos, indiceTags);
    }
    void setEstrategia(unique_ptr<IBusquedaStrategy> nueva) {
        estrategiaActual = std::move(nueva);
    }

    void indexarPelicula(const Pelicula& p) {
        procesarYAgregar(p.titulo, p.id, indiceTitulos);
        procesarYAgregar(p.sinopsis, p.id, indiceTitulos);
        procesarYAgregar(p.tagsRaw, p.id, indiceTags);
    }

    unordered_set<string> realizarBusqueda(string query) {
        // Normalizar a minúsculas
        transform(query.begin(), query.end(), query.begin(),
                  [](unsigned char c){ return static_cast<char>(tolower(c)); });
        // Limpiar puntuación
        replace_if(query.begin(), query.end(),
                   [](unsigned char c){ return ispunct(c); }, ' ');
        // Tokenizar y hacer UNION (busca cada palabra por separado)
        stringstream ss(query);
        string token;
        unordered_set<string> total;
        while (ss >> token) {
            auto parcial = estrategiaActual->buscar(token);
            total.insert(parcial.begin(), parcial.end());
        }
        return total;
    }

    SuffixTrie& getIndiceTitulos() { return indiceTitulos; }
};

// 3. TEMA 5.4 y 5.5: CONCURRENCIA Y SINGLETON (Gestor de Base de Datos)

class DatabaseManager {
private:
    // TEMA 5.6.3: Hash Table para búsqueda de películas en O(1)
    unordered_map<string, Pelicula> baseDeDatos;
    mutex dbMutex;

    DatabaseManager() {}

    void procesarLoteConcurrente(vector<vector<string>> loteFilas) {
        unordered_map<string, Pelicula> loteProcesado;
        auto aMinusculas = [](string& str) {
            transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return tolower(c); });
        };

        for (const auto& fila : loteFilas) {
            if (fila.size() >= 6 && fila[0] != "imdb_id") {
                Pelicula p = {fila[0], fila[1], fila[2], fila[3], fila[4], fila[5]};
                // Pre-procesamiento a minúsculas
                aMinusculas(p.titulo);
                aMinusculas(p.sinopsis);
                aMinusculas(p.tagsRaw);
                loteProcesado[p.id] = p;
            }
        }

        // Bloqueo del mutex para evitar Data Races al escribir en la Hash Table global
        lock_guard<mutex> lock(dbMutex);
        for (const auto& par : loteProcesado) {
            baseDeDatos[par.first] = par.second;
        }
    }

public:
    DatabaseManager(const DatabaseManager&) = delete;
    void operator=(const DatabaseManager&) = delete;

    static DatabaseManager& getInstance() {
        static DatabaseManager instancia;
        return instancia;
    }

    void cargarDesdeCSV(const string& ruta) {
        ifstream archivo(ruta);
        if (!archivo.is_open()) return;

        char c;
        bool dentroComillas = false;
        string celda = "";
        vector<string> fila;
        vector<vector<string>> lote;
        vector<future<void>> hilos;

        cout << "1. Cargando CSV con hilos concurrentes (std::async)..." << endl;
        while (archivo.get(c)) {
            if (c == '"') {
                if (dentroComillas && archivo.peek() == '"') { celda += '"'; archivo.get(c); }
                else { dentroComillas = !dentroComillas; }
            }
            else if (c == ',' && !dentroComillas) { fila.push_back(celda); celda = ""; }
            else if ((c == '\n' || c == '\r') && !dentroComillas) {
                if (!celda.empty() || !fila.empty()) {
                    fila.push_back(celda); lote.push_back(fila);
                    fila.clear(); celda = "";
                    if (lote.size() >= 1000) {
                        hilos.push_back(async(launch::async, &DatabaseManager::procesarLoteConcurrente, this, lote));
                        lote.clear();
                    }
                }
            } else {
                if (dentroComillas && (c == '\n' || c == '\r')) celda += ' ';
                else if (c != '\r' && c != '\n') celda += c;
            }
        }
        if (!celda.empty() || !fila.empty()) { fila.push_back(celda); lote.push_back(fila); }
        if (!lote.empty()) hilos.push_back(async(launch::async, &DatabaseManager::procesarLoteConcurrente, this, lote));
        for (auto& h : hilos) h.wait();
        archivo.close();
        cout << "   -> Carga completada. " << baseDeDatos.size() << " peliculas en Hash Table (O(1)).\n" << endl;
    }

    const unordered_map<string, Pelicula>& getBaseDeDatos() const { return baseDeDatos; }

    Pelicula obtenerPelicula(const string& id) {
        auto it = baseDeDatos.find(id);
        if (it != baseDeDatos.end()) return it->second;
        return Pelicula();
    }
};




// Estructura auxiliar para guardar en el Max-Heap
struct ResultadoPuntuado {
    string id;
    int score;
    // Sobrecargamos el operador < para que la Priority Queue sepa cómo ordenar.
    bool operator<(const ResultadoPuntuado& otro) const {
        return score < otro.score;
    }
};

class SistemaRanking {
public:
    // Calcula la "Importancia" de una película respecto a la búsqueda
    int calcularImportancia(const string& query, const Pelicula& p) {
        int score = 0;

        // Asumimos  query, p.titulo y p.tagsRaw están en minúscula por el pre-procesamiento anterior

        // 1. Coincidencia exacta en título (Mayor importancia)
        if (p.titulo == query) {
            score += 100;
        }
        // 2. Coincidencia parcial en título
        else if (p.titulo.find(query) != string::npos) {
            score += 40;
        }

        // 3. Frecuencia en los Tags (Term Frequency simple)
        size_t pos = p.tagsRaw.find(query, 0);
        while (pos != string::npos) {
            score += 25;
            pos = p.tagsRaw.find(query, pos + query.length());
        }

        // 4. Coincidencia en la SINOPSIS (menor peso)
        size_t posSinopsis = p.sinopsis.find(query, 0);
        while (posSinopsis != string::npos) {
            score += 3;
            posSinopsis = p.sinopsis.find(query, posSinopsis + query.length());
        }

        return score;
    }
};

//  PAGINADOR CON HEAP

class PaginadorResultados {
private:
    priority_queue<ResultadoPuntuado> maxHeap;
    DatabaseManager& db;

public:
    // Construir el Heap toma O(K * L) donde K es el número de IDs encontrados y L es la longitud del texto.
    PaginadorResultados(const unordered_set<string>& idsEncontrados, const string& query): db(DatabaseManager::getInstance())
    {
        SistemaRanking ranking;

        for (const string& id : idsEncontrados) {
            Pelicula p = db.obtenerPelicula(id); // O(1) gracias al Hash Table
            int score = ranking.calcularImportancia(query, p);
            // Insertamos en el Max-Heap: O(log K) por inserción
            maxHeap.push({id, score});
        }
    }

    // TEMA 5.3: Extraer N elementos toma O(N log K)
    vector<string> mostrarSiguientePagina(int cantidadElementos) {
        vector<string> idsMostrados;
        if (maxHeap.empty()) {
            cout << "--- No hay mas resultados para mostrar ---" << endl;
            return idsMostrados;
        }
        cout << "\n----- RESULTADOS ---" << endl;
        for (int i = 0; i < cantidadElementos && !maxHeap.empty(); i++) {
            ResultadoPuntuado top = maxHeap.top();
            maxHeap.pop();
            idsMostrados.push_back(top.id);

            Pelicula p = db.obtenerPelicula(top.id);
            cout << i + 1 << ". " << p.titulo << " (Score: " << top.score << ")" << endl;
        }
        cout << "------------------------------------------" << endl;
        return idsMostrados;
    }

    bool hayMasResultados() { return !maxHeap.empty(); }
};


class Usuario {
private:
    string nombre;
    queue<string> verMasTarde;      // IDs de películas guardadas (O(1) inserción)
    vector<string> peliculasGustadas; // IDs de películas que le gustaron (Likes)

public:
    Usuario(string _nombre) : nombre(_nombre) {}

    void agregarVerMasTarde(const string& idPelicula) {
        verMasTarde.push(idPelicula);
        cout << "Pelicula " << idPelicula << " agregada a 'Ver mas tarde'." << endl;
    }

    void agregarLike(const string& idPelicula) {
        peliculasGustadas.push_back(idPelicula);
        cout << "Te ha gustado la pelicula " << idPelicula << "!" << endl;
    }

    string obtenerProximaVerMasTarde() {
        if (verMasTarde.empty()) return "";
        string id = verMasTarde.front();
        verMasTarde.pop(); // La sacamos de la cola
        return id;
    }

    vector<string> obtenerListaVerMasTarde() {
        vector<string> lista;
        queue<string> copia = verMasTarde; // Copiamos para no afectar la original
        while(!copia.empty()){
            lista.push_back(copia.front());
            copia.pop();
        }
        return lista;
    }

    const vector<string>& getGustadas() const { return peliculasGustadas; }
    string getNombre() const { return nombre; }
};


class GrafoRecomendaciones {
private:
    unordered_map<string, vector<string>> adyacenciaTagAPeliculas;

    unordered_map<string, vector<string>> adyacenciaPeliculaATags;

public:
    void construirGrafo(const unordered_map<string, Pelicula>& baseDeDatos) {
        cout << "Construyendo Grafo de Recomendaciones (Peliculas <-> Tags)..." << endl;
        for (const auto& par : baseDeDatos) {
            const Pelicula& p = par.second;
            string id = p.id;

            // Separar los tags crudos en palabras individuales
            string tagsLimpios = p.tagsRaw;
            replace(tagsLimpios.begin(), tagsLimpios.end(), ',', ' ');
            stringstream ss(tagsLimpios);
            string tag;

            while (ss >> tag) {
                // Pasamos a minúscula para uniformidad
                transform(tag.begin(), tag.end(), tag.begin(), ::tolower);

                // Creamos las aristas bidireccionales del Grafo Bipartito
                adyacenciaTagAPeliculas[tag].push_back(id);
                adyacenciaPeliculaATags[id].push_back(tag);
            }
        }
    }

    vector<pair<string, int>> recomendar(const vector<string>& idsGustadas, int topN) {
        if (idsGustadas.empty()) return {};

        // Mapa para contar cuántos "caminos" (tags compartidos) llevan a cada película
        unordered_map<string, int> conteoSimilitud;
        unordered_set<string> setGustadas(idsGustadas.begin(), idsGustadas.end());

        // Recorrido BFS de 2 Niveles desde los gustos del usuario
        for (const string& idOrigen : idsGustadas) {
            // NIVEL 1 BFS: Vamos a los nodos TAG conectados a esta película
            for (const string& tag : adyacenciaPeliculaATags[idOrigen]) {

                // NIVEL 2 BFS: Vamos a todas las PELÍCULAS conectadas a este TAG
                for (const string& idDestino : adyacenciaTagAPeliculas[tag]) {
                    // Evitamos recomendar películas que ya le gustaron
                    if (setGustadas.find(idDestino) == setGustadas.end()) {
                        conteoSimilitud[idDestino]++; // Incrementamos peso de la arista
                    }
                }
            }
        }

        // TEMA 5.6.2: Max-Heap para sacar las N más similares en O(K log K)
        priority_queue<pair<int, string>> maxHeap;
        for (const auto& par : conteoSimilitud) {
            maxHeap.push({par.second, par.first}); // Ordena por la similitud (par.second)
        }

        // Extraemos el Top N del Heap
        vector<pair<string, int>> recomendaciones;
        for (int i = 0; i < topN && !maxHeap.empty(); i++) {
            // Invertimos el par al guardarlo para que sea {ID, Peso}
            recomendaciones.push_back({maxHeap.top().second, maxHeap.top().first});
            maxHeap.pop();
        }

        return recomendaciones;
    }
};



int main() {
    ios_base::sync_with_stdio(false); cin.tie(NULL);

    cout << "======================================================" << endl;
    cout << "> Iniciando sistema..." << endl;

    // 1. Cargar Base de Datos
    DatabaseManager& db = DatabaseManager::getInstance();
    db.cargarDesdeCSV("mpst_full_data.csv"); // Asegúrate que el archivo exista en la ruta

    // 2. Indexar en el Árbol
    cout << "> Construyendo Arbol de Busqueda..." << endl;
    MotorBusqueda motor;
    for (const auto& par : db.getBaseDeDatos()) {
        motor.indexarPelicula(par.second);
    }
    cout << "  -> Arbol construido con exito.\n" << endl;

    // 3. Construir Grafo
    GrafoRecomendaciones grafo;
    grafo.construirGrafo(db.getBaseDeDatos());
    cout << "======================================================\n" << endl;

    Usuario miUsuario("Cinefilo");

    // Bucle principal de la aplicación
    bool ejecutando = true;
    while (ejecutando) {
        cout << "\n¡BIENVENIDO A LA PLATAFORMA DE STREAMING!" << endl;

        // Mostrar "Ver Más Tarde"
        cout << "\n TU LISTA: VER MAS TARDE" << endl;
        vector<string> listaPendientes = miUsuario.obtenerListaVerMasTarde();
        if (listaPendientes.empty()) {
            cout << "   (No tienes peliculas pendientes)" << endl;
        } else {
            for (size_t i = 0; i < listaPendientes.size(); i++) {
                cout << "   " << i+1 << ". " << db.obtenerPelicula(listaPendientes[i]).titulo << endl;
            }
        }

        // Mostrar Recomendaciones
        cout << "\n RECOMENDADOS PARA TI" << endl;
        vector<pair<string, int>> recomendaciones = grafo.recomendar(miUsuario.getGustadas(), 3);
        if (recomendaciones.empty()) {
            cout << "   (Dale Like a algunas peliculas para recibir recomendaciones)" << endl;
        } else {
            for (const auto& rec : recomendaciones) {
                cout << "   - " << db.obtenerPelicula(rec.first).titulo << endl;
            }
        }

        // Menú Principal
        cout << "\n========================================================" << endl;
        cout << "                    MENU PRINCIPAL" << endl;
        cout << "======================================================" << endl;
        cout << "1. Buscar pelicula (Palabra, Frase, Etiqueta o Fragmento)" << endl;
        cout << "2. Salir" << endl;
        cout << "------------------------------------------------------" << endl;
        cout << "> Ingrese una opcion: " << endl;

        string opcionMenu;
        cin >> opcionMenu;
        cin.ignore(); // Limpiar el buffer del enter

        if (opcionMenu == "2") {
            ejecutando = false;
            cout << "\n¡Gracias por usar la plataforma! Hasta pronto." << endl;
            break;
        }
        else if (opcionMenu == "1") {
            cout << "> Ingrese el texto a buscar: "<< endl;
            string query;
            getline(cin, query);

            auto resultadosBusqueda = motor.realizarBusqueda(query);
            cout << "\nBuscando '" << query << "'... ¡Encontradas " << resultadosBusqueda.size() << " coincidencias!" << endl;

            if (resultadosBusqueda.empty()) continue;

            PaginadorResultados paginador(resultadosBusqueda, query);
            bool viendoResultados = true;

            // Bucle de Paginación
            while (viendoResultados) {
                vector<string> paginaActual = paginador.mostrarSiguientePagina(5);

                bool enMenuPaginacion = true;
                while (enMenuPaginacion) {
                    cout << "\n> Opciones: [1-" << paginaActual.size() << "] Ver detalle | ";
                    if (paginador.hayMasResultados()) cout << "[S] Siguientes 5 | ";
                    cout << "[V] Volver al menu principal" << endl;
                    cout << "> Ingrese opcion: "<< endl;

                    string optResultados;
                    cin >> optResultados;
                    cin.ignore();

                    if (optResultados == "V" || optResultados == "v") {
                        viendoResultados = false;
                        enMenuPaginacion = false;
                    }
                    else if ((optResultados == "S" || optResultados == "s") && paginador.hayMasResultados()) {
                        enMenuPaginacion = false; // Rompe este sub-bucle para que el bucle padre imprima la sig. página
                    }
                    else if (isdigit(optResultados[0])) {
                        int index = stoi(optResultados) - 1;
                        if (index >= 0 && index < paginaActual.size()) {
                            // Mostrar Detalle de la película
                            Pelicula pSeleccionada = db.obtenerPelicula(paginaActual[index]);

                            bool viendoDetalle = true;
                            while (viendoDetalle) {
                                cout << "\n======================================================" << endl;
                                cout << "                 DETALLE DE PELICULA" << endl;
                                cout << "======================================================" << endl;
                                cout << "TITULO: " << pSeleccionada.titulo << endl;
                                cout << "TAGS: " << pSeleccionada.tagsRaw << endl;
                                cout << "\nSINOPSIS:\n" << pSeleccionada.sinopsis << endl;
                                cout << "------------------------------------------------------" << endl;
                                cout << "¿Que deseas hacer?" << endl;
                                cout << "[L] Dar Like" << endl;
                                cout << "[T] Anadir a 'Ver mas tarde'" << endl;
                                cout << "[V] Volver a los resultados" << endl;
                                cout << "> Ingrese opcion: " << endl;

                                string optDetalle;
                                cin >> optDetalle;
                                cin.ignore();

                                if (optDetalle == "L" || optDetalle == "l") {
                                    miUsuario.agregarLike(pSeleccionada.id);
                                } else if (optDetalle == "T" || optDetalle == "t") {
                                    miUsuario.agregarVerMasTarde(pSeleccionada.id);
                                } else if (optDetalle == "V" || optDetalle == "v") {
                                    viendoDetalle = false;
                                } else {
                                    cout << "Opcion invalida." << endl;
                                }
                            }
                            // Al salir del detalle, volvemos a imprimir las opciones de la página actual
                        } else {
                            cout << "Numero fuera de rango." << endl;
                        }
                    } else {
                        cout << "Opcion no reconocida." << endl;
                    }
                }
            }
        } else {
            cout << "Opcion no valida. Intente de nuevo." << endl;
        }
    }

    return 0;
}