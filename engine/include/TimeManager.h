/** @brief Grant Singleton< TimeManager > access to the internal state of TimeManager.
 */
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
	/** @brief Permit the singleton base to construct and release the TimeManager manager.
	 */
	friend class Singleton<TimeManager>;
	/** @brief Grant Engine access to the internal state of TimeManager.
	 */
	friend class Engine;

private:
	/** @brief Delta in TimeManager.
	 * @return Result of delta in TimeManager.
	 */
	float Delta() const noexcept { return delta; }
	/** @brief Fixed delta in TimeManager.
	 * @return Result of fixed delta in TimeManager.
	 */
	inline float FixedDelta() const noexcept { return fixedDt; }
	/** @brief Since start in TimeManager.
	 * @return Result of since start in TimeManager.
	 */
	double SinceStart() const noexcept;
	/** @brief Set time scale in TimeManager.
	 * @param s S supplied to TimeManager.
	 */
	void SetTimeScale(float s) noexcept;
	/** @brief Time scale in TimeManager.
	 * @return Result of time scale in TimeManager.
	 */
	float TimeScale() const noexcept { return timeScale_; }
	/** @brief Retrieve the high res timestamp from TimeManager.
	 * @return Requested get high res timestamp value or reference.
	 */
	uint64_t GetHighResTimestamp() const noexcept;

	/** @brief Construct a TimeManager instance.
	 */
	TimeManager() = default;
	/** @brief Destroy a TimeManager instance and release owned resources.
	 */
	~TimeManager() = default;

	/** @brief Initialize the TimeManager subsystem.
	 * @param fixedDt Fixed dt supplied to TimeManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool Init(float fixedDt = 1.f / 60.f) noexcept;
	/** @brief Update the TimeManager state for the current frame.
	 */
	void Tick() noexcept;

	/** @brief Construct a TimeManager instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	TimeManager(const TimeManager& other) = delete;
	/** @brief Implement the operator= operation for TimeManager.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	TimeManager& operator=(const TimeManager& other) = delete;
	/** @brief Construct a TimeManager instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	TimeManager(TimeManager&& other) = delete;
	/** @brief Implement the operator= operation for TimeManager.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	TimeManager& operator=(TimeManager&& other) = delete;

	/** @brief Configured start ticks value used by TimeManager.
	 */
	double startTicks = 0.0;
	/** @brief Configured prev ticks value used by TimeManager.
	 */
	double prevTicks = 0.0;
	/** @brief Configured delta value used by TimeManager.
	 */
	float delta = 0.f;
	/** @brief Stored time scale value used by TimeManager.
	 */
	float timeScale_ = 1.f;
	/** @brief Configured fixed dt value used by TimeManager.
	 */
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
