/*
 * vrintf - A vr interface
 *
 * Implementation
 * 2015	by takumadx @ ohmydigifab <hirayama.takuma@ohmydigifab.com>
 *
 * License:
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License as
 *		published by the Free Software Foundation;
 *		strictly version 2 only.
 *
 *		This program is distributed in the hope that it will be useful,
 *		but WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *		GNU General Public License for more details.
 *
 *		You should have received a copy of the GNU General Public
 *		License	along with this program; if not, write to the
 *		Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *		Boston, MA  02110-1301  USA
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <linux/input.h>

//macro difinition
#define SEND_EVENT(_fd, _type, _code, _value) do{\
	struct input_event eve;\
	eve.type = _type;\
	eve.code = _code;\
	eve.value = _value;\
	int n = write(_fd, &eve, sizeof(struct input_event));\
	if (n < 0) {\
		fprintf(stderr,"write");\
		exit(-1);\
	}\
}while(0)

//pre procedure difinition
void showhelp(void);
void parse_args(int argc, char ** argv);

//enum difinition
typedef enum _OutputType {
	RawString, LinuxMouseInput,
} OutputType;

//structure difinition
typedef struct _Frame {
	struct timeval time;
	unsigned char *frame_buffer;
	int width;
	int stride;
	int height;
	int frame_num;
	int flag;
} Frame;

#define TOGGLE_START_BIT 1
#define TOGGLE_END_BIT 2
typedef struct _Toggle {
	struct timeval time;
	float duration;
	int on;
	float x;
	float y;
	int flag;
} Toggle;

typedef struct _Detection {
	struct timeval time;
	float x;
	float y;
	float z;
	int c;
	int with_c;
} Detection;

//global variables
static int running = 1;
static const char *event_out_fifoname = NULL;
static const char *event_in_fifoname = NULL;
static int frame_w = 480;
static int frame_h = 240;
static int frame_rate = 30;
static int shuter_duration = 10000;
static int screen_w = 1920;
static int screen_h = 1080;
static float last_x = 0;
static float last_y = 0;
static int last_c = 0;
static int repeat_num = 0;
static int now_on = 0;
static OutputType output_type = RawString;
static int decimal_place = 0;

#define DETECTION_BUFF_LENGTH 1024
static unsigned int buff_cur = 0;
static Detection detection_buffer[DETECTION_BUFF_LENGTH];
static unsigned int receive_buff_cur = 0;
static Detection receive_detection_buffer[DETECTION_BUFF_LENGTH];

#define TOGGLE_BUFF_LENGTH 1024
static unsigned int toggle_buff_cur = 0;
static Toggle toggle_buffer[TOGGLE_BUFF_LENGTH];

#define FRAME_BUFF_LENGTH 16
static unsigned int frame_buff_cur = 0;
static Frame frame_buffer[FRAME_BUFF_LENGTH];

static uint32_t frames = 0;
static uint32_t skip_frames = 0;
static uint32_t frame_size;
static struct timeval last_detected;
static struct timezone tzone;

static int last_screen_x = 0;
static int last_screen_y = 0;

void send_detection_as_linux_mouse_input(int fd, Detection *d) {
	int i;
	{
		int dx = d->x - last_screen_x;
		int dy = d->y - last_screen_y;
		if (d->x < 0) {
			dx -= abs(d->x) / 10;
		} else if (d->x > screen_w) {
			dx += abs(d->x - screen_w) / 10;
		}
		if (d->y < 0) {
			dy -= abs(d->y) / 10;
		} else if (d->y > screen_h) {
			dy += abs(d->y - screen_h) / 10;
		}
		if (dx != 0)
			SEND_EVENT(fd, EV_REL, ABS_X, dx);
		if (dy != 0)
			SEND_EVENT(fd, EV_REL, ABS_Y, dy);
		last_screen_x = d->x;
		last_screen_y = d->y;
	}
	if (d->with_c) {
		int c = d->c;
		if (d->c == 2) {
			c = last_c;
			repeat_num++;
		} else {
			last_c = d->c;
			repeat_num = 0;
		}
		switch (c) {
		case 1:
//					SEND_EVENT(fd, EV_KEY, KEY_C, 1);
//					SEND_EVENT(fd, EV_KEY, KEY_C, 0);
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 1);
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 0);
			break;
		case 2:
			//repeat
			break;
		case 3:
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 1);
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 0);
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 1);
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 0);
			break;
		case 4:
			for (i = 0; i < fmin(5, repeat_num + 1); i++)
				SEND_EVENT(fd, EV_REL, ABS_Z, -1);
			break;
		case 5:
			for (i = 0; i < fmin(5, repeat_num + 1); i++)
				SEND_EVENT(fd, EV_REL, ABS_Z, 1);
			break;
		case 6:
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 1);
			break;
		case 7:
			SEND_EVENT(fd, EV_KEY, BTN_LEFT, 0);
			break;
		case 8:
			SEND_EVENT(fd, EV_KEY, BTN_RIGHT, 1);
			SEND_EVENT(fd, EV_KEY, BTN_RIGHT, 0);
			break;
		}
	}
}
void send_detection_as_raw_string(int fd, Detection *d) {
	char buf[512];
	int count;
	int dp = decimal_place;
	if (d->with_c) {
		count = sprintf(buf, "x=%.*f,y=%.*f,z=%.*f,c=%d\n", dp, d->x, dp, d->y,
				dp, d->z, d->c);
	} else {
		count = sprintf(buf, "x=%.*f,y=%.*f,z=%.*f\n", dp, d->x, dp, d->y, dp,
				d->z);
	}
	//fprintf(stderr,buf);
	write(fd, buf, count);
}

/*
 * receive ui event to fifo
 */
void *receive_from_fifo(void *args) {
	int i;
	char buf[512];
	int fd = (int) args;
	while (1) {
		for (i = 0; i < sizeof(buf) - 1;) {
			char *cur = buf + i;
			int n = read(fd, cur, 1);
			if (n <= 0) {
				usleep(100);
				continue;
			}
			if (*cur == '\0' || *cur == '\r') {
				//skip
				continue;
			} else {
				i++;
			}
			if (*cur == '\n') {
				break;
			}
		}
		buf[i] = '\0';
		//fprintf(stderr,buf);
		if (i == sizeof(buf) - 1) {
			fprintf(stderr, "input event is too long");
			exit(1);
		}
		Detection *d = &receive_detection_buffer[receive_buff_cur
				% DETECTION_BUFF_LENGTH];
		memset(d, 0, sizeof(Detection));
		int num = sscanf(buf, "x=%f,y=%f,z=%f,c=%d", &d->x, &d->y, &d->z,
				&d->c);
		if (num == 3) {
			d->with_c = 0;
		} else if (num == 4) {
			d->with_c = 1;
		} else {
			//skip
			continue;
		}
		gettimeofday(&d->time, &tzone);
		receive_buff_cur++;
	}
	return 0;
}

/*
 * send ui event to fifo
 */
void *send_to_fifo(void *args) {
	int receive_cur = 0;
	int send_cur = 0;
	int fd = (int) args;
	Detection *rd = NULL;

	while (1) {
		if (send_cur >= buff_cur) {
			usleep(1000);
			continue;
		}
		if (buff_cur - send_cur > DETECTION_BUFF_LENGTH / 2) {
			send_cur = buff_cur - DETECTION_BUFF_LENGTH / 2;
		}
		while (send_cur < buff_cur) {
			Detection *d = &detection_buffer[send_cur % DETECTION_BUFF_LENGTH];
			Detection *d_pre = &detection_buffer[(send_cur - 1)
					% DETECTION_BUFF_LENGTH];
			send_cur++;

			if (d->with_c == 0 && d->x == d_pre->x && d->y == d_pre->y) {
				continue;
			}
			while (receive_cur < receive_buff_cur) {
				Detection *rd_next = &receive_detection_buffer[receive_cur
						% DETECTION_BUFF_LENGTH];
				if (rd == NULL) {
					rd = rd_next;
					receive_cur++;
					continue;
				}
				double t1 = (double) (d->time.tv_sec - rd->time.tv_sec)
						+ (double) (d->time.tv_usec - rd->time.tv_usec)
								/ 1000000.0;
				double t2 = (double) (d->time.tv_sec - rd_next->time.tv_sec)
						+ (double) (d->time.tv_usec - rd_next->time.tv_usec)
								/ 1000000.0;
				if (t2 > t1) {
					break;
				}
				rd = rd_next;
				receive_cur++;
			}
			if (rd != NULL) {
				double t = (double) (d->time.tv_sec - rd->time.tv_sec)
						+ (double) (d->time.tv_usec - rd->time.tv_usec)
								/ 1000000.0;
				if (t > 5) {
					rd = NULL;
				}

			}
			if (rd != NULL) { //z
				double F = 1080; //distance between virtual screen and lens focal
				double W = 0.25 * 1000; //250mm distance between cam1 and cam2
				int x1 = d->x - screen_w / 2;
				int x2 = rd->x - screen_w / 2;
				if (x1 == x2) {
					//infinite far away
				} else if (x1 == 0) {
					double tan2 = F / x2;
					d->z = W * tan2;
				} else if (x2 == 0) {
					double tan1 = F / -x1;
					d->z = W * tan1;
				} else {
					double tan1 = F / -x1;
					double tan2 = F / x2;
					double m = tan1 * tan2;
					double p = tan1 + tan2;
					if (p != 0) { //fail safe
						d->z = W * m / p;
					}
				}
				if (d->z != 0) {
					int y1 = d->y - screen_h / 2;
					d->x = x1 * d->z / F + W / 2;
					d->y = -y1 * d->z / F;
				}
			}
			switch (output_type) {
			case LinuxMouseInput:
				send_detection_as_linux_mouse_input(fd, d);
				break;
			case RawString:
			default:
				send_detection_as_raw_string(fd, d);
				break;
			}
		}
	}

	{ //shutdown
		SEND_EVENT(fd, EV_KEY, KEY_PAUSE, 0);
	}

	return 0;
}

/*
 *      open_fifo(filename) - creates (if necessary) and opens fifo
 *      instead of event devices. If filename exists and is NOT a fifo,
 *      abort with error.
 */
int open_fifo(const char *filename) {
	int fd = -1;
	struct stat ss;
	if ( NULL == filename)
		return 0;
	if (0 == stat(filename, &ss)) {
		if (!S_ISFIFO(ss.st_mode)) {
			fprintf(stderr, "File [%s] exists, but is not a fifo.\n", filename);
			return 0;
		}
	} else {
		if (0 != mkfifo(filename, S_IRUSR | S_IWUSR)) { // default permissions for created fifo is rw------- (user=rw)
			fprintf(stderr, "Failed to create new fifo [%s]\n", filename);
			return 0;
		}
	}
	fd = open(filename, O_RDWR | O_NONBLOCK);
	if (0 >= fd) {
		fprintf(stderr, "Failed to open fifo [%s] for writing.\n", filename);
		return 0;
	}
	return fd;
}

#define AVERAGE_FACTOR 64
float elapse_ave = 0.1;
float elapse_ave2 = 0.15 * 0.15;
void image_process(unsigned char *imageData, int width, int widthStep,
		int height, int nChannels, struct timeval now, int frame) {

	float max = 0;
	float max_x = 0;
	float max_y = 0;
	float x, y;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int offset = widthStep * y + nChannels * x;
			int v = (imageData + offset)[0];
			if (v > max) {
				max = v;
				max_x = x;
				max_y = y;
			}
		}
	}

	//update duration
	Toggle *tg = &toggle_buffer[toggle_buff_cur % TOGGLE_BUFF_LENGTH];
	tg->duration = (double) (now.tv_sec - tg->time.tv_sec)
			+ (double) (now.tv_usec - tg->time.tv_usec) / 1000000.0;
	if (last_x < 0 || last_x > screen_w || last_y < 0 || last_y > screen_h) {
		tg->flag = 1;
	}

	int toggled = 0;
	int on = (max > 128);
	if (on != now_on) {
		Toggle *tg_next = &toggle_buffer[(toggle_buff_cur + 1)
				% TOGGLE_BUFF_LENGTH];
		tg_next->on = on;
		tg_next->time = now;
		tg_next->x = last_x;
		tg_next->y = last_y;
		tg_next->flag = 0;
		tg_next->duration = 0;

		toggle_buff_cur++;

		now_on = on;
		toggled = 1;

		fprintf(stderr, "id=%d; on: %d; duration=%f\n", toggle_buff_cur, tg->on,
				tg->duration);
		tg = tg_next;
	}
	{
		Toggle *tg_pre = &toggle_buffer[(toggle_buff_cur - 1)
				% TOGGLE_BUFF_LENGTH];
		if (tg_pre->flag == 0 && tg_pre->on == 0 && tg_pre->duration < 1.0
				&& tg_pre->duration > 0.030
				&& tg->duration > tg_pre->duration * 2) {
			int c = 0;
			int i;
			for (i = toggle_buff_cur - 1; i >= 0; i--) {
				Toggle *tg_i = &toggle_buffer[i % TOGGLE_BUFF_LENGTH];
				if (tg_i->flag == 0 && tg_i->duration < 1.0
						&& tg_i->duration > 0.030) {
					tg_i->flag = 1;
					if (tg_i->on == 0) {
						c++;
					}
				} else {
					Toggle *tg_start = &toggle_buffer[(i + 1)
							% TOGGLE_BUFF_LENGTH];
					Detection *d = &detection_buffer[(buff_cur + 1)
							% DETECTION_BUFF_LENGTH];
					memset(d, 0, sizeof(Detection));
					d->time = now;
					d->x = tg_start->x;
					d->y = tg_start->y;
					d->with_c = 1;
					d->c = c;
					buff_cur++;
					fprintf(stderr, "duration: %f; x: %f; y: %f; c: %d\n",
							tg_start->duration, d->x, d->y, d->c);
					break;
				}
			}
		}
	}
	if (now_on) {
		float left = max_x;
		float right = max_x;
		float top = max_y;
		float bottom = max_y;
		int v_pre;
		float thresh = max / 2;
		for (x = max_x, y = max_y; y >= 0; y--) {
			int offset = widthStep * y + nChannels * x;
			int v = (imageData + offset)[0];
			if (v < thresh) {
				top = y + (thresh - v) / (v_pre - v);
				break;
			}
			v_pre = v;
		}
		for (x = max_x, y = max_y; y < height; y++) {
			int offset = widthStep * y + nChannels * x;
			int v = (imageData + offset)[0];
			if (v < thresh) {
				bottom = y - (thresh - v) / (v_pre - v);
				break;
			}
			v_pre = v;
		}
		for (x = max_x, y = max_y; x >= 0; x--) {
			int offset = widthStep * y + nChannels * x;
			int v = (imageData + offset)[0];
			if (v < thresh) {
				left = x + (thresh - v) / (v_pre - v);
				break;
			}
			v_pre = v;
		}
		for (x = max_x, y = max_y; x < width; x++) {
			int offset = widthStep * y + nChannels * x;
			int v = (imageData + offset)[0];
			if (v < thresh) {
				right = x - (thresh - v) / (v_pre - v);
				break;
			}
			v_pre = v;
		}
		max_x = (left + right) / 2;
		max_y = (top + bottom) / 2;
		float screen_x = 0;
		float screen_y = 0;
		float factor = fmaxf(screen_w / width, screen_h / height) * 1.2;
		screen_x = screen_w / 2 - (max_x - width / 2) * factor;
		screen_y = screen_h / 2 + (max_y - height / 2) * factor;
//screen_x = fmin(screen_w, fmax(0, screen_x));
//screen_y = fmin(screen_h, fmax(0, screen_y));
//		fprintf(stderr,"left=%f, right=%f, top=%f, bottom=%f\n", left, right, top,
//				bottom);
		float elapsedTime = (double) (now.tv_sec - last_detected.tv_sec)
				+ (double) (now.tv_usec - last_detected.tv_usec) / 1000000.0;
		last_detected = now;
		float elapse_sigma = sqrt(elapse_ave2 - elapse_ave * elapse_ave);
		if (elapsedTime < 1.0 && elapsedTime < 3 * elapse_sigma) {
			elapse_ave = (elapse_ave * AVERAGE_FACTOR + elapsedTime)
					/ (AVERAGE_FACTOR + 1);
			elapse_ave2 = (elapse_ave2 * AVERAGE_FACTOR
					+ elapsedTime * elapsedTime) / (AVERAGE_FACTOR + 1);
		}

		Detection *d = &detection_buffer[(buff_cur + 1) % DETECTION_BUFF_LENGTH];
		memset(d, 0, sizeof(Detection));
		d->time = now;
		d->x = screen_x;
		d->y = screen_y;
		d->with_c = 0;
		d->c = 0;
		buff_cur++;
//		{
//			float dx = screen_x - last_x;
//			float dy = screen_y - last_y;
//			float d = sqrt(dx * dx + dy * dy);
//			fprintf(stderr, "d=%f: dx=%f, dx=%f\n", d, dx, dy);
//		}
		last_x = screen_x;
		last_y = screen_y;
		if (toggled) {
			Toggle *tg = &toggle_buffer[(toggle_buff_cur + 1)
					% TOGGLE_BUFF_LENGTH];
			tg->x = last_x;
			tg->y = last_y;
		}
	}
	//fprintf(stderr, "max=%f, x=%f, y=%f\n", max, max_x, max_y);
}

extern void detect_face_init();
extern void detect_face_deinit();
extern int detect_face(unsigned char *imageData, int width, int widthStep, int height,
		int nChannels, struct timeval now, int nFrame);
extern void detect_finger_init();
extern void detect_finger_deinit();
extern int detect_finger(unsigned char *imageData, int width, int widthStep,
		int height, int nChannels, struct timeval now, int nFrame);

void *process_poling(void *args) {
	int process_cur = 0;
	while (1) {
		if (process_cur >= frame_buff_cur) {
			usleep(1000);
			continue;
		}
		Frame *frame = &frame_buffer[process_cur % FRAME_BUFF_LENGTH];
//			gettimeofday(&start, &tzone);
		detect_finger(frame->frame_buffer, frame->width, frame->stride,
				frame->height, 1, frame->time, frame->frame_num);
//			gettimeofday(&now, &tzone);
//			elapsedTime = (double) (now.tv_sec - start.tv_sec)
//					+ (double) (now.tv_usec - start.tv_usec) / 1000000.0;
//			fprintf(stderr, "frames : %d; %4.6f\n", frames, elapsedTime);
		frame->flag = 0;
		process_cur++;
	}
}

int main(int argc, char ** argv) {
	parse_args(argc, argv);

	detect_finger_init();

	if (event_out_fifoname != NULL) {
		int fd;
		if (0 == strcmp(event_out_fifoname, "-")) {
			fd = 1; //stdout
		} else {
			fd = open_fifo(event_out_fifoname);
		}
		if (fd < 0) {
			fprintf(stderr, "Invalid fifo.\nPlease refer -h");
			return -1;
		}
		pthread_t pt;
		pthread_create(&pt, NULL, &send_to_fifo, (void*) fd);
	} else {
		fprintf(stderr, "Invalid args.\nPlease refer -h");
		return -1;
	}
	if (event_in_fifoname != NULL) {
		int fd;
		if (0 == strcmp(event_in_fifoname, "-")) {
			fd = 0; //stdin
		} else {
			fd = open_fifo(event_in_fifoname);
		}
		if (fd < 0) {
			fprintf(stderr, "Invalid fifo.\nPlease refer -h");
			return -1;
		}
		pthread_t pt;
		pthread_create(&pt, NULL, &receive_from_fifo, (void*) fd);
	}

	char cam_command[256];
	sprintf(cam_command,
			"/opt/vc/bin/raspividyuv -n -w %d -h %d -fps %d -t 0 -ss %d -ex fixedfps -awb off -ISO 100 -ifx none -o -",
			frame_w, frame_h, frame_rate, shuter_duration);
	FILE *fp = popen(cam_command, "r");

	pthread_t pt;
	pthread_create(&pt, NULL, &process_poling, (void*) 0);

	//Frame size for yuv format
	frame_size = frame_w * frame_h * 3 / 2;

	struct timeval start, now;
	double elapsedTime = 0.0;
	gettimeofday(&start, &tzone);

	int i;
	memset(frame_buffer, 0, sizeof(frame_buffer));
	for (i = 0; i < FRAME_BUFF_LENGTH; i++) {
		Frame *frame_i = &frame_buffer[i];
		frame_i->frame_buffer = (unsigned char*) malloc(frame_size);
		frame_i->width = frame_w;
		frame_i->stride = frame_w;
		frame_i->height = frame_h;
	}

	int cur = 0;
	int block_buff_size = 1024 * 128;
	while (running) {
		Frame *frame = &frame_buffer[frame_buff_cur % FRAME_BUFF_LENGTH];
		int size = fread(frame->frame_buffer + cur, 1,
				fmin(block_buff_size, frame_size - cur), fp);
		if (size == 0) {
			break;
		}
		cur += size;
		if (cur >= frame_size) {
			Frame *frame_next = &frame_buffer[(frame_buff_cur + 1)
					% FRAME_BUFF_LENGTH];
			if (frame_next->flag == 0) {
				gettimeofday(&frame->time, &tzone);
				frame->flag = 1;
				frame->frame_num = frames;
				frame_buff_cur++;
			} else {
				skip_frames++;
			}
			cur = 0;
			frames++;
		}
	}

	gettimeofday(&now, &tzone);
	elapsedTime = (double) (now.tv_sec - start.tv_sec)
			+ (double) (now.tv_usec - start.tv_usec) / 1000000.0;
	fprintf(stderr, "frames: %d; skip_frames: %d;  %4.6f fps\n", frames,
			skip_frames, frames / elapsedTime);

	fprintf(stderr, "ok\n");

	return 0;
}

void parse_args(int argc, char ** argv) {
	int i;
	// Parse command line
	for (i = 1; i < argc; ++i) {
		if ((0 == strcmp(argv[i], "-h")) || (0 == strcmp(argv[i], "-?"))
				|| (0 == strcmp(argv[i], "--help"))) {
			showhelp();
			exit(0);
		} else if (0 == strncmp(argv[i], "-dp", 3)) {
			sscanf(argv[i] + 3, "%d", &decimal_place);
		} else if (0 == strncmp(argv[i], "-fps", 4)) {
			sscanf(argv[i] + 4, "%d", &frame_rate);
		} else if (0 == strncmp(argv[i], "-w", 2)) {
			sscanf(argv[i] + 2, "%d", &frame_w);
		} else if (0 == strncmp(argv[i], "-h", 2)) {
			sscanf(argv[i] + 2, "%d", &frame_h);
		} else if (0 == strncmp(argv[i], "-ss", 3)) {
			sscanf(argv[i] + 3, "%d", &shuter_duration);
		} else if (0 == strncmp(argv[i], "-i", 2)) {
			event_in_fifoname = argv[i] + 2;
		} else if (0 == strncmp(argv[i], "-o", 2)) {
			event_out_fifoname = argv[i] + 2;
		} else if (0 == strcmp(argv[i], "--linux-mouse-input-emu")) {
			output_type = LinuxMouseInput;
		} else {
			fprintf(stderr, "Invalid argument: \'%s\'\n", argv[i]);
			exit(1);
		}
	}
}

void showhelp(void) {
	fprintf(stdout, "vrintf");
	return;
}

