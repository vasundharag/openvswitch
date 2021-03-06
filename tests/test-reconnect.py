# Copyright (c) 2009, 2010 Nicira Networks.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import errno
import logging
import sys

import ovs.reconnect

def do_enable(arg):
    r.enable(now)

def do_disable(arg):
    r.disable(now)

def do_force_reconnect(arg):
    r.force_reconnect(now)

def error_from_string(s):
    if not s:
        return 0
    elif s == "ECONNREFUSED":
        return errno.ECONNREFUSED
    elif s == "EOF":
        return EOF
    else:
        sys.stderr.write("unknown error '%s'\n" % s)
        sys.exit(1)

def do_disconnected(arg):
    r.disconnected(now, error_from_string(arg))
    
def do_connecting(arg):
    r.connecting(now)

def do_connect_failed(arg):
    r.connect_failed(now, error_from_string(arg))

def do_connected(arg):
    r.connected(now)

def do_received(arg):
    r.received(now)

def do_run(arg):
    global now
    if arg is not None:
        now += int(arg)

    action = r.run(now)
    if action is None:
        pass
    elif action == ovs.reconnect.CONNECT:
        print "  should connect"
    elif action == ovs.reconnect.DISCONNECT:
        print "  should disconnect"
    elif action == ovs.reconnect.PROBE:
        print "  should send probe"
    else:
        assert False

def do_advance(arg):
    global now
    now += int(arg)

def do_timeout(arg):
    global now
    timeout = r.timeout(now)
    if timeout >= 0:
        print "  advance %d ms" % timeout
        now += timeout
    else:
        print "  no timeout"

def do_set_max_tries(arg):
    r.set_max_tries(int(arg))

def diff_stats(old, new):
    if (old.state != new.state or
        old.state_elapsed != new.state_elapsed or
        old.backoff != new.backoff):
        print("  in %s for %d ms (%d ms backoff)"
              % (new.state, new.state_elapsed, new.backoff))

    if (old.creation_time != new.creation_time or
        old.last_received != new.last_received or
        old.last_connected != new.last_connected):
        print("  created %d, last received %d, last connected %d"
              % (new.creation_time, new.last_received, new.last_connected))

    if (old.n_successful_connections != new.n_successful_connections or
        old.n_attempted_connections != new.n_attempted_connections or
        old.seqno != new.seqno):
        print("  %d successful connections out of %d attempts, seqno %d"
              % (new.n_successful_connections, new.n_attempted_connections,
                 new.seqno))

    if (old.is_connected != new.is_connected or
        old.current_connection_duration != new.current_connection_duration or
        old.total_connected_duration != new.total_connected_duration):
        if new.is_connected:
            negate = ""
        else:
            negate = "not "
        print("  %sconnected (%d ms), total %d ms connected"
              % (negate, new.current_connection_duration,
                 new.total_connected_duration))

def do_set_passive(arg):
    r.set_passive(True, now)

def do_listening(arg):
    r.listening(now)

def do_listen_error(arg):
    r.listen_error(now, int(arg))

def main():
    commands = {
        "enable": do_enable,
        "disable": do_disable,
        "force-reconnect": do_force_reconnect,
        "disconnected": do_disconnected,
        "connecting": do_connecting,
        "connect-failed": do_connect_failed,
        "connected": do_connected,
        "received": do_received,
        "run": do_run,
        "advance": do_advance,
        "timeout": do_timeout,
        "set-max-tries": do_set_max_tries,
        "passive": do_set_passive,
        "listening": do_listening,
        "listen-error": do_listen_error
    }

    logging.basicConfig(level=logging.CRITICAL)

    global now
    global r

    now = 1000
    r = ovs.reconnect.Reconnect(now)
    r.set_name("remote")
    prev = r.get_stats(now)
    print "### t=%d ###" % now
    old_time = now
    old_max_tries = r.get_max_tries()
    while True:
        line = sys.stdin.readline()
        if line == "":
            break

        print line[:-1]
        if line[0] == "#":
            continue

        args = line.split()
        if len(args) == 0:
            continue

        command = args[0]
        if len(args) > 1:
            op = args[1]
        else:
            op = None
        commands[command](op)

        if old_time != now:
            print
            print "### t=%d ###" % now
            old_time = now

        cur = r.get_stats(now)
        diff_stats(prev, cur)
        prev = cur
        if r.get_max_tries() != old_max_tries:
            old_max_tries = r.get_max_tries()
            print "  %d tries left" % old_max_tries

if __name__ == '__main__':
    main()


