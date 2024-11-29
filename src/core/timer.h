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

	inline float remainder() const { return m_cooldown; }
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

class RateLimit {
public:
	/// @param units_per_second Higher = allow a larger sum per second
	/// @param seconds_limit    Burst tolerance. Hard-limit after which a cooldown must happen.
	RateLimit(float units_per_second, float seconds_limit) :
		m_weight(1.0f / units_per_second),
		m_limit(seconds_limit)
	{}

	void step(float dtime)
	{
		if (m_timer > 0)
			m_timer -= dtime;
	}

	bool add(float units)
	{
		float seconds = units * m_weight;
		// Avoid permanent lock-out
		if (m_timer < 1.5f * (m_limit + seconds))
			m_timer += seconds;
		return isActive();
	}

	inline float getSum() const { return m_timer / m_weight; }
	inline float getSumLimit() const { return m_limit / m_weight; }
	inline bool isActive() const { return m_timer > m_limit; }

private:
	float m_weight,
		m_limit,
		m_timer = 0;
};
