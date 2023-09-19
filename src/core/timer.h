#pragma once

class Timer {
public:
	Timer(float cooldown = 0) :
		m_cooldown(cooldown) {}

	/// @param cooldown >= 0 to reset, < 0 to apply only when stopped
	/// @return whether the time was newly started
	bool set(float cooldown)
	{
		bool to_start = !isActive();

		if (cooldown < 0) {
			// Refill only when stopped
			if (to_start)
				m_cooldown = -cooldown;
		} else {
			// Refill in any case
			m_cooldown = cooldown;
		}
		return to_start;
	}

	inline void stop() { m_cooldown = 0; }

	/// @return true if the timer stopped in this step
	bool step(float dtime)
	{
		if (isActive())
			m_cooldown -= dtime;
		else
			return false;

		// return true exactly once
		return m_cooldown <= 0;
	}

	inline bool isActive() const { return m_cooldown > 0; }

private:
	float m_cooldown = 0;
};
