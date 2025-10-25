rm -rf build CMakeCache.txt CMakeFiles && cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build --parallel
