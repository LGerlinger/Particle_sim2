# Particle simulator 2

This program is a collision-based particle simulator.  
But don't get too hopeful, "particle" is a just a fancy way to say disc.

Anyway, I have done another particle simulator 2 years ago.
But with time I realised I may have done some mistakes back then and intend to correct them.  
So this is kind of a revenge on this previous project and I hope to make a truly improved version with what I have learned. This should include :
- Better file organisation
- Multi-threading of the simulation
- A thread for displaying and event handling
- Use of shaders
- Not being a mess (no garantee)

## UI
**KEYBOARD**  
**escape :** close the window  
**space :** pause the simulation  
**; :** step forward the simulation  
**+ :** x2 the simulation's timestep (long timesteps can make the simulation unstable)  
**- :** /2 the simulation's timestep (shorter timesteps make the simulation more stable)  
**suppr :** if some particles are selected : delete them
**F :** toggles fullscreen

**MOUSE**  
**mouse wheel :** zoom / unzoom the view  
**right mouse and drag :** move around the view  

**left mouse** on a particle to select it.  
**left mouse and drag :** move around selected particle  
**left ctrl** to _not_ unselect already selected particles when left clicking

**suppr + left mouse :** if no particle is selected, delete all particles in the pointed area  
**A + left mouse :** apply an attractive / repulsive force on every particle   
**Z + left mouse :** apply an attractive / repulsive force on every particle in the pointed area  
**E + left mouse :** apply a rotational force on every particle   
**R + left mouse :** apply a rotational force on every particle in the pointed area  
**T + left mouse :** apply both an attractive / repulsive and rotational force on every particle   
**Y + left mouse :** apply both an attractive / repulsive and rotational force on every particle in the pointed area  

**DETAIL**  
Left clicking on a particle is considered _selecting_ that particle, so it will not apply any force or delete it.  
To apply a force or a deletion in a zone, first press the corresponding key then left click (not on a particle). You can then keep left mouse pressed to keep the force or deletion happening. You also don't need to keep the key pressed once you have left clicked.


## This program uses SFML

The c++ program uses SFML 2.5.1. I think I used [this guide](https://www.sfml-dev.org/tutorials/2.6/start-linux.php) for the installation.