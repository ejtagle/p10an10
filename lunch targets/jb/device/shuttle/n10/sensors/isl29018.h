/*
 * MOCK to emulate proximity and ilumination sensors to be able to use 
 * the Closed source ventana MPU3050 Sensor HAL in our hardware
 */

#include <stdint.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <math.h>
#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <linux/socket.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/socket.h>

#define  LOG_TAG "Isl29018Mock"

#include <cutils/log.h>
#include <cutils/sockets.h>
#include <cutils/properties.h>

/*****************************************************************************/

struct sensors_event_t;

class SensorBase {
protected:
	char _dummy[4*4 + PATH_MAX + 16]; // Allocated space for the base class

public:
	SensorBase(const char* , const char* );
    virtual ~SensorBase() = 0;

    virtual int readEvents(sensors_event_t* data, int count) = 0;
    virtual bool hasPendingEvents() const = 0;
    virtual int getFd() const = 0;
    virtual int setDelay(int32_t handle, int64_t ns) = 0;
    virtual int enable(int32_t handle, int enabled) = 0;
};

class Isl29018Light : public SensorBase {

	// We need something that we can poll() on without disturbing other things...
	// [0] = Write end, [1] = Read end. Suitable to wait for
	int control[2];	
	
public:
	Isl29018Light(const char* dev_name, int idx);
    virtual ~Isl29018Light();
	virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int getFd() const;
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);
};

class Isl29018Prox : public SensorBase {

	// We need something that we can poll() on without disturbing other things...
	// [0] = Write end, [1] = Read end. Suitable to wait for
	int control[2];	
	
public:
	Isl29018Prox(const char* dev_name, int idx);
    virtual ~Isl29018Prox();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int getFd() const;
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);
};

