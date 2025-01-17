#include "includes.h"

#include "common/wpa_ctrl.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/edit.h"
#include "utils/list.h"
#include "common/version.h"
#include "common/ieee802_11_defs.h"
#ifdef ANDROID
#include <cutils/properties.h>
#endif /* ANDROID */

static struct wpa_ctrl *mon_conn;
#ifndef CONFIG_CTRL_IFACE_DIR
#define CONFIG_CTRL_IFACE_DIR "/var/run/wpa_supplicant"
#endif /* CONFIG_CTRL_IFACE_DIR */

static const char *ctrl_iface_dir = CONFIG_CTRL_IFACE_DIR;

#ifdef ANDROID
static const char WIPUSHSOCKNAME[] = "@android:wpa_wipush";
#else
static const char WIPUSHSOCKNAME[] = "wpa_wipush";
#endif /* ANDROID */

static int not_disp_attached = 0;


static int not_open_connection(void);
static void not_disp_cleanup(void);

struct wipush_not{
	u8 addr[ETH_ALEN];
	u32 mid;
	int type;
	char *payload;
	u16 check;
	struct wipush_not *next;
};

u32* cur_mid;
char*  cur_addr;
struct wipush_not *notlist;

static void clean_wipush_messages(){
	struct wipush_not *not = notlist;
	struct wipush_not *t;

	while(not){
		t = not;
		not = not->next;
		os_free(t->payload);
		os_free(t);
	}
}

static void not_disp_close_connection(void) /* Mallesh: predicted the func name based on errors. */
{
	if (mon_conn == NULL)
		return;

	if (not_disp_attached) {
		wpa_ctrl_detach(mon_conn);
		not_disp_attached = 0;
	}

	if (mon_conn) {
		eloop_unregister_read_sock(wpa_ctrl_get_fd(mon_conn));
		wpa_ctrl_close(mon_conn);
		mon_conn = NULL;
	}
}

static void not_disp_reconnect(void)
{
	not_disp_close_connection();
	if (not_open_connection() < 0)
		return;
	
	edit_clear_line();
	printf("\rConnection to notification handler re-established\n");
	edit_redraw();
	
}

static void display_message(const char *str){
	printf("\rUnknown message: %s\n", str);
	edit_redraw();
}

static void display_not(const char *str){
	printf("\r%s\n", str);
	edit_redraw();
}

/*static void show_not(const char *sa, const char *str, int type, u32 mid){
	time_t now = time(NULL);
	printf("\r%ld: %s> %s%s (%u)\n", now, sa, str, type ? "?" : "", mid);
	edit_redraw();
}*/

static void show_not(const struct wipush_not *not, const char *addr){
	if(not == NULL)
		return;

	struct os_time t;
	double now;
	os_get_time(&t);

	now = (double)t.sec + 1e-6*(double)t.usec;

	//printf("\rN %.5f: %s> %s%s\tCheck:%u\n", now, addr, not->payload, not->type ? "?" : "", not->check);
	printf("\rN %.5f\tMID:%u\tCheck:%u\t%s\n", now, not->mid, not->check, not->payload);
	edit_redraw();
}

static struct wipush_not * add_not(u8 *addr, u32 mid, int type, u16 check, const char* payload, size_t len){
	struct wipush_not *not = notlist;

	while(not){
		if(not->mid == mid && os_memcmp(not->addr, addr, ETH_ALEN) == 0)
			return NULL;
		not = not->next;
	}

	not = os_malloc(sizeof(struct wipush_not));

	not->mid = mid;
	os_memcpy(not->addr, addr, ETH_ALEN);
	not->type = type;
	not->check = check;


	not->payload = os_malloc(len+1);//os_strdup(payload);
	os_memcpy(not->payload, payload, len);
	not->payload[len] = '\0';

	not->next = notlist;
	notlist= not;

	return not;
}


static void process_notification(const char *not){
	int type;
	char hwaddr[18];
	u8 addr[ETH_ALEN];
	u32 mid;
	const char *buf, *id;
	struct wipush_not *myn;
	u16 check;
	
	buf = not;
	
	
	if(os_strncmp(buf, "Type:",5)==0){
		buf += 5;
		type = atoi(buf);
		buf += 2;
	}else{
		goto fail;
	}
	
	if(os_strncmp(buf, "Addr:",5)==0){
		buf += 5;
		os_snprintf(hwaddr, 18, "%s", buf);
		hwaddr_aton(buf, addr);
	}else{
		goto fail;
	}
	
	buf = os_strstr(not, "MID");
	if(buf == NULL){
		goto fail;
	}
	
	buf += 4;
	mid = atoi(buf);
	
	buf = os_strchr(buf,'-');
	if(buf == NULL){
		goto fail;
	}
	buf++;

	id = os_strstr(buf, "CheckId:");
	if(id == NULL){
		goto fail;
	}
	check = atoi(id + 8);
	
	myn = add_not(addr, mid, type, check, buf, id-buf);

	if(myn)
		show_not(myn, hwaddr);

	if(type){
		if(cur_mid != NULL){
			printf("I am already processing a notification\n");
			goto fail;
		}
		cur_mid = os_malloc(sizeof(u32));
		cur_addr = os_strdup(hwaddr);
		*cur_mid = mid;
		printf("Please give a response for the notification\n");
	}
	
	return;
	
fail:
	display_not("FAILURE");
	
}

static void announce(const char *buf){
	char line[1024];
	char *pos = line;
	char *end = pos + sizeof(line);

	struct os_time t;
	double now;
	char* id = os_strstr(buf, "id:");

	os_get_time(&t);

	now = (double)t.sec + 1e-6*(double)t.usec;

	//res = os_snprintf(pos, end-pos, "A %.5f: Annoucing myself with a ", now);
	os_snprintf(pos, end-pos, "A %.5f\tCheck:%s", now, id + 4);
	//pos += res;




	/*switch(type){
		case 0:
			res = os_snprintf(pos, end-pos, "probe request\tCheck:%s", id + 3);
			break;
		case 1:
			res = os_snprintf(pos, end-pos, "action frame to stemed from a probe response\n");
			break;
		case 2:
			res = os_snprintf(pos, end-pos, "action frame to stemed from an action frame\n");
			break;
		default:
			res = os_snprintf(pos, end-pos, "unknown type of frame\n");

	}*/
	display_not(line);
}


static void not_event(const char *str){
	const char *buf;
	
	buf = str;
	
	if(os_strcmp(buf, "OK") == 0 || os_strcmp(buf, "FAIL") == 0){
		display_not(buf);
	}else if(os_strncmp(buf, "NOT:",4)==0){
		buf += 4;
		process_notification(buf);
	}else if(os_strncmp(buf, "ANNOUNCE ", 9) == 0){
		announce(buf + 9);
	}else if(os_strncmp(buf, "PONG", 4) == 0){

	}else{
		display_message(str);
	}
}

static void not_disp_recv_pending(struct wpa_ctrl *ctrl)
{
	while (wpa_ctrl_pending(ctrl) > 0) {
		char buf[4096];
		size_t len = sizeof(buf) - 1;
		if (wpa_ctrl_recv(ctrl, buf, &len) == 0) {
			buf[len] = '\0';
			edit_clear_line();
			not_event(buf);
			edit_redraw();
		} else {
			printf("Could not read pending message.\n");
			break;
		}
	}

	if (wpa_ctrl_pending(ctrl) < 0) {
		printf("Connection to wpa_supplicant lost - trying to "
			   "reconnect\n");
		not_disp_reconnect();
	}
}

static void not_disp_mon_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	not_disp_recv_pending(mon_conn);
}

static void wpa_cli_msg_cb(char *msg, size_t len)
{
	printf("%s\n", msg);
}

static int _wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd, int print)
{
	char buf[4096];
	size_t len;
	int ret;

	if (ctrl == NULL) {
		printf("Not connected to wpa_supplicant - command dropped.\n");
		return -1;
	}
	
	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, cmd, os_strlen(cmd), buf, &len,
			       wpa_cli_msg_cb);
	if (ret == -2) {
		printf("'%s' command timed out.\n", cmd);
		return -2;
	} else if (ret < 0) {
		printf("'%s' command failed.\n", cmd);
		return -1;
	}
	if (print) {
		buf[len] = '\0';
		printf("%s", buf);
	}
	return 0;
}

static int wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd)
{
	return _wpa_ctrl_command(ctrl, cmd, 1);
}

static int wpa_not_cmd_hl_query(struct wpa_ctrl *ctrl, int argc,
								char *argv[])
{
	int i, len;
	char cmd[1024];
	char *pos, *end;
	if (argc < 1)
	{
		printf("usage: hl_query message\n");
		return -1;
	}

	pos = cmd;
	end = pos + 1024;

	len = os_snprintf(pos, end - pos, "ACTION");
	if (len < 0 || len >= end - pos)
		return -1;
	pos += len;

	for (i = 0; i < argc; i++)
	{
		len = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (len < 0 || len >= end - pos)
			return -1;
		/*strcat(cmd, " ");
		 strcat(cmd, argv[i]);*/
		pos += len;
	}

	printf("cmd: %s (argc = %d)\n", cmd, argc);

	return wpa_ctrl_command(ctrl, cmd);
}

struct not_cmd {
	const char *cmd;
	int (*handler)(struct wpa_ctrl *ctrl, int argc, char *argv[]);
};

static struct not_cmd not_commands[] = {
	{ "hl_query" , wpa_not_cmd_hl_query },
	{ NULL, NULL }
};

static void wpa_not_cmd_handler(int argc, char *argv[])
{
	struct wpa_ctrl *ctrl = mon_conn;
	struct not_cmd *cmd, *match = NULL;
	int count;

	if (argc < 1 || argv == NULL){
		printf("No message given\n ");
		return;
	}

	count = 0;
	cmd = not_commands;
	while (cmd->cmd) {
		if (strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) == 0) {
			match = cmd;
			if (os_strcasecmp(cmd->cmd, argv[0]) == 0) {
				/* we have an exact match */
				count = 1;
				break;
			}
			count++;
		}
		cmd++;
	}
	
	if (count > 1) {
		printf("Ambiguous command '%s'; possible commands:", argv[0]);
		cmd = not_commands;
		while (cmd->cmd) {
			if (strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) ==
				0) {
				printf(" %s", cmd->cmd);
			}
			cmd++;
		}
		printf("\n");
	} else if (count == 0) {
		printf("Unknown command '%s'\n", argv[0]);
	} else {
		char *tmsg = "00:e0:4c:33:0c:cd 0 Hello, how are you?";
		match->handler(ctrl, argc - 1, &tmsg);
		//match->handler(ctrl, argc - 1, &argv[1]);
	}
}

int wpa_not_process_command(char *msg)
{
	int argc = 2;

	wpa_not_cmd_handler(argc, &msg);
}

static int not_open_connection()
{
	char *cfile = NULL;
	int flen, res;


#ifdef ANDROID
	cfile = os_strdup(WIPUSHSOCKNAME);
#endif /* ANDROID */


	if (cfile == NULL) {
		flen = os_strlen(ctrl_iface_dir) + os_strlen(WIPUSHSOCKNAME) + 2;
		cfile = os_malloc(flen);
		if (cfile == NULL)
			return -1;
		res = os_snprintf(cfile, flen, "%s/%s", ctrl_iface_dir,
						  WIPUSHSOCKNAME);


		if (res < 0 || res >= flen) {
			os_free(cfile);
			return -1;
		}
	}

	mon_conn = wpa_ctrl_open(cfile);

	os_free(cfile);

	if (mon_conn) {
		if (wpa_ctrl_attach(mon_conn) == 0) {
			not_disp_attached = 1;
			eloop_register_read_sock(wpa_ctrl_get_fd(mon_conn),
										 not_disp_mon_receive, NULL, NULL);
		} else {
			printf("Warning: Failed to attach to "
				   "wpa_supplicant.\n");
			not_disp_close_connection();
			return -1;
		}
	}else{
		return -1;
	}

	return 0;
}

static void not_disp_terminate(int sig, void *ctx)
{
	eloop_terminate();
}

static void not_disp_cleanup(void)
{
	not_disp_close_connection();
	clean_wipush_messages();
}

static void not_disp_end(void *eloop_ctx, void *timeout_ctx){
	not_disp_cleanup();
	eloop_destroy();
	os_program_deinit();
}

int wpa_not_init(char *msg)
{
	if (os_program_init())
		return -1;

	if (eloop_init())
		return -1;

	cur_mid = NULL;
	cur_addr = NULL;
	notlist = NULL;

	eloop_register_signal_terminate(not_disp_terminate, NULL);
	eloop_register_timeout(4000, 0, not_disp_end, NULL, NULL);

	if (not_open_connection() < 0){
		printf("Cannot start the connection\n");
		return -1;
	}
	
	return 0;
}

int wpa_not_deinit()
{
	eloop_destroy();
	not_disp_cleanup();
	os_program_deinit();

	return 0;
}
