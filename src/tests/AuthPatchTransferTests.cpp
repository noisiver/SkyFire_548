/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "AuthPatchTransfer.h"

#include <iostream>

int main()
{
    using Skyfire::Auth::PatchTransferAction;
    using Skyfire::Auth::PatchTransferDecision;
    using Skyfire::Auth::EvaluatePatchTransferRequest;

    if (EvaluatePatchTransferRequest(PatchTransferAction::Resume, 8) != PatchTransferDecision::NeedResumeOffset)
    {
        std::cerr << "Resume transfer did not require an opcode plus 8-byte offset\n";
        return 1;
    }

    if (EvaluatePatchTransferRequest(PatchTransferAction::Resume, 9) != PatchTransferDecision::Unsupported)
    {
        std::cerr << "Resume transfer with complete packet was not marked unsupported\n";
        return 1;
    }

    if (EvaluatePatchTransferRequest(PatchTransferAction::Accept, 1) != PatchTransferDecision::Unsupported)
    {
        std::cerr << "Accept transfer was not marked unsupported\n";
        return 1;
    }

    if (EvaluatePatchTransferRequest(PatchTransferAction::Cancel, 1) != PatchTransferDecision::Cancel)
    {
        std::cerr << "Cancel transfer was not treated as a supported close request\n";
        return 1;
    }

    return 0;
}
