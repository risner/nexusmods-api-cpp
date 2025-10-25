rm -rf build && cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build --parallel
