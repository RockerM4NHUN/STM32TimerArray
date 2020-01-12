# Advanced delay change
This example shows how to handle changing delay of timers correctly, and showcases traps for young players.\
A ready to use baseline project is required to continue.

See the `project_setup_with_cubemx` example for getting started and setting up a baseline project.\
If you are experienced with the STM32 HAL environment and with writing setup code, see `project_setup_with_hal`.

### 1. Configure the hardware (same as Blinky)
- Open the STM32CubeMX configuration file.
- Under *Timers* open *TIM2* and set the clock source to *Internal Clock*. (By default the frequency of TIM2 will match the CPU's.)
- Under *NVIC Settings* enable *TIM2 global interrupt*.
- Make sure, that the user LED is named `LD2` and configured as output. (This is the case by default.)
- Click *Generate Code* to update settings in source.

### 2. Setup software
- Copy the contents of *advanced_delay_change.cpp* to *src/app.cpp* in your project.
- Click PlatformIO Upload.
- The user LED should flash with changing frequency, without glitches.

### 3. Try different implementations
- By commenting, select different delay change implementations.
- Note that some implementations work weirdly depending on circumstances.
