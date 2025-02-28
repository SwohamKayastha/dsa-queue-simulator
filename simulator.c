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
#define MAIN_FONT "DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1.1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15
#define MAX_QUEUE_SIZE 100
#define MAX_VEHICLE_ID 20
#define VEHICLE_LENGTH 20  // Length of vehicle rectangle
#define VEHICLE_GAP 15    // Minimum gap between vehicles
#define VEHICLE_WIDTH 10  // Width of vehicle rectangle
#define TURN_DURATION 1500.0f
#define BEZIER_CONTROL_OFFSET 80.0f
#define TURN_SPEED 0.0008f


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
    bool turning;           // new: indicates if a turn is in progress
    float turnProgress;     // new: progress value from 0.0 to 1.0 for a turn
    float turnPosX;         // New fields for turning animation positions (x and y)
    float turnPosY;
    float angle;
    float targetAngle;
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

// Function to get queue size
int getQueueSize(VehicleQueue *queue) {
    return queue->size;
}

// Function declarations
bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font);
void displayText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y);
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
void drawUI(SDL_Renderer *renderer, SharedData* sharedData);
void drawLaneCongestion(SDL_Renderer *renderer, int x, int y, int numVehicles, char lane);
void rotateVehicle(Vehicle* vehicle, Uint32 delta);

void printMessageHelper(const char* message, int count) {
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
        drawUI(renderer, &sharedData);
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
    // Terminate threads before exiting
    pthread_kill(tQueue, SIGTERM);
    pthread_kill(tReadFile, SIGTERM);
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

#define MAX_VEHICLES 10
// Function to draw the lane congestion view (progress bars)
void drawLaneCongestion(SDL_Renderer *renderer, int x, int y, int numVehicles, char lane) {
    int barWidth = 180, barHeight = 24;
    
    // Background
    SDL_Rect background = {x, y, barWidth, barHeight};
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderFillRect(renderer, &background);
    
    // Border
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &background);

    // Fill based on congestion level
    if (numVehicles > 0) {
        // Change color based on congestion level
        if (numVehicles < MAX_VEHICLES/3) {
            SDL_SetRenderDrawColor(renderer, 50, 200, 50, 255);  // Green for low congestion
        } else if (numVehicles < 2*MAX_VEHICLES/3) {
            SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255);  // Orange for medium congestion
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);  // Red for high congestion
        }
        
        int fillWidth = (barWidth * numVehicles) / MAX_VEHICLES;
        SDL_Rect fill = {x, y, fillWidth, barHeight};
        SDL_RenderFillRect(renderer, &fill);
    }
    
    // Draw markers for thresholds
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, x + barWidth/3, y, x + barWidth/3, y + barHeight);
    SDL_RenderDrawLine(renderer, x + 2*barWidth/3, y, x + 2*barWidth/3, y + barHeight);
    
    // Label for the lane
    char label[20];
    sprintf(label, "Lane %c: %d", lane, numVehicles);
    // Note: displayText implementation is assumed from the existing code
}

// Updated draw function to include lane congestion visualization and traffic statistics
void drawUI(SDL_Renderer *renderer, SharedData *sharedData) {
    TTF_Font* smallFont = TTF_OpenFont(MAIN_FONT, 16);
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 200);
    
    // UI background panel
    SDL_Rect uiPanel = {20, 20, 200, 200};
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 220);
    SDL_RenderFillRect(renderer, &uiPanel);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &uiPanel);
    
    // Display title
    SDL_Rect titleRect = {30, 25, 180, 25};
    SDL_SetRenderDrawColor(renderer, 50, 50, 100, 255);
    SDL_RenderFillRect(renderer, &titleRect);
    
    // Get queue sizes
    int queueA_size = getQueueSize(queueA);
    int queueB_size = getQueueSize(queueB);
    int queueC_size = getQueueSize(queueC);
    int queueD_size = getQueueSize(queueD);
    
    // Calculate total and average
    int totalVehicles = queueA_size + queueB_size + queueC_size + queueD_size;
    float avgWaitingTime = 0; // This would require tracking entry/exit times
    
    // Draw congestion bars
    drawLaneCongestion(renderer, 30, 60, queueA_size, 'A');
    drawLaneCongestion(renderer, 30, 90, queueB_size, 'B');
    drawLaneCongestion(renderer, 30, 120, queueC_size, 'C');
    drawLaneCongestion(renderer, 30, 150, queueD_size, 'D');
    
    // Text labels using the existing displayText function
    if (smallFont) {
        displayText(renderer, smallFont, "Traffic Monitor", 40, 30);
        
        char statsText[50];
        sprintf(statsText, "Total: %d vehicles", totalVehicles);
        displayText(renderer, smallFont, statsText, 30, 180);
        
        // Display current active lane
        char activeLaneText[20];
        if (sharedData->currentLight == 1)
            sprintf(activeLaneText, "Active: Lane A");
        else if (sharedData->currentLight == 2)
            sprintf(activeLaneText, "Active: Lane B");
        else if (sharedData->currentLight == 3)
            sprintf(activeLaneText, "Active: Lane C");
        else if (sharedData->currentLight == 4)
            sprintf(activeLaneText, "Active: Lane D");
        else
            sprintf(activeLaneText, "Active: None");
        // This would need to be set based on the current traffic light state
        displayText(renderer, smallFont, activeLaneText, 30, 200);
        
        TTF_CloseFont(smallFont);
    }
    
    // Draw real-time traffic flow indicator
    int flowIndicatorX = 650;
    int flowIndicatorY = 30;
    int flowIndicatorSize = 40;
    
    // Traffic flow circular indicator
    SDL_Rect flowRect = {flowIndicatorX, flowIndicatorY, flowIndicatorSize, flowIndicatorSize};
    
    // Color based on overall traffic condition
    if (totalVehicles < MAX_VEHICLES) {
        SDL_SetRenderDrawColor(renderer, 50, 200, 50, 255); // Green - good flow
    } else if (totalVehicles < 2*MAX_VEHICLES) {
        SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); // Orange - moderate congestion
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255); // Red - heavy congestion
    }
    
    // // Draw filled circle (approximated with multiple rectangles)
    // for (int i = 0; i < flowIndicatorSize; i++) {
    //     int arcHeight = sqrt(flowIndicatorSize*flowIndicatorSize - (i-flowIndicatorSize/2)*(i-flowIndicatorSize/2));
    //     SDL_Rect arcRect = {
    //         flowIndicatorX + i, 
    //         flowIndicatorY + flowIndicatorSize/2 - arcHeight/2, 
    //         1, 
    //         arcHeight
    //     };
    //     SDL_RenderFillRect(renderer, &arcRect);
    // }
    
    // Update the renderer
    SDL_RenderPresent(renderer);
}

void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
    // Clear background with light color
    SDL_SetRenderDrawColor(renderer, 240, 240, 230, 255);
    SDL_Rect backgroundRect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &backgroundRect);
    
    // Draw road surface
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    
    // Vertical road
    SDL_Rect vRoad = {WINDOW_WIDTH/2 - ROAD_WIDTH/2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &vRoad);
    
    // Horizontal road
    SDL_Rect hRoad = {0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &hRoad);
    
    // Draw lane markings
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    // Draw lanes for vertical roads (A and B)
    for (int i = 1; i < 3; i++) {
        int x = WINDOW_WIDTH/2 - ROAD_WIDTH/2 + i * LANE_WIDTH;
        
        // North of intersection
        for (int y = 20; y < WINDOW_HEIGHT/2 - ROAD_WIDTH/2; y += 30) {
            SDL_Rect dash = {x-1, y, 2, 15};
            SDL_RenderFillRect(renderer, &dash);
        }
        
        // South of intersection
        for (int y = WINDOW_HEIGHT/2 + ROAD_WIDTH/2; y < WINDOW_HEIGHT - 20; y += 30) {
            SDL_Rect dash = {x-1, y, 2, 15};
            SDL_RenderFillRect(renderer, &dash);
        }
    }
    
    // Draw lanes for horizontal roads (C and D)
    for (int i = 1; i < 3; i++) {
        int y = WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + i * LANE_WIDTH;
        
        // West of intersection
        for (int x = 20; x < WINDOW_WIDTH/2 - ROAD_WIDTH/2; x += 30) {
            SDL_Rect dash = {x, y-1, 15, 2};
            SDL_RenderFillRect(renderer, &dash);
        }
        
        // East of intersection
        for (int x = WINDOW_WIDTH/2 + ROAD_WIDTH/2; x < WINDOW_WIDTH - 20; x += 30) {
            SDL_Rect dash = {x, y-1, 15, 2};
            SDL_RenderFillRect(renderer, &dash);
        }
    }
    
    // Simple road labels
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    
    // Label backgrounds
    SDL_Rect labelA = {WINDOW_WIDTH/2 - 15, 10, 30, 30};
    SDL_Rect labelB = {WINDOW_WIDTH/2 - 15, WINDOW_HEIGHT - 40, 30, 30};
    SDL_Rect labelC = {WINDOW_WIDTH - 40, WINDOW_HEIGHT/2 - 15, 30, 30};
    SDL_Rect labelD = {10, WINDOW_HEIGHT/2 - 15, 30, 30};
    
    // SDL_RenderFillRect(renderer, &labelA);
    // SDL_RenderFillRect(renderer, &labelB);
    // SDL_RenderFillRect(renderer, &labelC);
    // SDL_RenderFillRect(renderer, &labelD);
    
    // White text
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    displayText(renderer, font, "A", WINDOW_WIDTH/2, 10);
    displayText(renderer, font, "B", WINDOW_WIDTH/2, WINDOW_HEIGHT - 30);
    displayText(renderer, font, "C", WINDOW_WIDTH - 30, WINDOW_HEIGHT/2);
    displayText(renderer, font, "D", 10, WINDOW_HEIGHT/2);
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

typedef struct {
    char text[MAX_LINE_LENGTH];
    SDL_Texture *texture;
} TextCache;

TextCache textCache[MAX_QUEUE_SIZE];
int textCacheSize = 0;

SDL_Texture* getCachedTexture(SDL_Renderer *renderer, TTF_Font *font, const char *text) {
    for (int i = 0; i < textCacheSize; i++) {
        if (strcmp(textCache[i].text, text) == 0) {
            return textCache[i].texture;
        }
    }

    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);

    if (textCacheSize < MAX_QUEUE_SIZE) {
        strncpy(textCache[textCacheSize].text, text, MAX_LINE_LENGTH - 1);
        textCache[textCacheSize].text[MAX_LINE_LENGTH - 1] = '\0';
        textCache[textCacheSize].texture = texture;
        textCacheSize++;
    }

    return texture;
}

void displayText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y) {
    SDL_Texture *texture = getCachedTexture(renderer, font, text);
    SDL_Rect textRect = {x, y, 0, 0};
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
}


void refreshLight(SDL_Renderer *renderer, SharedData* sharedData) {
    // Always display the traffic lights according to nextLight state
    if (sharedData->nextLight == 0) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    else if (sharedData->nextLight == 1) {
        drawLightForA(renderer, false);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    else if (sharedData->nextLight == 2) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, false);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    else if (sharedData->nextLight == 3) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, false);
        drawLightForD(renderer, true);
    }
    else if (sharedData->nextLight == 4) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, false);
    }
    
    // log only if there's a change in state.
    if (sharedData->nextLight != sharedData->currentLight) {
         printf("Light updated from %d to %d\n", sharedData->currentLight, sharedData->nextLight);
         sharedData->currentLight = sharedData->nextLight;
         fflush(stdout);
    }
}

// Define the estimated time (in seconds) for one vehicle to pass.
#define T_PASS_TIME 2
// New helper: Count vehicles in a given queue with a specific lane number.
int countVehicles(VehicleQueue* queue, int lane_num) {
    int count = 0;
    pthread_mutex_lock(&queue->lock);
    for (int i = 0; i < queue->size; i++) {
        int idx = (queue->front + i) % MAX_QUEUE_SIZE;
        if (queue->vehicles[idx]->lane_number == lane_num)
            count++;
        }
    pthread_mutex_unlock(&queue->lock);
    return count;
    }

// New helper: Count all vehicles in Road A (queueA)
int countVehiclesLaneA(VehicleQueue* queue) {
    int count = 0;
    pthread_mutex_lock(&queue->lock);
    count = queue->size; // all vehicles in queueA are from Road A
    pthread_mutex_unlock(&queue->lock);
    return count;
}
// Modified chequeQueue to serve Road A with highest priority.

void* chequeQueue(void* arg) {
    SharedData* sharedData = (SharedData*)arg;
    while (1) {
        // Priority: Serve Road A if any vehicles waiting.
        int countA = countVehiclesLaneA(queueA);
            if (countA > 5) {
                sharedData->nextLight = 1; // 1 corresponds to Road A.
                sleep(3);  // Fixed green time for Road A priority.
            } else {
                // Normal lanes
                // Check for priority condition first (>10 vehicles)
                int priorityB = countVehicles(queueB, 2);
                int priorityC = countVehicles(queueC, 2);
                int priorityD = countVehicles(queueD, 2);

                // Handle priority roads first
                if (priorityB > 10) {
                    sharedData->nextLight = 2; // B lane
                    while (countVehicles(queueB, 2) > 5) {
                        sleep(T_PASS_TIME);
                    }
                } else if (priorityC > 10) {
                    sharedData->nextLight = 3; // C lane
                    while (countVehicles(queueC, 2) > 5) {
                        sleep(T_PASS_TIME);
                    }
                } else if (priorityD > 10) {
                    sharedData->nextLight = 4; // D lane
                    while (countVehicles(queueD, 2) > 5) {
                        sleep(T_PASS_TIME);
                    }
                } else {
                    // Normal operation when no priority condition
                    int L1 = countVehicles(queueA, 2); // AL2
                    int L2 = countVehicles(queueB, 2); // BL2
                    int L3 = countVehicles(queueC, 2); // CL2
                    int L4 = countVehicles(queueD, 2); // DL2
                    
                    // Calculate average vehicles waiting (V)
                    float V = (float)(L1 + L2 + L3 +L4) / 4.0f;
                    
                    // Calculate green light duration
                    int greenTime = (int)(V * T_PASS_TIME);
                    if (greenTime < 1) greenTime = 1;
                    
                     // Serve each lane based on calculated time
                    if (L1 > 0) {
                        sharedData->nextLight = 1; // A lane
                        sleep(greenTime);
                    }
                    if (L2 > 0) {
                        sharedData->nextLight = 2; // B lane
                        sleep(greenTime);
                    }
                    if (L3 > 0) {
                        sharedData->nextLight = 3; // C lane
                        sleep(greenTime);
                    }
                    if (L4 > 0) {
                        sharedData->nextLight = 4; // D lane
                        sleep(greenTime);
                    }
                }
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
    // Lateral separation offset based on queue position
    int offset = (pos % 2 == 0) ? -10 : 10;

    if (v->turning) {
        // Use the turning coordinates if the vehicle is turning
        x = (int)v->turnPosX;
        y = (int)v->turnPosY;
    } else {
            switch (v->lane) {
                case 'A': {
                    int offsetX = (v->lane_number == 1) ? -LANE_WIDTH :
                                  (v->lane_number == 3) ? LANE_WIDTH : 0;
                    x = WINDOW_WIDTH/2 - w/2 + offsetX;
                    y = (int)v->animPos;
                    break;
                }
                case 'B': {
                    int offsetX = (v->lane_number == 1) ? LANE_WIDTH : (v->lane_number == 3) ? -LANE_WIDTH : 0;
                    x = WINDOW_WIDTH/2 - w/2 + offsetX;
                    y = (int)v->animPos;
                    break;
                }
                case 'C': {
                    int offsetY = (v->lane_number == 1) ? -LANE_WIDTH :
                                  (v->lane_number == 3) ? LANE_WIDTH : 0;
                    x = (int)v->animPos;
                    y = WINDOW_HEIGHT/2 - h/2 + offsetY;
                    break;
                }
                case 'D': {
                    int offsetY = (v->lane_number == 1) ? LANE_WIDTH :
                                  (v->lane_number == 3) ? -LANE_WIDTH : 0;
                    x = (int)v->animPos;
                    y = WINDOW_HEIGHT/2 - h/2 + offsetY;
                    break;
                }
                default: {
                    x = WINDOW_WIDTH/2 + 10;
                    y = (int)v->animPos;
                }
            }
    }
    
    if(v->isEmergency)
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

    // Create the vehicle rectangle
    SDL_Rect vehicleRect = { x, y, w, h };
    
    // If vehicle is rotating, use SDL's rotation capabilities
    if (v->turning && fabs(v->angle) > 0.1f) {
        // Create a texture for the rotated vehicle
        SDL_Surface* vehicleSurface = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
        if (vehicleSurface) {
            // Fill the surface with the vehicle color
            SDL_FillRect(vehicleSurface, NULL, 
                         v->isEmergency ? SDL_MapRGB(vehicleSurface->format, 255, 0, 0) 
                                       : SDL_MapRGB(vehicleSurface->format, 0, 0, 255));
            
            // Create texture from surface
            SDL_Texture* vehicleTexture = SDL_CreateTextureFromSurface(renderer, vehicleSurface);
            if (vehicleTexture) {
                // Define destination rectangle
                SDL_Rect dstRect = { x - w/2, y - h/2, w, h };
                
                // Render the rotated vehicle
                SDL_RenderCopyEx(renderer, vehicleTexture, NULL, &dstRect, 
                                v->angle, NULL, SDL_FLIP_NONE);
                
                // Clean up
                SDL_DestroyTexture(vehicleTexture);
            }
            SDL_FreeSurface(vehicleSurface);
        }
    } else {
        // Draw normally for non-rotating vehicles
        SDL_RenderFillRect(renderer, &vehicleRect);
    }
    // SDL_Rect rect = { x, y, w, h };
    // SDL_RenderFillRect(renderer, &rect);
    
    // char idLabel[MAX_VEHICLE_ID];
    // strncpy(idLabel, v->id, MAX_VEHICLE_ID);
    // idLabel[MAX_VEHICLE_ID-1] = '\0';
    // // displayText(renderer, font, idLabel, x, y - h - 2);
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

float easeInOutQuad(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

void calculateTurnCurve(float t, float startX, float startY, float controlX, float controlY, 
                       float endX, float endY, float* outX, float* outY) {
    *outX = (1-t)*(1-t)*startX + 2*(1-t)*t*controlX + t*t*endX;
    *outY = (1-t)*(1-t)*startY + 2*(1-t)*t*controlY + t*t*endY;
}

bool processTurn(Vehicle* v, Uint32 delta, 
                 float startX, float startY, float controlX, float controlY, 
                 float endX, float endY, char newLane, int newLaneNum, float newAnimPos) {
    v->turnProgress += delta * TURN_SPEED;
    if (v->turnProgress > 1.0f)
        v->turnProgress = 1.0f;
    float t = easeInOutQuad(v->turnProgress);
    calculateTurnCurve(t, startX, startY, controlX, controlY, endX, endY,
                         &v->turnPosX, &v->turnPosY);
    if (v->turnProgress >= 1.0f) {
        v->lane = newLane;
        v->lane_number = newLaneNum;
        v->animPos = newAnimPos;
        v->turning = false;
        v->turnProgress = 0.0f;
        return true;
    }
    return false;
}

void updateVehicles(SharedData* sharedData) {
    float speed = 0.2f;
    Uint32 currentTime = SDL_GetTicks();
    static Uint32 lastTime = 0;
    if (lastTime == 0) 
        lastTime = currentTime;
    Uint32 delta = currentTime - lastTime;
    lastTime = currentTime;

    // Define stop positions for each lane
    const int stopA = WINDOW_HEIGHT/2 - ROAD_WIDTH/2 - 20;
    const int stopB = WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 20;
    const int stopC = WINDOW_WIDTH/2 + ROAD_WIDTH/2 + 20;
    const int stopD = WINDOW_WIDTH/2 - ROAD_WIDTH/2 - 20;
    
    char activeLane = '\0';
    if (sharedData->currentLight == 1) activeLane = 'A';
    else if (sharedData->currentLight == 2) activeLane = 'B';
    else if (sharedData->currentLight == 3) activeLane = 'C';
    else if (sharedData->currentLight == 4) activeLane = 'D';


    // Lane A (north to south)
    pthread_mutex_lock(&queueA->lock);
    for (int i = 0; i < queueA->size; i++) {
        int idx = (queueA->front + i) % MAX_QUEUE_SIZE;
        Vehicle *v = queueA->vehicles[idx];
        
        // Check if this is a vehicle that recently completed a turn from DL3 to AL1
        if (v->lane == 'A' && v->lane_number == 1 && v->animPos > 0 && v->animPos <= stopA) {
            // This is likely a vehicle that came from DL3
            // Continue moving it upward (negative direction for A lane)
            float nextPos = v->animPos - speed * delta * 1.5; // Move slightly faster
            v->animPos = nextPos;
            
            if ((int)v->animPos % 50 == 0) {
                printf("[POST-TURN] DL3->AL1 Vehicle %s moving upward at pos %.1f\n", 
                       v->id, v->animPos);
            }
            
            // If it reaches the top of the screen, it will be dequeued below
            continue; // Skip other movement logic for this vehicle
        }
        
        // Define vehicle gap and speed multipliers for all lanes
        float a_l3_vehicle_gap = VEHICLE_GAP * 0.7; // 30% smaller gap for lane A
        float a_l2_vehicle_gap = VEHICLE_GAP * 1.5; // 50% larger gap for lane A
        float a_l3_speed_multiplier = 1.3;         // 30% faster for lane A
        float a_l2_speed_multiplier = 0.85;        // 15% slower for lane A

        float b_l3_vehicle_gap = VEHICLE_GAP * 0.7; // 30% smaller gap for lane B
        float b_l2_vehicle_gap = VEHICLE_GAP * 1.5; // 50% larger gap for lane B
        float b_l3_speed_multiplier = 1.3;         // 30% faster for lane B
        float b_l2_speed_multiplier = 0.85;        // 15% slower for lane B

        float c_l3_vehicle_gap = VEHICLE_GAP * 0.7; // 30% smaller gap for lane C
        float c_l2_vehicle_gap = VEHICLE_GAP * 1.5; // 50% larger gap for lane C
        float c_l3_speed_multiplier = 1.3;         // 30% faster for lane C
        float c_l2_speed_multiplier = 0.85;        // 15% slower for lane C

        float d_l3_vehicle_gap = VEHICLE_GAP * 0.7; // 30% smaller gap for lane D
        float d_l2_vehicle_gap = VEHICLE_GAP * 1.5; // 50% larger gap for lane D
        float d_l3_speed_multiplier = 1.3;         // 30% faster for lane D
        float d_l2_speed_multiplier = 0.85;        // 15% slower for lane D

        // VEHICLE GROUP 1: Rightmost lane (AL3) vehicles turning to road C
        if (v->lane == 'A' && v->lane_number == 3) {
            // L3 vehicles use reduced spacing (they can be closer to each other)
            float l3_vehicle_gap = a_l3_vehicle_gap; // 30% smaller gap
            
            // Check for vehicle ahead with reduced spacing
            if (i > 0) {
                int prevIdx = (queueA->front + i - 1) % MAX_QUEUE_SIZE;
                Vehicle *ahead = queueA->vehicles[prevIdx];
                if (ahead->lane == v->lane && ahead->lane_number == v->lane_number &&
                    !ahead->turning) { // Only check spacing for non-turning vehicles
                    if (v->animPos + VEHICLE_LENGTH + l3_vehicle_gap > ahead->animPos) {
                        v->animPos = ahead->animPos - VEHICLE_LENGTH - l3_vehicle_gap;
                    }
                }
            }
            
            // Begin turning upon reaching threshold - independent from other lanes
            if (!v->turning && v->animPos >= (WINDOW_HEIGHT/2 - ROAD_WIDTH/2 - 20)) {
                printf("Vehicle %s (AL3) reached turning threshold. Starting turn.\n", v->id);
                v->turning = true;
                v->turnProgress = 0.0f;
                // Use the vehicle's current position as turning start.
                v->turnPosX = WINDOW_WIDTH/2 + LANE_WIDTH; // initial x for rightmost lane
                v->turnPosY = v->animPos;                  // current vertical position
            }
            
            if (v->turning) {
                float turnSpeed = 0.001f;
                v->turnProgress += delta * turnSpeed;
                if (v->turnProgress > 0.23f)
                    v->turnProgress = 1.0f;
                // Use the saved starting position for turning:
                float sX = v->turnPosX;
                float sY = v->turnPosY;
                // Define control point to curve naturally toward road C.
                float cX = sX + 50.0f; // adjust offset as needed
                float targetX = (WINDOW_WIDTH/2 + ROAD_WIDTH/2) + (ROAD_WIDTH/6); 
                // For a smooth vertical adjustment, set start and end y's based on current positions.
                float sY_target = sY; // starting y remains as at turn initiation
                float eY = WINDOW_HEIGHT/2 - 5; // target y on road C
                float cY = sY_target + (eY - sY_target) / 2; // control y is midway
                float t = v->turnProgress;
                // Compute quadratic Bezier: B(t)= (1-t)^2 * start + 2(1-t)t * control + t^2 * end.
                v->turnPosX = (1-t)*(1-t)*sX + 2*(1-t)*t*cX + t*t*targetX;
                v->turnPosY = (1-t)*(1-t)*sY + 2*(1-t)*t*cY + t*t*eY;
                
                if (v->turnProgress >= 1.0f) {
                    // Turn is complete: update lane and reset flags.
                    v->lane = 'C';          // Transition to road C.
                    v->lane_number = 1;     // Set as incoming lane for road C.
                    v->turning = false;
                    v->turnProgress = 0.0f;
                    // Set animPos to the final x-position on road C.
                    v->animPos = targetX;
                    printf("AL3 Vehicle %s completed turn into road C.\n", v->id);
                }
                // Draw using computed turning coordinates (handled in drawVehicle).
                continue;
            }
            
            // L3 vehicles move faster when not turning
            float l3_speed_multiplier = a_l3_speed_multiplier; // 30% faster
            float nextPos = v->animPos + speed * delta * l3_speed_multiplier;
            v->animPos = nextPos;
            continue; // Skip regular movement logic
        }
        
        // VEHICLE GROUP 2: Middle lane (AL2) vehicles turning to BL1
        if (v->lane == 'A' && v->lane_number == 2) {
            // L2 vehicles have larger spacing requirements
            float l2_vehicle_gap = a_l2_vehicle_gap; // 50% larger gap
            
            if (activeLane != 'A') {
                float nextPos = v->animPos + speed * delta;
                // Check for vehicle ahead with increased spacing
                if (i > 0) {
                    int prevIdx = (queueA->front + i - 1) % MAX_QUEUE_SIZE;
                    Vehicle *ahead = queueA->vehicles[prevIdx];
                    // Only check spacing for vehicles in same lane and not turning
                    if (ahead->lane == v->lane && ahead->lane_number == v->lane_number &&
                        !ahead->turning) {
                        if (nextPos + VEHICLE_LENGTH + l2_vehicle_gap > ahead->animPos)
                            nextPos = ahead->animPos - VEHICLE_LENGTH - l2_vehicle_gap;
                    }
                }
                // ensure we don't exceed stopA.
                if (nextPos > stopA)
                    v->animPos = stopA;
                else
                    v->animPos = nextPos;
                continue;
            }
            
            // When light is green, begin turning from AL2 to BL1 - independent of AL3
            if (!v->turning && v->animPos >= stopA) {
                 printf("Vehicle %s from AL2 reached stop position. Starting turn to BL1.\n", v->id);
                 v->turning = true;
                 v->turnProgress = 0.0f;
                 // initializing turning start position using current drawing position.
                 v->turnPosX = WINDOW_WIDTH/2;       // for middle lane, x center of road A
                 v->turnPosY = stopA;                // start at stopA
            }
            
            if (v->turning) {
                 float turnSpeed = 0.001f;
                 v->turnProgress += delta * turnSpeed*0.75;
                 if (v->turnProgress > 1.0f)
                     v->turnProgress = 1.0f;
                 // Define the turning trajectory via a quadratic Bezier curve.
                 // Start point:
                 float sX = WINDOW_WIDTH/2;
                 float sY = stopA;
                 // Control point (for curvature – adjust offset as needed):
                 float cX = WINDOW_WIDTH/2 + 50.0f;
                 float targetX = WINDOW_WIDTH/2;
                 float eY = WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 20;
                 float cY = sY + (eY - sY) / 2;
                 float t = v->turnProgress;
                 // Compute Bezier (B(t)= (1-t)^2 * start + 2(1-t)t * control + t^2 * target)
                 v->turnPosX = (1-t)*(1-t)*sX + 2*(1-t)*t*cX + t*t*targetX;
                 v->turnPosY = (1-t)*(1-t)*sY + 2*(1-t)*t*cY + t*t*eY;
                 
                 if (v->turnProgress >= 1.0f) {
                      // Finish turn: update lane and reset turning flags.
                      v->lane = 'B';
                      v->lane_number = 1;
                      v->turning = false;
                      v->turnProgress = 0.0f;
                      // Set animPos for road B (vertical position)
                      v->animPos = eY;
                      printf("Vehicle %s completed turning into BL1. Final pos: %f\n", v->id, eY);
                 }
                 continue; // Skip normal forward motion while turning.
            }
            
            // L2 vehicles move slower when not turning
            float l2_speed_multiplier = a_l2_speed_multiplier; // 15% slower
            float nextPos = v->animPos + speed * delta * l2_speed_multiplier;
            
            // Check for vehicle ahead with increased spacing (excluding turning vehicles)
            if (i > 0) {
                int prevIdx = (queueA->front + i - 1) % MAX_QUEUE_SIZE;
                Vehicle *ahead = queueA->vehicles[prevIdx];
                if (ahead->lane == v->lane && ahead->lane_number == v->lane_number &&
                    !ahead->turning) {
                    if (nextPos + VEHICLE_LENGTH + l2_vehicle_gap > ahead->animPos) {
                        nextPos = ahead->animPos - VEHICLE_LENGTH - l2_vehicle_gap;
                    }
                }
            }
            
            if (activeLane == 'A' || v->animPos > WINDOW_HEIGHT/2 || v->animPos > stopA) {
                v->animPos = nextPos;
            } else if (v->animPos < stopA) {
                v->animPos = (nextPos > stopA) ? stopA : nextPos;
            }
            continue; // Skip regular movement logic
        }
        
        // VEHICLE GROUP 3: Lane 1 (leftmost) uses default spacing and speed
        float nextPos = v->animPos + speed * delta;
        
        // Check for vehicle ahead, but only consider vehicles in same lane and not turning
        bool canMove = true;
        if (i > 0) {
            int prevIdx = (queueA->front + i - 1) % MAX_QUEUE_SIZE;
            Vehicle *ahead = queueA->vehicles[prevIdx];
            if (ahead->lane == v->lane && ahead->lane_number == v->lane_number && 
                !ahead->turning) {
                if (nextPos + VEHICLE_LENGTH + VEHICLE_GAP > ahead->animPos) {
                    canMove = false;
                    nextPos = ahead->animPos - VEHICLE_LENGTH - VEHICLE_GAP;
                }
            }
        }

        if (canMove) {
            if (activeLane == 'A' || v->animPos > WINDOW_HEIGHT/2 || v->animPos > stopA) {
                v->animPos = nextPos;
            } else if (v->animPos < stopA) {
                v->animPos = (nextPos > stopA) ? stopA : nextPos;
            }
        } else {
            v->animPos = nextPos;
        }
    }
    
    // Enhanced dequeuing logic - check if any vehicles at front of queueA have moved off screen
    while (!isQueueEmpty(queueA) && 
           (queueA->vehicles[queueA->front]->animPos > WINDOW_HEIGHT ||  // Regular movement
            queueA->vehicles[queueA->front]->animPos < 0)) {             // DL3->AL1 movement
        
        Vehicle* v = queueA->vehicles[queueA->front];
        if (v->animPos < 0) {
            printf("[DEQUEUE] Vehicle %s reached end of AL1 and has been removed (pos=%.1f)\n", 
                   v->id, v->animPos);
        }
        dequeueUnlocked(queueA);
    }
    pthread_mutex_unlock(&queueA->lock);
    
    // Lane B (south to north)
    pthread_mutex_lock(&queueB->lock);
    for (int i = 0; i < queueB->size; i++) {
        int idx = (queueB->front + i) % MAX_QUEUE_SIZE;
        Vehicle *v = queueB->vehicles[idx];

        float b_l3_vehicle_gap = VEHICLE_GAP * 0.7; // 30% smaller gap for turning lane
        float b_l2_vehicle_gap = VEHICLE_GAP * 1.5; // 50% larger gap for middle lane
        float b_l3_speed_multiplier = 1.3;         // 30% faster for turning lane
        float b_l2_speed_multiplier = 0.5;   

        // Check if this is a vehicle that recently completed a turn from CL3 to BL1
        if (v->lane == 'B' && v->lane_number == 1 && v->animPos < WINDOW_HEIGHT && v->animPos >= stopB) {
            // Continue moving it downward (negative direction for B lane)
            float nextPos = v->animPos - speed * delta * 1.5; // Move slightly faster
            v->animPos = nextPos;
            continue;
        }
        
        // Improved turning logic for vehicles from BL3 (rightmost lane)
// Enhanced turning logic for vehicles from BL3 (rightmost lane)
if (v->lane == 'B' && v->lane_number == 3) {
    // Only log occasionally to reduce console spam
    if ((int)v->animPos % 50 == 0) {
        printf("Vehicle %s is in BL3, animPos: %.1f\n", v->id, v->animPos);
    }
    
    // Begin turning upon reaching threshold
    if (!v->turning && v->animPos <= (WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 20)) {
        printf("[TURN-START] BL3 Vehicle %s starting rotation to DL1 at pos=%.1f\n", 
              v->id, v->animPos);
        v->turning = true;
        v->turnProgress = 0.0f;
        v->angle = 0.0f;
        v->targetAngle = -90.0f; // Counter-clockwise rotation for left turn
        
        // Save starting position
        v->turnPosX = WINDOW_WIDTH/2 - LANE_WIDTH;        // BL3 X position
        v->turnPosY = WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 20; // Current Y position
    }
    
    if (v->turning) {
        // Use rotation animation instead of Bezier curve
        rotateVehicle(v, delta);
        continue; // Skip other movement processing
    }
    
    // Normal movement logic for BL3 vehicles not yet turning
    float nextPos = v->animPos - speed * delta;
    
    // Check for vehicle ahead to prevent collisions
    if (i > 0) {
        int prevIdx = (queueB->front + i - 1) % MAX_QUEUE_SIZE;
        Vehicle *ahead = queueB->vehicles[prevIdx];
        if (ahead->lane == v->lane && ahead->lane_number == v->lane_number &&
            !ahead->turning) {
            if (nextPos - VEHICLE_LENGTH - VEHICLE_GAP < ahead->animPos) {
                nextPos = ahead->animPos + VEHICLE_LENGTH + VEHICLE_GAP;
            }
        }
    }
    
    v->animPos = nextPos;
    continue;
}

        
        if (v->lane == 'B' && v->lane_number == 2) {
            if (activeLane != 'B') {
                float nextPos = v->animPos - speed * delta;
                if (i > 0) {
                    int prevIdx = (queueB->front + i - 1) % MAX_QUEUE_SIZE;
                    Vehicle *ahead = queueB->vehicles[prevIdx];
                    if (nextPos - VEHICLE_LENGTH - VEHICLE_GAP < ahead->animPos)
                        nextPos = ahead->animPos + VEHICLE_LENGTH + VEHICLE_GAP;
                    // if (ahead->lane == v->lane && ahead->lane_number == v->lane_number &&
                    //     !ahead->turning) {
                    //     if (nextPos - VEHICLE_LENGTH - b_l2_vehicle_gap < ahead->animPos)
                    //         nextPos = ahead->animPos + VEHICLE_LENGTH + b_l2_vehicle_gap;
                    // }
                }
                if (nextPos < stopB)
                    v->animPos = stopB;
                else
                    v->animPos = nextPos;
                continue;
            }

            // When light is green, begin turning from BL2 to AL1
            if (!v->turning && v->animPos <= stopB) {
                printf("BL2 Vehicle %s reached threshold. Starting turn to AL1.\n", v->id);
                v->turning = true;
                v->turnProgress = 0.0f;
                v->turnPosX = WINDOW_WIDTH/2;
                v->turnPosY = stopB;
            }
            
            if (v->turning) {
                float turnSpeed = 0.001f;
                v->turnProgress += delta * turnSpeed * 0.75;
                if (v->turnProgress > 1.0f)
                    v->turnProgress = 1.0f;
                float sX = WINDOW_WIDTH/2, sY = stopB;
                // Adjust control points to match CL2 style curve
                float cX = sX - 50.0f;
                float targetX = WINDOW_WIDTH/2 - LANE_WIDTH;
                float targetY = stopA;
                float cY = sY + (targetY - sY) / 2;
                float t = v->turnProgress;
                v->turnPosX = (1-t)*(1-t)*sX + 2*(1-t)*t*cX + t*t*targetX;
                v->turnPosY = (1-t)*(1-t)*sY + 2*(1-t)*t*cY + t*t*targetY;
                if (v->turnProgress >= 1.0f) {
                    v->lane = 'A';
                    v->lane_number = 1;
                    v->turning = false;
                    v->turnProgress = 0.0f;
                    v->animPos = targetY;
                    printf("BL2 Vehicle %s completed turn into AL1. Final pos: %f\n", v->id, targetY);
                }
                continue;
            }
        }
        
        float nextPos = v->animPos - speed * delta;
        
        // Check for vehicle ahead
        bool canMove = true;
        if (i > 0) {
            int prevIdx = (queueB->front + i - 1) % MAX_QUEUE_SIZE;
            Vehicle *ahead = queueB->vehicles[prevIdx];
            
            // Modified collision detection for BL3
            bool shouldCheck = false;
            if (v->lane_number == 3) {
                // Check for any vehicle in lane B that could block our path
                shouldCheck = (ahead->lane == 'B' && 
                              (ahead->lane_number == 2 || ahead->lane_number == 3) &&
                              !ahead->turning);
            } else {
                // Original same-lane check for other vehicles
                shouldCheck = (ahead->lane == v->lane && 
                              ahead->lane_number == v->lane_number &&
                              !ahead->turning);
            }
            
            if (shouldCheck && nextPos - VEHICLE_LENGTH - VEHICLE_GAP < ahead->animPos) {
                nextPos = ahead->animPos + VEHICLE_LENGTH + VEHICLE_GAP;
            }
        }

        if (canMove) {
            if (activeLane == 'B' || v->animPos < WINDOW_HEIGHT/2 || v->animPos > stopB) {
                v->animPos = nextPos;
            } else if (v->animPos > stopB) {
                v->animPos = (nextPos < stopB) ? stopB : nextPos;
            }
        } else {
            v->animPos = nextPos;
        }
    }
    while (!isQueueEmpty(queueB) && queueB->vehicles[queueB->front]->animPos < 0)
        dequeueUnlocked(queueB);
    pthread_mutex_unlock(&queueB->lock);

    // Lane C (east to west)
    pthread_mutex_lock(&queueC->lock);
    for (int i = 0; i < queueC->size; i++) {
        int idx = (queueC->front + i) % MAX_QUEUE_SIZE;
        Vehicle *v = queueC->vehicles[idx];
        
        // Check if this is a vehicle that recently completed a turn
        if (v->lane == 'C' && v->lane_number == 1 && v->animPos > 0 && v->animPos >= stopC) {
            // This is likely a vehicle that came from a turn
            float nextPos = v->animPos - speed * delta * 1.5; // Move slightly faster
            v->animPos = nextPos;
            
            // Optional debugging
            if ((int)v->animPos % 50 == 0) {
                printf("[POST-TURN] Vehicle %s moving along CL1 at pos %.1f\n", v->id, v->animPos);
            }
            continue;
        }

        // For vehicles from road C (CL3) turning into BL1:
        if (v->lane == 'C' && v->lane_number == 3) {
             // L3 vehicles use reduced spacing
            float l3_vehicle_gap = VEHICLE_GAP * 0.7; // 30% smaller gap

            // Check for vehicle ahead with reduced spacing
            if (i > 0) {
                int prevIdx = (queueC->front + i - 1) % MAX_QUEUE_SIZE;
                Vehicle *ahead = queueC->vehicles[prevIdx];
                if (ahead->lane == v->lane && ahead->lane_number == v->lane_number &&
                    !ahead->turning) {
                    if (v->animPos - l3_vehicle_gap - VEHICLE_LENGTH < ahead->animPos) {
                        v->animPos = ahead->animPos + VEHICLE_LENGTH + l3_vehicle_gap;
                    }
                }
            }
    
            if (!v->turning && v->animPos <= stopC) {
                printf("[TURN-START] CL3 Vehicle %s starting rotation to AL1 at pos=%.1f\n", 
                       v->id, v->animPos);
                v->turning = true;
                v->turnProgress = 0.0f;
                v->angle = 0.0f;
                v->targetAngle = 90.0f; // Clockwise rotation for right turn
                
                // Save starting position
                v->turnPosX = stopC;
                v->turnPosY = WINDOW_HEIGHT/2 - LANE_WIDTH; // CL3 Y position
            }

            if (v->turning) {
                // Use rotation animation instead of Bezier curve
                rotateVehicle(v, delta);
                continue; // Skip other movement processing
            }
            
            // L3 vehicles move faster when not turning
            float l3_speed_multiplier = 1.3;
            float nextPos = v->animPos - speed * delta * l3_speed_multiplier;
            v->animPos = nextPos;
            continue;
        }
        
        if (v->lane == 'C' && v->lane_number == 2) {
            if (activeLane != 'C') {
                float nextPos = v->animPos - speed * delta;
                if (i > 0) {
                    int prevIdx = (queueC->front + i - 1) % MAX_QUEUE_SIZE;
                    Vehicle *ahead = queueC->vehicles[prevIdx];
                    if (nextPos - VEHICLE_LENGTH - VEHICLE_GAP < ahead->animPos)
                        nextPos = ahead->animPos + VEHICLE_LENGTH + VEHICLE_GAP;
                }
                if (nextPos < stopC)
                    v->animPos = stopC;
                else
                    v->animPos = nextPos;
                continue;
            }
            
            // When light is green, turn from CL2 to DL1
            if (!v->turning && v->animPos <= stopC) {
                printf("CL2 Vehicle %s reached threshold. Starting turn to DL1.\n", v->id);
                v->turning = true;
                v->turnProgress = 0.0f;
                v->turnPosX = stopC;
                v->turnPosY = WINDOW_HEIGHT/2;
            }
            
            if (v->turning) {
                float turnSpeed = 0.001f;
                // Adjust progress as in AL2
                v->turnProgress += delta * turnSpeed * 0.75;
                if (v->turnProgress > 1.0f)
                    v->turnProgress = 1.0f;
                float sX = stopC, sY = WINDOW_HEIGHT/2;
                float cX = sX - 50.0f; // control point for smooth curve
                float targetX = stopD;
                float targetY = WINDOW_HEIGHT/2 + LANE_WIDTH;
                float t = v->turnProgress;
                v->turnPosX = (1-t)*(1-t)*sX + 2*(1-t)*t*cX + t*t*targetX;
                v->turnPosY = (1-t)*(1-t)*sY + 2*(1-t)*t*(sY + 50.0f) + t*t*targetY;
                if (v->turnProgress >= 1.0f) {
                    v->lane = 'D';
                    v->lane_number = 1;
                    v->turning = false;
                    v->turnProgress = 0.0f;
                    v->animPos = targetX;
                    printf("CL2 Vehicle %s completed turn into DL1.\n", v->id);
                }
                continue;
            }
        }
        
        float nextPos = v->animPos - speed * delta;
        
        // Check for vehicle ahead
        bool canMove = true;
        if (i > 0) {
            int prevIdx = (queueC->front + i - 1) % MAX_QUEUE_SIZE;
            Vehicle *ahead = queueC->vehicles[prevIdx];
            if (nextPos - VEHICLE_LENGTH - VEHICLE_GAP < ahead->animPos) {
                canMove = false;
                nextPos = ahead->animPos + VEHICLE_LENGTH + VEHICLE_GAP;
            }
        }

        if (canMove) {
            if (activeLane == 'C' || v->animPos < WINDOW_WIDTH/2 || v->animPos > stopC) {
                v->animPos = nextPos;
            } else if (v->animPos > stopC) {
                v->animPos = (nextPos < stopC) ? stopC : nextPos;
            }
        } else {
            v->animPos = nextPos;
        }
    }
    while (!isQueueEmpty(queueC) && queueC->vehicles[queueC->front]->animPos < 0)
        dequeueUnlocked(queueC);
    pthread_mutex_unlock(&queueC->lock);

    // Lane D (west to east)
    pthread_mutex_lock(&queueD->lock);
    for (int i = 0; i < queueD->size; i++) {
        int idx = (queueD->front + i) % MAX_QUEUE_SIZE;
        Vehicle *v = queueD->vehicles[idx];
        
        // For vehicles from road D (DL3) turning into AL1:
            // For vehicles from road D (DL3) turning into AL1:
        if (v->lane == 'D' && v->lane_number == 3) {
            if (!v->turning && v->animPos >= stopD) {
                printf("DL3 Vehicle %s starting turn to AL1\n", v->id);
                v->turning = true;
                v->turnProgress = 0.0f;
                v->turnPosX = stopD;
                v->turnPosY = WINDOW_HEIGHT/2 - LANE_WIDTH;
            }

            if (v->turning) {
                float turnSpeed = 0.001f;
                v->turnProgress += delta * turnSpeed * 0.75;
                if (v->turnProgress > 1.0f)
                    v->turnProgress = 1.0f;
                
                // Use t directly instead of easeInOutQuad for consistency
                float t = v->turnProgress;
                float sX = stopD;
                float sY = WINDOW_HEIGHT/2 - LANE_WIDTH;
                float eX = WINDOW_WIDTH/2 - LANE_WIDTH/2;
                float eY = 20; // Adjusted to prevent overshooting
                
                // Adjust control points for smoother curve - using 50.0f offset
                float cX = sX + 50.0f;
                float cY = sY - 50.0f;
                
                v->turnPosX = (1-t)*(1-t)*sX + 2*(1-t)*t*cX + t*t*eX;
                v->turnPosY = (1-t)*(1-t)*sY + 2*(1-t)*t*cY + t*t*eY;
                
                // Debug log for tracking position during turn
                if (t == 0.0f || t == 0.5f || t == 1.0f) {
                    printf("DL3 Vehicle %s turn progress: %.2f, pos: (%.1f, %.1f)\n", 
                           v->id, t, v->turnPosX, v->turnPosY);
                }
                if (v->turnProgress >= 1.0f) {
                    v->lane = 'A';
                    v->lane_number = 1;
                    v->turning = false;
                    v->turnProgress = 0.0f;
                    v->animPos = stopA - 50;
                    printf("DL3 Vehicle %s completed turn into AL1\n", v->id);
                }
                continue;
            }
        }
        
        if (v->lane == 'D' && v->lane_number == 2) {
            if (activeLane != 'D') {
                float nextPos = v->animPos + speed * delta;
                if (i > 0) {
                    int prevIdx = (queueD->front + i - 1) % MAX_QUEUE_SIZE;
                    Vehicle *ahead = queueD->vehicles[prevIdx];
                    if (nextPos + VEHICLE_LENGTH + VEHICLE_GAP > ahead->animPos)
                        nextPos = ahead->animPos - VEHICLE_LENGTH - VEHICLE_GAP;
                }
                if (nextPos > stopD)
                    v->animPos = stopD;
                else
                    v->animPos = nextPos;
                continue;
            }
            
            // When light is green, turn from DL2 to CL1
            if (!v->turning && v->animPos >= stopD) {
                printf("DL2 Vehicle %s reached threshold. Starting turn to CL1.\n", v->id);
                v->turning = true;
                v->turnProgress = 0.0f;
                v->turnPosX = stopD;
                v->turnPosY = WINDOW_HEIGHT/2;
            }
            
            if (v->turning) {
                float turnSpeed = 0.001f;
                // Consistent progress update as for AL2
                v->turnProgress += delta * turnSpeed * 0.75;
                if (v->turnProgress > 1.0f)
                    v->turnProgress = 1.0f;
                float sX = stopD, sY = WINDOW_HEIGHT/2;
                float cX = sX + 50.0f; // control point adjusted for DL2
                float targetX = stopC;
                float targetY = WINDOW_WIDTH/2 - LANE_WIDTH;
                float t = v->turnProgress;
                v->turnPosX = (1-t)*(1-t)*sX + 2*(1-t)*t*cX + t*t*targetX;
                v->turnPosY = (1-t)*(1-t)*sY + 2*(1-t)*t*(sY - 50.0f) + t*t*targetY;
                if (v->turnProgress >= 1.0f) {
                    v->lane = 'C';
                    v->lane_number = 1;
                    v->turning = false;
                    v->turnProgress = 0.0f;
                    v->animPos = targetX;
                    printf("DL2 Vehicle %s completed turn into CL1.\n", v->id);
                }
                continue;
            }
        }
        
        float nextPos = v->animPos + speed * delta;
        
        // Check for vehicle ahead
        bool canMove = true;
        if (i > 0) {
            int prevIdx = (queueD->front + i - 1) % MAX_QUEUE_SIZE;
            Vehicle *ahead = queueD->vehicles[prevIdx];
            if (nextPos + VEHICLE_LENGTH + VEHICLE_GAP > ahead->animPos) {
                canMove = false;
                nextPos = ahead->animPos - VEHICLE_LENGTH - VEHICLE_GAP;
            }
        }

        if (canMove) {
            if (activeLane == 'D' || v->animPos > WINDOW_WIDTH/2 || v->animPos > stopD) {
                v->animPos = nextPos;
            } else if (v->animPos < stopD) {
                v->animPos = (nextPos > stopD) ? stopD : nextPos;
            }
        } else {
            v->animPos = nextPos;
        }
    }
    while (!isQueueEmpty(queueD) && queueD->vehicles[queueD->front]->animPos > WINDOW_WIDTH)
        dequeueUnlocked(queueD);
    pthread_mutex_unlock(&queueD->lock);
}

// delay reduced to 3 sec
void* processVehiclesSequentially(void* arg) {
    FILE* file = fopen(VEHICLE_FILE, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\n")] = 0;
        char* vehicleNumber = strtok(buffer, ":");
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
                newVehicle->animPos = (float)WINDOW_WIDTH - 10.0f; // Start slightly in view
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
        sleep(1); // Reduced from 3 to 1 second for more frequent spawns
    }
    fclose(file);
    return NULL;
}

void rotateVehicle(Vehicle* vehicle, Uint32 delta) {
    if (!vehicle->turning) return;
    
    // Define rotation speed (degrees per millisecond)
    float rotationSpeed = 0.1f;
    // Position during turn - CL3 to BL1
    const int stopA = WINDOW_HEIGHT/2 - ROAD_WIDTH/2 - 20;
    const int stopB = WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 20;
    const int stopC = WINDOW_WIDTH/2 + ROAD_WIDTH/2 + 20;
    const int stopD = WINDOW_WIDTH/2 - ROAD_WIDTH/2 - 20;
    
    // Update progress for tracking
    vehicle->turnProgress += delta * 0.001f; // Convert to seconds for easier debugging
    if (vehicle->turnProgress > 1.0f) vehicle->turnProgress = 1.0f;
    
    // Calculate current angle and position based on vehicle lane and progress
    float t = easeInOutQuad(vehicle->turnProgress);
    
    if (vehicle->lane == 'B' && vehicle->lane_number == 3) {
        // BL3 vehicles turning left to DL1 (counter-clockwise rotation)
        float startAngle = 0.0f;      // Initial angle (0 = straight down)
        float endAngle = -90.0f;      // Final angle (-90 = facing left)
        
        vehicle->angle = startAngle + (endAngle - startAngle) * t;
        
        // Position during turn
        float startX = WINDOW_WIDTH/2 - LANE_WIDTH;
        float startY = WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 20;
        float endX = WINDOW_WIDTH/2 - ROAD_WIDTH/2 - 20;
        float endY = WINDOW_HEIGHT/2 + LANE_WIDTH;
        
        vehicle->turnPosX = startX + (endX - startX) * t;
        vehicle->turnPosY = startY + (endY - startY) * t;
        
        // Check if rotation is complete
        if (vehicle->turnProgress >= 1.0f) {
            // Update vehicle properties for new lane
            vehicle->lane = 'D';
            vehicle->lane_number = 1;
            vehicle->turning = false;
            vehicle->turnProgress = 0.0f;
            vehicle->angle = 0.0f;
            // Position in new lane
            vehicle->animPos = endX;
            printf("BL3 Vehicle %s completed turn into DL1\n", vehicle->id);
        }
    }
    else if (vehicle->lane == 'C' && vehicle->lane_number == 3) {
        // CL3 vehicles turning right to BL1 (clockwise rotation)
        float startAngle = 0.0f;      // Initial angle (0 = straight left)
        float endAngle = 90.0f;       // Final angle (90 = facing down)
        vehicle->angle = startAngle + (endAngle - startAngle) * t;
        
        // Position during turn - CL3 to BL1
        float startX = stopC;
        float startY = WINDOW_HEIGHT/2 - LANE_WIDTH;
        float endX = WINDOW_WIDTH/2;
        float endY = stopB - 30; // Position just above stopB
        
        // Log the turn progress
        if ((int)(vehicle->turnProgress * 100) % 30 == 0 && 
            (int)(vehicle->turnProgress * 100) / 30 != (int)((vehicle->turnProgress - delta * 0.001f) * 100) / 30) {
            printf("[CL3→BL1] Progress: %.1f%%, Angle: %.1f, Pos: (%.1f, %.1f)\n",
                   vehicle->turnProgress * 100, vehicle->angle, 
                   startX + (endX - startX) * t, startY + (endY - startY) * t);
        }
        
        // Use quadratic bezier curve for smoother movement
        float controlX = startX - 30.0f;
        float controlY = endY - 30.0f;
        
        vehicle->turnPosX = (1-t)*(1-t)*startX + 2*(1-t)*t*controlX + t*t*endX;
        vehicle->turnPosY = (1-t)*(1-t)*startY + 2*(1-t)*t*controlY + t*t*endY;
        
        // Check if rotation is complete
        if (vehicle->turnProgress >= 1.0f) {
            // FIX: Set lane to 'B' (not 'A') and lane_number to 1
            vehicle->lane = 'B';
            vehicle->lane_number = 1;
            vehicle->turning = false;
            vehicle->turnProgress = 0.0f;
            vehicle->angle = 0.0f;
            // FIX: Set animPos correctly for B lane (vertical position)
            vehicle->animPos = endY;
            printf("CL3 Vehicle %s completed turn into BL1\n", vehicle->id);
        }
    }
    else if (vehicle->lane == 'D' && vehicle->lane_number == 3) {
        // DL3 vehicles turning right to AL1
        float startAngle = 0.0f;      // Initial angle (0 = straight right)
        float endAngle = -90.0f;      // Final angle (-90 = facing up)
        vehicle->angle = startAngle + (endAngle - startAngle) * t;
        
        // Position during turn - DL3 to AL1
        float startX = stopD;
        float startY = WINDOW_HEIGHT/2 - LANE_WIDTH;
        float endX = WINDOW_WIDTH/2 - LANE_WIDTH/2;
        float endY = stopA;
        
        // Use quadratic interpolation for smoother curve
        float controlX = startX + 40.0f;
        float controlY = endY + 40.0f;
        
        vehicle->turnPosX = (1-t)*(1-t)*startX + 2*(1-t)*t*controlX + t*t*endX;
        vehicle->turnPosY = (1-t)*(1-t)*startY + 2*(1-t)*t*controlY + t*t*endY;
        
        // Check if rotation is complete
        if (vehicle->turnProgress >= 1.0f) {
            // Update vehicle properties for new lane
            vehicle->lane = 'A';
            vehicle->lane_number = 1;
            vehicle->turning = false;
            vehicle->turnProgress = 0.0f;
            vehicle->angle = 0.0f;
            // Position in new lane - set to stopA - 50 to prevent teleporting
            vehicle->animPos = stopA - 50;
            printf("DL3 Vehicle %s completed turn into AL1\n", vehicle->id);
        }
    }
}

// void rotateVehicle(Vehicle* vehicle, Uint32 delta) {
//     if (!vehicle->turning) return;
    
//     // Define rotation speed (degrees per millisecond)
//     float rotationSpeed = 0.1f;
    
//     // Calculate amount to rotate this frame
//     float rotationAmount = rotationSpeed * delta;
    
//     // Update progress for tracking
//     vehicle->turnProgress += delta * 0.001f; // Convert to seconds for easier debugging
//     if (vehicle->turnProgress > 1.0f) vehicle->turnProgress = 1.0f;
    
//     // Calculate current angle based on progress
//     if (vehicle->lane == 'B' && vehicle->lane_number == 3) {
//         // For BL3 vehicles turning left, we rotate counter-clockwise (negative rotation)
//         float startAngle = 0.0f;      // Initial angle (0 = straight down)
//         float endAngle = -90.0f;      // Final angle (-90 = facing left)
        
//         // Calculate this frame's angle using progress-based interpolation
//         // Using easeInOutQuad for smoother start/stop
//         float t = easeInOutQuad(vehicle->turnProgress);
//         vehicle->angle = startAngle + (endAngle - startAngle) * t;
        
//         // Also update position during turn (simplified linear movement)
//         // Start and end positions
//         float startX = WINDOW_WIDTH/2 - LANE_WIDTH;
//         float startY = WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 20;
//         float endX = WINDOW_WIDTH/2 - ROAD_WIDTH/2 - 20;
//         float endY = WINDOW_HEIGHT/2 + LANE_WIDTH;
        
//         // Simple linear interpolation for position during rotation
//         vehicle->turnPosX = startX + (endX - startX) * t;
//         vehicle->turnPosY = startY + (endY - startY) * t;
        
//         // Debug output
//         if ((int)(vehicle->turnProgress * 10) % 2 == 0 && 
//             (int)(vehicle->turnProgress * 10) != (int)((vehicle->turnProgress - delta * 0.001f) * 10)) {
//             printf("[ROTATION] %s: progress=%.2f, angle=%.1f, pos=(%.1f, %.1f)\n", 
//                   vehicle->id, vehicle->turnProgress, vehicle->angle, 
//                   vehicle->turnPosX, vehicle->turnPosY);
//         }
        
//         // Check if rotation is complete
//         if (vehicle->turnProgress >= 1.0f) {
//             vehicle->turning = false;
//             vehicle->lane = 'D';
//             vehicle->lane_number = 1;
//             vehicle->animPos = endX; // Set to the proper position in DL1
//             printf("[ROTATION-COMPLETE] %s: Completed turn to DL1. Final angle=%.1f\n", 
//                   vehicle->id, vehicle->angle);
//         }
//     }
// }