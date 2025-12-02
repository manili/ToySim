# ToySim
Here is a step by step to use the `ToySim`:

```
sudo apt install yosys

git clone --recurse-submodules https://github.com/manili/ToySim.git

cd ToySim/slang
cmake -B build -DCMAKE_INSTALL_PREFIX=$(pwd)/../slang_install
cmake --build build -j4
cmake --install build

cd ../src/examples/sync_half_adder
mkdir build; cd build
cmake ..
cmake --build .
./toysim
# Now you can open sync_half_adder_waves.vcd with GTKWave or other VCD viewers

cd ../../counter
mkdir build; cd build
cmake ..
cmake --build .
./toysim
# Now you can open counter_waves.vcd with GTKWave or other VCD viewers
```
