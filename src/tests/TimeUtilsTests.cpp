/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Platform/TimeUtils.h"

#include <iostream>

int main()
{
    Skyfire::SleepForMilliseconds(0);
    Skyfire::SleepForSeconds(0);

    if (Skyfire::GetMSTimeDiff(100, 150) != 50)
    {
        std::cerr << "GetMSTimeDiff did not handle normal forward time\n";
        return 1;
    }

    if (Skyfire::GetMSTimeDiff(0xFFFFFFF0, 20) != 35)
    {
        std::cerr << "GetMSTimeDiff did not handle wrapped time\n";
        return 1;
    }

    uint32 before = Skyfire::GetMSTime();
    Skyfire::SleepFor(0);
    uint32 after = Skyfire::GetMSTime();

    if (Skyfire::GetMSTimeDiff(before, after) > 1000)
    {
        std::cerr << "SleepFor compatibility wrapper slept unexpectedly long\n";
        return 1;
    }

    return 0;
}
