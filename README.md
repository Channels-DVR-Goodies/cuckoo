[![Codacy Badge](https://app.codacy.com/project/badge/Grade/a65b6ebd6ad34c0aa668bc7eef3a0429)](https://www.codacy.com/gh/paul-chambers/cuckoo/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=paul-chambers/cuckoo&amp;utm_campaign=Badge_Grade)
# cuckoo
An executable that transparently impersonates another, so that it becomes a 'hook' to execute
additional executables each time it is invoked.

It does this by moving the target executable into a new subsirectory, and putting a symlink to
this executable in the orginal location. When this executable is invoked through the symlink, 
it scans the subdirectory and executes any executable files it finds in it.

For example, if we had a target executable called `foobar`, executing `cuckoo foobar` would
create the subdirectory in the current working directory called `.foobar.d`, and moves `foobar`
into it, also renaming it to `00-foobar` so it executes first (executable files in the subdir
are executed in alphabetical order). Then cuckoo creates a symlink `foobar` that points to this
cuckoo executable (e.g. `foobar` -> `/usr/bin/cuckoo`).

If another process then executes the symlink that's impersonating `foobar`, then `/usr/ion/cuckoo`
is executed with the parameters the process provided. `cuckoo` then scans `.foobar.d` in alphabetical
order and executes any excutable files it finds, passing them the same parameters and environment
that the symlink was passed. Cuckoo will always attempt to execute all the executables it finds,
and will remember the first non-zero exit code it recieves, and will return that as its exit
status.

## The Motivation

The 'itch' that this scratches was a lack of a hook in Channels DVR to execute additional
user-specified post-processing when a recording completes.

If you want to stay in the Channels ecosystem (and there's nothing wrong with that) you really don't
need to post-process the recordings.

However, I use Channels DVR as an (excellent!) front end for Plex to record television.
In my opinion Channels DVR is a far better DVR than the one built into Plex itself.

On the other hand, I prefer Plex's browsing and playback experience over the Channels one.
So I use the two together. I wish they were more integrated in that respect, but since I have
some software skills, I've created a few tools to help that.

As part of that effort, I need to know when a recoding has completed so I can transfer the
recording over into my Plex hierarchy while renaming them to the preferred Plex convention
(for example. please see my DVR2Plex project).

The Channels developers haven't yet had chance to provide a hook to trigger post-processing.
This is understandable, since doing your own post-processing is certainly advanced usage,
and I presume it's a limited audience that wants to do so.. It could also be a significant
support burden for [Fancy Bits LLC](https://getchannles.com).
They *do* support postprocessing internally e.g. doing ad detection when a recording completes.

My intented use is to add my own post-processing steps after comskip is invoked (which is the
open source project that Channels DVR uses to generate the ad detection `.edl` files)
since the number of users doing their own post-processing are very much in the minority.

While I could use filesystem monitoring (via the iNotify syscall) or a FUSE filesystem, it's
somewhat huristic to determine when the Channels DVR has finished with a recording, particularly
with its own post-processing, like `comskip` for marking ads.

This executable pretends to be comskip, and does indeed call comskip to do its thing first.
But once comskip exits, this executable will run other executables that the user has configured.

## Why 'Cuckoo'?

The name comes from the strategy of Cuckoos of placing their eggs in the nests of other species 
of bird.

Some species of cuckoo take it a step further by laying eggs with an outer layer that mimics
the natural coloring of the species whose nest is being invaded.
