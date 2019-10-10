#include "Board.h"

Board::Board(Graphics& gfx)
	:
	gfx(gfx)
{}

int Board::GetWidth() const
{
	return width;
}

int Board::GetHeight() const
{
	return height;
}

int Board::GetDim() const
{
	return dimension;
}

void Board::DrawSegment(Color& c, Location& loc)
{	
	gfx.DrawRectPadded(loc.x*dimension + x_offset, loc.y*dimension + y_offset, dimension, dimension, c);
}

void Board::DrawBorder(Color c)
{
	gfx.DrawHollowRect(x_offset, y_offset, dimension*width, dimension*height, c);
}

bool Board::isOutsideBoard(Location loc)
{
	if (loc.x < 0 ||
		loc.y < 0 ||
		loc.x > width ||
		loc.y > height)
	{
		return true;
	}
	else 
		return false;
}

