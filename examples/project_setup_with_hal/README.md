# Project setup with STM32 HAL tutorial
This file walks through the creation of an STM32 microcontroller based project with PLatformIO and STM32 HAL, using C/C++ language.
You will need to install VSCode and the PlatformIO extension for VSCode before proceeding further.

### 1. Create a PlatformIO project
- In VSCode open PlatformIO (PIO) Home and click *New Project*.
- Choose a fitting project name.
- Select your STM32 board.
- Select *STM32Cube* framework.
- Optionally specify the project location.
- Click *Finish* to create the project.

### 2. Setup your own project layout
- I can't really help with that, you chose the "hard way" :)
- Keep *include* and *src* folders please.

### 3. Compile with STM32TimerArray library example setup
- In the *platformio.ini* file add: `lib_deps = STM32TimerArray`
- Copy the contents of *project_setup_with_hal.cpp* to *src/app.cpp*, *app.h* to *include*.
- Copy *stm32_hal.h* to *include* and update the contents according to your board and CPU.
- Include `app.h` and add `app_start();` in your main function.
- (For C++ headers use .hpp extension to have correct VSCode language detection).
- At this point your project should compile and the library is accessable from *app.cpp*.
- Optionally if you want to use the PIO Monitor for serial communication, add this: `monitor_speed = 115200` (and setup your UART periphery accordingly).

### 4. Run examples
- Follow the periphery setup instructions in the example's readme file, copy the contents of *\<example>.cpp* to *src/app.cpp*.
- Connect your STM32 board.
- Click PlatformIO Upload.
- The project should compile, upload and execute.
- In this project setup example the user LED will blink, using the HAL environment.
