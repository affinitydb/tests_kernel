/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _TIMEDEVENT_H
#define _TIMEDEVENT_H

class TimedEvent
{
	/*
	Helper function for recording how long one or more events take.
	
	Info about timing is logged into a provided output stream and also tracked
	for summary and analysis.

	A single TimedEvent object can track average time etc for 
	more than one different event.  For example, e.g. "time to open", independently of
	"time to close". This is done based on the string passed to "start".

	This object is not thread safe.  Use one TimedEvent for each thread and
	combine the results afterwards.
	*/
public:
	typedef std::map<std::string , std::vector<long> > TEventStats;

	TimedEvent(std::ostream & out):mOut(out),mStart(0),mDuration(-1)
	{
	}

	void start( const char * desc, const char * incategory = NULL )
	{
		// Begin timing of an event

		// By default the desc is used to track all related events
		// However if the description is not consistent for each
		// similar even you can pass "incategory"
		// For example you could call
		// start( "open store A", "open" );  ... end();
		// start( "open store B", "open" );  ... end();
		// and all the individual opening times would be recorded together
		// as "open" timings.

		mStart = getTimeInMs();	
		mDuration = -1;
		mOut << "Beginning " << desc << endl;
		mEventCategory = incategory==NULL?desc:incategory;
	}

	void end(off64_t bytes=0 /*optional estimate of "work done" during period*/)
	{
		// Signal end of the event
		mDuration=getTimeInMs() - mStart;
		if (mDuration == 0) mDuration=1;  //Some events can be too fast to measure!
		double bytesPerMS = double(bytes)/double(mDuration) ;
		double bytesPerS = 1000. * bytesPerMS;
		double MBPerSecond = bytesPerS/(1024.*1024.);

		mOut << "\tComplete " 
			<< mDuration << " milliseconds.";

		if (mDuration>1 && bytes>0)
			mOut << "Throughput " << MBPerSecond << " MB/s" ;

		mOut << endl;

		// Remember
		recordTiming(mEventCategory,mDuration);
	}
	
	void reset()
	{
		mStart=0;
		mEventHistory.clear();
	}

	void recordTiming(std::string& inEventCategory,long duration)
	{
		if (duration < 1) 
		{
			mOut << "ERROR: Invalid timing" << duration << endl;
			return ;
		}

		TEventStats::iterator it = mEventHistory.find( inEventCategory ) ;
		if ( it == mEventHistory.end() )
		{
			std::vector<long> firstTime;
			firstTime.push_back(duration);

			mEventHistory.insert(TEventStats::value_type(inEventCategory,firstTime));
		}
		else
		{
			(*it).second.push_back(duration);
		}	
	}

	void historyReport(bool listAll = true)
	{
		// Print summary of all the recorded events,
		// grouped by the event type

		// Title
		mOut << "Timing Summary in milliseconds"<<endl
			 <<"Description,Avg,Ttl,Cnt" ;
		if ( listAll ) mOut << ",individual timings..." ;
		mOut << endl;

		// List all events in a comma separates value format (for easy import to excel)
		for (TEventStats::iterator it = mEventHistory.begin() ;
			it != mEventHistory.end() ;
			it++ )
		{
			std::vector<long> & times=(*it).second;
			
			long ttl = 0 ; int i ;
			for ( i = 0 ; i < (long)times.size() ; i++ )
				ttl+=times[i];

			mOut << (*it).first << "," 
				<< ttl/(long)times.size() << "," 
				<< ttl << "," 
				<< (long)times.size();

			if ( listAll )
			{
				// For further analysis, would be too much if too many samples taken
				for ( i = 0 ; i < (long)times.size() ; i++ )
					mOut << "," << times[i] ;
			}
			mOut << endl;
		}
	}

	std::ostream & mOut; // Traced output goes here

	long mStart;	// When start() was called
	long mDuration; // Time for most recent timed event (start - end)

	std::string mEventCategory ; // Description of what was being sampled
	TEventStats mEventHistory;	 // History of all events
} ;

#endif
