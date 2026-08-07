#!/usr/bin/env python3
"""Simulate Memory::_initialize's page walk over a real hexmap, both ways.

    python3 Test/hexmap-walk.py [home/data/hexmap]

The walk rebuilds the free list at startup, and getting it wrong is the one
mistake in this codebase that loses data rather than misbehaving: a free entry
that covers a live record is handed to the next allocation, and then two records
share an address.

This reads the hexmap only -- it never writes -- and reports, per page:

  OLD  what the walk did before: register the first free run, measured with
       _pointer's arithmetic (skip CONTROL_COUNT control chunks, read to the
       standalone bit), and stop.
  NEW  what it does now: measure each free run from the hexmap directly and
       carry on to the end of the page.

The line that matters is OVERLAP -- a free entry covering chunks that are
allocated. There should never be one.

Hexmap format: one byte per 16-byte chunk. Bit 7 set means allocated, bit 6
means this is the last chunk of its run. A free run is zeroes then a single 64.
A record is CONTROL_COUNT control chunks then its data chunks.
"""

import sys

CHUNK = 16
PAGE = 8192
PER = PAGE // CHUNK              # 512 hexmap bytes per page
CONTROL_COUNT = 3
PAGEFILE_CONTROL_COUNT = 3

path = sys.argv[1] if len(sys.argv) > 1 else "home/data/hexmap"
hx = open(path, "rb").read()
pages = len(hx) // PER
print("hexmap: %s  (%d pages)" % (path, pages))


def allocated(b):
    return (b & 128) == 128


def standalone(b):
    return (b & 64) == 64


def record_chunks(page, i):
    """Chunks a record at page-relative chunk I spans, the way _pointer counts:
    CONTROL_COUNT control chunks, then data chunks to the standalone bit."""
    j = i + CONTROL_COUNT
    n = 0
    while j + n < len(page):
        if standalone(page[j + n]):
            return CONTROL_COUNT + n + 1
        n += 1
    return len(page) - i               # runs off the end: take the rest


def old_free_span(page, i):
    """What _pointer answered for a hole -- skip CONTROL_COUNT, read on to the
    standalone bit, then _pointer_actual_size adds the control chunks back."""
    j = i + CONTROL_COUNT
    n = 0
    while j + n < len(page):
        if standalone(page[j + n]):
            return CONTROL_COUNT + n            # actual_count = data_count + CONTROL
        n += 1
    return len(page) - i


def new_free_span(page, i):
    """The run as the hexmap describes it: clear high bits, ending after the
    chunk with the standalone bit."""
    n = 0
    while i + n < len(page):
        b = page[i + n]
        if allocated(b):
            break
        n += 1
        if standalone(b):
            break
    return n


bad_old = 0
bad_new = 0
holes = 0

for p in range(pages):
    page = hx[p * PER:(p + 1) * PER]
    i = PAGEFILE_CONTROL_COUNT

    # --- what the old walk registered, and then stopped ---------------
    old_entry = None
    j = i
    while j < len(page):
        if allocated(page[j]):
            j += record_chunks(page, j)
        else:
            old_entry = (j, old_free_span(page, j))
            break

    # --- what the new walk registers, all the way to the end ----------
    new_entries = []
    j = i
    while j < len(page):
        if allocated(page[j]):
            j += record_chunks(page, j)
        else:
            span = new_free_span(page, j)
            if span <= 0:
                break
            new_entries.append((j, span))
            j += span

    if old_entry is None:
        continue
    holes += 1

    def overlap(entry):
        start, span = entry
        return [k for k in range(start, min(start + span, len(page)))
                if allocated(page[k])]

    o = overlap(old_entry)
    n = [k for e in new_entries for k in overlap(e)]

    live_after = [k for k in range(old_entry[0], len(page)) if allocated(page[k])]

    if o or n or live_after:
        print("\n  page %d (offset %d)" % (p, p * PAGE))
        print("    first hole at chunk %d, %d allocated chunks lie after it"
              % (old_entry[0], len(live_after)))
        print("    OLD  one entry (%d, +%d)   OVERLAP: %d allocated chunk(s)%s"
              % (old_entry[0], old_entry[1], len(o), "  <-- corrupts" if o else ""))
        print("    NEW  %d entries %s   OVERLAP: %d allocated chunk(s)%s"
              % (len(new_entries), new_entries[:4], len(n), "  <-- corrupts" if n else ""))
    if o:
        bad_old += 1
    if n:
        bad_new += 1

print("\npages with a hole      : %d" % holes)
print("OLD walk, entries that cover live records: %d" % bad_old)
print("NEW walk, entries that cover live records: %d" % bad_new)
sys.exit(1 if bad_new else 0)
