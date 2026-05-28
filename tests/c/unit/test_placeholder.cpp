#include <gtest/gtest.h>

#include "hil_rig_protocol/version.h"

TEST( HilRigProtocolUnitPlaceholder, VersionHeaderIsReachable )
{
    // TODO: Replace with module-level tests for transport, application,
    // instruction encoding, result encoding, validation, and error handling.
    EXPECT_EQ( HIL_RIG_PROTOCOL_Version_Major(), 0u );
    EXPECT_EQ( HIL_RIG_PROTOCOL_Version_Minor(), 1u );
    EXPECT_EQ( HIL_RIG_PROTOCOL_Version_Patch(), 0u );
}
