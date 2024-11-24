#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Constants to represent the DEM size (replace with actual size if known)
#define ROWS 907
#define COLS  946

typedef struct {
    int row;
    int col;
    int elevation;
} Cell;

typedef struct {
    Cell* array;
    int size;
    int capacity;
} PriorityQueue;

typedef struct {
    int x;
    int y;
    int area;
    int max_depth;
    int volume;
} Basin;

typedef struct Node {
    Basin data;
    struct Node* next;
} ListNode;

typedef struct {
    ListNode* head;
} LinkedList;

// DEM elevation data (replace with actual DEM data)
int Elevation[ROWS][COLS];
int Spill[ROWS][COLS]; // Assuming you have Spill data as well


// Variables to store information about basins
Basin basins[ROWS * COLS];
int numBasins = 0;
bool CLOSED[ROWS][COLS];

void readCSV(const char* filename, int map[ROWS][COLS]) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("无法打开文件：%s\n", filename);
        return;
    }

    char line[ROWS*COLS]; // Increased buffer size
    int row = 0;
    while (fgets(line, sizeof(line), file)) {
        char* token;
        int col = 0;

        token = strtok(line, ",");
        while (token != NULL) {
            map[row][col] = atoi(token);
            printf("读取值：%d\n", map[row][col]); // Debug output
            col++;
            token = strtok(NULL, ",");
        }

        row++;
        if (row >= ROWS) {
            break;  // Avoid exceeding the array size
        }
    }

    fclose(file);
}
// Priority queue implementation
PriorityQueue* createPriorityQueue(int capacity) {
    PriorityQueue* pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    pq->array = (Cell*)malloc(capacity * sizeof(Cell));
    pq->size = 0;
    pq->capacity = capacity;
    return pq;
}
void swap(Cell* a, Cell* b) {
    Cell temp = *a;
    *a = *b;
    *b = temp;
}

void push(PriorityQueue* pq, int row, int col, int elevation) {
    if (pq->size == pq->capacity) {
        pq->capacity *= 2;
        pq->array = (Cell*)realloc(pq->array, pq->capacity * sizeof(Cell));
    }

    int i = pq->size;
    pq->array[i].row = row;
    pq->array[i].col = col;
    pq->array[i].elevation = elevation;

    while (i != 0 && pq->array[i].elevation < pq->array[(i - 1) / 2].elevation) {
        swap(&pq->array[i], &pq->array[(i - 1) / 2]);
        i = (i - 1) / 2;
    }

    pq->size++;
}
int isEmpty(PriorityQueue* pq) {
    return pq->size == 0;
}

void heapify(PriorityQueue* pq, int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < pq->size && pq->array[left].elevation < pq->array[smallest].elevation)
        smallest = left;
    if (right < pq->size && pq->array[right].elevation < pq->array[smallest].elevation)
        smallest = right;

    if (smallest != i) {
        swap(&pq->array[i], &pq->array[smallest]);
        heapify(pq, smallest);
    }
}

Cell pop(PriorityQueue* pq) {
    if (pq->size == 0) {
        fprintf(stderr, "Error: Priority queue is empty.\n");
        exit(EXIT_FAILURE);
    }

    Cell minCell = pq->array[0];
    pq->array[0] = pq->array[pq->size - 1];
    pq->size--;

    heapify(pq, 0);
    return minCell;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}


void freePriorityQueue(PriorityQueue* pq) {
    free(pq->array);
    free(pq);
}

void dfs(int x, int y, int* area) {
    if (x < 0 || x >= ROWS || y < 0 || y >= COLS || CLOSED[x][y] || Elevation[x][y] > Spill[x][y]) {
        return;
    }

    CLOSED[x][y] = true;
    (*area)++;

    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    for (int i = 0; i < 8; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        dfs(nx, ny, area);
    }
}

void identifyAndProcessBasin(int x, int y, FILE* outputFile) {
    int area = 0;
    int max_depth = 0;
    int volume = 0;
    int cx, cy, nx, ny;

    PriorityQueue* openPQ = createPriorityQueue(ROWS * COLS);
    push(openPQ, x, y, Elevation[x][y]);
    CLOSED[x][y] = true;

    while (!isEmpty(openPQ)) {
        Cell current = pop(openPQ);
        cx = current.row;
        cy = current.col;
        area++;
        max_depth = (Elevation[cx][cy] - Spill[cx][cy] > max_depth) ? Elevation[cx][cy] - Spill[cx][cy] : max_depth;

        // Process neighbors
        int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        for (int i = 0; i < 8; i++) {
            nx = cx + dx[i];
            ny = cy + dy[i];
            if (nx >= 0 && nx < ROWS && ny >= 0 && ny < COLS && !CLOSED[nx][ny] && Elevation[nx][ny] <= Spill[cx][cy]) {
                push(openPQ, nx, ny, max(Spill[cx][cy], Elevation[nx][ny]));
                CLOSED[nx][ny] = true;
            }
        }
    }

    // Calculate volume
    for (int i = x; i < x + area; i++) {
        for (int j = y; j < y + area; j++) {
            volume += (Elevation[i][j] - Spill[i][j]);
        }
    }

    
    // Store basin information in the list
    Basin newBasin;
    newBasin.x = x;
    newBasin.y = y;
    newBasin.area = area;
    newBasin.max_depth = max_depth;
    newBasin.volume = volume;
    basins[numBasins++] = newBasin;

    // Write basin information to the file
    fprintf(outputFile, "Basin at (%d, %d):\n", x, y);
    fprintf(outputFile, "Area: %d\n", area);
    fprintf(outputFile, "Max Depth: %d\n", max_depth);
    fprintf(outputFile, "Volume: %d\n\n", volume);

    freePriorityQueue(openPQ);
}





// Function to create a boolean array CLOSED initialized to false
void initializeClosedArray() {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            CLOSED[i][j] = false;
        }
    }
}



// Function to compute statistics for the whole region based on the information of basins
void computeStatistics() {
    int totalBasins = numBasins;
    int totalArea = 0;
    int totalMaxDepth = 0;
    int totalVolume = 0;

    for (int i = 0; i < totalBasins; i++) {
        totalArea += basins[i].area;
        totalMaxDepth = (basins[i].max_depth > totalMaxDepth) ? basins[i].max_depth : totalMaxDepth;
        totalVolume += basins[i].volume;
    }

    // Open the file for writing
    FILE* outputFile = fopen("statistics.txt", "w");
    if (outputFile == NULL) {
        printf("Error: Cannot open the file for writing.\n");
        return;
    }

    // Write the computed statistics to the file
    fprintf(outputFile, "Total number of basins: %d\n", totalBasins);
    fprintf(outputFile, "Total area of basins: %d\n", totalArea);
    fprintf(outputFile, "Total maximum depth of basins: %d\n", totalMaxDepth);
    fprintf(outputFile, "Total volume of basins: %d\n", totalVolume);

    // Close the file
    fclose(outputFile);
}

// Function to free memory for the LinkedList
void freeLinkedList(LinkedList* list) {
    ListNode* current = list->head;
    while (current != NULL) {
        ListNode* next = current->next;
        free(current);
        current = next;
    }
    free(list);
}



int main() {
    
    const char* filename = "D:/2023/数据结构/ASTER_GDEM_GD/clip_2.csv";
    readCSV(filename, Elevation);

    // Initialize data structures
    initializeClosedArray();

    // Open the file for writing
    FILE* outputFile = fopen("basin_info.txt", "w");
    if (outputFile == NULL) {
        printf("Error: Cannot open the file for writing.\n");
        return 1;
    }
    
    // Identify and process basins for each cell in the boundary of the DEM
    for (int i = 0; i < ROWS; i++) {
        identifyAndProcessBasin(i, 0, outputFile);
        identifyAndProcessBasin(i, COLS - 1, outputFile);
    }

    for (int j = 1; j < COLS - 1; j++) {
        identifyAndProcessBasin(0, j, outputFile);
        identifyAndProcessBasin(ROWS - 1, j, outputFile);
    }

    // Close the file
    fclose(outputFile);


    // Compute statistics for the whole region based on the information of basins
    computeStatistics();
    while(1){
	}

    return 0;
}

