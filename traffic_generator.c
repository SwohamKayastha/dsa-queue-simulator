/*This program will be responsible to generate the vehicle on each lane.*/
// #include <stdio.h>

// int main()
// {
//     printf("Working");
// }


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
    #include <windows.h>
    #define sleep(seconds) Sleep((seconds) * 1000)
#else
    #include <unistd.h>
#endif

#define FILENAME "vehicles.data"

// Function to generate a random vehicle number
void generateVehicleNumber(char* buffer) {
    buffer[0] = 'A' + rand() % 26;
    buffer[1] = 'A' + rand() % 26;
    buffer[2] = '0' + rand() % 10;
    buffer[3] = 'A' + rand() % 26;
    buffer[4] = 'A' + rand() % 26;
    buffer[5] = '0' + rand() % 10;
    buffer[6] = '0' + rand() % 10;
    buffer[7] = '0' + rand() % 10;
    buffer[8] = '\0';
}

// Function to generate a random lane
char* generateLane() {
    static char* lanes[] = {"A", "B", "C", "D", "L3"};
    return lanes[rand() % 5];
}

// Function to get main lane for L3 vehicles
char getMainLaneForL3() {
    static char mainLanes[] = "ABCD";
    return mainLanes[rand() % 4];
}

int main() {
    FILE* file = fopen(FILENAME, "a");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    srand(time(NULL)); // Initialize random seed

    while (1) {
        char vehicle[9];
        generateVehicleNumber(vehicle);
        char* lane = generateLane();

        // Write to file
        if (strcmp(lane, "L3") == 0) {
            char mainLane = getMainLaneForL3();
            fprintf(file, "%sL3:%c\n", vehicle, mainLane);
            printf("Generated: %sL3:%c\n", vehicle, mainLane);
        } else {
            fprintf(file, "%s:%s\n", vehicle, lane);
            printf("Generated: %s:%s\n", vehicle, lane);
        }
        fflush(file); // Ensure data is written immediately

        sleep(1); // Wait 1 second before generating next entry
    }

    fclose(file);
    return 0;
}