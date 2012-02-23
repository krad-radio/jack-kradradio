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

#include "JackDummyDriver.h"
#include "JackEngineControl.h"
#include "JackGraphManager.h"
#include "JackDriverLoader.h"
#include "JackThreadedDriver.h"
#include "JackCompilerDeps.h"
#include <iostream>
#include <unistd.h>
#include <math.h>

static inline unsigned long long ts_to_nsec(struct timespec ts)
{
    return (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
}

static inline struct timespec nsec_to_ts(unsigned long long nsecs)
{
    struct timespec ts;
    ts.tv_sec = nsecs / (1000000000LL);
    ts.tv_nsec = nsecs % (1000000000LL);
    return ts;
}

static inline struct timespec add_ts(struct timespec ts, unsigned long long add_nsecs)
{
    unsigned long long nsecs = ts_to_nsec(ts);
    nsecs += add_nsecs;
    return nsec_to_ts(nsecs);
}

static inline int cmp_lt_ts(struct timespec ts1, struct timespec ts2)
{
    if(ts1.tv_sec < ts2.tv_sec) {
        return 1;
    } else if (ts1.tv_sec == ts2.tv_sec && ts1.tv_nsec < ts2.tv_nsec) {
        return 1;
    } else return 0;
}

static void KradSleep ( krad_driver_t *kraddriver  )
{


}


namespace Jack
{

int JackDummyDriver::Open(jack_nframes_t buffer_size,
                          jack_nframes_t samplerate,
                          bool capturing,
                          bool playing,
                          int inchannels,
                          int outchannels,
                          bool monitor,
                          const char* capture_driver_name,
                          const char* playback_driver_name,
                          jack_nframes_t capture_latency,
                          jack_nframes_t playback_latency)
{


	kraddriver = &kraddriver_a;
	kraddriver->total_nanosecs = 0;
	kraddriver->started = 0;
	kraddriver->total_periods = 0;
	kraddriver->total_frames = 0;
	kraddriver->frame_counter = 0;
    kraddriver->remaining_period_nanosecs = 0;
	kraddriver->next_wakeup.tv_sec = 0;
	kraddriver->frames_per_period = buffer_size;
	kraddriver->sample_rate = samplerate;
	
	kraddriver->frames_per_period_f = buffer_size;
	kraddriver->sample_rate_f = samplerate;
	
    kraddriver->period_nanosecs = floor ((kraddriver->frames_per_period_f / kraddriver->sample_rate_f) * 1000000000.0f);
	

	jack_error("Krad Sez period nanosecs: %lu frames per period: %d sample rate: %d", kraddriver->period_nanosecs, kraddriver->frames_per_period, kraddriver->sample_rate);




    if (JackAudioDriver::Open(buffer_size,
                            samplerate,
                            capturing,
                            playing,
                            inchannels,
                            outchannels,
                            monitor,
                            capture_driver_name,
                            playback_driver_name,
                            capture_latency,
                            playback_latency) == 0) {
        fEngineControl->fPeriod = 0;
        fEngineControl->fComputation = 500 * 1000;
        fEngineControl->fConstraint = 500 * 1000;
        //int buffer_size = int((fWaitTime * fEngineControl->fSampleRate) / 1000000.0f);
        if (buffer_size > BUFFER_SIZE_MAX) {
            buffer_size = BUFFER_SIZE_MAX;
            jack_error("Buffer size set to %d ", BUFFER_SIZE_MAX);
        }
        SetBufferSize(buffer_size);
        return 0;
    } else {
        return -1;
    }
}

int JackDummyDriver::Process()
{
    JackDriver::CycleTakeBeginTime();
    
    if (kraddriver->started == 0) {
		clock_gettime(CLOCK_MONOTONIC, &kraddriver->first_start_time);
		//clock_gettime(CLOCK_MONOTONIC, &kraddriver->cycle_start_time);
		memcpy(&kraddriver->cycle_start_time, &kraddriver->first_start_time, sizeof(struct timespec));
		kraddriver->started = 1;
    } else {
		clock_gettime(CLOCK_MONOTONIC, &kraddriver->cycle_start_time);
    }
    
    kraddriver->total_periods++;
    
    JackAudioDriver::Process();
    
	clock_gettime(CLOCK_MONOTONIC, &kraddriver->cycle_finish_time);
    
	kraddriver->cycle_nanosecs = ts_to_nsec(kraddriver->cycle_finish_time) - ts_to_nsec (kraddriver->cycle_start_time);
    
	kraddriver->remaining_period_nanosecs = kraddriver->period_nanosecs - kraddriver->cycle_nanosecs;
	kraddriver->total_nanosecs = ts_to_nsec(kraddriver->cycle_finish_time) - ts_to_nsec (kraddriver->first_start_time);
	kraddriver->expected_nanosecs = (kraddriver->period_nanosecs * kraddriver->total_periods) - kraddriver->remaining_period_nanosecs;
	
	kraddriver->nanosecs_off = kraddriver->expected_nanosecs - kraddriver->total_nanosecs;
	
	kraddriver->frame_counter += kraddriver->frames_per_period;
	kraddriver->total_frames += kraddriver->frames_per_period;
	if (kraddriver->frame_counter > kraddriver->sample_rate * 240) {
		jack_error("Krad Sez remaining period nanosecs: %lu Nanosecs off: %lld\n", kraddriver->remaining_period_nanosecs, kraddriver->nanosecs_off);

		kraddriver->expected_frames = (kraddriver->total_nanosecs / kraddriver->period_nanosecs) * kraddriver->frames_per_period;
		kraddriver->frames_off = kraddriver->total_frames - kraddriver->expected_frames;
		jack_error("Expected Total Frames: %d Actual Total Frames: %d Difference: %d Total Nanoseconds: %lu\n", kraddriver->expected_frames, kraddriver->total_frames, kraddriver->frames_off, kraddriver->total_nanosecs);
		kraddriver->frame_counter = 0;
	}
	
	kraddriver->next_wakeup = add_ts(kraddriver->cycle_finish_time, kraddriver->remaining_period_nanosecs + kraddriver->nanosecs_off);

	if(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &kraddriver->next_wakeup, NULL)) {
		jack_error("error while sleeping");
	}
    
    return 0;
}

int JackDummyDriver::SetBufferSize(jack_nframes_t buffer_size)
{
    JackAudioDriver::SetBufferSize(buffer_size);
    return 0;
}

} // end of namespace

#ifdef __cplusplus
extern "C"
{
#endif

    SERVER_EXPORT jack_driver_desc_t * driver_get_descriptor () {
        jack_driver_desc_t * desc;
        unsigned int i;

        desc = (jack_driver_desc_t*)calloc (1, sizeof (jack_driver_desc_t));
        strcpy(desc->name, "dummy");                  // size MUST be less then JACK_DRIVER_NAME_MAX + 1
        strcpy(desc->desc, "Timer based backend");    // size MUST be less then JACK_DRIVER_PARAM_DESC + 1

        desc->nparams = 6;
        desc->params = (jack_driver_param_desc_t*)calloc (desc->nparams, sizeof (jack_driver_param_desc_t));

        i = 0;
        strcpy(desc->params[i].name, "capture");
        desc->params[i].character = 'C';
        desc->params[i].type = JackDriverParamUInt;
        desc->params[i].value.ui = 2U;
        strcpy(desc->params[i].short_desc, "Number of capture ports");
        strcpy(desc->params[i].long_desc, desc->params[i].short_desc);

        i++;
        strcpy(desc->params[i].name, "playback");
        desc->params[i].character = 'P';
        desc->params[i].type = JackDriverParamUInt;
        desc->params[1].value.ui = 2U;
        strcpy(desc->params[i].short_desc, "Number of playback ports");
        strcpy(desc->params[i].long_desc, desc->params[i].short_desc);

        i++;
        strcpy(desc->params[i].name, "rate");
        desc->params[i].character = 'r';
        desc->params[i].type = JackDriverParamUInt;
        desc->params[i].value.ui = 48000U;
        strcpy(desc->params[i].short_desc, "Sample rate");
        strcpy(desc->params[i].long_desc, desc->params[i].short_desc);

        i++;
        strcpy(desc->params[i].name, "monitor");
        desc->params[i].character = 'm';
        desc->params[i].type = JackDriverParamBool;
        desc->params[i].value.i = 0;
        strcpy(desc->params[i].short_desc, "Provide monitor ports for the output");
        strcpy(desc->params[i].long_desc, desc->params[i].short_desc);

        i++;
        strcpy(desc->params[i].name, "period");
        desc->params[i].character = 'p';
        desc->params[i].type = JackDriverParamUInt;
        desc->params[i].value.ui = 1024U;
        strcpy(desc->params[i].short_desc, "Frames per period");
        strcpy(desc->params[i].long_desc, desc->params[i].short_desc);

        i++;
        strcpy(desc->params[i].name, "wait");
        desc->params[i].character = 'w';
        desc->params[i].type = JackDriverParamUInt;
        desc->params[i].value.ui = 21333U;
        strcpy(desc->params[i].short_desc,
               "Number of usecs to wait between engine processes");
        strcpy(desc->params[i].long_desc, desc->params[i].short_desc);

        return desc;
    }

    SERVER_EXPORT Jack::JackDriverClientInterface* driver_initialize(Jack::JackLockedEngine* engine, Jack::JackSynchro* table, const JSList* params) {
        jack_nframes_t sample_rate = 48000;
        jack_nframes_t period_size = 1024;
        unsigned int capture_ports = 2;
        unsigned int playback_ports = 2;
        unsigned long wait_time = 0;
        const JSList * node;
        const jack_driver_param_t * param;
        bool monitor = false;

        for (node = params; node; node = jack_slist_next (node)) {
            param = (const jack_driver_param_t *) node->data;

            switch (param->character) {

                case 'C':
                    capture_ports = param->value.ui;
                    break;

                case 'P':
                    playback_ports = param->value.ui;
                    break;

                case 'r':
                    sample_rate = param->value.ui;
                    break;

                case 'p':
                    period_size = param->value.ui;
                    break;

                case 'w':
                    wait_time = param->value.ui;
                    break;

                case 'm':
                    monitor = param->value.i;
                    break;
            }
        }

        if (wait_time == 0) // Not set
            wait_time = (unsigned long)((((float)period_size) / ((float)sample_rate)) * 1000000.0f);

        Jack::JackDriverClientInterface* driver = new Jack::JackThreadedDriver(new Jack::JackDummyDriver("system", "dummy_pcm", engine, table, wait_time));
        if (driver->Open(period_size, sample_rate, 1, 1, capture_ports, playback_ports, monitor, "dummy", "dummy", 0, 0) == 0) {
            return driver;
        } else {
            delete driver;
            return NULL;
        }
    }

#ifdef __cplusplus
}
#endif
