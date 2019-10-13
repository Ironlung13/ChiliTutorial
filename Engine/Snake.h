#pragma once
#include "Graphics.h"
#include "Location.h"
#include "Board.h"
#include "Keyboard.h"
#include "Food.h"
#include <random>


class Snake
{
public:
	void InitHead();
	void InitSegment();
	void CheckForInput(Keyboard& kbd);
	void Move();
	bool CheckFood(Food& food);
	void Grow();
	void DrawToBoard(Board& brd);
	bool EatsItself();
	Location& GetNextHeadLocation();
	float GetSpeed();

	void Reset();

private:
	
		class Segment
		{
		public:
			void MoveHead(Location& delta_loc);
			void SetLocation(Segment& new_loc);
			void SetLocation(Location new_loc);
			void SetColor(Color color);
			Location& GetLocation();
			Color& GetColor();
		private:
			Location loc;
			Color c;
		};


private:
	static constexpr int MaxSegments = 100;
	Segment SegmentNumber[MaxSegments];
	int nSegments = 1;
	Location delta_loc;
	Location prev_delta_loc = delta_loc;
	static constexpr float SpeedMin = 0.2f;
	static constexpr float SpeedMax = 0.06f;
};


