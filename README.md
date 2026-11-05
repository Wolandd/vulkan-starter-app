# Лабораторная №1 — вариант 3: параллелепипед

Прямоугольный параллелепипед 2.0 × 1.5 × 1.0 с однотонными цветами граней.
Реализованы матрицы Model/View/Projection, перспективная и ортографическая
проекции, проверка глубины и отсечение обратных граней.

## Сборка и запуск на macOS

Нужны компилятор C++20 (Apple Command Line Tools), CMake 3.20+, Ninja и
официальный [Vulkan SDK LunarG 1.4.321.0](https://vulkan.lunarg.com/sdk/home#mac)
с MoltenVK. Установите SDK в `~/VulkanSDK/1.4.321.0`.
GLFW, ImGui, vk-bootstrap, VulkanMemoryAllocator и GLM загружает CMake;
для первой конфигурации нужен интернет. Шейдеры компилируются через `glslc`.

Из корня проекта:

```sh
./scripts/build-macos.sh
./scripts/run-macos.sh
```

Скрипты подключают SDK автоматически. Если рядом с репозиторием есть личный
`../vulkan-env.sh`, используется он; иначе — `setup-env.sh` из установленного SDK.
Для другого расположения задайте `CG_VULKAN_ENV` с путём к скрипту окружения.

Можно собрать и без вспомогательных скриптов:

```sh
source "$HOME/VulkanSDK/1.4.321.0/setup-env.sh"
cmake --preset debug
cmake --build build-debug --parallel
./build-debug/vulkan-starter-app
```

Запускайте из корня проекта: пути к шейдерам относительные.
Вместо `debug` можно выбрать `release` и каталог `build-release`.
Проверено на Intel macOS Sequoia 15.8.1 с MoltenVK.

## Windows и Linux

Сохранены пресеты исходного [Vulkan Starter App](https://github.com/vladeemerr/vulkan-starter-app):
`debug` для Linux, `msvc-debug` для Visual Studio 2022 и `mingw-debug` для MinGW.
После настройки Vulkan SDK выполните `cmake --preset <имя пресета>`, затем
`cmake --build build-debug --parallel`. Для release используйте соответствующий
release-пресет и `build-release`. Эта лабораторная на Windows/Linux не проверялась.

## Управление

В панели ImGui: Perspective / Orthographic — проекция; Position — положение;
Rotation — поворот по осям в градусах; Scale — масштаб. Reset возвращает
исходные преобразования и перспективную проекцию. Размер окна можно менять.
