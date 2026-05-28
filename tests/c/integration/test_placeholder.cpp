#include <gtest/gtest.h>

#include "hil_rig_protocol/version.h"

TEST( HilRigProtocolIntegrationPlaceholder, VersionStringIsReachable )
{
    // TODO: Replace with pipeline tests for round trips, malformed frames,
    // corruption, partial delivery, inserted noise, and golden vectors.
    EXPECT_STREQ( HIL_RIG_PROTOCOL_Version_String(), HIL_RIG_PROTOCOL_VERSION_STRING );
}
