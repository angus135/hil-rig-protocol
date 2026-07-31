#include "hil_rig_protocol/application/application.h"

#include <string.h>

HIL_Application_Status_T HIL_APPLICATION_Default_Config( HIL_Application_Config_T* config )
{
    /*
     * TODO: Validate config and initialize every structural codec bound
     * deterministically. Preserve the contract that no production capacity,
     * hardware capability, transaction policy, or wire-layout decision is
     * invented: zero disables each bound until integration selects it. Allocate
     * nothing, touch no context, and publish the complete configuration only
     * after every field is initialized. Fixed GPIO, analogue, and PWM array
     * extents are protocol constants and must not become configurable limits.
     */
    if ( config != NULL )
    {
        ( void )memset( config, 0, sizeof( *config ) );
    }

    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}

HIL_Application_Status_T HIL_APPLICATION_Init( HIL_Application_Context_T*      context,
                                               const HIL_Application_Config_T* config )
{
    /*
     * TODO: Validate context/config, then validate each structural codec limit
     * and every relationship needed for later checked message-size arithmetic.
     * Do not expect or derive configurable fixed signal-array counts; use the
     * public protocol constants when future codec operations size those bodies.
     * Copy the complete configuration into context and set initialized only
     * after all validation/copying succeeds. Retain no config pointer, allocate
     * no memory, and store no caller buffers, messages, test IDs, ticks,
     * active Test ID, upload/tick/result progress, outstanding declarations,
     * retention ownership, execution state, endpoint role, protocol-version
     * selection, request identity, outstanding operations, statistics, or test
     * data.
     * Application integration separately owns transaction bookkeeping and must
     * ensure Transport's maximum Application-message size is at least
     * max_encoded_message_size; this codec has no Transport dependency. On
     * failure leave context uninitialized and publish no partial configuration.
     */
    ( void )context;
    ( void )config;

    return HIL_APPLICATION_STATUS_NOT_IMPLEMENTED;
}
