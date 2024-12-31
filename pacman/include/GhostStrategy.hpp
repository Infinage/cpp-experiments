#pragma once

#include <vector>

#include "Constants.hpp"

// Forward Decl
class Ghost; class Pacman;

class Strategy {
    protected:
        Ghost &ghost;
        std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map;
        std::vector<std::tuple<std::size_t, std::size_t, DIRS>> getNeighbouringCells();

    public:
        virtual ~Strategy() = default;
        Strategy(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, Ghost &ghost);
        virtual DIRS getNext() = 0;
};

class Shadow: public Strategy {
    private:
        Pacman &pacman;

    public:
        Shadow(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, Ghost &ghost, Pacman &pacman);
        DIRS getNext() override;
};
