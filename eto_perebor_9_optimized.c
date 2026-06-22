// eto_perebor_9_optimized.c - Thread-safe parallel version
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <omp.h>
#include <time.h>

#define N 18
#define NUM_EDGES (N * (N - 1) / 2)  // 153
#define MAX_COLOR 4
#define MAX_COMPONENT_SIZE 5

typedef uint64_t bitmask64;

// === Глобальные константы (только для чтения) ===
static bitmask64 vertex_mask[N];
static int edges_u[NUM_EDGES];
static int edges_v[NUM_EDGES];
static int mask[NUM_EDGES];
static int edge_order[NUM_EDGES];

// === Глобальные флаги ===
static bool solution_found = false;
static int solutions_count = 0;
static long long nodes_visited = 0;

// === Базовое состояние (фиксированная часть) ===
static bitmask64 base_adj[N][MAX_COLOR + 1];
static int base_coloring[NUM_EDGES];

// === DFS с ранним выходом (работает с переданным adj) ===
static inline int dfs_component_size_fast(int start_node, int color, bitmask64 adj[N][MAX_COLOR + 1]) {
    bitmask64 visited = 0;
    bitmask64 stack = (bitmask64)1 << start_node;
    int size = 0;
    
    while (stack) {
        int node = __builtin_ctzll(stack);
        stack &= stack - 1;
        if (visited & ((bitmask64)1 << node)) continue;
        visited |= ((bitmask64)1 << node);
        size++;
        if (size > MAX_COMPONENT_SIZE) return size;
        bitmask64 neighbors = adj[node][color] & ~visited;
        stack |= neighbors;
    }
    return size;
}

// === Проверка цвета (работает с переданными массивами) ===
static inline bool check_color_fast(int edge_idx, int color, int *comp_size, 
                                    bitmask64 adj[N][MAX_COLOR + 1]) {
    int u = edges_u[edge_idx];
    int v = edges_v[edge_idx];
    adj[u][color] |= vertex_mask[v];
    adj[v][color] |= vertex_mask[u];
    *comp_size = dfs_component_size_fast(u, color, adj);
    bool valid = (*comp_size <= MAX_COMPONENT_SIZE);
    if (!valid) {
        adj[u][color] &= ~vertex_mask[v];
        adj[v][color] &= ~vertex_mask[u];
        return false;
    }
    return true;
}

// === Откат цвета ===
static inline void undo_color(int edge_idx, int color, bitmask64 adj[N][MAX_COLOR + 1]) {
    int u = edges_u[edge_idx];
    int v = edges_v[edge_idx];
    adj[u][color] &= ~vertex_mask[v];
    adj[v][color] &= ~vertex_mask[u];
}

// === Backtracking с параметрами (локальные копии) ===
static bool backtrack_sequential(int idx, int coloring[NUM_EDGES], bitmask64 adj[N][MAX_COLOR + 1]) {
    nodes_visited++;
    
    while (idx < NUM_EDGES) {
        int e = edge_order[idx];
        if (mask[e] == -1 && coloring[e] == -1) break;
        idx++;
    }
    
    if (idx >= NUM_EDGES) {
        solutions_count++;
        if (solutions_count == 1) {
            solution_found = true;
            for (int i = 0; i < NUM_EDGES; i++) {
                printf("%d ", coloring[i]);
            }
            printf("\n");
            fflush(stdout);
        }
        return true;
    }
    
    int e = edge_order[idx];
    
    // Фиксированное ребро
    if (mask[e] != -1) {
        int c = mask[e];
        int sz;
        if (!check_color_fast(e, c, &sz, adj)) return false;
        coloring[e] = c;
        if (backtrack_sequential(idx + 1, coloring, adj)) return true;
        coloring[e] = -1;
        undo_color(e, c, adj);
        return false;
    }
    
    // Перебор цветов
    for (int c = 0; c <= MAX_COLOR; c++) {
        int sz;
        if (!check_color_fast(e, c, &sz, adj)) continue;
        coloring[e] = c;
        if (backtrack_sequential(idx + 1, coloring, adj)) return true;
        coloring[e] = -1;
        undo_color(e, c, adj);
    }
    return false;
}

// === Параллельный запуск (каждый поток со своими копиями) ===
static void parallel_search_top_level() {
    int first_free_idx = -1;
    for (int i = 0; i < NUM_EDGES; i++) {
        int e = edge_order[i];
        if (mask[e] == -1 && base_coloring[e] == -1) {
            first_free_idx = i;
            break;
        }
    }
    
    if (first_free_idx == -1) {
        printf("All edges fixed.\n");
        return;
    }
    
    int e = edge_order[first_free_idx];
    
    #pragma omp parallel for schedule(dynamic)
    for (int c = 0; c <= MAX_COLOR; c++) {
        if (solution_found) continue;
        
        // Локальные копии для потока
        bitmask64 thread_adj[N][MAX_COLOR + 1];
        int thread_coloring[NUM_EDGES];
        memcpy(thread_adj, base_adj, sizeof(base_adj));
        memcpy(thread_coloring, base_coloring, sizeof(base_coloring));
        
        int sz;
        if (!check_color_fast(e, c, &sz, thread_adj)) continue;
        thread_coloring[e] = c;
        
        if (backtrack_sequential(first_free_idx + 1, thread_coloring, thread_adj)) {
            #pragma omp critical
            {
                if (!solution_found) {
                    solution_found = true;
                }
            }
        }
    }
}

// === Инициализация порядка рёбер ===
static void init_edge_order() {
    int degree[N] = {0};
    for (int i = 0; i < NUM_EDGES; i++) {
        degree[edges_u[i]]++;
        degree[edges_v[i]]++;
    }
    for (int i = 0; i < NUM_EDGES; i++) edge_order[i] = i;
    for (int i = 0; i < NUM_EDGES - 1; i++) {
        for (int j = 0; j < NUM_EDGES - i - 1; j++) {
            int e1 = edge_order[j], e2 = edge_order[j+1];
            int s1 = degree[edges_u[e1]] + degree[edges_v[e1]];
            int s2 = degree[edges_u[e2]] + degree[edges_v[e2]];
            if (s1 < s2) { edge_order[j] = e2; edge_order[j+1] = e1; }
        }
    }
}

int main(void) {
    const char* mask_str = "00001111222233334????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????";
    if ((int)strlen(mask_str) != NUM_EDGES) {
        fprintf(stderr, "Ошибка: длина маски (%zu) != %d\n", strlen(mask_str), NUM_EDGES);
        return 1;
    }
    
    int fixed_count = 0;
    const char *q = strchr(mask_str, '?');
    fixed_count = q ? (int)(q - mask_str) : NUM_EDGES;
    
    printf("=== OPTIMIZED PARALLEL SEARCH ===\n");
    printf("N=%d, Edges=%d, Colors=0..%d, MaxComp=%d\n", N, NUM_EDGES, MAX_COLOR, MAX_COMPONENT_SIZE);
    printf("Fixed: %d, Free: %d\n", fixed_count, NUM_EDGES - fixed_count);
    
    int nthreads = omp_get_max_threads();
    printf("Available threads: %d\n", nthreads);
    omp_set_num_threads(nthreads);
    
    clock_t start_time = clock();
    
    // Инициализация
    for (int u = 0; u < N; u++) vertex_mask[u] = (bitmask64)1 << u;
    int eidx = 0;
    for (int u = 0; u < N; u++) {
        for (int v = u + 1; v < N; v++) {
            edges_u[eidx] = u; edges_v[eidx] = v; eidx++;
        }
    }
    
    memset(mask, -1, sizeof(mask));
    memset(base_coloring, -1, sizeof(base_coloring));
    memset(base_adj, 0, sizeof(base_adj));
    
    // Парсинг маски
    for (int i = 0; i < NUM_EDGES; i++) {
        char c = mask_str[i];
        if (c == '?') mask[i] = -1;
        else if (isdigit(c) && (c - '0') >= 0 && (c - '0') <= MAX_COLOR) mask[i] = c - '0';
        else { printf("Ошибка формата маски на позиции %d: '%c'\n", i, c); return 1; }
    }
    
    // Применение фиксированных значений в base_adj/base_coloring
    for (int i = 0; i < NUM_EDGES; i++) {
        if (mask[i] != -1) {
            int u = edges_u[i], v = edges_v[i], c = mask[i], sz;
            if (!check_color_fast(i, c, &sz, base_adj)) {
                printf("Вектор с заданной маской не существует (ребро %d, цвет %d).\n", i, c);
                return 0;
            }
            base_coloring[i] = c;
        }
    }
    
    init_edge_order();
    printf("Starting parallel search...\n");
    fflush(stdout);
    
    parallel_search_top_level();
    
    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    if (!solution_found) printf("Вектор с заданной маской не существует.\n");
    printf("Time: %.2f seconds\n", time_spent);
    printf("Nodes visited: %lld\n", nodes_visited);
    printf("Solutions found: %d\n", solutions_count);
    
    return 0;
}
