/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_AUTHPATCHTRANSFER_H
#define SF_AUTHPATCHTRANSFER_H

#include <cstddef>

namespace Skyfire
{
namespace Auth
{
    enum class PatchTransferAction
    {
        Accept,
        Resume,
        Cancel
    };

    enum class PatchTransferDecision
    {
        Unsupported,
        NeedResumeOffset,
        Cancel
    };

    inline PatchTransferDecision EvaluatePatchTransferRequest(PatchTransferAction action, size_t availableBytes)
    {
        switch (action)
        {
            case PatchTransferAction::Resume:
                return availableBytes < 9 ? PatchTransferDecision::NeedResumeOffset : PatchTransferDecision::Unsupported;
            case PatchTransferAction::Cancel:
                return PatchTransferDecision::Cancel;
            case PatchTransferAction::Accept:
                return PatchTransferDecision::Unsupported;
        }

        return PatchTransferDecision::Unsupported;
    }
}
}

#endif
