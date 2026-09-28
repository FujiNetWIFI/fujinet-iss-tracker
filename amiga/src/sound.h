/**
 * @brief   ISS Tracker
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Paula sound effects through audio.device
 */

#ifndef SOUND_H
#define SOUND_H

/* Allocate one audio channel and build the samples. Returns 0 if there is
 * no free channel or memory; every other call is then a no-op. */
int sound_open(void);
void sound_close(void);

/* Turn sound effects on or off (on by default). */
void sound_enable(int on);

/* Short sonar ping: a new position fix arrived. */
void sound_ping(void);

/* Three pings: the ISS has come over the home horizon. */
void sound_alert(void);

/* NASA "Quindar" radio tone: 2525 Hz to start (astronaut goes out),
 * 2475 Hz to end. */
void sound_quindar(int start);

/* Spooky theremin warble while a UFO is about. On starts it when the
 * channel is idle, so calling it every frame resumes it after another
 * effect; off stops it (and nothing else). */
void sound_ufo(int on);

#endif /* SOUND_H */
