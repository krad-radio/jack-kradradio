/*
Copyright (C) 2001 Paul Davis
Copyright (C) 2004-2008 Grame

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __JackDummyDriver__
#define __JackDummyDriver__

#include "JackAudioDriver.h"


typedef struct _krad_driver krad_driver_t;

struct _krad_driver
{
	int total_periods;
	int frames_off;
    int  sample_rate;
    int  frames_per_period;
    float  sample_rate_f;
    float  frames_per_period_f;
    unsigned long long remaining_period_nanosecs;
    unsigned long long cycle_nanosecs;
    unsigned long long period_nanosecs;
    unsigned long long total_nanosecs;
	signed long long nanosecs_off;
	unsigned long long expected_nanosecs;
    int frame_counter;
    int total_frames;
    int expected_frames;

	struct timespec first_start_time;
	int started;
    struct timespec cycle_start_time;
    struct timespec cycle_finish_time;
    struct timespec next_wakeup;
};

namespace Jack
{

/*!
\brief The dummy driver.
*/

class JackDummyDriver : public JackAudioDriver
{
    private:

        unsigned long fWaitTime;
		krad_driver_t kraddriver_a;
		krad_driver_t *kraddriver;
    public:

        JackDummyDriver(const char* name, const char* alias, JackLockedEngine* engine, JackSynchro* table, unsigned long wait_time)
                : JackAudioDriver(name, alias, engine, table), fWaitTime(wait_time)
        {}
        virtual ~JackDummyDriver()
        {}

        int Open(jack_nframes_t buffersize,
                 jack_nframes_t samplerate,
                 bool capturing,
                 bool playing,
                 int chan_in,
                 int chan_out,
                 bool monitor,
                 const char* capture_driver_name,
                 const char* playback_driver_name,
                 jack_nframes_t capture_latency,
                 jack_nframes_t playback_latency);

        int Process();
        
        // BufferSize can be changed
        bool IsFixedBufferSize()
        {
            return false;
        }

        int SetBufferSize(jack_nframes_t buffer_size);

};

} // end of namespace

#endif
