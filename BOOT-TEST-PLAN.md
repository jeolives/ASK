# ASK Boot Test Plan — LS1046A + OpenWRT-ASK-Claude

Target: validate that a freshly-built ASK image (`mt-6.12.y` branch) boots,
programs the FMAN, forwards traffic, and stays stable under load on
LS1046A hardware (Mono Gateway DK or equivalent).

The plan is organised as sequential phases. Each phase has specific
observations and go/no-go criteria. **Don't skip phases** — earlier
phases gate later ones, so a failure in Phase 2 tells you not to bother
running Phase 3 until it's fixed.

---

## Pre-boot setup

Before flashing, have these in place:

- **Serial console attached and logged to disk.** This is non-negotiable
  — most first-boot failures happen in dmesg before SSH is reachable.
  Use `screen -L` or `picocom -g`.
- **Previous working image on a TFTP server** for fast rollback. If the
  new image hangs on init, the 30-second U-Boot dance back to a
  known-good image is the difference between "moved on" and "bricked a
  day."
- **Host machine reachable over console + eventually SSH.** `ssh -T`
  test ahead of time.
- **On the host, a shell with these open:**
  ```bash
  # Terminal 1: serial console
  picocom -b 115200 -g boot.log /dev/ttyUSB0

  # Terminal 2: once SSH works
  ssh root@<router> 'dmesg -w'

  # Terminal 3: ready for test commands
  ssh root@<router>
  ```

---

## Phase 1 — Cold-boot (first 5 minutes) — CRITICAL

### Expected dmesg anchors, in order

```
[   N.N]  951-nxp-ask kernel bits visible (sdk_dpaa, sdk_fman)
[   N.N]  fsl_dpa ...  probe of DPAA interfaces
[   N.N]  cdx: FMAN firmware X.Y.Z - ASK supported
[   N.N]  cdx_module_init
[   N.N]  cdx_ctrl_init success
[   N.N]  start_dpa_app successful
[   N.N]  fci init
[   N.N]  ABM: initializing automatic bridging module
```

### Go / no-go

| Observation | Meaning | Action |
|---|---|---|
| `cdx: FMAN firmware X.Y.Z lacks ASK support` | Microcode not loaded by U-Boot | Check U-Boot env, reflash microcode |
| `cdx: cannot read FMAN firmware revision` | `fm_get_fw_rev` symbol missing — kernel patch 955 not applied | Rebuild kernel with `955-nxp-ask-fm-get-fw-rev.patch` |
| `modprobe: cdx: Invalid module format` / `unknown symbol fm_get_fw_rev` | Same as above but caught at modprobe | Same fix |
| `start_dpa_app failed rc N` (module then unloads) | `dpa_app` binary missing / broken — fatal by design after `b150699` | Check `/usr/bin/dpa_app`, run it manually for diagnostics |
| `cdx_init_frag_module failed` | FMAN MURAM not accessible — sdk_fman not probed yet or broken | Timing issue; check probe order in dmesg |
| `cdx_dpa_ipsec_init failed` | IPsec-specific init — offline port not configured by `dpa_app` | `dpa_app` didn't program PCD fully |
| **Module loads silently, `/dev/cdx_ctrl` exists** | ✅ First-boot class passed | Proceed |

### Quick smoke commands

```bash
ls -l /dev/cdx_ctrl                           # exists, 660 root:root
lsmod | grep -E 'cdx|fci|auto_bridge'         # all three present
cat /proc/modules | grep cdx                  # refcount = 1 (fci linking)
cat /proc/fqid_stats/list 2>/dev/null | head  # fast-path FQs exist
ls /proc/net/abm 2>/dev/null                  # auto_bridge proc exists
```

> **If anything above this line fails, stop here.** Diagnose with serial
> dmesg; do not proceed to Phase 2.

---

## Phase 2 — Smoke test (next 10 minutes)

### 2a. Control-plane daemon up

```bash
ps | grep cmm
tail -50 /var/log/cmm.log        # or wherever cmm logs go
```

**Look for:** no `fci_catch() failed` spam, no `recvmsg() failed`, no
format-string errors. One-time startup lines are fine.

### 2b. Interface enumeration

```bash
ip link show | grep -E 'fm1-|dpa-|mac'       # DPAA interfaces present
ethtool -i eth0                               # driver = fsl_dpa
ls /sys/class/net/*/device/driver/module 2>/dev/null
```

**Look for:** all expected FMAN ports show up as netdevs.

### 2c. Stats-callback sanity check

Exercises our `virt_addr_valid` fix:

```bash
ip -s link                       # should print cleanly, no crash
for i in $(seq 5); do ip -s link >/dev/null; sleep 1; done
dmesg | tail -30                 # nothing new logged
```

**Look for:** no `NULL pointer dereference`, no `BUG:`, no `Unable to
handle kernel paging request`. The `virt_addr_valid` fix should make
this safe even on interfaces without populated stats.

### 2d. Basic forwarding

Two hosts connected on different ports. Static IPs, no firewall rules.

```bash
# On the router
ping -c 5 <host-A>
ping -c 5 <host-B>
# On host-A
ping -c 5 <host-B>                # forwarding through the router
```

**Look for:** zero packet loss on forwarding path. Check
`ifconfig` / `ip -s link` for rx/tx increments.

---

## Phase 3 — Feature validation (next 30 minutes)

Per-feature tests, each gated by dmesg watch. **Before each test, clear
dmesg with `dmesg -c`** so new issues are easy to spot.

### 3a. Auto-bridge offload

```bash
brctl addbr br0 2>/dev/null || ip link add br0 type bridge
ip link set eth1 master br0
ip link set eth2 master br0
ip link set br0 up
ip link set eth1 up; ip link set eth2 up
cat /proc/net/abm                # after some traffic, L2 flows should appear
dmesg | grep -iE 'abm|BUG|oops'
```

**Look for:** `cat /proc/net/abm` shows entries in SEEN → CONFIRMED →
FF state after traffic. No kernel traces.

### 3b. Connection tracking + fast-path

```bash
# From host-A through the router, establish a TCP session
nc -l 5555 &                      # on host-B
nc <host-B> 5555 <<< 'hello'      # on host-A
# On router:
conntrack -L | head               # should see the flow
cmm_cli show routes               # or equivalent cmm CLI
```

**Look for:** entry appears in conntrack AND shows up as offloaded in
cmm's view.

### 3c. IPsec — the biggest single risk

Set up an IPsec tunnel to a known-good peer. Exercise both directions.

```bash
# After tunnel is up:
ip xfrm state                     # SAs present
ping -c 100 <peer-inner-ip>       # expect ~100% success
dmesg | grep -i 'dpa_ipsec\|SEC dequeue'
```

**Critical dmesg check for our mask fix:**

- **Zero** `dpa_ipsec: SEC error status=0x... dropping` messages during
  legitimate traffic
- If you DO see them, capture the `status=` value — tells you which
  benign bit the mask should also ignore
- Force a failure (wrong key on peer) and confirm you DO see the message
  then — proves the check is wired up

### 3d. Multicast

Exercises the bitwise-AND → logical-AND fix (origin `cf1964e`):

```bash
# Subscribe a multicast group on a downstream host
# Send from upstream
# Check that the router forwards correctly
cat /proc/net/igmp               # membership
cmm_cli show multicast           # or equivalent
```

### 3e. PPPoE (if applicable)

Bring up a PPPoE session on the WAN side. The
`900-nxp-ask-ifindex.patch` we just added should make CMM's tunnel
correlation work.

```bash
pppoe-discovery -I eth0
# bring up pppoe with pppd
ls /var/log/ppp/ip-up.log 2>/dev/null   # should include ifindex arg
cmm_cli show tunnels                    # the PPP tunnel should be correlated
```

### 3f. Stats query under load

Exercises RCU + `virt_addr_valid` + refcount fixes:

```bash
# On router, generate some traffic
iperf3 -s &
# From a host: iperf3 -c <router> -t 60
# On router during iperf:
while :; do ip -s link; sleep 0.2; done &
# Let run for a minute
# Then: dmesg | grep -iE 'bug|oops|warn'
```

**Look for:** nothing new in dmesg. Our RCU conversion of
`find_osdev_by_fman_params` and the stats callback NULL checks should
keep this smooth.

---

## Phase 4 — Sustained load (1 hour, then overnight)

### 4a. Sustained traffic

```bash
iperf3 -s &                       # on one host
iperf3 -c <router> -t 3600 -P 4   # from another, 1-hour run
# In parallel on router:
while :; do ip -s link; sleep 30; done > /tmp/stats.log 2>&1 &
dmesg -w > /tmp/dmesg.log 2>&1 &
```

After 1 hour:

- `dmesg | grep -iE 'oops|bug|stall|lockup|refcount|leak'` → empty
- `cat /proc/meminfo` → memory not drifting
- `ip -s link | grep errors` → zero errors

### 4b. Netlink pressure

Exercises our ENOBUFS resync for link/ifaddr/route/rule:

```bash
# Flood conntrack with short-lived flows
for i in $(seq 1000); do
    (nc -w1 <host> 80 <<< "x") &
done
# Simultaneously monitor cmm
tail -f /var/log/cmm.log &

# Also try route flush/restore
ip route save > /tmp/routes
ip route flush table main
ip route restore < /tmp/routes
```

**Look for:**

- No `netlink ENOBUFS — events dropped` spam (1-2 instances OK, steady
  stream is bad)
- If you DO see ENOBUFS, the resync should trigger — check that
  fast-path state re-converges (test with `conntrack -L` and cmm CLI)
- No cmm crash / restart

### 4c. Module unload/reload

Exercises the three deregister fixes we just added:

```bash
# Save working state
conntrack -L > /tmp/conntrack.pre
ip -s link > /tmp/link.pre

# Try unload
rmmod cdx            # should succeed cleanly IF modprobe was done with all packages
# Check dmesg: no UAF, no oops

# Reload
modprobe cdx
# /proc/ucode_frag/stats should recreate cleanly (not EEXIST)
# /dev/cdx_ctrl should reappear
```

If `rmmod` refuses with `Device or resource busy`, it's because
`fci` / `auto_bridge` have references or cmm is using `/dev/cdx_ctrl`.
That's expected — stop cmm first, rmmod cdx's consumers, then cdx.

### 4d. Overnight run

Leave iperf3 or a more realistic traffic generator running overnight.
Check in morning:

```bash
dmesg | grep -iE 'oops|bug|warn|stall|soft lockup'   # zero
cat /proc/meminfo | grep -E 'MemFree|Slab|SReclaim'   # stable (compare to initial)
ip -s link                                            # error counts stable, not growing
ps -o pid,vsz,rss,cmd -p $(pidof cmm)                 # RSS not growing
```

---

## Go/no-go decision matrix

| Result | PoS outcome |
|---|---|
| Phase 1 fully passes | ~90% — can deploy to staging |
| Phase 2 passes | ~93% |
| Phase 3 passes | ~96% (the SEC error check validation is the big one) |
| Phase 4a + 4b pass | ~98% — production-ready with a week of monitoring |
| Phase 4c (unload/reload) passes | ~98% (bonus — live-upgrade capability) |
| Phase 4d (overnight) clean | ~99% — "1 in 100 deployments need a follow-up" class |

---

## If something fails

Most failures fall into one of these buckets. For each, what the failure
looks like and where to look:

| Failure signature | Likely cause | Where to look |
|---|---|---|
| dmesg oops in `find_osdev_by_fman_params` | RCU iteration issue, bridge interface present | `ip link show type bridge` |
| `SEC dequeue error` on legitimate traffic | Mask too narrow or silicon quirk | Capture `status=0x...`, widen mask to include that bit in `cdx/dpa_ipsec.c` |
| Stale routing / wrong fast-path | CTA enum slot misalignment | Verify libnetfilter_conntrack was rebuilt with our new patch |
| `EEXIST` on modprobe cdx after rmmod | Bug 2 (frag procfs) not fixed or old cdx still loaded | Check `/proc/ucode_frag/` |
| cmm crash with SIGSEGV and no backtrace | Signal handler still using unsafe functions (shouldn't happen after our fix) | Worth knowing if seen |
| `netlink ENOBUFS` spam | cmm falling behind on events | Reproducible? Real bottleneck worth sizing |

---

## Checklist for "ready to ship"

- [ ] All of Phase 1 logs match expected
- [ ] Phase 2 smoke: basic ping + iperf work
- [ ] Phase 3: each feature individually tested
- [ ] Phase 4a: 1 hour iperf, no dmesg, no memory drift
- [ ] Phase 4d: overnight clean
- [ ] Rollback image tested (optional but recommended)
- [ ] Serial console capture saved for post-analysis

---

## Summary

Print this. Tape it to the monitor. Work through top to bottom.

Most tests take a few minutes each; the long ones (4a, 4d) just need to
be left running. If you hit a phase boundary cleanly, you've actually
moved the PoS. If you hit a failure, the diagnostic paths above point at
likely causes.

The PoS ceiling from code inspection alone is about 75%. Working
through this plan on hardware takes you from 75% to 95%+ in a single
testing session. The last few percent is just wall-clock.
