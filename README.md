# scp
simple cron parser


# simple

```cpp
#include <iostream>

#include "cron_parser.h"

int main() {

    std::cout << "hello scp" << std::endl;

    scp::CronParser scp("5 4 L * ?", std::time(nullptr), 0, 0);

    for(auto i = 0; i < 10; i++) {
        auto t = scp.next();
        std::tm tm = *std::localtime(&t);
        std::cout << std::asctime(&tm) << std::endl;
    }

    return 0;
}
```