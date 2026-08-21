# Third-party notices

This file is an engineering attribution inventory for the source candidate. It
does not change any component's license and is not legal advice. The project
MIT license applies only to project-owned material described in `LICENSE`.

The inventory covers both current firmware build inputs and vendor source that
is distributed in this repository but may be removed by the linker or not used
by the current target. Existing copyright headers and license files must remain
intact.

| Component | Version | Copyright / origin | License | Repository scope | License text |
|---|---|---|---|---|---|
| CMSIS Core | 5.9.0 | Arm Limited | Apache-2.0 | `Drivers/CMSIS/Include/` | `Drivers/CMSIS/LICENSE.txt` |
| STM32F4 CMSIS Device | 2.6.11 | Arm Limited and STMicroelectronics | Apache-2.0, subject to the package terms | `Drivers/CMSIS/Device/ST/STM32F4xx/`, including device/system material used by the target | `Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.txt`; `LICENSES/STM32CubeF4-1.28.3-Package_license.md` |
| STM32F4 HAL Driver | 1.8.5 | STMicroelectronics | BSD-3-Clause, subject to the package terms | `Drivers/STM32F4xx_HAL_Driver/` | `Drivers/STM32F4xx_HAL_Driver/LICENSE.txt`; `LICENSES/STM32CubeF4-1.28.3-Package_license.md` |
| FreeRTOS Kernel | 10.3.1 | Amazon.com, Inc. or its affiliates | MIT | Used subset under `Middlewares/Third_Party/FreeRTOS/Source/` | `Middlewares/Third_Party/FreeRTOS/Source/LICENSE` |
| candleLight adapter-routing and one-shot capability patches | Base commit `d13b6db511d76885533c6e7e0ef22d74a5e2d817` | Hubert Denkmair / normaldotcom candleLight_fw contributors; project-local patches | MIT | Patches and reproducible build helper under `tools/candlelight_one_shot/`; upstream source is not vendored | Upstream `LICENSE.md`; patch README records the fixed source commit |
| ST FreeRTOS integration notes | STM32CubeF4 1.28.3 integration | STMicroelectronics and upstream authors | Preserve the file notice; ST integration header states BSD-3-Clause | `Middlewares/Third_Party/FreeRTOS/Source/st_readme.txt` | The notice is embedded in `st_readme.txt`; package terms are also retained |
| CubeMX-generated application material | STM32CubeMX 6.18.0 / STM32CubeF4 1.28.3 | STMicroelectronics and named contributors | File-level notices and applicable STM32CubeF4 package terms | `Core/`, `freertos_modbus_can_node.ioc`, generated startup/linker material | File headers; `LICENSES/STM32CubeF4-1.28.3-Package_license.md` |
| Linker script | STM32F446 target template | STMicroelectronics | Redistribution terms embedded in the file | `STM32F446xx_FLASH.ld` | Embedded copyright, conditions, and disclaimer |

## Project-owned sensor implementations

The BME280, VEML7700, and ADXL345 project drivers were inspected as
project-authored implementations based on public datasheet register maps and
formulas. The repository reports cite Bosch, Vishay, and Analog Devices source
documents; no vendor reference software is vendored in `sensors/`. Datasheet
attribution does not replace a software license if vendor source is added later.

## Distribution notes

- Do not remove or rewrite vendor copyright headers.
- Do not apply the root project MIT license to the paths listed above.
- Keep this notice, the component license files, and the exact STM32CubeF4
  1.28.3 package license with a source candidate.
- A future binary package must reproduce the notices required by the applicable
  component terms; P5-S7-T02 and P5-S7-T05 own the binary bundle and Release.
- The official package-license copy has SHA-256
  `2fe52ca80ec3d84064631a051a914b418aa65b9126ae5131ea323213a8be5aa2`.
