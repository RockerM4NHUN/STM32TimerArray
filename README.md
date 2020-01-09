# STM32TimerArray
To aid the user with hardware based programmable timers, this library provides a timer array implementation for the STM32 platform.\
If any problem is encountered, please open an issue, or write an email.

Visit the [examples][examples_dir] folder to get started, [project_setup_with_cubemx][project_setup_with_cubemx_dir] is the recommended starting point.

## Environment
The library is intended for PlatformIO + STM32Cube or pure HAL projects, with C++ language. The examples contain CubeMX setup instructions for inexperienced users. If you are familiar with STM32, feel free to write setup code in pure HAL, see [project_setup_with_hal][project_setup_with_hal_dir].

## Short overview of the library
The library works with the `TimerArrayControl` class handling the hardware and `Timer` instances holding callbacks and the required timing for them. A `Timer` holds the amount of ticks until the callback is fired. Timers can be attached to a `TimerArrayControl`, counting from that moment the callback will be fired after the specified amount of ticks elapsed in the controller. Users can set the controller's counting frequency to match their needs. Multiple controllers can be used, but a timer can only be attached to one controller at a time. Also a hardware timer can only be used by one controller at a time. `ContextTimer<ContextType>` behaves exactly like a `Timer`, but it also carries a context pointer provided for the callback. This can be useful when objects want to have their own timers.

## Versions
- *Planned Version 1.0.0*\
  Examples for all functionality. Proper documentation of API.
<<<<<<< HEAD
  
- *Planned Version 0.5.0*\
  New implementation to support every STM32 timer.
  
- *Planned Version 0.4.0*\
  Refactor of TimerArrayControl. Unit tests. New manual fire and query functions.
=======
  
- *Planned Version 0.5.0*\
  New implementation to support every STM32 timer.
>>>>>>> dev
  
- **@ Version 0.4.0**\
  Refactor of TimerArrayControl. Unit tests. New manual fire and query functions.
  
- Version 0.3.0\
  New ContextTimer for callbacks with context, avoiding the use of expensive lambdas. No unit tests yet.

- Version 0.2.0\
  Interrupt safe version, but only lightly tested.
  
- Version 0.1.0\
  Basic functionality exists.

[examples_dir]: https://github.com/RockerM4NHUN/STM32TimerArray/blob/master/examples
[project_setup_with_cubemx_dir]: https://github.com/RockerM4NHUN/STM32TimerArray/blob/master/examples/project_setup_with_cubemx
[project_setup_with_hal_dir]: https://github.com/RockerM4NHUN/STM32TimerArray/blob/master/examples/project_setup_with_hal