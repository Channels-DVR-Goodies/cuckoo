[![Codacy Badge](https://app.codacy.com/project/badge/Grade/a65b6ebd6ad34c0aa668bc7eef3a0429)](https://www.codacy.com/gh/paul-chambers/cuckoo/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=paul-chambers/cuckoo&amp;utm_campaign=Badge_Grade)
# cuckoo
An executable that transparently impersonates another, so that it becomes a 'hook' to execute
additional executables each time it is invoked.

It does this by moving the target executable into a new subdirectory, and replacing it with
a synlink to `Cuckoo`. When the symlink is executed, `cuckoo` is executed instead and
passed the parameters intended for the original executable it replaced. It then invokes
every executable it finds in the subdirectory.

For example, if we had a target executable called `foobar`, executing `cuckoo foobar` would
create the subdirectory called `.foobar.d` in the same directory as `foobar`, then renames
`foobar` to `50-foobar` and moves it into the subdirectory. Since the subdirectory's contents
are executed in alphabetical order, naming it `50-foobar` leaves room for other executables
before it and after. Then `Cuckoo` creates a symlink `foobar` that points to this executable
(i.e. `foobar` -> `/usr/bin/cuckoo`).

If another process then executes the symlink that's impersonating `foobar`, then `/usr/bin/cuckoo`
is executed with the parameters the process provided. `Cuckoo` then scans `.foobar.d` in
alphabetical order and executes any executable files it finds, passing them the same parameters
and environment that the symlink was passed. Cuckoo will always attempt to execute all the
executables it finds, and will remember the first non-zero exit code it receives, and will
return that as its exit status.

In addition to the subdirectory created when installing the intercept - `.foobar.d/` in
the example above - the directory `/etc/cuckoo/foobar`would also be scanned and combined
with it, Then the combination is sorted before being executed in alphabetical order.

This is useful in the case where auto-updates happen. This is the case when intercepting
comskip at `/usr/share/channels-dvr/latest` for [Channels DVR](https://getchannels.com/plus/#dvr). 
When Channels DVR updates itself, it downloads the new version into another directory,
and updates `/usr/share/channels-dvr/latest` (which is a symbolic link) to point to the
new downloaded version. The problem for Cuckoo is that will 'unhook' the previous Cuckoo
install. To work around this, a cron job can execute the install step, and the extra
scripts can be put into `/etc/cuckoo/comskip`, so they won't be 'left behind' when
Channels DVR updates itself.

## The Motivation

The 'itch' that this scratches was a lack of a hook in Channels DVR to execute additional
post-processing specified by a user when a recording completes.

This isn't a serious omission if you want to stay entirely in the Channels ecosystem (and
there's nothing wrong with that), you really don't need to post-process the recordings.

However, I use Channels DVR as an (excellent!) DVR to record television for Plex.
In my opinion, Channels DVR is far better than the one built into Plex itself.

On the other hand, I prefer Plex's browsing and playback experience over the Channels one.
So I use the two together. I wish they were more integrated in that respect, but since I have
some software skills, I've created a few tools to help that.

As part of that effort, I need to know when a recoding has completed so I can transfer the
recording over into my Plex hierarchy while renaming them to the preferred Plex convention
(for example. please see my [DVR2Plex](https://channels-dvr-goodies.github.io/DVR2Plex/) project).

The Channels developers haven't yet had chance to provide a hook to trigger post-processing.
This is understandable, since doing your own post-processing is certainly advanced usage,
and I presume it's a limited audience that wants to do so.. It could also be a significant
support burden for [Fancy Bits LLC](https://getchannles.com). Channels DVR *does* support
postprocessing internally e.g. doing ad detection when a recording completes.

My intended use is to add my own post-processing steps after `comskip` is invoked (which is the
open source project that Channels DVR uses to generate the ad detection `.edl` files).

While I could use filesystem monitoring (via the iNotify syscall) or a FUSE filesystem, it's
somewhat huristic to determine when the Channels DVR has finished with a recording, particularly
with its own post-processing, like `comskip` for marking ads.

## Why 'Cuckoo'?

The name comes from the strategy of Cuckoos of placing their eggs in the nests of other species 
of bird.

Some species of cuckoo take it a step further by laying eggs with an outer layer that mimics
the natural coloring of the species whose nest is being invaded.
