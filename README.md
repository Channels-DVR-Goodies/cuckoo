[![Codacy Badge](https://app.codacy.com/project/badge/Grade/a65b6ebd6ad34c0aa668bc7eef3a0429)](https://www.codacy.com/gh/paul-chambers/cuckoo/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=paul-chambers/cuckoo&amp;utm_campaign=Badge_Grade)
# cuckoo
An executable that impersonates another, so that additional executables can also be called each time.

The 'itch' that this scratches was Channels DVR lacking a hook to execute additional user-specificed
post-processing when a recording completes.

If you want to stay in the Channels ecosystem (and there's nothing wrong with that) you really don't
need to post-process the files. However, I use Channels DVR as a (great!) recording front end for
Plex. Channels DVR puts the Plex DVR to shame, but personally I prefer Plex's browsing and playback
experience.

The Channels developers haven't yet provided a hook to trigger post-processing. Understandable,
since the number of users doing their own post-processing are very much in the minority.

I need to transfer recordings over to my Plex hierarchy and rename them to the Plex convention 
(see my DVR2Plex project for that aspect).

While I could use filesystem monitoring (via the iNotify syscall) or a FUSE filesystem, it's
somewhat huristic to determine when the Channels DVR has finished with a recording, particularly
with its own post-processing, like `comskip` for marking ads.

This executable pretends to be comskip, and does indeed call comskip to do its thing first.
But once comskip exits, this executable will run other executables that the user has configured.

The name comes from the stratey of Cuckoos of laying their eggs in other bird species' nests. 
