/*
 * MOCK to emulate proximity and ilumination sensors to be able to use 
 * the Closed source ventana MPU3050 Sensor HAL in our hardware
 */

#include "isl29018.h"

/*****************************************************************************/

Isl29018Light::Isl29018Light(const char* dev_name, int idx) : SensorBase("",NULL) 
{
	// Create a socket pair we can wait on...
	socketpair( AF_LOCAL, SOCK_STREAM, 0, control );
}

Isl29018Light::~Isl29018Light()
{
	close( control[0] );
	close( control[1] );
}
	
int Isl29018Light::readEvents(sensors_event_t* data, int count) 
{ 
	return 0; 
}

bool Isl29018Light::hasPendingEvents() const 
{ 
	return false; 
}

int Isl29018Light::getFd() const 
{ 
	return control[1]; 
}

int Isl29018Light::setDelay(int32_t handle, int64_t ns) 
{ 
	return 0; 
}

int Isl29018Light::enable(int32_t handle, int enabled) 
{ 
	return 0; 
}

Isl29018Prox::Isl29018Prox(const char* dev_name, int idx) : SensorBase("",NULL) 
{
	// Create a socket pair we can wait on...
	socketpair( AF_LOCAL, SOCK_STREAM, 0, control );
}

Isl29018Prox::~Isl29018Prox() 
{
	close( control[0] );
	close( control[1] );
}

int Isl29018Prox::readEvents(sensors_event_t* data, int count) 
{ 
	return 0; 
}

bool Isl29018Prox::hasPendingEvents() const 
{ 
	return false; 
}

int Isl29018Prox::getFd() const 
{ 
	return control[1]; 
}

int Isl29018Prox::setDelay(int32_t handle, int64_t ns) 
{ 
	return 0; 
}

int Isl29018Prox::enable(int32_t handle, int enabled) 
{ 
	return 0; 
}
