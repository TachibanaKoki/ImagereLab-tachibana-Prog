#include "framework.h"

namespace Framework
{
	// Timer implementation

	Timer::Timer()
	:	m_timestep(0.0),
		m_time(0.0),
		m_smoothstep(0.0),
		m_frameCount(0)
	{
		i64 frequency;
		QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);
		m_period = 1.0f / double(frequency);

		QueryPerformanceCounter((LARGE_INTEGER *)&m_startupTimestamp);
		m_previousFrameTimestamp = m_startupTimestamp;
	}

	void Timer::OnFrameStart()
	{
		++m_frameCount;

		i64 timestamp;
		QueryPerformanceCounter((LARGE_INTEGER *)&timestamp);

		m_time = double(timestamp - m_startupTimestamp) * m_period;

		m_timestep = double(timestamp - m_previousFrameTimestamp) * m_period;
		m_previousFrameTimestamp = timestamp;

		m_smoothstep = m_smoothstep * 0.9 + m_timestep * 0.1;
	}
}
