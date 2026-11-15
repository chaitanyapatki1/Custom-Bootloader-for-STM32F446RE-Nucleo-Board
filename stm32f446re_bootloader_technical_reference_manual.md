# STM32F446RE Bootloader Technical Reference Manual
*Production-Grade Firmware Architecture, Launcher Routine & Diagnostic Interface*

---

## 1. High-Level Overview & Workspace Setup
This technical reference manual defines the design principles, operational mechanics, and firmware implementation of a production-grade custom bootloader for the STM32F446RE (ARM Cortex-M4) microcontroller. The workspace is organized into a dual-project structure within STM32CubeIDE to isolate the bootloader binary from the main user application:
- **`BootloaderF446RENucleo`**: Occupies Sector 0 and Sector 1 of Flash memory. Responsible for system initialization, diagnostic CLI handling over USART2, application vector validation, and safe execution jumping.
- **`UserAppF446RE`**: Placed starting at Sector 2 (address `0x08008000`). Represents the target application firmware image to be updated or launched by the bootloader.

### 1.1 Boot Sequence & PC13 Jumper Logic
Upon Power-On Reset (POR), system execution begins inside Sector 0 at the bootloader reset handler. The bootloader polls the user blue push-button (GPIO pin `PC13`) during a 3-second startup timeout window:
- **Button Pressed (`PC13` LOW)**: The bootloader halts the auto-boot counter and enters the interactive Diagnostic Command Line Interface (CLI) over USART2 (115200 baud, 8N1 format).
- **Timeout Expired / Button Not Pressed**: The bootloader validates the application Main Stack Pointer (MSP) at address `0x08008000` and automatically launches the user application via `go2APP()`.

### 1.2 Verification & Build Workflow
Both projects must be compiled using ARM GCC with target-specific linker script configurations ensuring non-overlapping Flash allocation. Verification is performed by programming the bootloader binary into Flash, observing initial LED indicator state during the 3-second window, and monitoring serial CLI output via USART2 terminal loggers.

---

## 2. System Architecture & Memory Mapping
The STM32F446RE microcontroller features 512 KB of internal Flash memory and 128 KB of System SRAM. The memory map is partitioned to isolate the primary bootloader from the main application image, ensuring safe execution, clear failure domains, and field-upgradability.

### 2.1 Flash Memory Partitioning
Flash memory is organized into distinct physical sectors. The bootloader resides in Sectors 0 and 1, occupying 32 KB. The user application occupies Sectors 2 through 7, providing 480 KB of executable space.

| Region | Physical Sector(s) | Address Range | Size | Allocation / Description |
| :--- | :--- | :--- | :--- | :--- |
| **Bootloader** | Sector 0 - Sector 1 | `0x08000000` - `0x08007FFF` | 32 KB | Core Bootloader Binary & Diagnostics CLI |
| **Application** | Sector 2 - Sector 7 | `0x08008000` - `0x0807FFFF` | 480 KB | Main Application Firmware Image |
| **SRAM1** | N/A | `0x20000000` - `0x2001C000` | 112 KB | Main SRAM Data & Stack Space |
| **SRAM2** | N/A | `0x2001C000` - `0x2001FFFF` | 16 KB | Shared SRAM / Boot Flags & Buffers |

### 2.2 SRAM Boundary Setup & Stack Allocation
The SRAM configuration enforces strict separation between peripheral buffers and system stacks.
- **Bootloader Stack Top**: `0x20020000` (End of SRAM2)
- **Application Stack Top**: Defined dynamically by word 0 of the Application Vector Table (`*(uint32_t*)0x08008000`).
- **SRAM Range Check**: Valid Stack Pointer addresses must fall strictly within `0x20000000` and `0x20020000`.

---

## 3. Application Launcher & Vector Relocation Routine
The application launcher (`go2APP`) transitions execution from the bootloader context to the user application. This process requires validation of the destination stack pointer, de-initialization of active peripherals, disabling interrupt sources, remapping the vector table offset register (`VTOR`), and executing the reset handler jump.

```text
+-------------------------------------------------------------------+
|                        Bootloader Context                         |
|  1. Check User Force-Entry Pin / Command                          |
|  2. Read Application MSP from 0x0800 8000                         |
|  3. Validate MSP (0x20000000 <= MSP <= 0x20020000)                |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                       Hardware De-init Phase                      |
|  1. Disable Global Interrupts (__disable_irq())                   |
|  2. Reset RCC Clock Config to HSI Internal Oscillator             |
|  3. Clear & Disable NVIC Enable / Pending Registers               |
|  4. De-init Peripherals (USART, GPIO, SysTick)                    |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                      Vector Relocation & Jump                     |
|  1. Set SCB->VTOR = 0x0800 8000                                   |
|  2. Load Reset Vector Pointer from 0x0800 8004                    |
|  3. Set Main Stack Pointer (__set_MSP(app_msp))                   |
|  4. Execute Branch to Reset Handler Function                      |
+-------------------------------------------------------------------+
```

### 3.1 C Launch Implementation
```c
#include "stm32f4xx.h"

#define APP_START_ADDRESS    0x08008000U
#define SRAM_START          0x20000000U
#define SRAM_END            0x20020000U

typedef void (*pFunction)(void);

void go2APP(void) {
    uint32_t app_msp = *(__IO uint32_t*)APP_START_ADDRESS;
    uint32_t app_reset_handler = *(__IO uint32_t*)(APP_START_ADDRESS + 4);

    /* 1. Validate Main Stack Pointer boundary */
    if (app_msp < SRAM_START || app_msp > SRAM_END) {
        return; /* Invalid image; return to CLI menu */
    }

    /* 2. Disable global interrupts */
    __disable_irq();

    /* 3. Disable SysTick and reset core state */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 4. Clear and disable all NVIC IRQs */
    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 5. Reset Clock Settings to default HSI */
    RCC_DeInit();

    /* 6. Relocate Vector Table Offset Register (VTOR) */
    SCB->VTOR = APP_START_ADDRESS;

    /* 7. Set Main Stack Pointer and execute Jump */
    __set_MSP(app_msp);
    pFunction jump_to_app = (pFunction)app_reset_handler;
    jump_to_app();
}
```

---

## 4. Diagnostic CLI Command Suite
The Bootloader includes an interactive Command Line Interface (CLI) over USART2 (115200 8N1) for board bring-up, runtime verification, and manual execution control.

| Command | Syntax | Operational Description | Output Structure / Parameters |
| :--- | :--- | :--- | :--- |
| **help** | `help` | Prints all available commands and basic parameter usage notes. | String table of commands and summaries. |
| **version** | `version` | Displays bootloader release version, build date, and target MCU. | Format: `v1.2.0 (ARM GCC, Built: [Date])` |
| **info** | `info` | Queries Flash sector layout, Application validity status, and clock settings. | Outputs structured memory analysis text block. |
| **jump** | `jump` | Invokes `go2APP()` launcher routine manually. | Returns error message if MSP check fails. |
| **reset** | `reset` | Triggers a software System Reset via `NVIC_SystemReset()`. | Instantly reboots system into sector 0. |
| **dump** | `dump <hex_addr> <len>` | Reads and displays hex values from a designated memory address. | Hexdump view (16-byte aligned output rows). |
| **pins** | `pins` | Samples state of diagnostic force-boot GPIO pins. | Format: `PA0: HIGH, PC13: LOW` |
| **clear** | `clear` | Transmits ANSI escape sequence to clear terminal screen. | `\033[2J\033[H` |
| **uid** | `uid` | Reads the 96-bit Unique Device ID (UDID) registers. | 32-bit Hex triplet: `0xXXXX 0xYYYY 0xZZZZ` |

---

## 5. Design Scope & Zero-Bricking Safety Rationale
Field-deployed firmware updates carry inherent risks, including partial writes, power failures during flashing, and invalid application images. The architecture incorporates critical safeguards to maintain operational safety.

### 5.1 Strict Memory Boundaries & Isolation
- **Hardware Read-Out Protection (ROP)**: Bootloader sectors can be protected via Flash Option Bytes (PCROP/WRP) to prevent unintended write or erase operations from the Application environment.
- **Non-Overlapping Segments**: Sector 0 (`0x08000000`) is never erased or overwritten during application update routines.

### 5.2 Hardware & Vector Clean-Up Rationale
- **Peripherals De-initialization**: Peripherals left active by the bootloader can trigger unhandled interrupt requests (IRQs) within the newly launched Application before its vector table or handlers are fully initialized. Disabling all interrupts and clearing peripheral registers prevents hard faults during boot transitions.
- **VTOR Alignment**: ARM Cortex-M4 architecture requires the Vector Table Offset Register to be aligned to the vector size boundary (aligned to `0x200` for 128 vectors). `0x08008000` satisfies this constraint (32 KB offset).

### 5.3 Fallback Execution Flow
1. **Power-On Reset (POR)**: The processor boots into Sector 0 and executes the Bootloader.
2. **Force Entry Evaluation**: The bootloader evaluates the state of a physical jumper/button pin (e.g., `PC13`) and a specific magic word located in shared SRAM2.
3. **Application Validation**: If no force-entry condition exists, the bootloader verifies the Application MSP at address `0x08008000`.
4. **Automatic Fallback**: If the MSP is invalid (e.g., memory is erased or corrupt), the launcher aborts the jump, prints an alert via USART2, and drops to the diagnostic CLI interface. The system cannot be bricked unless Sector 0 hardware itself is physically destroyed.

### 5.4 System Architecture & Memory Map Summary
The STM32F446RE microcontroller features an ARM Cortex-M4 core operating up to 180 MHz, with 512 KB internal Flash and 128 KB SRAM. Physical memory partitioning guarantees operational integrity during updates:

| Region | Flash Start Address | Size | Purpose |
| :--- | :--- | :--- | :--- |
| **Bootloader Sector** | `0x08000000` | 32 KB | Hardware Init, CLI, Validation, Jump Logic |
| **User Application Sector** | `0x08008000` | 480 KB | Target Firmware Execution Space |

### 5.5 Linker Script Configurations (`.ld`)
Spatial separation is enforced at compile-time by tailoring the `MEMORY` definitions within `STM32F446RETX_FLASH.ld` for each respective project:

**Bootloader Linker Configuration (`0x08000000`)**:
```ld
MEMORY
{
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
}
```

**User Application Linker Configuration (`UserAppF446RE`)**:
```ld
MEMORY
{
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH  (rx)  : ORIGIN = 0x08008000, LENGTH = 480K
}
```

### 5.6 Core Bootloader Sequence & Implementation

#### A. Boot Window & Button Sampling Logic
Upon hardware reset (NRST), system clocks and peripherals (USART2, GPIO) are initialized. A 3-second non-blocking tick window monitors the blue user button (B1 / `PC13`):
```c
uint32_t start_tick = HAL_GetTick();
uint8_t stay_in_bootloader = 0;

while (HAL_GetTick() - start_tick < 3000)
{
    // Nucleo button pulls pin LOW (GPIO_PIN_RESET) when pressed
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)
    {
        stay_in_bootloader = 1;
        break;
    }
}
```

#### B. Interactive UART CLI Parser
If `stay_in_bootloader` is asserted, control enters an infinite blocking command parsing loop utilizing UART byte reception over USART2 (115200 baud):
```c
if (stay_in_bootloader)
{
    char *stay_msg = "-> Bootloader Mode Active (Type 'help' for commands).\r\n > ";
    HAL_UART_Transmit(&huart2, (uint8_t*)stay_msg, strlen(stay_msg), HAL_MAX_DELAY);

    char cmd_buffer[32];
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    uint8_t idx = 0;

    while (1)
    {
        uint8_t rx_byte;
        if (HAL_UART_Receive(&huart2, &rx_byte, 1, 100) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2, &rx_byte, 1, HAL_MAX_DELAY); // Echo

            if (rx_byte == '\b' || rx_byte == 127) { // Backspace
                if (idx > 0) cmd_buffer[--idx] = '\0';
            }
            else if (rx_byte == '\r' || rx_byte == '\n') { // Enter
                HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
                cmd_buffer[idx] = '\0';

                if (strcmp(cmd_buffer, "help") == 0 || strcmp(cmd_buffer, "?") == 0) {
                    char *help_text = "Available commands:\r\n"
                                      "  help     - Show this help menu\r\n"
                                      "  version  - Display bootloader version\r\n"
                                      "  jump     - Jump to user application\r\n"
                                      "  erase    - Erase application sector (stub)\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)help_text, strlen(help_text), HAL_MAX_DELAY);
                }
                else if (strcmp(cmd_buffer, "version") == 0) {
                    char *ver_text = "STM32F446RE Custom Bootloader v1.0\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)ver_text, strlen(ver_text), HAL_MAX_DELAY);
                }
                else if (strcmp(cmd_buffer, "jump") == 0) {
                    char *jump_text = "Attempting manual jump to application...\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)jump_text, strlen(jump_text), HAL_MAX_DELAY);
                    go2APP();
                    char *fail_text = "Jump failed: No valid application found at 0x08008000.\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)fail_text, strlen(fail_text), HAL_MAX_DELAY);
                }
                else if (strcmp(cmd_buffer, "erase") == 0) {
                    char *erase_text = "Flash erase routine not yet implemented.\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)erase_text, strlen(erase_text), HAL_MAX_DELAY);
                }
                else if (strlen(cmd_buffer) > 0) {
                    char *err_text = "Unknown command. Type 'help' for options.\r\n";
                    HAL_UART_Transmit(&huart2, (uint8_t*)err_text, strlen(err_text), HAL_MAX_DELAY);
                }

                memset(cmd_buffer, 0, sizeof(cmd_buffer));
                idx = 0;
                HAL_UART_Transmit(&huart2, (uint8_t*)" > ", 3, HAL_MAX_DELAY);
            }
            else {
                if (idx < sizeof(cmd_buffer) - 1) cmd_buffer[idx++] = rx_byte;
            }
        }
    }
}
```

### 5.7 Application Validation & Vector Table Architecture

#### A. Vector Table Relocation (`SCB->VTOR`)
Because the application binary starts at `0x08008000`, exception vectors must be updated in Cortex-M4 System Control Block (`SCB`) early in `main()` of `UserAppF446RE`:
```c
// Relocate the Vector Table to the application's flash start address
SCB->VTOR = FLASH_BASE | 0x8000; // Resolves to 0x08008000
```

#### B. Safety Validation & Jump Mechanics Routine (`go2APP`)
To avoid hard fault conditions, `go2APP()` performs double-validation before branching:
```c
typedef void (*pFunction)(void);

void go2APP(void) {
    uint32_t app_address = 0x08008000;

    // 1. Validate Stack Pointer: Must point to a valid RAM address (0x20000000 - 0x20020000)
    uint32_t msp = *(__IO uint32_t*)app_address;
    if ((msp >= 0x20000000) && (msp <= 0x20020000)) {

        // 2. Extract Reset Handler entry address from vector table (+4 offset)
        uint32_t jump_address = *(__IO uint32_t*)(app_address + 4);
        pFunction jump_to_application = (pFunction)jump_address;

        // 3. Clean up hardware state & reset clock/peripherals
        HAL_RCC_DeInit();
        HAL_DeInit();
        __disable_irq();

        // 4. Load application's Main Stack Pointer (MSP)
        __set_MSP(msp);

        // 5. Branch/Jump to Application's Reset Handler
        jump_to_application();
    }
}
```

> **Key Operational Takeaway:** De-initializing peripheral hardware (`HAL_DeInit()`, `HAL_RCC_DeInit()`) and calling `__disable_irq()` prior to resetting the Main Stack Pointer (`__set_MSP()`) guarantees that stale interrupts or peripheral DMA requests from the bootloader do not corrupt application initialization.

### 5.8 End-to-End Verification Flow
1. **Compilation & Memory Placement**: Linker scripts enforce memory boundaries (Bootloader <32KB at `0x08000000`, App at `0x08008000`).
2. **Memory Inspection**: STM32CubeIDE Memory Monitor at `0x08008000` verifies valid MSP word layout (e.g., `00000220` -> `0x20000220` SRAM).
3. **Execution Hand-off**: Bootloader verifies valid RAM bounds, de-initializes hardware, sets MSP, branches to application Reset Handler, and prints application banner over UART.