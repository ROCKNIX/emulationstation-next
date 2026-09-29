#pragma once
#ifndef ES_CORE_UTILS_TIME_UTIL_H
#define ES_CORE_UTILS_TIME_UTIL_H

#include <string>
#include <time.h>

namespace Utils
{
	namespace Time
	{
		static int NOT_A_DATE_TIME = 0;

		class DateTime
		{
		public:
			 static DateTime now();

			 DateTime();
			 DateTime(const time_t& _time);
			 DateTime(const tm& _timeStruct);
			 DateTime(const std::string& _isoString);
			~DateTime();

			const bool operator<           (const DateTime& _other) const { return (mTime <  _other.mTime); }
			const bool operator<=          (const DateTime& _other) const { return (mTime <= _other.mTime); }
			const bool operator>           (const DateTime& _other) const { return (mTime >  _other.mTime); }
			const bool operator>=          (const DateTime& _other) const { return (mTime >= _other.mTime); }
			           operator time_t     ()                       const { return mTime; }
			           operator tm         ()                       const { return mTimeStruct; }
			           operator std::string()                       const { return mIsoString; }

			void               setTime      (const time_t& _time);
			const time_t&      getTime      () const { return mTime; }
			void               setTimeStruct(const tm& _timeStruct);
			const tm&          getTimeStruct() const { return mTimeStruct; }
			void               setIsoString (const std::string& _isoString);
			const std::string& getIsoString () const { return mIsoString; }
			std::string		   toLocalTimeString();
			// When this was, as a save state tile says it (fork #195, D-UI-089):
			// "TODAY at 17:07", "YESTERDAY at 14:03", "09/24/26 at 14:03" -- the
			// caller passes the three translated words; the date is the
			// locale's with a two-digit year; the time follows the 12-hour
			// switch (D-UI-058).
			std::string		   toRelativeLocalTimeString(const std::string& todayWord, const std::string& yesterdayWord, const std::string& atWord);
			// The locale's date of this stamp with a two-digit year, and the
			// time half alone, for callers that compose their own line.
			std::string		   toShortLocalDateString();
			static std::string localClockText(const tm& clockTstruct);

			double			   elapsedSecondsSince(const DateTime& _since);

			bool				isValid() { return mTime != 0; }

		private:

			time_t      mTime;
			tm          mTimeStruct;
			std::string mIsoString;

		}; // DateTime

		class Duration
		{
		public:

			 Duration(const time_t& _time);
			~Duration();

			unsigned int getDays   () const { return mDays; }
			unsigned int getHours  () const { return mHours; }
			unsigned int getMinutes() const { return mMinutes; }
			unsigned int getSeconds() const { return mSeconds; }

		private:

			unsigned int mTotalSeconds;
			unsigned int mDays;
			unsigned int mHours;
			unsigned int mMinutes;
			unsigned int mSeconds;

		}; // Duration

		time_t      now         ();
		time_t      stringToTime(const std::string& _string, const std::string& _format = "%Y%m%dT%H%M%S");
		std::string timeToString(const time_t& _time, const std::string& _format = "%Y%m%dT%H%M%S");
		int         daysInMonth (const int _year, const int _month);
		int         daysInYear  (const int _year);
		std::string secondsToString(const long seconds, bool asTime = false);

		std::string getSystemDateFormat(bool includeHours = false);
		std::string getElapsedSinceString(const time_t& _time);

	} // Time::

} // Utils::

#endif // ES_CORE_UTILS_TIME_UTIL_H
