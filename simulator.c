/*The main program will have graphics related code and your main logic to
process the queue and visualize them*/
// #include <stdio.h>

// int main()
// {
//     return 0;
// }


#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h> 
#include <stdio.h> 
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_LINE_LENGTH 20
#define MAIN_FONT "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1.1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15
#define MAX_QUEUE_SIZE 100
#define MAX_VEHICLE_ID 20


const char* VEHICLE_FILE = "vehicles.data";

typedef struct{
    int currentLight;
    int nextLight;
} SharedData;

// adding queue structures
// Vehicle structure
typedef struct {
    char id[MAX_VEHICLE_ID];
    char lane;              // A/B/C/D
    int arrivalTime;
    bool isEmergency;
    int lane_number;        // 1 for left, 2 for middle, 3 for right
    float animPos;          // field for animation
} Vehicle;

// Queue structure
typedef struct {
    Vehicle* vehicles[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
} VehicleQueue;

// global queue variables
VehicleQueue* queueA;
VehicleQueue* queueB;
VehicleQueue* queueC;
VehicleQueue* queueD;

// queue operations:
VehicleQueue* createQueue() {
    VehicleQueue* queue = (VehicleQueue*)malloc(sizeof(VehicleQueue));
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    pthread_mutex_init(&queue->lock, NULL);
    return queue;
}

bool isQueueFull(VehicleQueue* queue) {
    return queue->size >= MAX_QUEUE_SIZE;
}

bool isQueueEmpty(VehicleQueue* queue) {
    return queue->size == 0;
}

void enqueue(VehicleQueue* queue, Vehicle* vehicle) {
    pthread_mutex_lock(&queue->lock);
    if (!isQueueFull(queue)) {
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
        queue->vehicles[queue->rear] = vehicle;
        queue->size++;
        printf("Enqueued vehicle %s to lane %c (size: %d)\n", 
               vehicle->id, vehicle->lane, queue->size);
    } else {
        printf("Queue for lane %c is full!\n", vehicle->lane);
    }
    pthread_mutex_unlock(&queue->lock);
}

Vehicle* dequeue(VehicleQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    Vehicle* vehicle = NULL;
    if (!isQueueEmpty(queue)) {
        vehicle = queue->vehicles[queue->front];
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
        queue->size--;
        printf("Dequeued vehicle %s from lane %c (size: %d)\n", 
               vehicle->id, vehicle->lane, queue->size);
    }
    pthread_mutex_unlock(&queue->lock);
    return vehicle;
}

// Added unlocked dequeue version (assumes lock is held)
Vehicle* dequeueUnlocked(VehicleQueue* queue) {
    Vehicle* vehicle = NULL;
    if (!isQueueEmpty(queue)) {
        vehicle = queue->vehicles[queue->front];
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
        queue->size--;
        printf("Dequeued vehicle %s from lane %c (size: %d) [unlocked]\n", 
               vehicle->id, vehicle->lane, queue->size);
    }
    return vehicle;
}

// queue cleanup
void cleanupQueue(VehicleQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    for (int i = 0; i < queue->size; i++) {
        int idx = (queue->front + i) % MAX_QUEUE_SIZE;
        free(queue->vehicles[idx]);
    }
    pthread_mutex_unlock(&queue->lock);
    pthread_mutex_destroy(&queue->lock);
    free(queue);
}

// Function declarations
bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font);
void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y);
void drawLightForB(SDL_Renderer* renderer, bool isRed);
void drawLightForA(SDL_Renderer* renderer, bool isRed);
void drawLightForC(SDL_Renderer* renderer, bool isRed);
void drawLightForD(SDL_Renderer* renderer, bool isRed);
void refreshLight(SDL_Renderer *renderer, SharedData* sharedData);
void* chequeQueue(void* arg);
void* readAndParseFile(void* arg);
void drawVehicle(SDL_Renderer *renderer, TTF_Font *font, Vehicle *v, int pos);
void drawVehiclesFromQueue(SDL_Renderer *renderer, TTF_Font *font, VehicleQueue *queue);
void drawVehicles(SDL_Renderer *renderer, TTF_Font *font);
void updateVehicles(SharedData* sharedData);
void* processVehiclesSequentially(void* arg);

void printMessageHelper(const char* message, int count) {
    for (int i = 0; i < count; i++) printf("%s\n", message);
    for (int i = 0; i < count; i++) printf("%s\n", message);
}

int main(int argc, char* argv[]) {
    pthread_t tQueue, tReadFile;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;    
    SDL_Event event;    

    if (!initializeSDL(&window, &renderer)) {
        return -1;
    }
    SDL_mutex* mutex = SDL_CreateMutex();
    SharedData sharedData = { 0, 0 }; // 0 => all red
    
    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 24);
    if (!font) SDL_Log("Failed to load font: %s", TTF_GetError());

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer, font);
    drawLightForB(renderer, false);
    SDL_RenderPresent(renderer);

    // Initialize queues before creating threads
    queueA = createQueue();
    queueB = createQueue();
    queueC = createQueue();
    queueD = createQueue();

    // we need to create seprate long running thread for the queue processing and light
    // pthread_create(&tLight, NULL, refreshLight, &sharedData);
    pthread_create(&tQueue, NULL, chequeQueue, &sharedData);
    pthread_create(&tReadFile, NULL, processVehiclesSequentially, NULL);
    // readAndParseFile();

    // Continue the UI thread
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) running = false;
        updateVehicles(&sharedData);  // now synced with traffic lightr animation
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        drawRoadsAndLane(renderer, font);
        refreshLight(renderer, &sharedData);
        drawVehicles(renderer, font);
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }
    SDL_DestroyMutex(mutex);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    // Add cleanup before SDL_Quit
    cleanupQueue(queueA);
    cleanupQueue(queueB);
    cleanupQueue(queueC);
    cleanupQueue(queueD);
    // pthread_kil
    SDL_Quit();
    return 0;
}

bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    // font init
    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }


    *window = SDL_CreateWindow("Junction Diagram",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH*SCALE, WINDOW_HEIGHT*SCALE,
                               SDL_WINDOW_SHOWN);
    if (!*window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    // if you have high resolution monitor 2K or 4K then scale
    SDL_RenderSetScale(*renderer, SCALE, SCALE);

    if (!*renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(*window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    return true;
}


void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}


void drawArrwow(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3) {
    // Sort vertices by ascending Y (bubble sort approach)
    if (y1 > y2) { swap(&y1, &y2); swap(&x1, &x2); }
    if (y1 > y3) { swap(&y1, &y3); swap(&x1, &x3); }
    if (y2 > y3) { swap(&y2, &y3); swap(&x2, &x3); }

    // Compute slopes
    float dx1 = (y2 - y1) ? (float)(x2 - x1) / (y2 - y1) : 0;
    float dx2 = (y3 - y1) ? (float)(x3 - x1) / (y3 - y1) : 0;
    float dx3 = (y3 - y2) ? (float)(x3 - x2) / (y3 - y2) : 0;

    float sx1 = x1, sx2 = x1;

    // Fill first part (top to middle)
    for (int y = y1; y < y2; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx1;
        sx2 += dx2;
    }

    sx1 = x2;

    // Fill second part (middle to bottom)
    for (int y = y2; y <= y3; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx3;
        sx2 += dx2;
    }
}


void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
    // Draw intersection roads (dark gray)
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect vRoad = {WINDOW_WIDTH/2 - ROAD_WIDTH/2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &vRoad);
    SDL_Rect hRoad = {0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &hRoad);

    // dashed lane markings inside the intersection
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int dashLength = 15, gapLength = 10;
    for (int i = 0; i <= 3; i++) {
        int x = WINDOW_WIDTH/2 - ROAD_WIDTH/2 + i * LANE_WIDTH;
        for (int y = WINDOW_HEIGHT/2 - ROAD_WIDTH/2; y < WINDOW_HEIGHT/2 + ROAD_WIDTH/2; y += dashLength + gapLength)
            SDL_RenderDrawLine(renderer, x, y, x, y + dashLength);
    }
    for (int i = 0; i <= 3; i++) {
        int y = WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + i * LANE_WIDTH;
        for (int x = WINDOW_WIDTH/2 - ROAD_WIDTH/2; x < WINDOW_WIDTH/2 + ROAD_WIDTH/2; x += dashLength + gapLength)
            SDL_RenderDrawLine(renderer, x, y, x + dashLength, y);
    }

    // extended roads arms

    // road A (north): extended area from y = 0 to top of intersection.
    int roadA_x = WINDOW_WIDTH/2 - ROAD_WIDTH/2;
    int roadA_yStart = 0, roadA_yEnd = WINDOW_HEIGHT/2 - ROAD_WIDTH/2;
    int laneWidthAB = ROAD_WIDTH / 3;
    // draw vertical dividing lines inside road A.
    for (int i = 1; i < 3; i++) {
         int x = roadA_x + i * laneWidthAB;
         SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
         SDL_RenderDrawLine(renderer, x, roadA_yStart, x, roadA_yEnd);
    }
    // road B (south): extended area from bottom of intersection to window bottom.
    int roadB_x = WINDOW_WIDTH/2 - ROAD_WIDTH/2;
    int roadB_yStart = WINDOW_HEIGHT/2 + ROAD_WIDTH/2, roadB_yEnd = WINDOW_HEIGHT;
    for (int i = 1; i < 3; i++) {
         int x = roadB_x + i * laneWidthAB;
         SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
         SDL_RenderDrawLine(renderer, x, roadB_yStart, x, roadB_yEnd);
    }

    // road C (east): extended area from x = WINDOW_WIDTH/2 + ROAD_WIDTH/2 to WINDOW_WIDTH.
    int roadC_y = WINDOW_HEIGHT/2 - ROAD_WIDTH/2;
    int roadC_xStart = WINDOW_WIDTH/2 + ROAD_WIDTH/2, roadC_xEnd = WINDOW_WIDTH;
    int laneHeightCD = ROAD_WIDTH / 3;
    // Draw horizontal dividing lines for road C.
    for (int i = 1; i < 3; i++) {
         int y = roadC_y + i * laneHeightCD;
         SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
         SDL_RenderDrawLine(renderer, roadC_xStart, y, roadC_xEnd, y);
    }
    // road D (west): extended area from x = 0 to WINDOW_WIDTH/2 - ROAD_WIDTH/2.
    int roadD_y = WINDOW_HEIGHT/2 - ROAD_WIDTH/2;
    int roadD_xStart = 0, roadD_xEnd = WINDOW_WIDTH/2 - ROAD_WIDTH/2;
    for (int i = 1; i < 3; i++) {
         int y = roadD_y + i * laneHeightCD;
         SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
         SDL_RenderDrawLine(renderer, roadD_xStart, y, roadD_xEnd, y);
    }
    
    // Road A labels:
    int roadA_centerX = WINDOW_WIDTH/2;
    int laneHeightA = roadA_yEnd - roadA_yStart;
    for (int i = 0; i < 3; i++) {
         char label[4];
         sprintf(label, "A%d", i+1);
         int labelY = roadA_yStart + (i * laneHeightA + laneHeightA/2);
         displayText(renderer, font, label, roadA_centerX, labelY);
    }
    // Road B labels:
    int roadB_centerX = WINDOW_WIDTH/2;
    int laneHeightB = roadB_yEnd - roadB_yStart;
    for (int i = 0; i < 3; i++) {
         char label[4];
         sprintf(label, "B%d", i+1);
         int labelY = roadB_yStart + (i * laneHeightB + laneHeightB/2);
         displayText(renderer, font, label, roadB_centerX, labelY);
    }
    // Road C labels (east)
    int roadC_centerY = WINDOW_HEIGHT/2;
    int laneWidthC = ROAD_WIDTH / 3;
    int roadC_centerX = roadC_xStart + laneWidthC/2;
    for (int i = 0; i < 3; i++) {
         char label[4];
         sprintf(label, "C%d", i+1);
         int labelX = roadC_xStart + (i * laneWidthC + laneWidthC/2);
         displayText(renderer, font, label, labelX, roadC_centerY);
    }
    // Road D labels (west)
    int roadD_centerY = WINDOW_HEIGHT/2;
    int laneWidthD = ROAD_WIDTH / 3;
    int roadD_centerX = roadD_xEnd - laneWidthD/2;
    for (int i = 0; i < 3; i++) {
         char label[4];
         sprintf(label, "D%d", i+1);
         int labelX = roadD_xStart + (i * laneWidthD + laneWidthD/2);
         displayText(renderer, font, label, labelX, roadD_centerY);
    }
    
    // Intersection direction labels
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    displayText(renderer, font, "A", WINDOW_WIDTH/2, 10);
    displayText(renderer, font, "B", WINDOW_WIDTH/2, WINDOW_HEIGHT - 30);
    displayText(renderer, font, "D", 10, WINDOW_HEIGHT/2);
    displayText(renderer, font, "C", WINDOW_WIDTH - 30, WINDOW_HEIGHT/2);
}

void drawLightForA(SDL_Renderer* renderer, bool isRed) {
    SDL_Rect lightBox = {388, 288, 70, 30};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &lightBox);

    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect innerBox = {389, 289, 68, 28};
    SDL_RenderFillRect(renderer, &innerBox);
    
    // Right turn light - always green
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect right_Light = {433, 293, 20, 20};
    SDL_RenderFillRect(renderer, &right_Light);
    
    // Straight light - controlled by traffic signal
    if(isRed) 
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else 
        SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect straight_Light = {393, 293, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
}

void drawLightForB(SDL_Renderer* renderer, bool isRed) {
    SDL_Rect lightBox = {325, 488, 80, 30};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &lightBox);
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect innerBox = {326, 489, 78, 28};
    SDL_RenderFillRect(renderer, &innerBox);
    
    // left lane light -> always green
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect left_Light = {330, 493, 20, 20};
    SDL_RenderFillRect(renderer, &left_Light);
    
    // middle lane light -> controlled by traffic signal
    if(isRed) 
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else 
        SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect middle_Light = {380, 493, 20, 20};
    SDL_RenderFillRect(renderer, &middle_Light);
}

void drawLightForC(SDL_Renderer* renderer, bool isRed) {
    SDL_Rect lightBox = {488, 388, 30, 70};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &lightBox);
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect innerBox = {489, 389, 28, 68};
    SDL_RenderFillRect(renderer, &innerBox);
    
    // right turn light - always green
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect right_Light = {493, 433, 20, 20};
    SDL_RenderFillRect(renderer, &right_Light);
    
    // straight light 
    if(isRed) 
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else 
        SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect straight_Light = {493, 393, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
}

void drawLightForD(SDL_Renderer* renderer, bool isRed) {
    SDL_Rect lightBox = {288, 325, 30, 90};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &lightBox);
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect innerBox = {289, 326, 28, 88};
    SDL_RenderFillRect(renderer, &innerBox);
    
    // left turn light - always green
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect left_turn_Light = {293, 330, 20, 20};
    SDL_RenderFillRect(renderer, &left_turn_Light);
    
    // middle lane light
    if(isRed) 
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else 
        SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);
    SDL_Rect middle_Light = {293, 380, 20, 20};
    SDL_RenderFillRect(renderer, &middle_Light);
}

void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y){
    // display necessary text
    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    SDL_Rect textRect = {x,y,0,0 };
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    // SDL_Log("DIM of SDL_Rect %d %d %d %d", textRect.x, textRect.y, textRect.h, textRect.w);
    // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
}


void refreshLight(SDL_Renderer *renderer, SharedData* sharedData) {
    if(sharedData->nextLight == sharedData->currentLight) return;
    
    
    
    if(sharedData->nextLight == 0) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    else if(sharedData->nextLight == 1) {
        drawLightForA(renderer, false);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    else if(sharedData->nextLight == 2) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, false);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    else if(sharedData->nextLight == 3) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, false);
        drawLightForD(renderer, true);
    }
    else if(sharedData->nextLight == 4) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, false);
    }
    
    // Only update the stored state after debounce
    printf("Light of queue updated from %d to %d\n", sharedData->currentLight, sharedData->nextLight);
    sharedData->currentLight = sharedData->nextLight;
    fflush(stdout);
}

void* chequeQueue(void* arg) {
    SharedData* sharedData = (SharedData*)arg;
    while (1) {
        // All lights red
        sharedData->nextLight = 0;
        sleep(2);
        // Cycle through each lane
        for (int lane = 1; lane <= 4; lane++) {
            sharedData->nextLight = lane;
            sleep(5);
        }
    }
    return NULL;
}

void* readAndParseFile(void* arg) {
    while (1) {
        FILE* file = fopen(VEHICLE_FILE, "r");
        if (!file) {
            perror("Error opening file");
            sleep(2);
            continue;
        }
        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            // Remove newline if present
            line[strcspn(line, "\n")] = 0;
            // Split using ':'
            char* vehicleNumber = strtok(line, ":");
            char* road = strtok(NULL, ":");
            if (vehicleNumber && road) {
                Vehicle* newVehicle = (Vehicle*)malloc(sizeof(Vehicle));
                strncpy(newVehicle->id, vehicleNumber, MAX_VEHICLE_ID - 1);
                newVehicle->id[MAX_VEHICLE_ID - 1] = '\0';
                newVehicle->lane = road[0];
                newVehicle->arrivalTime = time(NULL);
                newVehicle->isEmergency = (strstr(vehicleNumber, "EMG") != NULL);

                if (strstr(vehicleNumber, "L1"))
                    newVehicle->lane_number = 1;
                else if (strstr(vehicleNumber, "L2"))
                    newVehicle->lane_number = 2;
                else if (strstr(vehicleNumber, "L3"))
                    newVehicle->lane_number = 3;
                else 
                    newVehicle->lane_number = 2; // default

                // Initialize animPos based on lane:
                if (road[0] == 'A')
                    newVehicle->animPos = 0.0f;                
                else if (road[0] == 'B')
                    newVehicle->animPos = (float)WINDOW_HEIGHT;
                else if (road[0] == 'C')
                    newVehicle->animPos = (float)WINDOW_WIDTH;
                else if (road[0] == 'D')
                    newVehicle->animPos = 0.0f;

                switch(newVehicle->lane) {
                    case 'A': enqueue(queueA, newVehicle); break;
                    case 'B': enqueue(queueB, newVehicle); break;
                    case 'C': enqueue(queueC, newVehicle); break;
                    case 'D': enqueue(queueD, newVehicle); break;
                    default: free(newVehicle);
                }
            }
        }
        fclose(file);
        sleep(2);
    }
    return NULL;
}

// drwaing a single vehicle as a colored rectangle and optionally display its ID.
void drawVehicle(SDL_Renderer *renderer, TTF_Font *font, Vehicle *v, int pos) {
    int w = 20, h = 10;
    int x = 0, y = 0;
    // animPos for dynamic positioning based on lane:
    switch(v->lane) {
        case 'A': 
            x = WINDOW_WIDTH/2 + 10;
            y = (int)v->animPos;  // lane A: y-coordinate from animPos
            break;
        case 'B': 
            x = WINDOW_WIDTH/2 + 10;
            y = (int)v->animPos;  // lane B: y-coordinate from animPos (decreases over time)
            break;
        case 'C': 
            x = (int)v->animPos;  // lane C: x-coordinate from animPos (decreases)
            y = WINDOW_HEIGHT/2 + 10;
            break;
        case 'D': 
            x = (int)v->animPos;  // lane D: x-coordinate from animPos (increases)
            y = WINDOW_HEIGHT/2 + 10;
            break;
        default:
            break;
    }
    // using red for emergency vehicles, otherwise blue.
    if(v->isEmergency)
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
    // drawing the vehicle ID above the rectangle.
    char idLabel[MAX_VEHICLE_ID];
    strncpy(idLabel, v->id, MAX_VEHICLE_ID);
    idLabel[MAX_VEHICLE_ID-1] = '\0';
    displayText(renderer, font, idLabel, x, y - h - 2);
}

// drawing vehicles from a given queue.
void drawVehiclesFromQueue(SDL_Renderer *renderer, TTF_Font *font, VehicleQueue *queue) {
    pthread_mutex_lock(&queue->lock);
    for (int i = 0; i < queue->size; i++) {
        int idx = (queue->front + i) % MAX_QUEUE_SIZE;
        drawVehicle(renderer, font, queue->vehicles[idx], i);
    }
    pthread_mutex_unlock(&queue->lock);
}

// drawing vehicles from all queues.
void drawVehicles(SDL_Renderer *renderer, TTF_Font *font) {
    drawVehiclesFromQueue(renderer, font, queueA);
    drawVehiclesFromQueue(renderer, font, queueB);
    drawVehiclesFromQueue(renderer, font, queueC);
    drawVehiclesFromQueue(renderer, font, queueD);
}


void updateVehicles(SharedData* sharedData) {
    float speed = 0.05f;  // adjust to slow down the vehicles
    Uint32 currentTime = SDL_GetTicks();
    static Uint32 lastTime = 0;
    if (lastTime == 0) 
        lastTime = currentTime;
    Uint32 delta = currentTime - lastTime;
    lastTime = currentTime;

    // Determine active lane letter based on sharedData->currentLight
    char activeLane = '\0';
    if (sharedData->currentLight == 1) activeLane = 'A';
    else if (sharedData->currentLight == 2) activeLane = 'B';
    else if (sharedData->currentLight == 3) activeLane = 'C';
    else if (sharedData->currentLight == 4) activeLane = 'D';
    
    // Lane A: vehicles coming from north, move downward (y increases)
    pthread_mutex_lock(&queueA->lock);
    if (activeLane == 'A') {
        for (int i = 0; i < queueA->size; i++) {
            int idx = (queueA->front + i) % MAX_QUEUE_SIZE;
            queueA->vehicles[idx]->animPos += speed * delta;
        }
    }
    // Remove lane A vehicles that exit the window (y > WINDOW_HEIGHT)
    while (!isQueueEmpty(queueA) && queueA->vehicles[queueA->front]->animPos > WINDOW_HEIGHT) {
        dequeueUnlocked(queueA);  // unlocked version (lock already held)
    }
    pthread_mutex_unlock(&queueA->lock);

    // Lane B: vehicles coming from south, move upward (y decreases)
    pthread_mutex_lock(&queueB->lock);
    if (activeLane == 'B') {
        for (int i = 0; i < queueB->size; i++) {
            int idx = (queueB->front + i) % MAX_QUEUE_SIZE;
            queueB->vehicles[idx]->animPos -= speed * delta;
        }
    }
    // Remove lane B vehicles once they leave the window (y < 0)
    while (!isQueueEmpty(queueB) && queueB->vehicles[queueB->front]->animPos < 0) {
        dequeueUnlocked(queueB);
    }
    pthread_mutex_unlock(&queueB->lock);

    // Lane C: vehicles coming from east, move left (x decreases)
    pthread_mutex_lock(&queueC->lock);
    if (activeLane == 'C') {
        for (int i = 0; i < queueC->size; i++) {
            int idx = (queueC->front + i) % MAX_QUEUE_SIZE;
            queueC->vehicles[idx]->animPos -= speed * delta;
        }
    }
    // Remove lane C vehicles once they leave the window (x < 0)
    while (!isQueueEmpty(queueC) && queueC->vehicles[queueC->front]->animPos < 0) {
        dequeueUnlocked(queueC);
    }
    pthread_mutex_unlock(&queueC->lock);

    // Lane D: vehicles coming from west, move right (x increases)
    pthread_mutex_lock(&queueD->lock);
    if (activeLane == 'D') {
        for (int i = 0; i < queueD->size; i++) {
            int idx = (queueD->front + i) % MAX_QUEUE_SIZE;
            queueD->vehicles[idx]->animPos += speed * delta;
        }
    }
    // Remove lane D vehicles once they leave the window (x > WINDOW_WIDTH)
    while (!isQueueEmpty(queueD) && queueD->vehicles[queueD->front]->animPos > WINDOW_WIDTH) {
        dequeueUnlocked(queueD);
    }
    pthread_mutex_unlock(&queueD->lock);
}

// delay reduced to 3 sec
void* processVehiclesSequentially(void* arg) {
    FILE* file = fopen(VEHICLE_FILE, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    char **lines = NULL;
    size_t count = 0;
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\n")] = 0;
        lines = realloc(lines, sizeof(char*) * (count + 1));
        lines[count] = strdup(buffer);
        count++;
    }
    fclose(file);

    for (size_t i = 0; i < count; i++) {
        char* line = lines[i];
        char* vehicleNumber = strtok(line, ":");
        char* road = strtok(NULL, ":");
        if (vehicleNumber && road) {
            Vehicle* newVehicle = (Vehicle*)malloc(sizeof(Vehicle));
            strncpy(newVehicle->id, vehicleNumber, MAX_VEHICLE_ID - 1);
            newVehicle->id[MAX_VEHICLE_ID - 1] = '\0';
            newVehicle->lane = road[0];
            newVehicle->arrivalTime = time(NULL);
            newVehicle->isEmergency = (strstr(vehicleNumber, "EMG") != NULL);
            if (strstr(vehicleNumber, "L1"))
                newVehicle->lane_number = 1;
            else if (strstr(vehicleNumber, "L2"))
                newVehicle->lane_number = 2;
            else if (strstr(vehicleNumber, "L3"))
                newVehicle->lane_number = 3;
            else
                newVehicle->lane_number = 2;
            if (road[0] == 'A')
                newVehicle->animPos = 0.0f;
            else if (road[0] == 'B')
                newVehicle->animPos = (float)WINDOW_HEIGHT;
            else if (road[0] == 'C')
                newVehicle->animPos = (float)WINDOW_WIDTH;
            else if (road[0] == 'D')
                newVehicle->animPos = 0.0f;
            switch(newVehicle->lane) {
                case 'A': enqueue(queueA, newVehicle); break;
                case 'B': enqueue(queueB, newVehicle); break;
                case 'C': enqueue(queueC, newVehicle); break;
                case 'D': enqueue(queueD, newVehicle); break;
                default: free(newVehicle);
            }
        }
        sleep(3); // Delay 3 seconds between vehicles
    }
    for (size_t i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
    return NULL;
}