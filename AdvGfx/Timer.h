#pragma once

class Timer
{
	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;
	using Duration = std::chrono::duration<float, std::milli>;

	bool is_timing{ false };
	TimePoint start_time;
	TimePoint last_delta_time;

public:
	//Starts timing from now
	void start();

	// Returns the time from start -> now in milliseconds
	float start_to_now();

	// Returns the time from last delta call -> now in milliseconds
	float lap_delta();

	// Returns the time from last delta call -> now in milliseconds without reseting the delta timer
	float peek_delta();
};