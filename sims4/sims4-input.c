#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char *env_or(const char *name, const char *def) {
	const char *v = getenv(name);
	return v && *v ? v : def;
}

static const char *dst_dir(void) { return env_or("SIMS4_INPUT_DIR", "/dev/input"); }

static bool node_name(const char *n) {
	return !strncmp(n, "event", 5) || !strncmp(n, "js", 2) || !strncmp(n, "mouse", 5);
}

#ifdef SIMS4_UINPUT

struct libevdev;
struct libevdev_uinput;

#define MAX_DEV 32
#define MAX_NODES 4

struct pub {
	struct libevdev_uinput *u;
	char names[MAX_NODES][32];
	int n;
	bool dead;
	unsigned gen;
};

struct job {
	struct pub *pb;
	unsigned gen;
	char sys[PATH_MAX];
};

static struct pub pubs[MAX_DEV];
static pthread_mutex_t pub_lock = PTHREAD_MUTEX_INITIALIZER;

static void wait_udev(const char *sys, const char *name) {
	char p[PATH_MAX], devno[32] = {0};
	snprintf(p, sizeof(p), "%s/%s/dev", sys, name);
	FILE *f = fopen(p, "r");
	if (!f) return;
	if (!fgets(devno, sizeof(devno), f)) devno[0] = 0;
	fclose(f);
	devno[strcspn(devno, "\n")] = 0;
	snprintf(p, sizeof(p), "/run/udev/data/c%s", devno);
	for (int i = 0; i < 300 && access(p, F_OK); i++) usleep(10000);
}

static void *publish(void *arg) {
	struct job *jb = arg;
	struct pub *pb = jb->pb;
	char sysdup[PATH_MAX];
	snprintf(sysdup, sizeof(sysdup), "%s", jb->sys);
	unsigned gen = jb->gen;
	free(jb);
	char names[MAX_NODES][32];
	int n = 0;
	for (int tries = 0; tries < 100 && n == 0; tries++) {
		DIR *d = opendir(sysdup);
		if (!d) break;
		struct dirent *e;
		while ((e = readdir(d)) && n < MAX_NODES)
			if (node_name(e->d_name)) snprintf(names[n++], 32, "%s", e->d_name);
		closedir(d);
		if (!n) usleep(10000);
	}
	const char *src = env_or("SIMS4_INPUT_SRC", "/dev/host-input");
	for (int i = 0; i < n; i++) {
		wait_udev(sysdup, names[i]);
		char target[PATH_MAX], link[PATH_MAX], tmp[PATH_MAX];
		snprintf(target, sizeof(target), "%s/%s", src, names[i]);
		snprintf(link, sizeof(link), "%s/%s", dst_dir(), names[i]);
		snprintf(tmp, sizeof(tmp), "%s/.%s.tmp", dst_dir(), names[i]);
		pthread_mutex_lock(&pub_lock);
		if (!pb->dead && pb->gen == gen && pb->n < MAX_NODES) {
			unlink(tmp);
			if (!symlink(target, tmp) && !rename(tmp, link)) {
				snprintf(pb->names[pb->n++], 32, "%s", names[i]);
				fprintf(stderr, "sims4-input: linked %s (%s)\n", names[i], sysdup);
			} else {
				fprintf(stderr, "sims4-input: link %s failed: %s\n", names[i], strerror(errno));
			}
		}
		pthread_mutex_unlock(&pub_lock);
	}
	return NULL;
}

__attribute__((constructor)) static void reset_links(void) {
	char comm[32] = {0};
	struct stat st;
	FILE *f = fopen("/proc/self/comm", "r");
	if (!f) return;
	if (!fgets(comm, sizeof(comm), f)) comm[0] = 0;
	fclose(f);
	comm[strcspn(comm, "\n")] = 0;
	if (strcmp(comm, "sunshine") || stat(env_or("SIMS4_INPUT_SRC", "/dev/host-input"), &st)) return;
	DIR *d = opendir(dst_dir());
	if (!d) return;
	struct dirent *e;
	while ((e = readdir(d)))
		if (e->d_name[0] != '.' || strstr(e->d_name, ".tmp")) {
			char p[PATH_MAX];
			snprintf(p, sizeof(p), "%s/%s", dst_dir(), e->d_name);
			if (!lstat(p, &st) && !S_ISDIR(st.st_mode)) unlink(p);
		}
	closedir(d);
}

int libevdev_uinput_create_from_device(const struct libevdev *dev, int fd, struct libevdev_uinput **out) {
	static int (*real)(const struct libevdev *, int, struct libevdev_uinput **);
	if (!real) real = dlsym(RTLD_NEXT, "libevdev_uinput_create_from_device");
	int rc = real(dev, fd, out);
	struct stat st;
	if (rc || !out || !*out || stat(env_or("SIMS4_INPUT_SRC", "/dev/host-input"), &st)) return rc;
	const char *(*get_syspath)(struct libevdev_uinput *) = dlsym(RTLD_DEFAULT, "libevdev_uinput_get_syspath");
	const char *sys = get_syspath ? get_syspath(*out) : NULL;
	struct job *jb = sys ? calloc(1, sizeof(*jb)) : NULL;
	if (!jb) return rc;
	snprintf(jb->sys, sizeof(jb->sys), "%s", sys);
	pthread_mutex_lock(&pub_lock);
	struct pub *pb = NULL;
	for (int i = 0; i < MAX_DEV && !pb; i++)
		if (!pubs[i].u) pb = &pubs[i];
	if (pb) {
		unsigned gen = pb->gen + 1;
		*pb = (struct pub){.u = *out, .gen = gen};
		jb->pb = pb;
		jb->gen = gen;
	}
	pthread_mutex_unlock(&pub_lock);
	if (!pb) {
		free(jb);
		return rc;
	}
	pthread_t t;
	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&t, &a, publish, jb)) publish(jb);
	pthread_attr_destroy(&a);
	return rc;
}

void libevdev_uinput_destroy(struct libevdev_uinput *u) {
	static void (*real)(struct libevdev_uinput *);
	if (!real) real = dlsym(RTLD_NEXT, "libevdev_uinput_destroy");
	pthread_mutex_lock(&pub_lock);
	for (int i = 0; i < MAX_DEV; i++) {
		if (pubs[i].u != u) continue;
		pubs[i].dead = true;
		for (int k = 0; k < pubs[i].n; k++) {
			char link[PATH_MAX];
			snprintf(link, sizeof(link), "%s/%s", dst_dir(), pubs[i].names[k]);
			unlink(link);
			fprintf(stderr, "sims4-input: unlinked %s\n", pubs[i].names[k]);
		}
		pubs[i].u = NULL;
	}
	pthread_mutex_unlock(&pub_lock);
	real(u);
}

#else

struct udev;
struct udev_monitor;
struct udev_device;

#define MAX_MON 8
#define MAX_Q 64
#define MAX_CACHE 32

struct qitem {
	char name[32];
	bool add;
};

struct fmon {
	struct udev_monitor *mon;
	struct udev *udev;
	bool fake;
	int ep, in, pipe[2];
	struct qitem q[MAX_Q];
	int qh, qt;
	struct {
		char name[32];
		struct udev_device *dev;
	} cache[MAX_CACHE];
};

static struct fmon mons[MAX_MON];
static struct {
	struct udev_device *dev;
	const char *action;
} actions[MAX_CACHE * 2];
static pthread_mutex_t mon_lock = PTHREAD_MUTEX_INITIALIZER;

#define REAL(ret, name, ...) \
	static ret (*real_##name)(__VA_ARGS__); \
	if (!real_##name) real_##name = dlsym(RTLD_NEXT, #name)

static bool is_sway(void) {
	static int v = -1;
	if (v < 0) {
		char comm[32] = {0};
		FILE *f = fopen("/proc/self/comm", "r");
		if (f) {
			if (!fgets(comm, sizeof(comm), f)) comm[0] = 0;
			fclose(f);
		}
		comm[strcspn(comm, "\n")] = 0;
		v = !strcmp(comm, "sway");
	}
	return v;
}

__attribute__((constructor)) static void scrub_env(void) {
	if (is_sway()) unsetenv("LD_PRELOAD");
}

static bool layout_active(void) {
	struct stat st;
	return is_sway() && (!stat(env_or("SIMS4_INPUT_SRC", "/dev/host-input"), &st) || getenv("SIMS4_INPUT_DIR"));
}

static struct fmon *find_mon(struct udev_monitor *m) {
	for (int i = 0; i < MAX_MON; i++)
		if (mons[i].mon == m) return &mons[i];
	return NULL;
}

static void set_action(struct udev_device *d, const char *a) {
	int free_i = -1;
	for (int i = 0; i < MAX_CACHE * 2; i++) {
		if (actions[i].dev == d) {
			actions[i].action = a;
			return;
		}
		if (!actions[i].dev && free_i < 0) free_i = i;
	}
	if (free_i >= 0) actions[free_i].dev = d, actions[free_i].action = a;
}

struct udev_monitor *udev_monitor_new_from_netlink(struct udev *udev, const char *name) {
	REAL(struct udev_monitor *, udev_monitor_new_from_netlink, struct udev *, const char *);
	struct udev_monitor *m = real_udev_monitor_new_from_netlink(udev, name);
	if (!m) return m;
	pthread_mutex_lock(&mon_lock);
	for (int i = 0; i < MAX_MON; i++)
		if (!mons[i].mon) {
			mons[i] = (struct fmon){.mon = m, .udev = udev, .ep = -1, .in = -1, .pipe = {-1, -1}};
			break;
		}
	pthread_mutex_unlock(&mon_lock);
	return m;
}

int udev_monitor_filter_add_match_subsystem_devtype(struct udev_monitor *m, const char *sub, const char *devtype) {
	REAL(int, udev_monitor_filter_add_match_subsystem_devtype, struct udev_monitor *, const char *, const char *);
	int rc = real_udev_monitor_filter_add_match_subsystem_devtype(m, sub, devtype);
	if (!sub || strcmp(sub, "input") || !layout_active()) return rc;
	pthread_mutex_lock(&mon_lock);
	struct fmon *f = find_mon(m);
	if (f && !f->fake) {
		f->in = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
		f->ep = epoll_create1(EPOLL_CLOEXEC);
		if (f->in >= 0 && f->ep >= 0 && !pipe2(f->pipe, O_NONBLOCK | O_CLOEXEC) &&
			inotify_add_watch(f->in, dst_dir(), IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM) >= 0) {
			struct epoll_event ev = {.events = EPOLLIN};
			epoll_ctl(f->ep, EPOLL_CTL_ADD, f->in, &ev);
			epoll_ctl(f->ep, EPOLL_CTL_ADD, f->pipe[0], &ev);
			f->fake = true;
			fprintf(stderr, "sims4-input: udev input monitor follows %s\n", dst_dir());
		}
	}
	pthread_mutex_unlock(&mon_lock);
	return rc;
}

int udev_monitor_get_fd(struct udev_monitor *m) {
	REAL(int, udev_monitor_get_fd, struct udev_monitor *);
	pthread_mutex_lock(&mon_lock);
	struct fmon *f = find_mon(m);
	int fd = f && f->fake ? f->ep : -1;
	pthread_mutex_unlock(&mon_lock);
	return fd >= 0 ? fd : real_udev_monitor_get_fd(m);
}

static void drain(struct fmon *f) {
	char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	ssize_t len;
	while ((len = read(f->in, buf, sizeof(buf))) > 0) {
		for (char *p = buf; p < buf + len;) {
			struct inotify_event *e = (struct inotify_event *)p;
			p += sizeof(*e) + e->len;
			if (!e->len || !node_name(e->name)) continue;
			int next = (f->qt + 1) % MAX_Q;
			if (next == f->qh) continue;
			snprintf(f->q[f->qt].name, 32, "%s", e->name);
			f->q[f->qt].add = e->mask & (IN_CREATE | IN_MOVED_TO);
			f->qt = next;
		}
	}
}

struct udev_device *udev_monitor_receive_device(struct udev_monitor *m) {
	REAL(struct udev_device *, udev_monitor_receive_device, struct udev_monitor *);
	pthread_mutex_lock(&mon_lock);
	struct fmon *f = find_mon(m);
	if (!f || !f->fake) {
		pthread_mutex_unlock(&mon_lock);
		return real_udev_monitor_receive_device(m);
	}
	struct udev_device *(*from_syspath)(struct udev *, const char *) = dlsym(RTLD_DEFAULT, "udev_device_new_from_syspath");
	struct udev_device *(*dref)(struct udev_device *) = dlsym(RTLD_DEFAULT, "udev_device_ref");
	struct udev_device *(*dunref)(struct udev_device *) = dlsym(RTLD_DEFAULT, "udev_device_unref");
	const char *(*prop)(struct udev_device *, const char *) = dlsym(RTLD_DEFAULT, "udev_device_get_property_value");
	drain(f);
	struct udev_device *out = NULL;
	while (!out && f->qh != f->qt) {
		struct qitem it = f->q[f->qh];
		f->qh = (f->qh + 1) % MAX_Q;
		int slot = -1;
		for (int i = 0; i < MAX_CACHE; i++)
			if (f->cache[i].dev && !strcmp(f->cache[i].name, it.name)) slot = i;
		if (it.add) {
			if (slot >= 0) continue;
			char cls[PATH_MAX], real[PATH_MAX];
			snprintf(cls, sizeof(cls), "/sys/class/input/%s", it.name);
			if (!realpath(cls, real)) continue;
			struct udev_device *d = from_syspath(f->udev, real);
			if (!d) continue;
			const char *tab = prop(d, "ID_INPUT_TABLET"), *touch = prop(d, "ID_INPUT_TOUCHSCREEN");
			if ((tab && !strcmp(tab, "1")) || (touch && !strcmp(touch, "1"))) {
				dunref(d);
				continue;
			}
			for (int i = 0; i < MAX_CACHE; i++)
				if (!f->cache[i].dev) {
					snprintf(f->cache[i].name, 32, "%s", it.name);
					f->cache[i].dev = dref(d);
					break;
				}
			set_action(d, "add");
			out = d;
		} else if (slot >= 0) {
			out = f->cache[slot].dev;
			f->cache[slot].dev = NULL;
			set_action(out, "remove");
		}
	}
	char c;
	while (read(f->pipe[0], &c, 1) > 0) {
	}
	if (f->qh != f->qt) {
		ssize_t w = write(f->pipe[1], "x", 1);
		(void)w;
	}
	pthread_mutex_unlock(&mon_lock);
	return out;
}

const char *udev_device_get_action(struct udev_device *d) {
	REAL(const char *, udev_device_get_action, struct udev_device *);
	pthread_mutex_lock(&mon_lock);
	const char *a = NULL;
	for (int i = 0; i < MAX_CACHE * 2 && d; i++)
		if (actions[i].dev == d) a = actions[i].action;
	pthread_mutex_unlock(&mon_lock);
	return a ? a : real_udev_device_get_action(d);
}

struct udev_monitor *udev_monitor_unref(struct udev_monitor *m) {
	REAL(struct udev_monitor *, udev_monitor_unref, struct udev_monitor *);
	pthread_mutex_lock(&mon_lock);
	struct fmon *f = find_mon(m);
	if (f) {
		if (f->fake) close(f->ep), close(f->in), close(f->pipe[0]), close(f->pipe[1]);
		f->mon = NULL;
		f->fake = false;
	}
	pthread_mutex_unlock(&mon_lock);
	return real_udev_monitor_unref(m);
}

static bool want_grab(void) {
	return strcmp(env_or("SIMS4_INPUT_GRAB", "1"), "0");
}

static int redirect(const char *path, char *target, size_t len) {
	if (strncmp(path, "/dev/input/", 11)) return 0;
	const char *base = path + 11;
	if (!node_name(base) || strchr(base, '/')) return 0;
	char link[PATH_MAX];
	snprintf(link, sizeof(link), "%s/%s", dst_dir(), base);
	ssize_t n = readlink(link, target, len - 1);
	if (n > 0) {
		target[n] = 0;
		return 1;
	}
	if (strcmp(dst_dir(), "/dev/input")) return -1;
	return 0;
}

static int finish_open(int fd, int r) {
	if (fd >= 0 && r == 1 && want_grab()) ioctl(fd, EVIOCGRAB, 1);
	return fd;
}

#define OPEN_WRAP(name, ...) \
	int name(const char *path, int flags, ...) { \
		REAL(int, name, const char *, int, ...); \
		mode_t mode = 0; \
		if (flags & (O_CREAT | O_TMPFILE)) { \
			va_list ap; \
			va_start(ap, flags); \
			mode = va_arg(ap, mode_t); \
			va_end(ap); \
		} \
		char t[PATH_MAX]; \
		int r = redirect(path, t, sizeof(t)); \
		if (r < 0) { \
			errno = ENOENT; \
			return -1; \
		} \
		return finish_open(real_##name(r ? t : path, r ? flags & ~O_NOFOLLOW : flags, mode), r); \
	}

OPEN_WRAP(open)
OPEN_WRAP(open64)

#define OPENAT_WRAP(name) \
	int name(int dirfd, const char *path, int flags, ...) { \
		REAL(int, name, int, const char *, int, ...); \
		mode_t mode = 0; \
		if (flags & (O_CREAT | O_TMPFILE)) { \
			va_list ap; \
			va_start(ap, flags); \
			mode = va_arg(ap, mode_t); \
			va_end(ap); \
		} \
		char t[PATH_MAX]; \
		int r = redirect(path, t, sizeof(t)); \
		if (r < 0) { \
			errno = ENOENT; \
			return -1; \
		} \
		return finish_open(real_##name(dirfd, r ? t : path, r ? flags & ~O_NOFOLLOW : flags, mode), r); \
	}

OPENAT_WRAP(openat)
OPENAT_WRAP(openat64)

#define OPEN2_WRAP(name) \
	int name(const char *path, int flags) { \
		REAL(int, name, const char *, int); \
		char t[PATH_MAX]; \
		int r = redirect(path, t, sizeof(t)); \
		if (r < 0) { \
			errno = ENOENT; \
			return -1; \
		} \
		return finish_open(real_##name(r ? t : path, r ? flags & ~O_NOFOLLOW : flags), r); \
	}

OPEN2_WRAP(__open_2)
OPEN2_WRAP(__open64_2)

#endif
