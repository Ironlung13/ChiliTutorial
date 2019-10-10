#include "Food.h"

Location Food::GetLocation()
{
	return loc;
}

void Food::DrawToBoard(Board & brd)
{
	Color c;
	c = Colors::Red;
	brd.DrawSegment(c, loc);
}

void Food::Jump(Location new_loc)
{
	loc = new_loc;
}


