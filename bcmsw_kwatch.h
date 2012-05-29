/*
 * bcmsw_kwatch.h
 *
 *  Created on: May 29, 2012
 *      Author: jhkim
 */

#ifndef BCMSW_KWATCH_H_
#define BCMSW_KWATCH_H_

typedef void* kwatch;

/*
 * create new kernel stop watch
 * handle = start("name");
 * start("10km");
 */
kwatch kwatch_start(const char* name);

/*
 * return current time and watch is going on
 * int lap(handle);
 */
int kwatch_lap_sec(kwatch handle);
int kwatch_lap_usec(kwatch handle);

/*
 * clear current time
 * reset(handle);
 */
void kwatch_reset(kwatch handle);

/*
 * return current time and kwatch will be stopped
 * if you want to re-start , please call start("name")
 * int stop(handle);
 */
int kwatch_stop(kwatch handle);


#endif /* BCMSW_KWATCH_H_ */
