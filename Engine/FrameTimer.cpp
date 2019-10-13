#include "FrameTimer.h"
using namespace std::chrono;

FrameTimer::FrameTimer()
{
	last = steady_clock::now();
}

float FrameTimer::Mark()
{
	const auto old = last;
	last = steady_clock::now();
	const duration<float> elapsed_time = last - old;
	
	return elapsed_time.count();
}

bool FrameTimer::time_to_move( float snake_speed )
{
	const duration<float> elapsed_time_since_move = steady_clock::now() - last;
	if (elapsed_time_since_move.count() >= snake_speed)
	{
		last = steady_clock::now();
		return true;
	}
	return false;
}
