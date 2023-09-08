#include "Timer.h"
#include "LogHelper.h"

// Starts timing
void Timer::start()
{
	if (is_timing)
		return;

	start_time = Clock::now();
	last_delta_time = start_time;
	is_timing = true;
}

// Sets the "start" time of the timer to now
void Timer::reset()
{
	start_time = Clock::now();
}

// Returns the time from start -> now in milliseconds
float Timer::to_now()
{
	if(!is_timing)
	{
		LOGMSG(Log::MessageType::Error, "Tried to get time on idle timer.");
		return 0.0f;
	}

	Duration duration = Clock::now() - start_time;
	return duration.count();
}

// Returns the time from last delta call -> now in milliseconds
float Timer::lap_delta()
{
	if(!is_timing)
	{
		LOGMSG(Log::MessageType::Error, "Tried to get delta time on idle timer.");
		return 0.0f;
	}

	TimePoint now = Clock::now();
	Duration duration = now - last_delta_time;
	last_delta_time = now;
	return duration.count();
}

// Returns the time from last delta call -> now in milliseconds without reseting the delta 
float Timer::peek_delta()
{
	if(!is_timing)
	{
		LOGMSG(Log::MessageType::Error, "Tried to peek delta time on idle timer.");
		return 0.0f;
	}
	TimePoint now = Clock::now();
	Duration duration = now - last_delta_time;
	return duration.count();
}