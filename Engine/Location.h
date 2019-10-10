#pragma once
struct Location
{
	Location Add(const Location& loc)
	{
		Location new_loc = { x,y };
		new_loc.x += loc.x;
		new_loc.y += loc.y;
		return new_loc;
	}

	int x = 0;
	int y = 0;

	bool operator== (const Location& loc) const
	{
		return x == loc.x && y == loc.y;
	}
};