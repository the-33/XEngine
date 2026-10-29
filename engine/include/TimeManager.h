#pragma once
/** @file TimeManager.h
 * @brief Scaled frame time, fixed timestep and engine time values.
 */

#include "Singleton.h"
#include "Property.h"
#include "Engine.h"

/** @brief Provides frame delta, fixed step, elapsed time and time scaling.
 * @ingroup xengine_services
 */
class TimeManager : public Singleton<TimeManager>
{
	friend class Singleton<TimeManager>;
	friend class Engine;

private:
	float Delta() const noexcept { return delta; }
	inline float FixedDelta() const noexcept { return fixedDt; }
	double SinceStart() const noexcept;
	void SetTimeScale(float s) noexcept;
	float TimeScale() const noexcept { return timeScale_; }
	uint64_t GetHighResTimestamp() const noexcept;

	TimeManager() = default;
	~TimeManager() = default;

	bool Init(float fixedDt = 1.f / 60.f) noexcept;
	void Tick() noexcept;

	TimeManager(const TimeManager&) = delete;
	TimeManager& operator=(const TimeManager&) = delete;
	TimeManager(TimeManager&&) = delete;
	TimeManager& operator=(TimeManager&&) = delete;

	double startTicks = 0.0;
	double prevTicks = 0.0;
	float delta = 0.f;
	float timeScale_ = 1.f;
	float fixedDt = 1.f / 60.f;

public:
	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `delta`; use the corresponding instance member in gameplay code.
	 */
	using DeltaProp = PropertyRO < TimeManager, float,
		&TimeManager::Delta>;
	/** @endcond */
	/** @brief Delta between successive frames, scaled by `timeScale` and capped before scaling.
	 * @note Raw frame delta is capped to 0.25 seconds before multiplying by the
	 *       nonnegative time scale; the first frame starts at zero.
	 */
	DeltaProp deltaTime{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `time stamp`; use the corresponding instance member in gameplay code.
	 */
	using TimeStampProp = PropertyRO < TimeManager, uint64_t,
		&TimeManager::GetHighResTimestamp>;
	/** @endcond */
	/** @brief Raw SDL performance-counter ticks (not seconds).
	 * @note Convert an interval to seconds using `SDL_GetPerformanceFrequency()`.
	 */
	TimeStampProp highResTimeStamp{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `fixed delta`; use the corresponding instance member in gameplay code.
	 */
	using FixedDeltaProp = PropertyRO < TimeManager, float,
		&TimeManager::FixedDelta>;
	/** @endcond */
	/** @brief Configured duration of one fixed simulation step, in seconds.
	 */
	FixedDeltaProp fixedDeltaTime{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `since start`; use the corresponding instance member in gameplay code.
	 */
	using SinceStartProp = PropertyRO < TimeManager, double,
		&TimeManager::SinceStart>;
	/** @endcond */
	/** @brief Real elapsed seconds since initialization, unaffected by `timeScale`.
	 */
	SinceStartProp timeSinceStart{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `time scale`; use the corresponding instance member in gameplay code.
	 */
	using TimeScaleProp = Property<TimeManager, float,
		&TimeManager::TimeScale,
		&TimeManager::SetTimeScale>;
	/** @endcond */
	/** @brief Nonnegative multiplier applied to frame delta; 0 pauses scaled updates.
	 * @note Setting a negative value clamps it to zero; `timeSinceStart` still
	 *       advances at real-world speed.
	 */
	TimeScaleProp timeScale{ this };
};
