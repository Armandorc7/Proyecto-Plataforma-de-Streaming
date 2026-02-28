# Plataforma de Streaming

Sistema de búsqueda y visualización de películas implementado en C++ mediante estructuras avanzadas de datos, programación paralela y patrones de diseño.

Video de presentación: [https://youtu.be/4-V8X2rl_SE]

---

## Integrantes

| Nombre | Código |
|--------|--------|
| Armando Andrés Ruesta Carrion | 202410753 |

---

## Descripción General

El sistema permite:

- Leer una base de datos en formato CSV.
- Indexar títulos, sinopsis y tags en un Suffix Trie.
- Buscar películas por palabra, frase o fragmento.
- Ordenar resultados por relevancia mediante un sistema de ranking.
- Mostrar resultados paginados.
- Gestionar lista "Ver más tarde".
- Registrar Likes.
- Generar recomendaciones personalizadas basadas en similitud de tags.

El diseño prioriza eficiencia temporal, uso intensivo de la STL y separación clara de responsabilidades.

---

## Arquitectura del Sistema

El sistema se divide en tres capas principales:

### 1. Capa de Presentación
- `main()`
- Interfaz de línea de comandos (CLI).

### 2. Capa de Lógica
- `MotorBusqueda`
- `SistemaRanking`
- `PaginadorResultados`
- `Usuario`
- `GrafoRecomendaciones`

### 3. Capa de Datos
- `DatabaseManager` (Singleton + Hash Table)
- `SuffixTrie` (Índice de búsqueda)

---

## Justificación del Árbol

Se requiere búsqueda de subcadenas arbitrarias dentro de títulos, sinopsis y tags. Se evaluaron distintas alternativas:

| Estructura | Subcadenas | Complejidad Búsqueda | Decisión |
|------------|------------|----------------------|----------|
| Suffix Trie | Sí | O(M) | Elegido |
| Suffix Tree (Ukkonen) | Sí | O(M) | Descartado — implementación compleja |
| Aho-Corasick | Sí | O(N+M) | Descartado — orientado a patrones múltiples fijos |
| Trie estándar | Solo prefijos | O(M) | Descartado |

El Suffix Trie permite búsquedas en O(M), donde M es la longitud de la consulta, independiente del tamaño total del dataset.

La inserción de cada palabra tiene complejidad O(L²), donde L es la longitud de la palabra, ya que se insertan todos sus sufijos.

---

## Estructuras de Datos Utilizadas

### 1. Suffix Trie
Permite búsqueda eficiente de fragmentos en títulos y sinopsis.

| Operación | Complejidad |
|-----------|------------|
| Insertar palabra | O(L²) |
| Buscar fragmento | O(M) |

---

### 2. Hash Table — `unordered_map<string, Pelicula>`

Almacena todas las películas indexadas por su ID.

- Acceso promedio en tiempo constante.
- Utilizada por `DatabaseManager`.

---

### 3. Max-Heap — `priority_queue`

Usado en:

- `PaginadorResultados` para ordenar resultados por relevancia.
- `GrafoRecomendaciones` para extraer el Top N de recomendaciones.

| Operación | Complejidad |
|-----------|------------|
| Inserción | O(log K) |
| Extracción máximo | O(log K) |

---

### 4. Cola FIFO — `queue<string>`

Implementa la lista "Ver más tarde" con comportamiento First-In-First-Out.

---

### 5. Grafo Bipartito Implícito

Dos `unordered_map<string, vector<string>>` modelan la relación:

Película ↔ Tag

Permite calcular similitud por cantidad de tags compartidos.

---

## Patrones de Diseño Aplicados

### Strategy

Clases:
- `IBusquedaStrategy`
- `BusquedaPorTituloStrategy`
- `BusquedaGlobalStrategy`

Permite intercambiar el algoritmo de búsqueda sin modificar `MotorBusqueda`. Cumple el principio Open/Closed de SOLID.

---

### Singleton

Clase:
- `DatabaseManager`

Garantiza una única instancia global para la carga y gestión del CSV.

La inicialización del objeto estático local es thread-safe según el estándar C++11.

---

## Sistema de Ranking

`SistemaRanking` calcula la relevancia de cada película según cuatro criterios ponderados:

| Criterio | Puntaje |
|-----------|---------|
| Coincidencia exacta en título | +100 |
| Coincidencia parcial en título | +40 |
| Cada aparición en tags | +25 |
| Cada aparición en sinopsis | +3 |

Los resultados se almacenan en un Max-Heap para extraer eficientemente los más relevantes.

---

## Algoritmo de Recomendaciones

`GrafoRecomendaciones` implementa un recorrido de dos niveles equivalente a un BFS sobre el grafo bipartito:

1. Desde cada película gustada se obtienen sus tags.
2. Desde cada tag se obtienen películas relacionadas.
3. Se incrementa un contador de similitud por cada tag compartido.
4. Se extraen las Top N mediante un Max-Heap.

Complejidad aproximada:

O(G × T × P + K log K)

Donde:
- G = películas gustadas
- T = promedio de tags por película
- P = promedio de películas por tag

---

## Programación Paralela

La carga del CSV utiliza `std::async` con `launch::async` para procesar lotes de 1000 filas en paralelo.

Cada hilo:
- Preprocesa su lote.
- Inserta en una estructura local.
- Usa `mutex` y `lock_guard` únicamente para fusionar resultados en la Hash Table global.

Esto reduce la contención y mejora el tiempo de inicialización en datasets grandes.

---

## Flujo de Ejecución

Al iniciar el programa:

1. Se carga el CSV.
2. Se indexan títulos y sinopsis en el Suffix Trie.
3. Se construye el grafo de recomendaciones.
4. Se muestra:
   - Lista "Ver más tarde".
   - Recomendaciones basadas en Likes.
   - Menú principal.

Los resultados se muestran paginados de 5 en 5 y pueden consultarse en detalle.

---

## Compilación y Ejecución

Colocar `mpst_full_data.csv` en el mismo directorio del ejecutable.

### Linux / macOS

```bash
g++ -std=c++14 -O2 -pthread main.cpp -o streaming
./streaming



## Referencias Bibliográficas

Cormen, T. H., Leiserson, C. E., Rivest, R. L., & Stein, C. (2009). *Introduction to algorithms* (3rd ed.). MIT Press.

Meyers, S. (2005). *Effective C++: 55 specific ways to improve your programs and designs* (3rd ed.). Addison-Wesley.

Mbrenndoerfer. (n.d.). BM25 search algorithm: Elasticsearch implementation. https://mbrenndoerfer.com/writing/bm25-search-algorithm-elasticsearch-implementation

cppreference.com. (2024). *std::unordered_map*. https://en.cppreference.com/w/cpp/container/unordered_map

cppreference.com. (2024). *std::queue*. https://en.cppreference.com/w/cpp/container/queue
