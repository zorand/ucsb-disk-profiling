## ScsiBench

### Authors: Zoran Dimitrijevic and David Watson

This code was developed during 1999 and 2000 at UCSB. We were 1st year graduate students at UCSB CS department.

Scsibench is a user-level SCSI disk feature extraction tool. It runs on Intel Linux machines, on top of Linux SCSI generic (/dev/sg) interface. Scsibench can automatically extract disk features like: rotational period, disk logical to physical block address mapping, seek curve, skew factors for rotational delay prediction, cache prefetch and write buffer sizes. Library for SCSI commands was written from scratch. You can use Scsibench to run SCSI disk traces.

Documentation

We have submitted Scsibench paper for review. Please refer to the paper for information about what can you do with Scsibench. The power point presentation is written in March 2000 as a graduate course final presentation.

[a Technical Report paper](http://alumni.cs.ucsb.edu/~zoran/scsibench/paper-scsibench.ps.gz)
[a Power point presentation for UCSB CS290I, Winter 2000.](http://alumni.cs.ucsb.edu/~zoran/scsibench/scsibench.ppt)
Authors

Zoran Dimitrijevic, zoran@cs.ucsb.edu
David Watson, davidw@google.com
Special thanks to:

Raju Rangaswami, raju@cs.ucsb.edu
Prof. Edward Chang, echang@ece.ucsb.edu
Prof. Anurag Acharya, acha@cs.ucsb.edu
