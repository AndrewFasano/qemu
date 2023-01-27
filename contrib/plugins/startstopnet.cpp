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

// Guest process:
// BIND socket
// LISTEN socket
// accept / epoll / poll socket
//        [now accept if e/poll] -> new sock
// recv sock
// (optional?) shutdown


// Want to map from sockaddr -> state
// [1, 3, 5, 7] At every accept return track the pid/tid. If we see a read in the same pid/tid, processing is done when we see a call to accept, poll, or epoll_wait in the same pid/tid.
 
struct sock_state_t {
  uint32_t listen_pid;
  uint32_t listen_tid;

  uint32_t accept_pid;
  uint32_t accept_tid;

  uint32_t read_pid;
  uint32_t read_tid;

  uint32_t recv_pos; // Increment on recv
  bool child_recvd; // True when the child has recvd and we should log on next accept/poll/epoll

  struct sock_state_t *child; // Only ever set one of these
  struct sock_state_t *parent;
};

// If this isn't on the heap it will vanish before our plugin_exit is called. Ugh!
typedef std::unordered_map<uint32_t, struct sock_state_t*> sock_map_t;
sock_map_t *active_socks = new std::unordered_map<uint32_t, sock_state_t*>;

proc_sock_t current_bind;
proc_sock_t current_listen;
proc_sock_t current_poll;
accept_t current_accept;
recv_t current_recv;
proc_sock_t current_shutdown;

void vcpu_hypercall(qemu_plugin_id_t id, unsigned int vcpu_index, int64_t num, uint64_t a1, uint64_t a2,
                    uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8) {

  uint32_t arg = (uint32_t)a1;
  //if (num > 999 && num < 2000) printf("HC %ld\n", num);

switch (num) {
    CASES(_bind)
      //printf("Bind in pid %d tgid %d with filep %x\n",current_bind.pid, current_bind.tgid, current_bind.filep);
      // We could mark this socket as "boring" until we listen on it.
      // Then we'd be able to distinguish between sockets made in outgoing traffic
      // and sockets where we have no idea what's going on. Or we could fix the bugs
    break;

    CASES(_listen)
      //printf("Listen in pid %d tgid %d with filep %x\n",
      //       current_listen.pid, current_listen.tgid, current_listen.filep);

      // Once a sock is listened on, let's consider it interesting and start tracking
      if (active_socks->find(current_accept.filep) == active_socks->end()) {
        (*active_socks)[current_listen.filep] = new sock_state_t({
            .listen_pid = current_listen.pid,
            .listen_tid = current_listen.tgid,
        });
      }else{
        printf("A socket just started listening but we already knew about it?\n");
      }
    break;

    CASES(_poll) {
      //printf("Poll in pid %d tgid %d with filep %x\n", current_poll.pid, current_poll.tgid, current_poll.filep);
      auto sock = active_socks->find(current_poll.filep);
      if (sock == active_socks->end())
        return;

      if (sock->second->child_recvd) {
        printf("\n\n%d (%d) is done processing input because it's back at poll\n\n", current_poll.pid, current_poll.tgid);
        sock->second->child_recvd = false;
      }
    }
    break;

    CASES(_accept)
      // No print, we have special cases to handle for accept
    break;
    case  (_accept + log_base +  4): {
      current_accept.newfilep = arg;
      printf("Accept in pid %d tgid %d with filep %x new filep %x\n",
             current_accept.pid, current_accept.tgid, current_accept.filep, current_accept.newfilep);

      // filep might exist (if we set it up earlier)
      auto parent = active_socks->find(current_accept.filep);
      if (parent == active_socks->end()) {
        printf("Unexpected: accept without prior listen\n");
        return;
      }

      parent->second->accept_pid = current_accept.pid;
      parent->second->accept_tid = current_accept.tgid;

      // We just created a new sock with newfilep
      if (active_socks->find(current_accept.newfilep) != active_socks->end()) {
        // Address got reused? Is this a bug? Let's free it, I guess
        auto ptr = (*active_socks)[current_accept.newfilep];
        active_socks->erase(current_accept.newfilep);
        delete ptr;
      }

      (*active_socks)[current_accept.newfilep] = new sock_state_t({
          .accept_pid = current_accept.pid,
          .accept_tid = current_accept.tgid,
          .parent = parent->second,
      });
      // Update parent
      parent->second->child = (*active_socks)[current_accept.newfilep];
       }
    break;

    CASES(_recv)
      // recv, recvmsg, or read
      // No print, we have special cases to handle for recv
    break;

    case  (_recv + log_base +  4):
      current_recv.datalen = arg;
    break;
    case  (_recv + log_base +  5): // Size of buffer
    {
      current_recv.datap = arg;

      char* buf_contents = (char*)malloc(current_recv.datalen);
      if (!buf_contents) {
          printf("Failed to allocate %d byte buffer for recv\n", current_recv.datalen);
          return;
      }

      if (qemu_plugin_read_guest_virt_mem(arg, buf_contents, current_recv.datalen) == -1) {
        free(buf_contents);
        printf("ERROR: couldn't read %d bytes from GVA %#x\n", current_recv.datalen, current_recv.datap);
        return;
      }

      auto sock = active_socks->find(current_recv.filep);
      if (sock == active_socks->end()) {
        printf("WARNING: in pid %d recv for %x but it's a non-tracked, non-listening socket\n", current_recv.pid,
                current_recv.filep);
        printf("\tBuffer %d bytes: %s\n", current_recv.datalen, buf_contents);
        free(buf_contents);
        return;
      }

      auto parent = sock->second->parent; // could parent be NULL of freed? Probably?
      // This sock that just recv'd couldn't be listened on, you don't listen() read()
      // you listen() accept() read() and accept gives us a new object, so listen is in parent, always


      printf("Recv up to %d bytes for sock %x: accepted in %d,%d, listened in %d,%d, now in %d,%d\n",
          current_recv.datalen, current_recv.filep,
          sock->second->accept_pid, sock->second->accept_tid, 
          parent->listen_pid, parent->listen_tid, 
          current_recv.pid, current_recv.tgid);

      printf("Buffer is %d bytes: %s\n", current_recv.datalen, buf_contents);
      free(buf_contents);

      sock->second->recv_pos += current_recv.datalen; // Could be less
      parent->child_recvd = true;

      //printf("Recv (%ld) in pid %d tgid %d with filep %x %d bytes of data at %x\n", num,
      //  current_recv.pid, current_recv.tgid, current_recv.filep, current_recv.datalen, current_recv.datap);
    }
    break;

    CASES(_shutdown) {
      //printf("Shutdown sock in pid %d tgid %d with filep %x\n", current_shutdown.pid, current_shutdown.tgid, current_shutdown.filep);
      auto s = active_socks->find(current_shutdown.filep);
      if (s != active_socks->end()) {
        // Look through active_socks, delete this (and its children??) if we find it
        auto ptr = (*active_socks)[current_shutdown.filep];
        active_socks->erase(current_shutdown.filep);
        delete ptr;
      }
    }
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
