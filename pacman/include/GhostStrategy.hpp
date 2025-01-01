#pragma once

#include <random>
#include <vector>

#include "Constants.hpp"
#include "Sprites.hpp"

// Forward Decl
class Ghost; class Pacman;

class Strategy {
    protected:
        Ghost* ghost;
        std::vector<std::tuple<std::size_t, std::size_t, DIRS>> getNeighbouringCells(MAP &map);

    public:
        virtual ~Strategy() = default;
        void setGhost(Ghost *ghost);
        virtual DIRS getNext(MAP &map, Pacman &pacman, GHOSTS &ghosts) = 0;
};

class Shadow: public Strategy {
    public:
        DIRS getNext(MAP &map, Pacman &pacman, GHOSTS &ghosts) override;
};

class Ambush: public Strategy {
    public:
        DIRS getNext(MAP &map, Pacman &pacman, GHOSTS &ghosts) override;
};

class Fright: public Strategy {
    private:
        std::mt19937 RANDOM_GEN{ std::random_device{}() };

    public:
        DIRS getNext(MAP &map, Pacman &pacman, GHOSTS &ghosts) override;
};
