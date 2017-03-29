#pragma once

namespace Framework
{
	class Timer
	{
	public:
				Timer();
		void	OnFrameStart();

		double	m_timestep;					// Delta time in seconds between frames
		double  m_smoothstep;				// Same delta with an IIR smoothing filter applied
		double	m_time;						// Time in seconds since startup
		int		m_frameCount;				// Frames since startup

		i64		m_startupTimestamp;			// QPC time of startup
		i64		m_previousFrameTimestamp;	// QPC time of the previous frame
		double	m_period;					// QPC period in seconds
	};
}
