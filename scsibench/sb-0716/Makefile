CCFLAGS = -Wall -Iinclude/scsi -g

H_FILES = scsicmd.h scsibench.h basics.h include/scsi/sg.h
O_FILES = scsibench.o scsidevice.o scsicmd.o tsc_time.o basics.o trace.o

all:	main.cc ${O_FILES}
	g++ ${CCFLAGS} -o scsibench main.cc ${O_FILES}

percent:	percent.c
	gcc -o percent percent.c

scsibench.o: scsibench.cc ${H_FILES}
	g++ ${CCFLAGS} -c scsibench.cc

trace.o: trace.cc ${H_FILES}
	g++ ${CCFLAGS} -c trace.cc

scsidevice.o: scsidevice.cc ${H_FILES}
	g++ ${CCFLAGS} -c scsidevice.cc

scsicmd.o: scsicmd.cc ${H_FILES}
	g++ ${CCFLAGS} -c scsicmd.cc

basics.o: basics.cc ${H_FILES}
	g++ ${CCFLAGS} -c basics.cc

tsc_time.o: time_cnt/tsc_time.h time_cnt/tsc_time.c
	gcc ${CFLAGS} -c time_cnt/tsc_time.c


clean:
	rm -f *.o scsibench percent
