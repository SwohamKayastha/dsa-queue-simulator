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
#define SCALE 1
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

// queu cleanup
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
    pthread_create(&tReadFile, NULL, readAndParseFile, NULL);
    // readAndParseFile();

    // Continue the UI thread
    bool running = true;
    while (running) {
        // update light
        refreshLight(renderer, &sharedData);
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT) running = false;
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


void drawLightForB(SDL_Renderer* renderer, bool isRed) {
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {325, 488, 80, 30};  // wider box to accommodate both lights
    SDL_RenderFillRect(renderer, &lightBox);
    
    // Left lane light - always green (L1)
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect left_Light = {330, 493, 20, 20};  // leftmost lane position
    SDL_RenderFillRect(renderer, &left_Light);
    
    // Middle lane light - controlled by traffic signal (L2)
    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect middle_Light = {380, 493, 20, 20};  // middle lane position
    SDL_RenderFillRect(renderer, &middle_Light);
}

void drawLightForA(SDL_Renderer* renderer, bool isRed) {
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {388, 288, 70, 30};
    SDL_RenderFillRect(renderer, &lightBox);
    
    // Right turn light - always green
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect right_Light = {433, 293, 20, 20};
    SDL_RenderFillRect(renderer, &right_Light);
    
    // Straight light - controlled by traffic signal
    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect straight_Light = {393, 293, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
}

void drawLightForC(SDL_Renderer* renderer, bool isRed) {
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {488, 388, 30, 70};
    SDL_RenderFillRect(renderer, &lightBox);
    
    // Right turn light - always green
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect right_Light = {493, 433, 20, 20};
    SDL_RenderFillRect(renderer, &right_Light);
    
    // Straight light - controlled by traffic signal
    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect straight_Light = {493, 393, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
}

void drawLightForD(SDL_Renderer* renderer, bool isRed) {
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {288, 325, 30, 90};  // taller box to accommodate both lights
    SDL_RenderFillRect(renderer, &lightBox);
    
    // Left turn light - always green (L3)
    SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect left_turn_Light = {293, 330, 20, 20};  // top position
    SDL_RenderFillRect(renderer, &left_turn_Light);
    
    // Middle lane light - controlled by traffic signal (L2)
    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect middle_Light = {293, 380, 20, 20};  // middle lane position
    SDL_RenderFillRect(renderer, &middle_Light);
}


void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_SetRenderDrawColor(renderer, 211,211,211,255);
    // Vertical road
    
    SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &verticalRoad);

    // Horizontal road
    SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &horizontalRoad);
    // draw horizontal lanes
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for(int i=0; i<=3; i++){
        // Horizontal lanes
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,  // x1,y1
            WINDOW_WIDTH/2 - ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i // x2, y2
        );
        SDL_RenderDrawLine(renderer, 
            800, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,
            WINDOW_WIDTH/2 + ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i
        );
        // Vertical lanes
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, 0,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, WINDOW_HEIGHT/2 - ROAD_WIDTH/2
        );
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, 800,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, WINDOW_HEIGHT/2 + ROAD_WIDTH/2
        );
    }
    displayText(renderer, font, "A",400, 10);
    displayText(renderer, font, "B",400,770);
    displayText(renderer, font, "D",10,400);
    displayText(renderer, font, "C",770,400);
    
}


void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y){
    // display necessary text
    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    SDL_Rect textRect = {x,y,0,0 };
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_Log("DIM of SDL_Rect %d %d %d %d", textRect.x, textRect.y, textRect.h, textRect.w);
    // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
}


void refreshLight(SDL_Renderer *renderer, SharedData* sharedData) {
    if(sharedData->nextLight == sharedData->currentLight) return; // early return

    // All lights red
    if(sharedData->nextLight == 0) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    // Lane A green
    else if(sharedData->nextLight == 1) {
        drawLightForA(renderer, false);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    // Lane B green
    else if(sharedData->nextLight == 2) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, false);
        drawLightForC(renderer, true);
        drawLightForD(renderer, true);
    }
    // Lane C green
    else if(sharedData->nextLight == 3) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, false);
        drawLightForD(renderer, true);
    }
    // Lane D green
    else if(sharedData->nextLight == 4) {
        drawLightForA(renderer, true);
        drawLightForB(renderer, true);
        drawLightForC(renderer, true);
        drawLightForD(renderer, false);
    }

    SDL_RenderPresent(renderer);
    printf("Light of queue updated from %d to %d\n", sharedData->currentLight, sharedData->nextLight);
    sharedData->currentLight = sharedData->nextLight;
    fflush(stdout);
}

void* chequeQueue(void* arg) {
    SharedData* sharedData = (SharedData*)arg;
    while (1) {
        // All red
        sharedData->nextLight = 0;
        sleep(2);
        // Cycle through each lane
        for(int lane = 1; lane <= 4; lane++) {
            sharedData->nextLight = lane;
            sleep(5);
        }
    }
}


void* readAndParseFile(void* arg) {
    while(1){ 
        FILE* file = fopen(VEHICLE_FILE, "r");
        if (!file) {
            perror("Error opening file");
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
                
                // lane number based on vehicle ID format
                if (strstr(vehicleNumber, "L1")) newVehicle->lane_number = 1;
                else if (strstr(vehicleNumber, "L2")) newVehicle->lane_number = 2;
                else if (strstr(vehicleNumber, "L3")) newVehicle->lane_number = 3;
                else newVehicle->lane_number = 2; // default to middle lane

                // eneque the vehicle to the respective lane
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
        sleep(2); // manage this time
    }
}