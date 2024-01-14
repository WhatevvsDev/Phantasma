
// Starts timing from now
void Timer::start()
{
	start_time = Clock::now();
	last_delta_time = start_time;
	is_timing = true;
}

// Returns the time from start -> now in milliseconds
float Timer::start_to_now()
{
	if(!is_timing)
	{
		LOGERROR("Tried to get time on idle timer.");
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
		LOGERROR("Tried to get delta time on idle timer.");
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
		LOGERROR("Tried to peek delta time on idle timer.");
		return 0.0f;
	}
	TimePoint now = Clock::now();
	Duration duration = now - last_delta_time;
	return duration.count();
}