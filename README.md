# Welcome to the JDK!

For build instructions please see the
[online documentation](https://openjdk.org/groups/build/doc/building.html),
or either of these files:

- [doc/building.html](doc/building.html) (html version)
- [doc/building.md](doc/building.md) (markdown version)

See <https://openjdk.org/> for more information about the OpenJDK
Community and the JDK and see <https://bugs.openjdk.org> for JDK issue
tracking.


## New GCs
### Full heap parallel mark compact collector
```bash
-Xms32g -Xmx32g -XX:+UseParallelGC -XX:+UseParallelFullMarkCompactGC -XX:NewSize=1k -XX:MaxNewSize=1k -XX:-UseAdaptiveSizePolicy
```

### Full heap parallel scavenge collector
```bash
-Xms32g -Xmx32g -XX:+UseParallelGC -XX:+UseParallelFullScavengeGC -XX:NewSize=32g -XX:MaxNewSize=32g -XX:SurvivorRatio=1 -XX:-UseAdaptiveSizePolicy
```

## Profile majflt of young and old space
### Require linux kernel
https://github.com/jiaweixiao/linux-5.11/tree/jdk-range-majflt?tab=readme-ov-file#profiling  
### Usage
Support psnew, psmc and ps.  
Enable profiling with flag `-XX:+UsePSProfileKernel`.
### Result format
The result is logged in gclog. 
* `Majflt` is read from `/proc/self/stat`.  
* `SysRegionMajflt` is system wide.  
* `RegionMajflt` is read from `/proc/self/statmajflt` or `/proc/self/<tid>/statmajflt`.  
* `In region` is majflt in young space, and `out region` is majflt in old space.  
* `thread_user_clk` is read from `/proc/self/stat`.  
* `thread_sys_clk` is read from `/proc/self/stat`.  
* `thread_rdma_read` read sync + async.  
* `thread_rdma_write` ondemand swapout.  
#### On JVM init and exit
```text
[0.008s][47675][info][gc     ] Majflt(init heap)=3
[0.008s][47675][info][gc     ] SysRegionMajflt(init heap) majflt 0, in region 0, out region 0
[0.008s][47675][info][gc     ] RegionMajflt(init heap) majflt 3, in region 0, out region 3
```
#### After GC
```text
[48.150s][47695][info][gc          ] GC(1) Majflt(young)=0 (112050 -> 112050)
[48.150s][47695][info][gc          ] GC(1) SysRegionMajflt(young) majflt 0 (114131 -> 114131), in region 0 (0 -> 0), out region 0 (114131 -> 114131)
[48.150s][47695][info][gc          ] GC(1) RegionMajflt(young) majflt 0 (112050 -> 112050), in region 0 (0 -> 0), out region 0 (112050 -> 112050
```
#### On thread exit
```text
[887.851s][54423][info][gc,thread      ] Exit JavaThread Streaming-EventLoop-4-4(tid=54423), Majflt=7, MajfltInRegion=6, MajfltOutRegion=1
[940.850s][54387][info][gc,thread      ] JavaThread SIGTERM handler(tid=54387), Majflt=736, MajfltInRegion=197, MajfltOutRegion=539
[940.850s][54387][info][gc,thread      ] NonJavaThread VM Thread(tid=47695), Majflt=290147, MajfltInRegion=1123, MajfltOutRegion=289024
```
