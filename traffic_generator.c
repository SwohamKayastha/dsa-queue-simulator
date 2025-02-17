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
char generateLane() {
    char lanes[] = {'A', 'B', 'C', 'D'};
    return lanes[rand() % 4];
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
        char lane = generateLane();

        // Write to file
        fprintf(file, "%s:%c\n", vehicle, lane);
        fflush(file); // Ensure data is written immediately

        printf("Generated: %s:%c\n", vehicle, lane); // Print to console

        sleep(1); // Wait 1 second before generating next entry
    }

    fclose(file);
    return 0;
}