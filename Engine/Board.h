#pragma once
#include "Graphics.h"
#include "Location.h"

class Board
{
public:
	Board(Graphics& gfx);
	int GetWidth() const;
	int GetHeight() const;
	int GetDim() const;

	void DrawSegment(Color& c, Location& loc);
	void DrawBorder(Color c);
	bool isOutsideBoard(Location loc);
private:
	static constexpr int dimension = 20;
	static constexpr int width = 30;
	static constexpr int height = 25;
	static constexpr int x_offset = (Graphics::ScreenWidth - dimension*width) / 2;
	static constexpr int y_offset = (Graphics::ScreenHeight - dimension*height) / 2;
	Graphics& gfx;
};