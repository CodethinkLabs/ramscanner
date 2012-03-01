CC=gcc
CFLAGS=$(shell pkg-config --cflags glib-2.0) -D_LARGEFILE64_SOURCE
LIBS=$(shell pkg-config --libs glib-2.0)
SOURCES=ramscanner_main.c ramscanner_common.c ramscanner_collect.c ramscanner_display.c
OBJECTS=ramscanner_main.o ramscanner_common.o ramscanner_collect.o ramscanner_display.o

ramscanner:$(OBJECTS)
	$(CC) $(OBJECTS) -o ramscanner $(LIBS) -Wall

depend:
	makedepend $(CFLAGS) $(SOURCES)


clean:
	rm -f ramscanner *.o
# DO NOT DELETE

ramscanner_main.o: /usr/include/stdlib.h /usr/include/features.h
ramscanner_main.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
ramscanner_main.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
ramscanner_main.o: /usr/include/gnu/stubs-32.h /usr/include/bits/waitflags.h
ramscanner_main.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
ramscanner_main.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
ramscanner_main.o: /usr/include/sys/types.h /usr/include/bits/types.h
ramscanner_main.o: /usr/include/bits/typesizes.h /usr/include/time.h
ramscanner_main.o: /usr/include/sys/select.h /usr/include/bits/select.h
ramscanner_main.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ramscanner_main.o: /usr/include/sys/sysmacros.h
ramscanner_main.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
ramscanner_main.o: /usr/include/string.h /usr/include/xlocale.h
ramscanner_main.o: ramscanner_common.h /usr/include/stdint.h
ramscanner_main.o: /usr/include/bits/wchar.h /usr/include/stdio.h
ramscanner_main.o: /usr/include/libio.h /usr/include/_G_config.h
ramscanner_main.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
ramscanner_main.o: /usr/include/bits/sys_errlist.h
ramscanner_main.o: /usr/include/glib-2.0/glib.h
ramscanner_main.o: /usr/include/glib-2.0/glib/galloca.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gtypes.h
ramscanner_main.o: /usr/include/glib-2.0/glibconfig.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
ramscanner_main.o: /usr/include/bits/posix1_lim.h
ramscanner_main.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
ramscanner_main.o: /usr/include/bits/posix2_lim.h
ramscanner_main.o: /usr/include/glib-2.0/glib/garray.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gasyncqueue.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gthread.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gerror.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gquark.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gutils.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gatomic.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gbacktrace.h
ramscanner_main.o: /usr/include/signal.h /usr/include/bits/signum.h
ramscanner_main.o: /usr/include/bits/siginfo.h /usr/include/bits/sigaction.h
ramscanner_main.o: /usr/include/bits/sigcontext.h
ramscanner_main.o: /usr/include/bits/sigstack.h /usr/include/sys/ucontext.h
ramscanner_main.o: /usr/include/bits/sigthread.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gbase64.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gbitlock.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gcache.h
ramscanner_main.o: /usr/include/glib-2.0/glib/glist.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gmem.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gslice.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gcompletion.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gconvert.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gdataset.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gdate.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gdatetime.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gtimezone.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gdir.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gfileutils.h
ramscanner_main.o: /usr/include/glib-2.0/glib/ghash.h
ramscanner_main.o: /usr/include/glib-2.0/glib/ghmac.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_main.o: /usr/include/glib-2.0/glib/ghook.h
ramscanner_main.o: /usr/include/glib-2.0/glib/ghostutils.h
ramscanner_main.o: /usr/include/glib-2.0/glib/giochannel.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gmain.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gpoll.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gslist.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gstring.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gunicode.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gkeyfile.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gmappedfile.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gmarkup.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gmessages.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gnode.h
ramscanner_main.o: /usr/include/glib-2.0/glib/goption.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gpattern.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gprimes.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gqsort.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gqueue.h
ramscanner_main.o: /usr/include/glib-2.0/glib/grand.h
ramscanner_main.o: /usr/include/glib-2.0/glib/grel.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gregex.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gscanner.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gsequence.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gshell.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gspawn.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gstrfuncs.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gtestutils.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gthreadpool.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gtimer.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gtree.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gurifuncs.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gvarianttype.h
ramscanner_main.o: /usr/include/glib-2.0/glib/gvariant.h
ramscanner_main.o: ramscanner_literals.h ramscanner_collect.h
ramscanner_main.o: ramscanner_display.h
ramscanner_common.o: /usr/include/errno.h /usr/include/features.h
ramscanner_common.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
ramscanner_common.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
ramscanner_common.o: /usr/include/gnu/stubs-32.h /usr/include/bits/errno.h
ramscanner_common.o: /usr/include/linux/errno.h /usr/include/stdlib.h
ramscanner_common.o: /usr/include/bits/waitflags.h
ramscanner_common.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
ramscanner_common.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
ramscanner_common.o: /usr/include/sys/types.h /usr/include/bits/types.h
ramscanner_common.o: /usr/include/bits/typesizes.h /usr/include/time.h
ramscanner_common.o: /usr/include/sys/select.h /usr/include/bits/select.h
ramscanner_common.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ramscanner_common.o: /usr/include/sys/sysmacros.h
ramscanner_common.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
ramscanner_common.o: /usr/include/string.h /usr/include/xlocale.h
ramscanner_common.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
ramscanner_common.o: /usr/include/bits/environments.h
ramscanner_common.o: /usr/include/bits/confname.h /usr/include/getopt.h
ramscanner_common.o: ramscanner_common.h /usr/include/stdint.h
ramscanner_common.o: /usr/include/bits/wchar.h /usr/include/stdio.h
ramscanner_common.o: /usr/include/libio.h /usr/include/_G_config.h
ramscanner_common.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
ramscanner_common.o: /usr/include/bits/sys_errlist.h
ramscanner_common.o: /usr/include/glib-2.0/glib.h
ramscanner_common.o: /usr/include/glib-2.0/glib/galloca.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gtypes.h
ramscanner_common.o: /usr/include/glib-2.0/glibconfig.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gmacros.h
ramscanner_common.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
ramscanner_common.o: /usr/include/bits/local_lim.h
ramscanner_common.o: /usr/include/linux/limits.h
ramscanner_common.o: /usr/include/bits/posix2_lim.h
ramscanner_common.o: /usr/include/glib-2.0/glib/garray.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gasyncqueue.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gthread.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gerror.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gquark.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gutils.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gatomic.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gbacktrace.h
ramscanner_common.o: /usr/include/signal.h /usr/include/bits/signum.h
ramscanner_common.o: /usr/include/bits/siginfo.h
ramscanner_common.o: /usr/include/bits/sigaction.h
ramscanner_common.o: /usr/include/bits/sigcontext.h
ramscanner_common.o: /usr/include/bits/sigstack.h /usr/include/sys/ucontext.h
ramscanner_common.o: /usr/include/bits/sigthread.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gbase64.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gbitlock.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gcache.h
ramscanner_common.o: /usr/include/glib-2.0/glib/glist.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gmem.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gslice.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gcompletion.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gconvert.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gdataset.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gdate.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gdatetime.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gtimezone.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gdir.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gfileutils.h
ramscanner_common.o: /usr/include/glib-2.0/glib/ghash.h
ramscanner_common.o: /usr/include/glib-2.0/glib/ghmac.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_common.o: /usr/include/glib-2.0/glib/ghook.h
ramscanner_common.o: /usr/include/glib-2.0/glib/ghostutils.h
ramscanner_common.o: /usr/include/glib-2.0/glib/giochannel.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gmain.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gpoll.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gslist.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gstring.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gunicode.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gkeyfile.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gmappedfile.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gmarkup.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gmessages.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gnode.h
ramscanner_common.o: /usr/include/glib-2.0/glib/goption.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gpattern.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gprimes.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gqsort.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gqueue.h
ramscanner_common.o: /usr/include/glib-2.0/glib/grand.h
ramscanner_common.o: /usr/include/glib-2.0/glib/grel.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gregex.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gscanner.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gsequence.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gshell.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gspawn.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gstrfuncs.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gtestutils.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gthreadpool.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gtimer.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gtree.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gurifuncs.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gvarianttype.h
ramscanner_common.o: /usr/include/glib-2.0/glib/gvariant.h
ramscanner_common.o: ramscanner_literals.h
ramscanner_collect.o: /usr/include/string.h /usr/include/features.h
ramscanner_collect.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
ramscanner_collect.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
ramscanner_collect.o: /usr/include/gnu/stubs-32.h /usr/include/xlocale.h
ramscanner_collect.o: /usr/include/errno.h /usr/include/bits/errno.h
ramscanner_collect.o: /usr/include/linux/errno.h /usr/include/stdlib.h
ramscanner_collect.o: /usr/include/bits/waitflags.h
ramscanner_collect.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
ramscanner_collect.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
ramscanner_collect.o: /usr/include/sys/types.h /usr/include/bits/types.h
ramscanner_collect.o: /usr/include/bits/typesizes.h /usr/include/time.h
ramscanner_collect.o: /usr/include/sys/select.h /usr/include/bits/select.h
ramscanner_collect.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ramscanner_collect.o: /usr/include/sys/sysmacros.h
ramscanner_collect.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
ramscanner_collect.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
ramscanner_collect.o: /usr/include/bits/environments.h
ramscanner_collect.o: /usr/include/bits/confname.h /usr/include/getopt.h
ramscanner_collect.o: ramscanner_collect.h ramscanner_common.h
ramscanner_collect.o: /usr/include/stdint.h /usr/include/bits/wchar.h
ramscanner_collect.o: /usr/include/stdio.h /usr/include/libio.h
ramscanner_collect.o: /usr/include/_G_config.h /usr/include/wchar.h
ramscanner_collect.o: /usr/include/bits/stdio_lim.h
ramscanner_collect.o: /usr/include/bits/sys_errlist.h
ramscanner_collect.o: /usr/include/glib-2.0/glib.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/galloca.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gtypes.h
ramscanner_collect.o: /usr/include/glib-2.0/glibconfig.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gmacros.h
ramscanner_collect.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
ramscanner_collect.o: /usr/include/bits/local_lim.h
ramscanner_collect.o: /usr/include/linux/limits.h
ramscanner_collect.o: /usr/include/bits/posix2_lim.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/garray.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gasyncqueue.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gthread.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gerror.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gquark.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gutils.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gatomic.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gbacktrace.h
ramscanner_collect.o: /usr/include/signal.h /usr/include/bits/signum.h
ramscanner_collect.o: /usr/include/bits/siginfo.h
ramscanner_collect.o: /usr/include/bits/sigaction.h
ramscanner_collect.o: /usr/include/bits/sigcontext.h
ramscanner_collect.o: /usr/include/bits/sigstack.h
ramscanner_collect.o: /usr/include/sys/ucontext.h
ramscanner_collect.o: /usr/include/bits/sigthread.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gbase64.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gbitlock.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gcache.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/glist.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gmem.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gslice.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gcompletion.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gconvert.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gdataset.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gdate.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gdatetime.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gtimezone.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gdir.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gfileutils.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/ghash.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/ghmac.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/ghook.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/ghostutils.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/giochannel.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gmain.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gpoll.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gslist.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gstring.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gunicode.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gkeyfile.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gmappedfile.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gmarkup.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gmessages.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gnode.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/goption.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gpattern.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gprimes.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gqsort.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gqueue.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/grand.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/grel.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gregex.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gscanner.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gsequence.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gshell.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gspawn.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gstrfuncs.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gtestutils.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gthreadpool.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gtimer.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gtree.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gurifuncs.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gvarianttype.h
ramscanner_collect.o: /usr/include/glib-2.0/glib/gvariant.h
ramscanner_collect.o: ramscanner_literals.h
ramscanner_display.o: /usr/include/stdio.h /usr/include/features.h
ramscanner_display.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
ramscanner_display.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
ramscanner_display.o: /usr/include/gnu/stubs-32.h /usr/include/bits/types.h
ramscanner_display.o: /usr/include/bits/typesizes.h /usr/include/libio.h
ramscanner_display.o: /usr/include/_G_config.h /usr/include/wchar.h
ramscanner_display.o: /usr/include/bits/stdio_lim.h
ramscanner_display.o: /usr/include/bits/sys_errlist.h /usr/include/unistd.h
ramscanner_display.o: /usr/include/bits/posix_opt.h
ramscanner_display.o: /usr/include/bits/environments.h
ramscanner_display.o: /usr/include/bits/confname.h /usr/include/getopt.h
ramscanner_display.o: ramscanner_display.h ramscanner_common.h
ramscanner_display.o: /usr/include/stdint.h /usr/include/bits/wchar.h
ramscanner_display.o: /usr/include/glib-2.0/glib.h
ramscanner_display.o: /usr/include/glib-2.0/glib/galloca.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gtypes.h
ramscanner_display.o: /usr/include/glib-2.0/glibconfig.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gmacros.h
ramscanner_display.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
ramscanner_display.o: /usr/include/bits/local_lim.h
ramscanner_display.o: /usr/include/linux/limits.h
ramscanner_display.o: /usr/include/bits/posix2_lim.h /usr/include/time.h
ramscanner_display.o: /usr/include/glib-2.0/glib/garray.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gasyncqueue.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gthread.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gerror.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gquark.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gutils.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gatomic.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gbacktrace.h
ramscanner_display.o: /usr/include/signal.h /usr/include/bits/sigset.h
ramscanner_display.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
ramscanner_display.o: /usr/include/bits/sigaction.h
ramscanner_display.o: /usr/include/bits/sigcontext.h
ramscanner_display.o: /usr/include/bits/sigstack.h
ramscanner_display.o: /usr/include/sys/ucontext.h
ramscanner_display.o: /usr/include/bits/pthreadtypes.h
ramscanner_display.o: /usr/include/bits/sigthread.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gbase64.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gbitlock.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gcache.h
ramscanner_display.o: /usr/include/glib-2.0/glib/glist.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gmem.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gslice.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gcompletion.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gconvert.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gdataset.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gdate.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gdatetime.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gtimezone.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gdir.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gfileutils.h
ramscanner_display.o: /usr/include/glib-2.0/glib/ghash.h
ramscanner_display.o: /usr/include/glib-2.0/glib/ghmac.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gchecksum.h
ramscanner_display.o: /usr/include/glib-2.0/glib/ghook.h
ramscanner_display.o: /usr/include/glib-2.0/glib/ghostutils.h
ramscanner_display.o: /usr/include/glib-2.0/glib/giochannel.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gmain.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gpoll.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gslist.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gstring.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gunicode.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gkeyfile.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gmappedfile.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gmarkup.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gmessages.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gnode.h
ramscanner_display.o: /usr/include/glib-2.0/glib/goption.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gpattern.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gprimes.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gqsort.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gqueue.h
ramscanner_display.o: /usr/include/glib-2.0/glib/grand.h
ramscanner_display.o: /usr/include/glib-2.0/glib/grel.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gregex.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gscanner.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gsequence.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gshell.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gspawn.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gstrfuncs.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gtestutils.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gthreadpool.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gtimer.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gtree.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gurifuncs.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gvarianttype.h
ramscanner_display.o: /usr/include/glib-2.0/glib/gvariant.h
ramscanner_display.o: ramscanner_literals.h
