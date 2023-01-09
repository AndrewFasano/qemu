#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <algorithm>    // std::find
#include <string.h>
#include <vector>
#include <tuple>
#include <set>
#include <unordered_map>

extern "C" {
#include <qemu-plugin.h>
QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
QEMU_PLUGIN_EXPORT const char *qemu_plugin_name = "startstopnet";
#include <plugin-qpp.h>
}

qemu_plugin_id_t self_id;
static GMutex lock;


/* Extras
  ACCEPT
  44    NEWFILEP

  RECV
  54  DATA_LEN
  55  DATAP
 */


#define log_base 1000
enum cmds { _bind=10,
            _listen=20,
            _poll=30,
            _accept=40,
            _recv=50,
            _shutdown=60};
#define PID(x)   x+log_base+1
#define TGID(x)  x+log_base+2
#define FILEP(x) x+log_base+3

 #define xstr(s) str(s)
 #define str(s) #s


// On start we 0 the current struct, if we get a second SC without zeroing, then we're in an unexpected
// state and we'll bail (even on the final FILEP)
#define set_struct(str, fld, val) if (str.fld != 0) break; str.fld = val;
#define CASES(sc) case PID(sc):   current ## sc = {0}; set_struct(current ## sc, pid, arg); break; \
                  case TGID(sc):  set_struct(current ## sc, tgid, arg); break; \
                  case FILEP(sc): set_struct(current ## sc, filep, arg); // XXX no break

struct proc_sock_t {
  uint32_t pid;
  uint32_t tgid;
  uint32_t filep;
};

struct accept_t {
  uint32_t pid;
  uint32_t tgid;
  uint32_t filep;
  uint32_t newfilep;
};

struct recv_t {
  uint32_t pid;
  uint32_t tgid;
  uint32_t filep;
  uint32_t datalen;
  uint32_t datap;
};

proc_sock_t current_bind;
proc_sock_t current_listen;
proc_sock_t current_poll;
accept_t current_accept;
recv_t current_recv;
proc_sock_t current_shutdown;

void vcpu_hypercall(qemu_plugin_id_t id, unsigned int vcpu_index, int64_t num, uint64_t a1, uint64_t a2,
                    uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8) {

  uint32_t arg = (uint32_t)a1;
  switch (num) {

    // BIND //
    CASES(_bind)
      printf("Bind in pid %d tgid %d with filep %x\n",current_bind.pid, current_bind.tgid, current_bind.filep);
    break;

    CASES(_listen)
      printf("Listen in pid %d tgid %d with filep %x\n",
      current_listen.pid, current_listen.tgid, current_listen.filep);
    break;

    CASES(_poll)
      printf("Listen in pid %d tgid %d with filep %x\n", current_poll.pid, current_poll.tgid, current_poll.filep);
    break;

    CASES(_accept)
      // No print, we have special cases to handle for accept
    break;
    case  (_accept + log_base +  4):
      current_accept.newfilep = arg;
      printf("Accept in pid %d tgid %d with filep %x new filep %x\n",
      current_accept.pid, current_accept.tgid, current_accept.filep, current_accept.newfilep);
    break;

    CASES(_recv)
      // No print, we have special cases to handle for recv
    break;
    case  (_recv + log_base +  4):
      current_recv.datalen = arg;
    break;
    case  (_recv + log_base +  5):
      current_recv.datap = arg;
      printf("Recv in pid %d tgid %d with filep %x %d bytes of data at %x\n",
      current_recv.pid, current_recv.tgid, current_recv.filep, current_recv.datalen, current_recv.datap);
    break;

    CASES(_shutdown)
      printf("Shutdown sock in pid %d tgid %d with filep %x\n", current_shutdown.pid, current_shutdown.tgid, current_shutdown.filep);
    break;
  
    //default:
    //  printf("Unhandled hypercall %d\n", num);
  }
}


QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    self_id = id;
    qemu_plugin_register_vcpu_hypercall_cb(id, vcpu_hypercall);
    return 0;
}
