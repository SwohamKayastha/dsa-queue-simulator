# 🚦 Traffic Junction Simulator

A sophisticated traffic management system that simulates real-world junction behavior with multi-lane roads and smooth vehicle animations.

## 🎥 Demo
![[Traffic Simulator Demo](path_to_your_demo.gif)](https://github.com/SwohamKayastha/dsa-queue-simulator/blob/main/dsa.gif)

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
- 🚑 Emergency Vehicles: Red color (prefix 'EMG' in vehicle ID)
Lane System
### Lane System
![[dsa-image](dsa-queue-simulator\dsa-img.png)](https://github.com/SwohamKayastha/dsa-queue-simulator/blob/main/dsa-img.png)
<!-- here implement the image -->
### Traffic Rules
- 🟢 Green light: Vehicles proceed
- 🔴 Red light: Vehicles stop
- 🚑 Emergency vehicles get priority
- ↩️ Turning animations at intersections

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

## 📚 References
- SDL2 Documentation
- Data Structures & Algorithms Course Material
- Traffic Management Systems Design Patterns
