#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <ucontext.h>
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
static void ufd_add(struct libevdev_uinput *u, int fd);
#define MAX_NODES 4

struct pub {
	struct libevdev_uinput *u;
	char names[MAX_NODES][32];
	int grabs[MAX_NODES];
	int n, ng;
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

static bool wait_udev(const char *sys, const char *name) {
	char p[PATH_MAX], devno[32] = {0}, line[256];
	snprintf(p, sizeof(p), "%s/%s/dev", sys, name);
	FILE *f = fopen(p, "r");
	if (!f) return false;
	if (!fgets(devno, sizeof(devno), f)) devno[0] = 0;
	fclose(f);
	devno[strcspn(devno, "\n")] = 0;
	snprintf(p, sizeof(p), "/run/udev/data/c%s", devno);
	for (int i = 0; i < 300 && access(p, F_OK); i++) usleep(10000);
	bool hold = false;
	if ((f = fopen(p, "r"))) {
		while (fgets(line, sizeof(line), f))
			if (!strcmp(line, "E:ID_INPUT_TOUCHSCREEN=1\n") || !strcmp(line, "E:ID_INPUT_TABLET=1\n")) hold = true;
		fclose(f);
	}
	return hold;
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
		bool hold = wait_udev(sysdup, names[i]);
		char target[PATH_MAX], link[PATH_MAX], tmp[PATH_MAX];
		snprintf(target, sizeof(target), "%s/%s", src, names[i]);
		int gfd = -1;
		if (hold && !strncmp(names[i], "event", 5)) {
			gfd = open(target, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
			if (gfd >= 0 && ioctl(gfd, EVIOCGRAB, 1)) {
				close(gfd);
				gfd = -1;
			}
			fprintf(stderr, "sims4-input: %s %s\n", gfd >= 0 ? "holding" : "failed to hold", names[i]);
		}
		snprintf(link, sizeof(link), "%s/%s", dst_dir(), names[i]);
		snprintf(tmp, sizeof(tmp), "%s/.%s.tmp", dst_dir(), names[i]);
		pthread_mutex_lock(&pub_lock);
		if (gfd >= 0) {
			if (!pb->dead && pb->gen == gen && pb->ng < MAX_NODES)
				pb->grabs[pb->ng++] = gfd;
			else
				close(gfd);
		}
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

static bool node_dead(struct libevdev_uinput *u) {
	const char *(*get_devnode)(struct libevdev_uinput *) = dlsym(RTLD_DEFAULT, "libevdev_uinput_get_devnode");
	const char *node = get_devnode ? get_devnode(u) : NULL;
	const char *base = node ? strrchr(node, '/') : NULL;
	if (!base) return false;
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s%s", env_or("SIMS4_INPUT_SRC", "/dev/host-input"), base);
	for (int i = 0; i < 25; i++) {
		int t = open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (t >= 0) {
			close(t);
			return false;
		}
		if (errno == ENODEV || errno == ENXIO) return true;
		usleep(20000);
	}
	return false;
}

int libevdev_uinput_create_from_device(const struct libevdev *dev, int fd, struct libevdev_uinput **out) {
	static int (*real)(const struct libevdev *, int, struct libevdev_uinput **);
	if (!real) real = dlsym(RTLD_NEXT, "libevdev_uinput_create_from_device");
	int rc = real(dev, fd, out);
	struct stat st;
	if (rc || !out || !*out) return rc;
	int (*get_fd)(struct libevdev_uinput *) = dlsym(RTLD_DEFAULT, "libevdev_uinput_get_fd");
	bool src = !stat(env_or("SIMS4_INPUT_SRC", "/dev/host-input"), &st);
	for (int i = 0; src && i < 10 && node_dead(*out); i++) {
		struct libevdev_uinput *stale = *out;
		if (real(dev, -2, out) || !*out) {
			*out = stale;
			break;
		}
		if (fd >= 0 && get_fd && dup(fd) >= 0) dup2(get_fd(*out), fd);
		fprintf(stderr, "sims4-input: parked a device on a dead event node\n");
	}
	ufd_add(*out, fd >= 0 ? fd : get_fd ? get_fd(*out) : -1);
	if (!src) return rc;
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

#define MAX_CLICK 32
#define MAX_EV 64

struct click {
	int fd;
	unsigned code, gen;
	long long down;
	bool pending;
};

struct delay {
	struct click *c;
	unsigned gen;
	long long at;
};

static ssize_t (*real_wr)(int, const void *, size_t);
static struct click clicks[MAX_CLICK];
static struct {
	struct libevdev_uinput *u;
	int fd;
} ufds[MAX_DEV];
static int nufds;
static pthread_mutex_t click_lock = PTHREAD_MUTEX_INITIALIZER;

static long long now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

static long click_min(void) {
	static long v = -1;
	if (v < 0) v = atol(env_or("SIMS4_CLICK_MIN_MS", "150"));
	return v;
}

static void ufd_add(struct libevdev_uinput *u, int fd) {
	if (fd < 0) return;
	pthread_mutex_lock(&click_lock);
	for (int i = 0; i < MAX_DEV; i++)
		if (!ufds[i].u) {
			ufds[i].u = u;
			ufds[i].fd = fd;
			nufds++;
			break;
		}
	pthread_mutex_unlock(&click_lock);
}

static bool is_ufd(int fd) {
	for (int i = 0; i < MAX_DEV; i++)
		if (ufds[i].u && ufds[i].fd == fd) return true;
	return false;
}

static struct click *click_slot(int fd, unsigned code) {
	struct click *empty = NULL;
	for (int i = 0; i < MAX_CLICK; i++) {
		if (clicks[i].fd == fd + 1 && clicks[i].code == code) return &clicks[i];
		if (!clicks[i].fd && !empty) empty = &clicks[i];
	}
	if (empty) *empty = (struct click){.fd = fd + 1, .code = code, .gen = empty->gen + 1};
	return empty;
}

static void release_now(struct click *c) {
	struct input_event ev[2] = {{.type = EV_KEY, .code = c->code, .value = 0}, {.type = EV_SYN, .code = SYN_REPORT}};
	real_wr(c->fd - 1, ev, sizeof(ev));
	c->pending = false;
	c->gen++;
}

static void *delayed_release(void *arg) {
	struct delay d = *(struct delay *)arg;
	free(arg);
	long long w = d.at - now_ms();
	if (w > 0) usleep(w * 1000);
	pthread_mutex_lock(&click_lock);
	if (d.c->pending && d.c->gen == d.gen) release_now(d.c);
	pthread_mutex_unlock(&click_lock);
	return NULL;
}

static bool defer_release(struct click *c) {
	struct delay *d = malloc(sizeof(*d));
	if (!d) return false;
	*d = (struct delay){.c = c, .gen = c->gen, .at = c->down + click_min()};
	pthread_t t;
	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
	bool ok = !pthread_create(&t, &a, delayed_release, d);
	pthread_attr_destroy(&a);
	if (!ok) free(d);
	c->pending = ok;
	return ok;
}

ssize_t write(int fd, const void *buf, size_t n) {
	if (!real_wr) real_wr = dlsym(RTLD_NEXT, "write");
	const size_t sz = sizeof(struct input_event);
	if (!nufds || !n || n % sz || n > MAX_EV * sz || click_min() <= 0) return real_wr(fd, buf, n);
	pthread_mutex_lock(&click_lock);
	if (!is_ufd(fd)) {
		pthread_mutex_unlock(&click_lock);
		return real_wr(fd, buf, n);
	}
	const struct input_event *in = buf;
	struct input_event out[MAX_EV];
	size_t k = 0;
	for (size_t i = 0; i < n / sz; i++) {
		const struct input_event *e = &in[i];
		bool btn = e->type == EV_KEY && e->code >= BTN_LEFT && e->code <= BTN_TASK && (e->value == 0 || e->value == 1);
		struct click *c = btn ? click_slot(fd, e->code) : NULL;
		if (c && e->value == 1) {
			if (c->pending) {
				if (k) real_wr(fd, out, k * sz);
				k = 0;
				release_now(c);
			}
			c->down = now_ms();
		} else if (c && c->down && now_ms() - c->down < click_min() && defer_release(c)) {
			c->down = 0;
			continue;
		} else if (c) {
			c->down = 0;
		}
		out[k++] = *e;
	}
	ssize_t rc = k ? real_wr(fd, out, k * sz) : 0;
	pthread_mutex_unlock(&click_lock);
	return rc < 0 ? rc : (ssize_t)n;
}

void libevdev_uinput_destroy(struct libevdev_uinput *u) {
	static void (*real)(struct libevdev_uinput *);
	if (!real) real = dlsym(RTLD_NEXT, "libevdev_uinput_destroy");
	pthread_mutex_lock(&click_lock);
	for (int i = 0; i < MAX_DEV; i++) {
		if (ufds[i].u != u) continue;
		for (int k = 0; k < MAX_CLICK; k++)
			if (clicks[k].fd == ufds[i].fd + 1) clicks[k] = (struct click){.gen = clicks[k].gen + 1};
		ufds[i].u = NULL;
		nufds--;
	}
	pthread_mutex_unlock(&click_lock);
	pthread_mutex_lock(&pub_lock);
	for (int i = 0; i < MAX_DEV; i++) {
		if (pubs[i].u != u) continue;
		pubs[i].dead = true;
		for (int k = 0; k < pubs[i].ng; k++) close(pubs[i].grabs[k]);
		pubs[i].ng = 0;
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

struct wl_event_loop;
struct wl_src {
	void *interface;
	struct wl_event_loop *loop;
	void *prev, *next;
	void *data;
	int fd;
};
typedef int (*wl_fd_func)(int, uint32_t, void *);

#define MAX_SRC 4096
static struct {
	struct wl_src *s;
	void *func;
	int fd;
	bool removed;
} srcs[MAX_SRC];
static int nsrc;
static pthread_mutex_t src_lock = PTHREAD_MUTEX_INITIALIZER;
static __thread int in_wl;

static void crash_sym(const char *tag, void *a) {
	Dl_info d;
	if (dladdr(a, &d) && d.dli_fname)
		dprintf(2, "sims4-crash %s %s+0x%lx %s\n", tag, d.dli_fname, (unsigned long)((char *)a - (char *)d.dli_fbase), d.dli_sname ? d.dli_sname : "?");
	else
		dprintf(2, "sims4-crash %s %p\n", tag, a);
}

static void crash_bt(const char *tag) {
	void *bt[32];
	int n = backtrace(bt, 32);
	for (int i = 1; i < n; i++) crash_sym(tag, bt[i]);
}

struct wl_src *wl_event_loop_add_fd(struct wl_event_loop *loop, int fd, uint32_t mask, wl_fd_func func, void *data) {
	REAL(struct wl_src *, wl_event_loop_add_fd, struct wl_event_loop *, int, uint32_t, wl_fd_func, void *);
	in_wl++;
	struct wl_src *s = real_wl_event_loop_add_fd(loop, fd, mask, func, data);
	in_wl--;
	if (!s || !is_sway()) return s;
	pthread_mutex_lock(&src_lock);
	int slot = -1;
	for (int i = 0; i < nsrc && slot < 0; i++)
		if (srcs[i].removed || srcs[i].s == s) slot = i;
	if (slot < 0 && nsrc < MAX_SRC) slot = nsrc++;
	if (slot >= 0) {
		srcs[slot].s = s;
		srcs[slot].func = (void *)func;
		srcs[slot].fd = s->fd;
		srcs[slot].removed = false;
	}
	pthread_mutex_unlock(&src_lock);
	return s;
}

int wl_event_source_remove(struct wl_src *s) {
	REAL(int, wl_event_source_remove, struct wl_src *);
	if (is_sway()) {
		pthread_mutex_lock(&src_lock);
		for (int i = 0; i < nsrc; i++)
			if (srcs[i].s == s) srcs[i].removed = true;
		pthread_mutex_unlock(&src_lock);
	}
	in_wl++;
	int r = real_wl_event_source_remove(s);
	in_wl--;
	return r;
}

int close(int fd) {
	REAL(int, close, int);
	if (fd >= 0 && !in_wl && is_sway()) {
		void *owner = NULL;
		pthread_mutex_lock(&src_lock);
		for (int i = 0; i < nsrc; i++)
			if (!srcs[i].removed && srcs[i].fd == fd) owner = srcs[i].func;
		pthread_mutex_unlock(&src_lock);
		if (owner) {
			dprintf(2, "sims4-crash foreign close of event source fd %d\n", fd);
			crash_sym("owner", owner);
			crash_bt("closer");
		}
	}
	return real_close(fd);
}

static void crash_handler(int sig, siginfo_t *si, void *uc) {
	void *rdi = (void *)((ucontext_t *)uc)->uc_mcontext.gregs[REG_RDI];
	dprintf(2, "sims4-crash signal %d addr %p\n", sig, si->si_addr);
	crash_sym("rip", (void *)((ucontext_t *)uc)->uc_mcontext.gregs[REG_RIP]);
	for (int i = 0; i < nsrc; i++)
		if ((void *)srcs[i].s == rdi) {
			dprintf(2, "sims4-crash source fd %d removed %d\n", srcs[i].fd, srcs[i].removed);
			crash_sym("source", srcs[i].func);
		}
	crash_bt("bt");
	raise(sig);
}

__attribute__((constructor)) static void scrub_env(void) {
	if (!is_sway()) return;
	unsetenv("LD_PRELOAD");
	void *bt[2];
	backtrace(bt, 2);
	struct sigaction sa = {.sa_sigaction = crash_handler, .sa_flags = SA_SIGINFO | SA_RESETHAND};
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
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

#define RETRY_OPEN(call) \
	({ \
		int fd_ = (call); \
		for (int i_ = 0; fd_ < 0 && r == 1 && (errno == ENODEV || errno == ENXIO) && i_ < 50; i_++) { \
			usleep(20000); \
			fd_ = (call); \
		} \
		fd_; \
	})

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
		return finish_open(RETRY_OPEN(real_##name(r ? t : path, r ? flags & ~O_NOFOLLOW : flags, mode)), r); \
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
		return finish_open(RETRY_OPEN(real_##name(dirfd, r ? t : path, r ? flags & ~O_NOFOLLOW : flags, mode)), r); \
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
		return finish_open(RETRY_OPEN(real_##name(r ? t : path, r ? flags & ~O_NOFOLLOW : flags)), r); \
	}

OPEN2_WRAP(__open_2)
OPEN2_WRAP(__open64_2)

#endif
