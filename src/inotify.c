#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/types.h>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/error.h>
#include <mruby/string.h>
#include <mruby/variable.h>

typedef struct inotify_event ino_ev_t;

static uint32_t
mrb_inotify_flags_array_to_mask(mrb_state* mrb, mrb_value* flags, mrb_int flags_len)
{
  uint32_t i;
  uint32_t mask = 0;

#define CHECK_FLAG(x,y) \
    else if(mrb_symbol(flags[i]) == mrb_intern_lit(mrb, #x)) { mask |= y; }

  for(i = 0; i < flags_len; ++i) {
    if(0) {}

    CHECK_FLAG(access, IN_ACCESS)
    CHECK_FLAG(attrib, IN_ATTRIB)
    CHECK_FLAG(close_write, IN_CLOSE_WRITE)
    CHECK_FLAG(close_nowrite, IN_CLOSE_NOWRITE)
    CHECK_FLAG(close, IN_CLOSE)
    CHECK_FLAG(modify, IN_MODIFY)
    CHECK_FLAG(open, IN_OPEN)

    // Directory only below.
    CHECK_FLAG(create, IN_CREATE)
    CHECK_FLAG(delete, IN_DELETE)
    CHECK_FLAG(delete_self, IN_DELETE_SELF)
    CHECK_FLAG(moved_from, IN_MOVED_FROM)
    CHECK_FLAG(moved_to, IN_MOVED_TO)
    CHECK_FLAG(move, IN_MOVE)
    CHECK_FLAG(move_self, IN_MOVE_SELF)

    CHECK_FLAG(all_events, IN_ALL_EVENTS)

    // Option flags.
    CHECK_FLAG(oneshot, IN_ONESHOT)
    CHECK_FLAG(onlydir, IN_ONLYDIR)
    CHECK_FLAG(dont_follow, IN_DONT_FOLLOW)
    CHECK_FLAG(excl_unlink, IN_EXCL_UNLINK)
    CHECK_FLAG(mask_add, IN_MASK_ADD)

    else {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "unknown flag :%S",
        mrb_sym2str(mrb, mrb_symbol(flags[i])));
    }

  }

  return mask;
}

static mrb_value
mrb_inotify_mask_to_flags_array(mrb_state* mrb, uint32_t mask)
{
  mrb_value rv = mrb_ary_new(mrb);

#define READ_FLAG(x,y) \
  if(mask & y) \
    mrb_ary_push(mrb, rv, mrb_symbol_value(mrb_intern_lit(mrb, #x)));

  READ_FLAG(access, IN_ACCESS)
  READ_FLAG(attrib, IN_ATTRIB)
  READ_FLAG(close_write, IN_CLOSE_WRITE)
  READ_FLAG(close_nowrite, IN_CLOSE_NOWRITE)
  READ_FLAG(close, IN_CLOSE)
  READ_FLAG(modify, IN_MODIFY)
  READ_FLAG(open, IN_OPEN)

  // Directory only below.
  READ_FLAG(create, IN_CREATE)
  READ_FLAG(delete, IN_DELETE)
  READ_FLAG(delete_self, IN_DELETE_SELF)
  READ_FLAG(moved_from, IN_MOVED_FROM)
  READ_FLAG(moved_to, IN_MOVED_TO)
  READ_FLAG(move, IN_MOVE)
  READ_FLAG(move_self, IN_MOVE_SELF)

  // Special flags from read().
  READ_FLAG(ignored, IN_IGNORED);
  READ_FLAG(isdir, IN_ISDIR);
  READ_FLAG(q_overflow, IN_Q_OVERFLOW);
  READ_FLAG(unmount, IN_UNMOUNT);

  return rv;
}

static mrb_value
mrb_inotify_event_from_struct(mrb_state* mrb, ino_ev_t* event)
{
  struct RClass* klass;
  mrb_value instance;

  klass = mrb_class_get_under(mrb, mrb_module_get(mrb, "Inotify"), "Event");
  instance = mrb_obj_new(mrb, klass, 0, NULL);

  mrb_iv_set(mrb, instance, mrb_intern_lit(mrb, "@wd"),
    mrb_fixnum_value(event->wd));
  mrb_iv_set(mrb, instance, mrb_intern_lit(mrb, "@cookie"),
    mrb_fixnum_value(event->cookie));
  mrb_iv_set(mrb, instance, mrb_intern_lit(mrb, "@name"),
    (event->len > 0) ? mrb_str_new_cstr(mrb, event->name) : mrb_nil_value());
  mrb_iv_set(mrb, instance, mrb_intern_lit(mrb, "@events"),
    mrb_inotify_mask_to_flags_array(mrb, event->mask));

  return instance;
}

// Used both by Inotify::Notifier and Inotify::RecursiveNotifier.
static mrb_value
mrb_inotify_notifier_klass_new(mrb_state* mrb, mrb_value self)
{
  int       fd;
  mrb_value instance;

  fd = inotify_init();
  if((-1) == fd)
    mrb_sys_fail(mrb, strerror(errno));

  instance = mrb_instance_new(mrb, self);
  mrb_iv_set(mrb, instance, mrb_intern_lit(mrb, "@fd"), mrb_fixnum_value(fd));

  return instance;
}

static mrb_value
mrb_inotify_notifier_add_watch(mrb_state* mrb, mrb_value self)
{
  int        fd;
  mrb_value  path;
  mrb_value* flags;
  mrb_int    flags_len;
  int        wd;

  fd = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@fd")));

  mrb_get_args(mrb, "Sa", &path, &flags, &flags_len);

  wd = inotify_add_watch(fd, mrb_string_value_cstr(mrb, &path), 
    mrb_inotify_flags_array_to_mask(mrb, flags, flags_len));

  if((-1) == wd)
    mrb_sys_fail(mrb, strerror(errno));

  return mrb_fixnum_value(wd);
}

static mrb_value
mrb_inotify_notifier_rm_watch(mrb_state* mrb, mrb_value self)
{
  int     fd;
  mrb_int wd;

  fd = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@fd")));
  mrb_get_args(mrb, "i", &wd);

  if((-1) == inotify_rm_watch(fd, wd))
    mrb_sys_fail(mrb, strerror(errno));

  return mrb_nil_value();
}

static mrb_value
mrb_inotify_notifier_read_events(mrb_state* mrb, mrb_value self)
{
  // Events coming from inotify have a length of sizeof(struct inotify_event)
  // + event->len (length of the event->name field).
  static const ssize_t EVENT_SIZE   = sizeof(struct inotify_event) + NAME_MAX + 1;
  static const ssize_t EVENT_BUFLEN = 1024 * EVENT_SIZE;

  mrb_value block;
  int       fd;
  int       nread;
  uint32_t  i;
  char      buffer[EVENT_BUFLEN];
  ino_ev_t* event;

  mrb_get_args(mrb, "&", &block);

  fd = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@fd")));

  nread = read(fd, buffer, EVENT_BUFLEN); 
  if((-1) == nread)
    mrb_sys_fail(mrb, strerror(errno));

  for(i = 0; i < nread;) {
    event = (ino_ev_t*) &buffer[i];
    mrb_yield(mrb, block, mrb_inotify_event_from_struct(mrb, event)); 
    i += sizeof(struct inotify_event) + event->len;
  }

  return mrb_nil_value();
}

static mrb_value
mrb_inotify_notifier_close(mrb_state* mrb, mrb_value self)
{
  int fd = mrb_fixnum(mrb_iv_get(mrb, self, mrb_intern_lit(mrb, "@fd")));

  if((-1) == close(fd))
    mrb_sys_fail(mrb, strerror(errno));

  return mrb_nil_value();
}

void
mrb_mruby_inotify_gem_init(mrb_state* mrb)
{
  struct RClass* m_inotify;
  struct RClass* c_notifier;
  struct RClass* c_recursive_notifier;

  m_inotify = mrb_define_module(mrb, "Inotify");

  c_notifier = mrb_define_class_under(mrb, m_inotify, "Notifier", mrb->object_class);
  mrb_define_class_method(mrb, c_notifier, "new",
    mrb_inotify_notifier_klass_new, MRB_ARGS_NONE());
  mrb_define_method(mrb, c_notifier, "add_watch",
    mrb_inotify_notifier_add_watch, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, c_notifier, "rm_watch",
    mrb_inotify_notifier_rm_watch, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, c_notifier, "read_events",
    mrb_inotify_notifier_read_events, MRB_ARGS_BLOCK());
  mrb_define_method(mrb, c_notifier, "close",
    mrb_inotify_notifier_close, MRB_ARGS_NONE());

  c_recursive_notifier = mrb_define_class_under(mrb, m_inotify, "RecursiveNotifier", c_notifier);
  mrb_define_class_method(mrb, c_recursive_notifier, "new",
    mrb_inotify_notifier_klass_new, MRB_ARGS_NONE());

  mrb_define_class_under(mrb, m_inotify, "Event", mrb->object_class);
}

void
mrb_mruby_inotify_gem_final(mrb_state* mrb)
{
}
