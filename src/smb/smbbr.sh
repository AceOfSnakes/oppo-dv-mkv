#!/bin/sh
# oppo-dv-mkv: SMB2 / SMB3 servers in the player's Network menu (smbbr.c). RAM only, gone after a restart.
# smbbr.sh on | off | status | check
# The player uses Samba 3.0 / mount programs for network shares; smbbr is bind-mounted over four of them. It runs the
# real one first (kept under /tmp/smbbr/real) and uses SMB2/3 only where that fails.
case "$0" in */*) W=${0%/*} ;; *) W=. ;; esac
D=/tmp/smbbr; B=/mnt/rootfs_normal/bin; L="net nmblookup umount.cifs mount"
say() { echo "$*"; }
is() { grep -q " $B/$1 tmpfs " /proc/mounts; }
undo() { for n in $L; do is $n && umount -n -l $B/$n; done; for n in $L; do is $n && { say "failed: cannot unwrap $n"; return 1; }; done; return 0; }
check() {
  for n in $L; do [ -x $B/$n ] || { say "not supported: no $B/$n"; return 1; }; done
  for f in nfs tmpfs cifs; do grep -qw $f /proc/filesystems || { say "not supported: no $f in the kernel"; return 1; }; done
  [ /bin -ef $B ] || { say "not supported: /bin is not $B"; return 1; }
  t=/tmp/smbbr.test.$$; cp $W/smbbr $t && chmod 755 $t; r=$($t hello 2>&1); rm -f $t
  [ "$r" = smbbr ] || { say "not supported: the program does not run ($r)"; return 1; }
  say supported
}
case "$1" in
check) check;;
on)
  for n in $L; do is $n && { say "already on"; exit 0; }; done
  r=$(check) || { say "$r"; exit 1; }
  mkdir -p $D
  grep -q " $D tmpfs " /proc/mounts || mount -n -t tmpfs -o size=4m,mode=700 none $D || { say "failed: no tmpfs"; exit 1; }
  mkdir -p $D/real; cp $W/smbbr $D/smbbr.new && chmod 755 $D/smbbr.new && mv $D/smbbr.new $D/smbbr || { say "failed: copy"; exit 1; }
  for n in $L; do
    grep -q " $D/real/$n " /proc/mounts || { : > $D/real/$n; mount -n --bind $B/$n $D/real/$n >/dev/null 2>&1; }
    [ $B/$n -ef $D/real/$n ] || { say "failed: real $n not kept"; exit 1; }
  done
  for n in $L; do $D/real/mount -n --bind $D/smbbr $B/$n || { say "failed: cannot wrap $n"; undo; exit 1; }; done
  # a mount that does not work would also stop USB sticks
  v=$(mount -V 2>&1); [ -n "$v" ] && [ "$v" = "$($D/real/mount -V 2>&1)" ] || { say "failed: mount test, undone"; undo; exit 1; }
  say on;;
off)
  undo || exit 1
  for n in $L; do umount -n -l $D/real/$n 2>/dev/null; done; umount -n -l $D 2>/dev/null; say off;;
status) for n in $L; do is $n && echo "$n on" || echo "$n off"; done;;
*) echo "usage: smbbr.sh on | off | status | check"; exit 2;;
esac
