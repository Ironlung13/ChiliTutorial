#include "Snake.h"


void Snake::InitHead()
{
	SegmentNumber[0].SetLocation({ 10,10 });
	SegmentNumber[0].SetColor({ 255, 120, 0 });
	delta_loc = { 1, 0 };
	prev_delta_loc = delta_loc;
}

void Snake::InitSegment()
{
	switch (nSegments % 3)
	{
	case 0:
		SegmentNumber[nSegments].SetColor({ 0,153,0 });
		break;
	case 1:
		SegmentNumber[nSegments].SetColor({ 0,204,0 });
		break;
	case 2:
		SegmentNumber[nSegments].SetColor({ 0,255,0 });
		break;
	}
	SegmentNumber[nSegments].SetLocation(SegmentNumber[nSegments - 1].GetLocation());
}

void Snake::CheckForInput(Keyboard& kbd)
{
	if (kbd.KeyIsPressed(VK_UP))
	{
		if (abs(prev_delta_loc.y) + 1 != 2)
		{
			delta_loc = { 0,-1 };
		}
	}

	if (kbd.KeyIsPressed(VK_DOWN))
	{
		if (abs(prev_delta_loc.y) + 1 != 2)
		{
			delta_loc = { 0,1 };
		}
	}

	if (kbd.KeyIsPressed(VK_LEFT))
	{
		if (abs(prev_delta_loc.x) + 1 != 2)
		{
			delta_loc = { -1,0 };
		}
	}

	if (kbd.KeyIsPressed(VK_RIGHT))
	{
		if (abs(prev_delta_loc.x) + 1 != 2)
		{
			delta_loc = { 1,0 };
		}
	}
}

void Snake::Move()
{
	for (int i = nSegments - 1; i > 0; --i)
	{
		SegmentNumber[i].SetLocation(SegmentNumber[i - 1]);
	}
	SegmentNumber[0].MoveHead(delta_loc);
	prev_delta_loc = delta_loc;
}


bool Snake::CheckFood(Food& food)
{
	if (((food.GetLocation().x - SegmentNumber[0].GetLocation().x) == 0) && ((food.GetLocation().y - SegmentNumber[0].GetLocation().y) == 0))
	{
		Grow();
		return true;
	}
	else
	{
		return false;
	}
	
}

void Snake::Grow()
{
	if (nSegments < MaxSegments)
	{
		InitSegment();
		++nSegments;
	}
}



void Snake::DrawToBoard(Board& brd)
{
	for (int i = nSegments - 1; i > 0; --i)
	{
		brd.DrawSegment(SegmentNumber[i].GetColor(), SegmentNumber[i].GetLocation());
	}

	
	brd.DrawSegment(SegmentNumber[0].GetColor(), SegmentNumber[0].GetLocation());
}

bool Snake::EatsItself()
{
	Location head_loc_next = SegmentNumber[0].GetLocation().Add(delta_loc);

	for (int i = 1; i < nSegments - 1; ++i)
	{
		if (SegmentNumber[i].GetLocation() == head_loc_next)
		{
			return true;
		}
	}
	return false;
}

Location& Snake::GetNextHeadLocation() 
{
	return SegmentNumber[0].GetLocation().Add(delta_loc);
}

float Snake::GetSpeed()
{
	if (SpeedMin - ((nSegments - 1)*0.02f) > SpeedMax)
	{
		return SpeedMin - ((nSegments - 1) * 0.02f);
	}
	else
		return SpeedMax;
}

void Snake::Reset()
{
	nSegments = 1;
	InitHead();
}






void Snake::Segment::MoveHead(Location& delta_loc)
{
	loc = { loc.x + delta_loc.x, loc.y + delta_loc.y };
}

void Snake::Segment::SetLocation(Segment& new_loc)
{
	loc = new_loc.loc;
}

void Snake::Segment::SetLocation(Location new_loc)
{
	loc = new_loc;
}

void Snake::Segment::SetColor(Color color)
{
	c = color;
}

Location& Snake::Segment::GetLocation()
{
	return loc;
}

Color& Snake::Segment::GetColor()
{
	return c;
}
