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
