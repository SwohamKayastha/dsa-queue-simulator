# 🚦 Traffic Junction Simulator

A sophisticated traffic management system that simulates real-world junction behavior with multi-lane roads and smooth vehicle animations.

## 🎥 Demo
!![[Traffic Simulator Demo](path_to_your_demo.gif)](https://github.com/SwohamKayastha/dsa-queue-simulator/blob/main/dsa-gif.gif)

## ✨ Key Features

🚗 **Advanced Traffic System**
- Multi-lane traffic system (3 lanes per road)
- Intelligent traffic light management
- Smooth vehicle turning animations
- Emergency vehicle priority system

🛣️ **Lane Management**
- Lane 1: Ingoing Lane
- Lane 2: Straight Lane
- Lane 3: Left turn only

🚨 **Vehicle Types**
- Regular vehicles (🚙 Blue)
- Emergency vehicles (🚑 Red)
- Automatic lane assignment
- Collision prevention system

## 🧠 System Architecture and Algorithms

### Traffic Light System Structure
The simulator implements a four-way junction with traffic lights controlling each road (A, B, C). The light control system follows these rules:

- Each road has dedicated traffic signals
- Signals alternate between red and green based on vehicle queue size
- Lanes 1 and 3 have dedicated turn signals that stay green

```bash
Road A (North) ↓
Road B (South) ↑
Road C (East)  ←
Road D (West)  →
```

### Vehicle Structure
Vehicles are implemented using a comprehensive data structure:

```bash
typedef struct {
    char id[MAX_VEHICLE_ID];     // Vehicle identifier
    char lane;                   // Road identifier (A, B, C, D)
    int arrivalTime;             // Time when vehicle entered the simulation
    bool isEmergency;            // Emergency vehicle flag
    int lane_number;             // Lane position (1, 2, 3)
    float animPos;               // Current animation position
    bool turning;                // Turning state flag
    float turnProgress;          // Progress value (0.0 to 1.0) for turns
    float turnPosX;              // X-coordinate during turns
    float turnPosY;              // Y-coordinate during turns
    float angle;                 // Current rotation angle
    float targetAngle;           // Target rotation angle
} Vehicle;
```

### Queue Management
Vehicles are stored in lane-specific queues with thread-safe operations:
```bash
typedef struct {
    Vehicle* vehicles[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
} VehicleQueue;
```

### Turning Logic
Vehicles perform turns using either:
1. Bezier Curve Turning: For gentle, realistic curves
```bash
v->turnPosX = (1-t)*(1-t)*startX + 2*(1-t)*t*controlX + t*t*endX;
v->turnPosY = (1-t)*(1-t)*startY + 2*(1-t)*t*controlY + t*t*endY;
```
2. Rotation-Based Turning: For angular turns
```bash
vehicle->angle = vehicle->angle + (vehicle->targetAngle - vehicle->angle) * 0.1f;
```
### Vehicle Movement Algorithm
The movement logic follows these key principles:

- Forward Movement: Vehicles move at variable speeds based on lane type
- Collision Avoidance: Vehicles maintain proper spacing
- Traffic Light Response: Vehicles stop at red lights and proceed at green
- Turn Execution: Vehicles follow Bezier curves for smooth turning

## Main Funcitions
### Traffic Light Control
- ```refreshLight()```: Updates traffic light states based on current priority
- ```chequeQueue()```: Determines which lane gets green light based on vehicle count
- ```drawLightForA/B/C/D()```: Renders traffic lights for each road
### Vehicle Management
- ```processVehiclesSequentially()```: Reads vehicle data from file and adds to simulation
- ```enqueue()/dequeue()```: Thread-safe operations for adding/removing vehicles
- ```updateVehicles()```: Core function handling all vehicle movement and interactions
### Animation
- ```drawVehicle()```: Renders vehicles with proper position, orientation, and color
- ```rotateVehicle()```: Handles vehicle rotation for turns
- ```calculateTurnCurve()```: Computes Bezier curve points for smooth turns
- ```easeInOutQuad()```: Provides smooth acceleration/deceleration for animations

## 🛠️ Prerequisites

You'll need MSYS2/MinGW64 with these packages. Open MinGW terminal and run:

```bash
# Update package database
pacman -Syu

# Install required packages
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-SDL2
pacman -S mingw-w64-x86_64-SDL2_ttf
```

## 📥 Installation
### Clone the repository:
```bash
git clone https://github.com/SwohamKayastha/dsa-queue-simulator
cd dsa-queue-simulator
```

### Compile and run:
```bash
gcc simulator.c -o sim -Dmain=SDL_main -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf && ./sim
```

## 🎮 Controls & Usage
### Vehicle Types
- 🚙 Regular Vehicles: Blue color
### Lane System
![[dsa-image](dsa-queue-simulator\dsa-img.png)](https://github.com/SwohamKayastha/dsa-queue-simulator/blob/main/dsa-img.png)
<!-- here implement the image -->
### Traffic Rules
- 🟢 Green light: Vehicles proceed
- 🔴 Red light: Vehicles stop
- ↩️ Turning animations at intersections

### Traffic Monitor Indications
| Color | Meaning | Action |
|-------|---------|--------|
| 🟢 Green | Road has right-of-way | Vehicles proceed through intersection |
| 🔴 Red | Must stop | Vehicles stop at the stop line |
| 🟠 Orange | Congestion warning | Indicates moderate traffic in UI |

## 🛠️ Customization
### Modify Traffic Patterns
Edit vehicles.data:
```bash
EMG001L2:A    # Emergency vehicle in lane 2 of road A
XX1YZ123L3:B  # Regular vehicle in lane 3 of road B
```
### Adjust Simulation Parameters
In simulator.c:
```bash
'#define VEHICLE_GAP 15'     // Space between vehicles
'#define TURN_SPEED 0.0008f' // Animation speed
```
## Limitations
1. Vehicle Stopping Issue: 
    - Vehicles sometimes stop in the middle of intersections when lights change

2. Lane Change Collisions: 
    - Occasional collisions during lane changes

3. Performance Impact: 
    - Heavy traffic can cause performance degradation

4. Turning Problem for CL3 and DL3:  
    - Vehicles in CL3 and DL3 lanes do not follow a smooth and realistic turning path when making a turn


## 📚 References
- SDL2 Documentation
- Data Structures & Algorithms Course Material
- Traffic Management Systems Design Patterns
