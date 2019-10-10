#pragma once
#include "Location.h"
#include "Board.h"


class Food
{
public:
	Location GetLocation();
	void DrawToBoard(Board& brd);
	void Jump(Location new_loc);
private:
	Location loc;
};