# Particle simulator 2

This program is a collision-based particle simulator.  
But don't get too hopeful, "particle" is a just a fancy way to say disc.

Anyway, I have done another particle simulator 2 years ago.
But with time I realised I may have done some mistakes back then and intend to correct them.  
So this is kind of a revenge on this previous project and I hope to make a truly improved version with what I have learned. This should include :
- Better file organisation - ✅
- Multi-threading of the simulation - ✅
- A thread for displaying and event handling - ✅
- Use of shaders - ✅
- Better interactivity - ✅  
- Not being a mess (no garantee) - :shipit:  

## Maps & simulation parameters
**You can now change simualtion and world parameters without looking at the code :**  
The simultation and its world use parameters that can be loaded from files.  
Those files are written in text according to a simple format, so anyone can easily write and change them.  
The file saves/loading_orders.sli describes if files should be loaded and which ones.  
If no file is loaded, the simulation will use the default parameters and world written in the code itself.  
Simulation parameters files are in saves/PSparameters and a few example can be found there.  
World parameters and contenance files are in saves/Map and a few examples can also be found there.  
Simulation and world parameters can be loaded independently of each other.  
Although some combinations might not work well with each other.  
For example particle radius is often half the size of the world's cells which are often square.  

**For more clarity** on what each parameter does, you can read the Default parameter files (saves/PSparameters/Default.psp or saves/Map/Default.map).  

## Saving and loading the simulation
In the file saves/loading_orders.sli, you can order to save the simulation that will be ran.  
You can also order to load the positions that were saved during a previous simulation.  
This way, you can run a slow simulation and then replay it later / faster.  
3 little precisions :
- Saving positions will stop at 30GB (limit set in Particle_simulator.cpp)  
- A compression level can be used to decrease the memory size of a saved time step (see more in saves/loading_orders.sli).  
- Saving is done following a mimimum simulation time step corresponding to the time step at the start of the simulation. This way you can decrease the time step without changing the simulation time between 2 consecutive positions saves. This is useful as high speeds don't go well with large time step. This can also be used to decrease the memory size per simulation seconds of the save. (Initial time step in saves/Psparameters/*.psp)  


## UI
**KEYBOARD**  
**escape :** close the window  
**space :** pause the simulation  
**; :** 1 step forward the simulation  
**, :** slowly step forward the simulation  
**+ :** x2 the simulation's timestep (long timesteps can make the simulation unstable)  
**- :** /2 the simulation's timestep (shorter timesteps make the simulation more stable)  
**suppr :** if some particles are selected : delete them  
**F :** toggle fullscreen  
**H :** reset to home view  
**Ctrl+H :** reset simulation  
**P :** toggle display of Particles  
**O :** toggle display of Zones  
**I :** toggle display of interaction circle  
**J :** toggle display of the World's borders  
**K :** toggle display of the FPS display (only works with SFML rendering for now)  
**L :** toggle display of Segments  
**M :** toggle display of World's grid  
**W :** toggle all displaying (including camera movement) but not the simulation. This can be used to slightly reduce the strain on the CPU  
**Ctrl+C :** toggle screen clearing before each frame (objects leave trails). WARNING this functionality doesn't work well in fullscreen (F) and will blink a lot.  
**C :** clear the screen before the next frame (as long as C is pressed)  
**S :** take a screenshot (saving it as result_images/screenshot.png)  

**MOUSE**  
**mouse wheel :** zoom / unzoom the view  
**right mouse and drag :** move around the view  

**left mouse** on a particle to select it.  
**left mouse + D** on a particle to follow it (doesn't select the particle).  
**left mouse and drag :** move around selected particle  
**Ctrl+left** to _not_ unselect already selected particles when left clicking

**suppr + left mouse :** delete all particles in the pointed area  
**A + left mouse :** apply an attractive / repulsive force on every particle   
**Z + left mouse :** apply an attractive / repulsive force on every particle in the pointed area  
**E + left mouse :** apply a rotational force on every particle   
**R + left mouse :** apply a rotational force on every particle in the pointed area  
**T + left mouse :** apply both an attractive / repulsive and rotational force on every particle   
**Y + left mouse :** apply both an attractive / repulsive and rotational force on every particle in the pointed area  

**DETAIL**  
Left clicking on a particle is considered _selecting_ that particle, so it will not apply any force or delete it.  
To apply a force or a deletion in a zone, first press the corresponding key then left click (not on a particle). You can then keep left mouse pressed to keep the force or deletion happening. You also don't need to keep the key pressed once you have left clicked.


## Compilation

**Commands**  
-"make" simply compiles.  
-"make sure" compiles and runs the program.  
-"make nopengl" compiles the program with OpenGL removed. Only SFML will be used for rendering.  
-"make clean" cleans the build files but not the executable.  
-"make again" cleans and compiles.  

**Possible issues**  
If the program doesn't compile because of linking issues with OpenGL libraries, "make clean" then "make nopengl" might solve it.

**Environment**  
The compilation of this program is tested with g++15, std=c++17 on Ubuntu 22.04.  
Unfortunately, I don't have any other machines to try it on but it should work with most g++ versions after 11 and SFML is cross platform. 

**This program uses SFML**  
This c++ program uses SFML 2.5.1.  
I used [this guide](https://www.sfml-dev.org/tutorials/2.5/start-linux.php) for the installation on Linux.  
For Windows you might be able to download SFML from [there](https://www.sfml-dev.org/download/sfml/2.5.1/).