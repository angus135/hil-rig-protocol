#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "hil_rig_protocol/transport/transport.h"
#include "transport/internal/common/transport_crc.h"
#include "transport/internal/common/transport_parser.h"
#include "transport/internal/mvp/transport_frame_codec_mvp.h"

namespace {
constexpr std::size_t   MaxPayload    = HIL_TRANSPORT_DEFAULT_MAX_APPLICATION_MESSAGE_SIZE;
constexpr std::size_t   MaxFrame      = HIL_TRANSPORT_DEFAULT_MAX_ENCODED_FRAME_SIZE;
constexpr std::size_t   MaxRaw        = MaxPayload + HIL_TRANSPORT_MVP_RAW_OVERHEAD;
constexpr std::uint64_t GoldenSession = UINT64_C( 0x0807060504030201 );

/* Generated independently with Python zlib.crc32 and a reference standard COBS encoder. */
constexpr std::array<std::uint8_t, 20> GoldenInitiate{
    0x0D, 0x01, 0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x34, 0x12, 0x01, 0x05, 0x5A, 0x06, 0x9E, 0x82, 0x00,
};
constexpr std::array<std::uint8_t, 20> GoldenResponse{
    0x13, 0x01, 0x02, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x78, 0x56, 0x34, 0x12, 0xB2, 0x2A, 0x96, 0xD1, 0x00,
};
constexpr std::array<std::uint8_t, 20> GoldenConfirm{
    0x13, 0x01, 0x03, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0xBC, 0x9A, 0x78, 0x56, 0xC2, 0x60, 0xE7, 0x40, 0x00,
};
constexpr std::array<std::uint8_t, 23> GoldenApplication{
    0x0C, 0x01, 0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x01,
    0x01, 0x01, 0x02, 0x11, 0x06, 0x22, 0xE3, 0x17, 0x1F, 0xD9, 0x00,
};
constexpr std::array<std::uint8_t, 20> GoldenAck{
    0x0B, 0x01, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x01, 0x07, 0xBC, 0x9A, 0xCA, 0xEF, 0x66, 0x7A, 0x00,
};
constexpr std::array<std::uint8_t, 20> GoldenReset{
    0x0B, 0x01, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x01, 0x01, 0x01, 0x05, 0xEA, 0x18, 0x06, 0x9F, 0x00,
};

struct EncodedFrame
{
    std::array<std::uint8_t, MaxRaw>   raw{};
    std::array<std::uint8_t, MaxFrame> bytes{};
    std::size_t                        size{};
};

HIL_Transport_Mvp_Frame_T MakeFrame( HIL_Transport_Mvp_Frame_Type_T type, std::uint16_t sequence,
                                     std::uint16_t       acknowledgement,
                                     const std::uint8_t* payload      = nullptr,
                                     std::size_t         payload_size = 0u,
                                     std::uint64_t       session      = GoldenSession )
{
    return HIL_Transport_Mvp_Frame_T{
        type, session, sequence, acknowledgement, payload, payload_size,
    };
}

EncodedFrame Encode( const HIL_Transport_Mvp_Frame_T& frame,
                     std::size_t                      maximum_payload = MaxPayload )
{
    EncodedFrame output{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, maximum_payload, output.raw.data(),
                                               output.raw.size(), output.bytes.data(),
                                               output.bytes.size(), &output.size ),
               HIL_TRANSPORT_STATUS_OK );
    return output;
}

std::vector<std::uint8_t> ReferenceCobsEncode( const std::vector<std::uint8_t>& input )
{
    std::vector<std::uint8_t> output( 1u, 0u );
    std::size_t               code_index = 0u;
    std::uint8_t              code       = 1u;

    for ( const auto byte : input )
    {
        if ( byte == 0u )
        {
            output[code_index] = code;
            code_index         = output.size();
            output.push_back( 0u );
            code = 1u;
        }
        else
        {
            output.push_back( byte );
            code++;
            if ( code == 0xFFu )
            {
                output[code_index] = code;
                code_index         = output.size();
                output.push_back( 0u );
                code = 1u;
            }
        }
    }
    output[code_index] = code;
    return output;
}

void AppendU16Le( std::vector<std::uint8_t>& bytes, std::uint16_t value )
{
    bytes.push_back( static_cast<std::uint8_t>( value & 0xFFu ) );
    bytes.push_back( static_cast<std::uint8_t>( value >> 8u ) );
}

void AppendU32Le( std::vector<std::uint8_t>& bytes, std::uint32_t value )
{
    for ( std::uint8_t index = 0u; index < 4u; ++index )
    {
        bytes.push_back( static_cast<std::uint8_t>( value & 0xFFu ) );
        value >>= 8u;
    }
}

void AppendU64Le( std::vector<std::uint8_t>& bytes, std::uint64_t value )
{
    for ( std::uint8_t index = 0u; index < 8u; ++index )
    {
        bytes.push_back( static_cast<std::uint8_t>( value & 0xFFu ) );
        value >>= 8u;
    }
}

std::vector<std::uint8_t> MakeEncodedBodyForDecode( std::uint8_t version, std::uint8_t type,
                                                    std::uint64_t session, std::uint16_t sequence,
                                                    std::uint16_t acknowledgement,
                                                    const std::vector<std::uint8_t>& payload = {} )
{
    std::vector<std::uint8_t> raw{ version, type };
    AppendU64Le( raw, session );
    AppendU16Le( raw, sequence );
    AppendU16Le( raw, acknowledgement );
    raw.insert( raw.end(), payload.begin(), payload.end() );
    AppendU32Le( raw, HIL_TRANSPORT_CRC32_Compute( raw.data(), raw.size() ) );
    return ReferenceCobsEncode( raw );
}

struct DecodeOutput
{
    std::array<std::uint8_t, MaxRaw>     raw{};
    std::array<std::uint8_t, MaxPayload> message{};
    HIL_Transport_Mvp_Frame_T            frame{};
    std::size_t                          message_size{};
    HIL_Transport_Mvp_Decode_Result_T    result{ HIL_TRANSPORT_MVP_DECODE_MALFORMED };
};

HIL_Transport_Status_T Decode( const std::uint8_t* body, std::size_t body_size,
                               DecodeOutput& output, std::size_t maximum_payload = MaxPayload )
{
    return HIL_TRANSPORT_MVP_Decode_Frame( body, body_size, output.raw.data(), output.raw.size(),
                                           &output.frame,
                                           maximum_payload == 0u ? nullptr : output.message.data(),
                                           maximum_payload, &output.message_size, &output.result );
}

void ExpectClearedFrameAndMessageSize( const DecodeOutput& output )
{
    EXPECT_EQ( output.frame.type, HIL_TRANSPORT_MVP_FRAME_INVALID );
    EXPECT_EQ( output.frame.session_identifier, 0u );
    EXPECT_EQ( output.frame.sequence, 0u );
    EXPECT_EQ( output.frame.acknowledgement_sequence, 0u );
    EXPECT_EQ( output.frame.payload, nullptr );
    EXPECT_EQ( output.frame.payload_size, 0u );
    EXPECT_EQ( output.message_size, 0u );
}

template <std::size_t Size> void ExpectGoldenFrame( const HIL_Transport_Mvp_Frame_T&      frame,
                                                    const std::array<std::uint8_t, Size>& golden )
{
    const auto encoded = Encode( frame );
    ASSERT_EQ( encoded.size, golden.size() );
    EXPECT_TRUE( std::equal( golden.begin(), golden.end(), encoded.bytes.begin() ) );

    DecodeOutput decoded{};
    ASSERT_EQ( Decode( golden.data(), golden.size() - 1u, decoded ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( decoded.result, HIL_TRANSPORT_MVP_DECODE_VALID );
    EXPECT_EQ( decoded.frame.type, frame.type );
    EXPECT_EQ( decoded.frame.session_identifier, frame.session_identifier );
    EXPECT_EQ( decoded.frame.sequence, frame.sequence );
    EXPECT_EQ( decoded.frame.acknowledgement_sequence, frame.acknowledgement_sequence );
    ASSERT_EQ( decoded.message_size, frame.payload_size );
    if ( frame.payload_size != 0u )
    {
        EXPECT_TRUE( std::equal( frame.payload, frame.payload + frame.payload_size,
                                 decoded.message.begin() ) );
    }
}
}  // namespace

TEST( TransportCrc32, MatchesIsoHdlcCheckAndEmptyValue )
{
    constexpr std::array<std::uint8_t, 9> Check{ '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    EXPECT_EQ( HIL_TRANSPORT_CRC32_Compute( Check.data(), Check.size() ), UINT32_C( 0xCBF43926 ) );
    EXPECT_EQ( HIL_TRANSPORT_CRC32_Compute( nullptr, 0u ), 0u );
}

TEST( TransportCrc32, IncrementalUpdatesMatchAllChunkBoundaries )
{
    constexpr std::array<std::uint8_t, 11> Bytes{ 0x00, 0x01, 0xFE, 0xFF, 0x42, 0x00,
                                                  0x11, 0x22, 0x33, 0x44, 0x55 };
    const auto expected = HIL_TRANSPORT_CRC32_Compute( Bytes.data(), Bytes.size() );

    for ( std::size_t split = 0u; split <= Bytes.size(); ++split )
    {
        auto crc = HIL_TRANSPORT_CRC32_Init();
        crc      = HIL_TRANSPORT_CRC32_Update( crc, Bytes.data(), split );
        crc      = HIL_TRANSPORT_CRC32_Update( crc, Bytes.data() + split, Bytes.size() - split );
        EXPECT_EQ( HIL_TRANSPORT_CRC32_Finish( crc ), expected ) << "split=" << split;
    }

    auto changed = Bytes;
    changed[5] ^= 0x01u;
    EXPECT_NE( HIL_TRANSPORT_CRC32_Compute( changed.data(), changed.size() ), expected );
}

TEST( TransportMvpCodec, MatchesIndependentGoldenVectorForEveryFrameType )
{
    constexpr std::array<std::uint8_t, 3> Payload{ 0x11, 0x00, 0x22 };
    ExpectGoldenFrame( MakeFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 0x1234u, 0u ), GoldenInitiate );
    ExpectGoldenFrame( MakeFrame( HIL_TRANSPORT_MVP_FRAME_RESPONSE, 0x5678u, 0x1234u ),
                       GoldenResponse );
    ExpectGoldenFrame( MakeFrame( HIL_TRANSPORT_MVP_FRAME_CONFIRM, 0x9ABCu, 0x5678u ),
                       GoldenConfirm );
    ExpectGoldenFrame( MakeFrame( HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 1u, 0u,
                                  Payload.data(), Payload.size() ),
                       GoldenApplication );
    ExpectGoldenFrame( MakeFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 0u, 0x9ABCu ), GoldenAck );
    ExpectGoldenFrame( MakeFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 0u, 0u ), GoldenReset );
}

TEST( TransportMvpCodec, UsesSpecifiedMaximumSizeCalculation )
{
    EXPECT_EQ( HIL_TRANSPORT_MVP_Max_Encoded_Size( 512u ), 534u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Max_Encoded_Size( 0u ), 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Max_Encoded_Size( std::numeric_limits<std::size_t>::max() ), 0u );
}

TEST( TransportMvpCodec, RoundTripsSequenceAndPayloadBoundaryCases )
{
    constexpr std::array<std::uint16_t, 4> Sequences{ 0u, 1u, 0xFFFEu, 0xFFFFu };
    constexpr std::array<std::size_t, 6>   Sizes{ 1u, 253u, 254u, 255u, 508u, 512u };

    for ( const auto sequence : Sequences )
    {
        const auto   frame   = MakeFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, sequence, 0u );
        const auto   encoded = Encode( frame );
        DecodeOutput decoded{};
        ASSERT_EQ( Decode( encoded.bytes.data(), encoded.size - 1u, decoded ),
                   HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( decoded.result, HIL_TRANSPORT_MVP_DECODE_VALID );
        EXPECT_EQ( decoded.frame.sequence, sequence );
    }

    for ( const auto size : Sizes )
    {
        std::vector<std::uint8_t> payload( size );
        for ( std::size_t index = 0u; index < size; ++index )
        {
            payload[index] = static_cast<std::uint8_t>( ( index % 3u ) == 0u ? 0u : index );
        }
        const auto frame =
            MakeFrame( HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 0xFFFFu, 0u, payload.data(),
                       payload.size(), UINT64_C( 0xAA000000BB00CC01 ) );
        const auto   encoded = Encode( frame );
        DecodeOutput decoded{};
        ASSERT_EQ( Decode( encoded.bytes.data(), encoded.size - 1u, decoded ),
                   HIL_TRANSPORT_STATUS_OK );
        ASSERT_EQ( decoded.result, HIL_TRANSPORT_MVP_DECODE_VALID );
        ASSERT_EQ( decoded.message_size, size );
        EXPECT_TRUE( std::equal( payload.begin(), payload.end(), decoded.message.begin() ) );
    }
}

TEST( TransportMvpCodec, EncodesAllZeroAndAllNonzeroApplicationPayloads )
{
    std::array<std::uint8_t, 255> zeros{};
    std::array<std::uint8_t, 255> nonzeros{};
    nonzeros.fill( 0xA5u );

    for ( const auto* payload : { &zeros, &nonzeros } )
    {
        const auto frame   = MakeFrame( HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 1u, 0u,
                                        payload->data(), payload->size() );
        const auto encoded = Encode( frame );
        EXPECT_EQ( encoded.bytes[encoded.size - 1u], 0u );
        EXPECT_EQ(
            std::find( encoded.bytes.begin(), encoded.bytes.begin() + encoded.size - 1u, 0u ),
            encoded.bytes.begin() + encoded.size - 1u );
    }
}

TEST( TransportMvpCodec, AcceptsExactOutputCapacityAndRejectsOneByteLess )
{
    const auto frame   = MakeFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 0x1234u, 0u );
    const auto initial = Encode( frame );
    std::array<std::uint8_t, MaxRaw> raw{};
    std::vector<std::uint8_t>        exact( initial.size );
    std::size_t                      output_size = 99u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, MaxPayload, raw.data(), raw.size(),
                                               exact.data(), exact.size(), &output_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output_size, exact.size() );

    output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, MaxPayload, raw.data(), raw.size(),
                                               exact.data(), exact.size() - 1u, &output_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output_size, 0u );
}

TEST( TransportMvpCodec, RejectsInvalidSemanticFramesWithoutPublishingSize )
{
    constexpr std::array<std::uint8_t, 2> Payload{ 0x11, 0x22 };
    std::array<std::uint8_t, MaxRaw>      raw{};
    std::array<std::uint8_t, MaxFrame>    encoded{};

    const std::array<HIL_Transport_Mvp_Frame_T, 9> invalid{
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_INVALID, 0u, 0u ),
        MakeFrame( static_cast<HIL_Transport_Mvp_Frame_Type_T>( 0x07 ), 0u, 0u ),
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 1u, 1u ),
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 1u, 1u, Payload.data(),
                   Payload.size() ),
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 1u, 0u ),
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 1u, 1u ),
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 1u, 0u ),
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_RESET, 0u, 1u ),
        MakeFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 1u, 0u, nullptr, 0u, 0u ),
    };

    for ( const auto& frame : invalid )
    {
        std::size_t output_size = 99u;
        EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, MaxPayload, raw.data(), raw.size(),
                                                   encoded.data(), encoded.size(), &output_size ),
                   HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
        EXPECT_EQ( output_size, 0u );
    }

    auto oversized = MakeFrame( HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 1u, 0u, Payload.data(),
                                Payload.size() );
    std::size_t output_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &oversized, 1u, raw.data(), raw.size(),
                                               encoded.data(), encoded.size(), &output_size ),
               HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE );
    EXPECT_EQ( output_size, 0u );
}

TEST( TransportMvpCodec, ClassifiesIntegrityMalformedAndIncompatibleInput )
{
    DecodeOutput output{};

    auto corrupt_crc = GoldenInitiate;
    corrupt_crc[17] ^= 0x01u;
    EXPECT_EQ( Decode( corrupt_crc.data(), corrupt_crc.size() - 1u, output ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_INTEGRITY_INVALID );

    auto corrupt_header = GoldenInitiate;
    corrupt_header[2] ^= 0x02u;
    EXPECT_EQ( Decode( corrupt_header.data(), corrupt_header.size() - 1u, output ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_INTEGRITY_INVALID );

    auto corrupt_payload = GoldenApplication;
    corrupt_payload[15] ^= 0x01u;
    EXPECT_EQ( Decode( corrupt_payload.data(), corrupt_payload.size() - 1u, output ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_INTEGRITY_INVALID );

    const auto unsupported = MakeEncodedBodyForDecode( 2u, 1u, GoldenSession, 1u, 0u );
    EXPECT_EQ( Decode( unsupported.data(), unsupported.size(), output ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_SESSION_INCOMPATIBLE );

    const auto truncated_header = ReferenceCobsEncode( std::vector<std::uint8_t>( 13u, 0x11u ) );
    const auto truncated_crc    = ReferenceCobsEncode( std::vector<std::uint8_t>( 17u, 0x11u ) );
    for ( const auto& malformed :
          { std::vector<std::uint8_t>{ 0x02 }, std::vector<std::uint8_t>{ 0x01, 0x00, 0x01 },
            truncated_header, truncated_crc } )
    {
        EXPECT_EQ( Decode( malformed.data(), malformed.size(), output ), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_MALFORMED );
    }
}

TEST( TransportMvpCodec, RejectsStructurallyInvalidDecodedFrames )
{
    const std::array<std::vector<std::uint8_t>, 9> invalid{
        MakeEncodedBodyForDecode( 1u, 0u, GoldenSession, 0u, 0u ),
        MakeEncodedBodyForDecode( 1u, 7u, GoldenSession, 0u, 0u ),
        MakeEncodedBodyForDecode( 1u, 1u, 0u, 1u, 0u ),
        MakeEncodedBodyForDecode( 1u, 1u, GoldenSession, 1u, 0u, { 0x11u } ),
        MakeEncodedBodyForDecode( 1u, 1u, GoldenSession, 1u, 1u ),
        MakeEncodedBodyForDecode( 1u, 4u, GoldenSession, 1u, 0u ),
        MakeEncodedBodyForDecode( 1u, 4u, GoldenSession, 1u, 1u, { 0x11u } ),
        MakeEncodedBodyForDecode( 1u, 5u, GoldenSession, 1u, 1u ),
        MakeEncodedBodyForDecode( 1u, 6u, GoldenSession, 0u, 1u ),
    };

    for ( const auto& encoded : invalid )
    {
        DecodeOutput output{};
        EXPECT_EQ( Decode( encoded.data(), encoded.size(), output ), HIL_TRANSPORT_STATUS_OK );
        EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_MALFORMED );
        EXPECT_EQ( output.message_size, 0u );
        EXPECT_EQ( output.frame.type, HIL_TRANSPORT_MVP_FRAME_INVALID );
    }
}

TEST( TransportMvpCodec, RejectsPayloadBeyondConfiguredMaximumAndClearsStaleOutputs )
{
    const auto encoded =
        MakeEncodedBodyForDecode( 1u, 4u, GoldenSession, 1u, 0u, { 0x11u, 0x22u } );
    DecodeOutput output{};
    output.frame        = MakeFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 0u, 12u );
    output.message_size = 99u;
    output.result       = HIL_TRANSPORT_MVP_DECODE_VALID;
    output.message.fill( 0xA5u );

    EXPECT_EQ( Decode( encoded.data(), encoded.size(), output, 1u ),
               HIL_TRANSPORT_STATUS_MESSAGE_TOO_LARGE );
    ExpectClearedFrameAndMessageSize( output );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_MALFORMED );
    EXPECT_TRUE( std::all_of( output.message.begin(), output.message.end(),
                              []( std::uint8_t value ) { return value == 0xA5u; } ) );
}

TEST( TransportMvpCodec, ClassifiesBodyBeyondConfiguredRawMaximumAsMalformed )
{
    /* This ordinary COBS body deterministically expands to MaxRaw + 1 bytes. */
    const auto oversized_body =
        ReferenceCobsEncode( std::vector<std::uint8_t>( MaxRaw + 1u, 0xA5u ) );
    ASSERT_FALSE( oversized_body.empty() );
    ASSERT_EQ( std::find( oversized_body.begin(), oversized_body.end(), 0u ),
               oversized_body.end() );
    DecodeOutput output{};
    output.frame        = MakeFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 0u, 0x1234u );
    output.message_size = 99u;
    output.result       = HIL_TRANSPORT_MVP_DECODE_VALID;
    output.message.fill( 0x5Au );
    const auto unchanged_message = output.message;

    const auto status = Decode( oversized_body.data(), oversized_body.size(), output );
    EXPECT_NE( status, HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( status, HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_MALFORMED );
    ExpectClearedFrameAndMessageSize( output );
    EXPECT_EQ( output.message, unchanged_message );
}

TEST( TransportMvpCodec, DecodesExactConfiguredMaximumRawFrameWithExactScratch )
{
    std::vector<std::uint8_t> payload( MaxPayload );
    for ( std::size_t index = 0u; index < payload.size(); ++index )
    {
        payload[index] = static_cast<std::uint8_t>( index );
    }
    const auto   frame   = MakeFrame( HIL_TRANSPORT_MVP_FRAME_APPLICATION_MESSAGE, 0xFFFFu, 0u,
                                      payload.data(), payload.size() );
    const auto   encoded = Encode( frame );
    DecodeOutput output{};

    ASSERT_EQ( output.raw.size(), MaxPayload + HIL_TRANSPORT_MVP_RAW_OVERHEAD );
    ASSERT_EQ( Decode( encoded.bytes.data(), encoded.size - 1u, output ), HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_VALID );
    ASSERT_EQ( output.message_size, payload.size() );
    EXPECT_TRUE( std::equal( payload.begin(), payload.end(), output.message.begin() ) );
}

TEST( TransportMvpCodec, ReportsGenuinelyUndersizedConfiguredRawScratch )
{
    std::array<std::uint8_t, MaxRaw - 1u> short_raw{};
    DecodeOutput                          output{};
    output.frame        = MakeFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 0u, 0x1234u );
    output.message_size = 99u;
    output.result       = HIL_TRANSPORT_MVP_DECODE_VALID;
    output.message.fill( 0xC3u );
    const auto unchanged_message = output.message;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame( GoldenInitiate.data(), GoldenInitiate.size() - 1u,
                                               short_raw.data(), short_raw.size(), &output.frame,
                                               output.message.data(), output.message.size(),
                                               &output.message_size, &output.result ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_MALFORMED );
    ExpectClearedFrameAndMessageSize( output );
    EXPECT_EQ( output.message, unchanged_message );
}

TEST( TransportMvpCodec, RejectsOverflowingConfiguredRawMaximumWithoutPublishingOutput )
{
    DecodeOutput output{};
    output.frame        = MakeFrame( HIL_TRANSPORT_MVP_FRAME_ACK, 0u, 0x1234u );
    output.message_size = 99u;
    output.result       = HIL_TRANSPORT_MVP_DECODE_VALID;
    output.message.fill( 0x7Eu );
    const auto unchanged_message = output.message;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame(
                   GoldenInitiate.data(), GoldenInitiate.size() - 1u, output.raw.data(),
                   output.raw.size(), &output.frame, output.message.data(),
                   std::numeric_limits<std::size_t>::max(), &output.message_size, &output.result ),
               HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );
    EXPECT_EQ( output.result, HIL_TRANSPORT_MVP_DECODE_MALFORMED );
    ExpectClearedFrameAndMessageSize( output );
    EXPECT_EQ( output.message, unchanged_message );
}

TEST( TransportMvpCodec, ValidatesPointersCapacitiesAndNonoverlap )
{
    const auto frame = MakeFrame( HIL_TRANSPORT_MVP_FRAME_INITIATE, 1u, 0u );
    std::array<std::uint8_t, MaxFrame> buffer{};
    std::size_t                        size = 99u;

    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( nullptr, MaxPayload, buffer.data(), MaxRaw,
                                               buffer.data(), buffer.size(), &size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, MaxPayload, buffer.data(), MaxRaw,
                                               buffer.data(), buffer.size(), &size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    std::array<std::uint8_t, 8> short_raw{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Encode_Frame( &frame, MaxPayload, short_raw.data(),
                                               short_raw.size(), buffer.data(), buffer.size(),
                                               &size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );

    DecodeOutput output{};
    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame( nullptr, 1u, output.raw.data(), output.raw.size(),
                                               &output.frame, output.message.data(),
                                               output.message.size(), &output.message_size,
                                               &output.result ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame( GoldenInitiate.data(), GoldenInitiate.size() - 1u,
                                               const_cast<std::uint8_t*>( GoldenInitiate.data() ),
                                               GoldenInitiate.size() - 1u, &output.frame,
                                               output.message.data(), output.message.size(),
                                               &output.message_size, &output.result ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_MVP_Decode_Frame( GoldenInitiate.data(), GoldenInitiate.size() - 1u,
                                               short_raw.data(), short_raw.size(), &output.frame,
                                               output.message.data(), output.message.size(),
                                               &output.message_size, &output.result ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
}

TEST( TransportParser, AcceptsCompleteAndByteAtATimeFrames )
{
    std::array<std::uint8_t, MaxFrame> scratch{};
    HIL_Transport_Parser_T             parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, GoldenInitiate.data(),
                                                GoldenInitiate.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    EXPECT_EQ( consumed, GoldenInitiate.size() );

    std::array<std::uint8_t, MaxFrame> body{};
    std::size_t                        body_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, body.data(), body.size(), &body_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( body_size, GoldenInitiate.size() - 1u );
    EXPECT_TRUE( std::equal( GoldenInitiate.begin(), GoldenInitiate.end() - 1, body.begin() ) );

    for ( const auto byte : GoldenApplication )
    {
        const auto expected = byte == 0u ? HIL_TRANSPORT_PARSER_RESULT_BODY_READY
                                         : HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA;
        EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Byte( &parser, byte ), expected );
    }
}

TEST( TransportParser, ReadBodySupportsExactScratchAlias )
{
    constexpr std::array<std::uint8_t, 5> Frame{ 0x11u, 0x22u, 0x33u, 0x44u, 0u };
    std::array<std::uint8_t, 8>           scratch{};
    HIL_Transport_Parser_T                parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, Frame.data(), Frame.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    ASSERT_EQ( consumed, Frame.size() );

    std::size_t body_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, parser.scratch_buffer,
                                               parser.scratch_buffer_size, &body_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( body_size, Frame.size() - 1u );
    EXPECT_TRUE( std::equal( Frame.begin(), Frame.end() - 1, scratch.begin() ) );
    EXPECT_EQ( parser.accumulated_size, 0u );
    EXPECT_EQ( parser.body_ready, 0u );
    EXPECT_EQ( parser.discarding, 0u );
    EXPECT_EQ(
        HIL_TRANSPORT_Parser_Read_Body( &parser, scratch.data(), scratch.size(), &body_size ),
        HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( body_size, 0u );
}

TEST( TransportParser, PeekAndConsumeMakeBodyOwnershipExplicitWithoutClearingScratch )
{
    constexpr std::array<std::uint8_t, 5> Frame{ 0x11u, 0x22u, 0x33u, 0x44u, 0u };
    std::array<std::uint8_t, 8>           scratch{};
    HIL_Transport_Parser_T                parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );

    const std::uint8_t* body      = reinterpret_cast<const std::uint8_t*>( 1u );
    std::size_t         body_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Peek_Body( &parser, &body, &body_size ),
               HIL_TRANSPORT_STATUS_NOT_READY );
    EXPECT_EQ( body, nullptr );
    EXPECT_EQ( body_size, 0u );

    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, Frame.data(), Frame.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    ASSERT_EQ( HIL_TRANSPORT_Parser_Peek_Body( &parser, &body, &body_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( body, scratch.data() );
    EXPECT_EQ( body_size, Frame.size() - 1u );
    EXPECT_EQ( parser.body_ready, 1u );
    const auto retained = scratch;

    ASSERT_EQ( HIL_TRANSPORT_Parser_Consume_Body( &parser ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( parser.scratch_buffer, scratch.data() );
    EXPECT_EQ( parser.scratch_buffer_size, scratch.size() );
    EXPECT_EQ( parser.accumulated_size, 0u );
    EXPECT_EQ( parser.body_ready, 0u );
    EXPECT_EQ( scratch, retained );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Consume_Body( &parser ), HIL_TRANSPORT_STATUS_NOT_READY );
}

TEST( TransportParser, PeekAndConsumeRejectInconsistentPrivateState )
{
    std::array<std::uint8_t, 8> scratch{};
    HIL_Transport_Parser_T      parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );
    parser.body_ready = 1u;

    const std::uint8_t* body      = reinterpret_cast<const std::uint8_t*>( 1u );
    std::size_t         body_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Peek_Body( &parser, &body, &body_size ),
               HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
    EXPECT_EQ( body, nullptr );
    EXPECT_EQ( body_size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Consume_Body( &parser ), HIL_TRANSPORT_STATUS_INTERNAL_ERROR );
}

TEST( TransportParser, ReadBodySupportsForwardPartialOverlap )
{
    constexpr std::array<std::uint8_t, 5> Frame{ 0x21u, 0x32u, 0x43u, 0x54u, 0u };
    std::array<std::uint8_t, 10>          backing{};
    HIL_Transport_Parser_T                parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, backing.data(), 8u ), HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, Frame.data(), Frame.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    ASSERT_EQ( consumed, Frame.size() );

    std::size_t body_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, backing.data() + 2u, backing.size() - 2u,
                                               &body_size ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( body_size, Frame.size() - 1u );
    EXPECT_TRUE( std::equal( Frame.begin(), Frame.end() - 1, backing.begin() + 2 ) );
    EXPECT_EQ( parser.accumulated_size, 0u );
    EXPECT_EQ( parser.body_ready, 0u );
    EXPECT_EQ( parser.discarding, 0u );
}

TEST( TransportParser, ReadBodySupportsBackwardPartialOverlap )
{
    constexpr std::array<std::uint8_t, 5> Frame{ 0x31u, 0x42u, 0x53u, 0x64u, 0u };
    std::array<std::uint8_t, 10>          backing{};
    HIL_Transport_Parser_T                parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, backing.data() + 2u, 8u ),
               HIL_TRANSPORT_STATUS_OK );
    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, Frame.data(), Frame.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    ASSERT_EQ( consumed, Frame.size() );

    std::size_t body_size = 0u;
    ASSERT_EQ(
        HIL_TRANSPORT_Parser_Read_Body( &parser, backing.data(), backing.size(), &body_size ),
        HIL_TRANSPORT_STATUS_OK );
    ASSERT_EQ( body_size, Frame.size() - 1u );
    EXPECT_TRUE( std::equal( Frame.begin(), Frame.end() - 1, backing.begin() ) );
    EXPECT_EQ( parser.accumulated_size, 0u );
    EXPECT_EQ( parser.body_ready, 0u );
    EXPECT_EQ( parser.discarding, 0u );
}

TEST( TransportParser, AcceptsEveryTwoChunkSplit )
{
    for ( std::size_t split = 0u; split <= GoldenApplication.size(); ++split )
    {
        std::array<std::uint8_t, MaxFrame> scratch{};
        HIL_Transport_Parser_T             parser{};
        ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
                   HIL_TRANSPORT_STATUS_OK );
        std::size_t consumed = 0u;
        const auto  first =
            HIL_TRANSPORT_Parser_Push_Bytes( &parser, GoldenApplication.data(), split, &consumed );
        EXPECT_EQ( consumed, split );
        if ( split < GoldenApplication.size() )
        {
            EXPECT_EQ( first, HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA );
            EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, GoldenApplication.data() + split,
                                                        GoldenApplication.size() - split,
                                                        &consumed ),
                       HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
            EXPECT_EQ( consumed, GoldenApplication.size() - split );
        }
        else
        {
            EXPECT_EQ( first, HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
        }
    }
}

TEST( TransportParser, StopsExactlyAtEachCompleteFrameAndProtectsUnreadBody )
{
    std::vector<std::uint8_t> stream( GoldenInitiate.begin(), GoldenInitiate.end() );
    stream.insert( stream.end(), GoldenAck.begin(), GoldenAck.end() );
    std::array<std::uint8_t, MaxFrame> scratch{};
    HIL_Transport_Parser_T             parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, stream.data(), stream.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    EXPECT_EQ( consumed, GoldenInitiate.size() );

    std::size_t second_consumed = 99u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, stream.data() + consumed,
                                                stream.size() - consumed, &second_consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    EXPECT_EQ( second_consumed, 0u );

    std::array<std::uint8_t, MaxFrame> body{};
    std::size_t                        body_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, body.data(), body.size(), &body_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, stream.data() + consumed,
                                                stream.size() - consumed, &second_consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    EXPECT_EQ( second_consumed, GoldenAck.size() );
}

TEST( TransportParser, IgnoresEmptyDelimitersAndRetainsMissingDelimiterData )
{
    std::array<std::uint8_t, 16> scratch{};
    HIL_Transport_Parser_T       parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );

    constexpr std::array<std::uint8_t, 5> Prefix{ 0u, 0u, 1u, 2u, 3u };
    std::size_t                           consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, Prefix.data(), Prefix.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA );
    EXPECT_EQ( parser.accumulated_size, 3u );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Byte( &parser, 4u ),
               HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Byte( &parser, 0u ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
}

TEST( TransportParser, DiscardsOversizeThroughDelimiterThenRecovers )
{
    std::array<std::uint8_t, 4> scratch{};
    HIL_Transport_Parser_T      parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );
    constexpr std::array<std::uint8_t, 9> stream{ 1u, 2u, 3u, 4u, 5u, 0u, 7u, 8u, 0u };

    std::size_t consumed = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, stream.data(), stream.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_DISCARDED_BODY );
    EXPECT_EQ( consumed, 6u );
    EXPECT_EQ( parser.discarding, 0u );
    EXPECT_EQ( parser.accumulated_size, 0u );

    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, stream.data() + consumed,
                                                stream.size() - consumed, &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    EXPECT_EQ( consumed, 3u );
}

TEST( TransportParser, SizeQuerySmallBufferAndResetPreserveExpectedState )
{
    std::array<std::uint8_t, 8> scratch{};
    HIL_Transport_Parser_T      parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );
    constexpr std::array<std::uint8_t, 4> frame{ 1u, 2u, 3u, 0u };
    std::size_t                           consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, frame.data(), frame.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );

    std::size_t body_size = 0u;
    EXPECT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, nullptr, 0u, &body_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( body_size, 3u );
    std::array<std::uint8_t, 2> small{};
    EXPECT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, small.data(), small.size(), &body_size ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( parser.body_ready, 1u );

    HIL_TRANSPORT_Parser_Reset( &parser );
    EXPECT_EQ( parser.scratch_buffer, scratch.data() );
    EXPECT_EQ( parser.scratch_buffer_size, scratch.size() );
    EXPECT_EQ( parser.accumulated_size, 0u );
    EXPECT_EQ( parser.body_ready, 0u );
    EXPECT_EQ( parser.discarding, 0u );

    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Byte( &parser, 0x11u ),
               HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Byte( &parser, 0x22u ),
               HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA );
    ASSERT_EQ( parser.accumulated_size, 2u );
    HIL_TRANSPORT_Parser_Reset( &parser );
    EXPECT_EQ( parser.accumulated_size, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Byte( &parser, 0u ),
               HIL_TRANSPORT_PARSER_RESULT_NEED_MORE_DATA );
}

TEST( TransportParser, ValidatesPointerAndLengthCombinations )
{
    HIL_Transport_Parser_T      parser{};
    std::array<std::uint8_t, 8> scratch{};
    std::size_t                 consumed  = 99u;
    std::size_t                 body_size = 99u;

    EXPECT_EQ( HIL_TRANSPORT_Parser_Init( nullptr, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Init( &parser, nullptr, scratch.size() ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, nullptr, 1u, &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_ERROR );
    EXPECT_EQ( consumed, 0u );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, nullptr, 0u, nullptr ),
               HIL_TRANSPORT_PARSER_RESULT_ERROR );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, scratch.data(), scratch.size(), nullptr ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, nullptr, 1u, &body_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( body_size, 0u );
}

TEST( TransportParser, ConsumesMalformedBodyAndRecoversAtFollowingDelimiter )
{
    constexpr std::array<std::uint8_t, 4> malformed{ 0x02u, 0xFFu, 0xAAu, 0x00u };
    std::vector<std::uint8_t>             stream( malformed.begin(), malformed.end() );
    stream.insert( stream.end(), GoldenInitiate.begin(), GoldenInitiate.end() );
    std::array<std::uint8_t, MaxFrame> scratch{};
    std::array<std::uint8_t, MaxFrame> body{};
    HIL_Transport_Parser_T             parser{};
    ASSERT_EQ( HIL_TRANSPORT_Parser_Init( &parser, scratch.data(), scratch.size() ),
               HIL_TRANSPORT_STATUS_OK );

    std::size_t consumed = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, stream.data(), stream.size(), &consumed ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    ASSERT_EQ( consumed, malformed.size() );
    std::size_t body_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Parser_Read_Body( &parser, body.data(), body.size(), &body_size ),
               HIL_TRANSPORT_STATUS_OK );
    DecodeOutput decoded{};
    EXPECT_EQ( Decode( body.data(), body_size, decoded ), HIL_TRANSPORT_STATUS_OK );
    EXPECT_EQ( decoded.result, HIL_TRANSPORT_MVP_DECODE_MALFORMED );

    ASSERT_EQ( HIL_TRANSPORT_Parser_Push_Bytes( &parser, stream.data() + consumed,
                                                stream.size() - consumed, &body_size ),
               HIL_TRANSPORT_PARSER_RESULT_BODY_READY );
    EXPECT_EQ( body_size, GoldenInitiate.size() );
}

TEST( TransportStorage, SizesAndPartitionsWorkspaceForWireBuffers )
{
    HIL_Transport_Config_T config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed       = UINT64_C( 0x1234 );
    std::size_t required_size = 0u;
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );
    EXPECT_GT( required_size, MaxPayload * 2u + MaxFrame * 2u + MaxRaw );

    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 4096> workspace{};
    ASSERT_LE( required_size, workspace.size() );
    HIL_Transport_Context_T context{};
    HIL_Transport_Storage_T storage{ workspace.data(), workspace.size() };
    ASSERT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage ),
               HIL_TRANSPORT_STATUS_OK );
    ASSERT_NE( context.implementation, nullptr );
    EXPECT_EQ( context.implementation_size, required_size );
    EXPECT_NE( context.initialization_cookie, 0u );

    const auto* root = static_cast<const HIL_Transport_Mvp_Root_T*>( context.implementation );
    EXPECT_EQ( root->parser.scratch_buffer_size, config.max_encoded_frame_size - 1u );
    EXPECT_EQ( root->codec_scratch_size,
               config.max_application_message_size + HIL_TRANSPORT_MVP_RAW_OVERHEAD );
    EXPECT_NE( root->codec_scratch, root->parser.scratch_buffer );

    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &storage ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
}

TEST( TransportStorage, RejectsUnsupportedAndInsufficientConfigurations )
{
    HIL_Transport_Config_T config{};
    HIL_TRANSPORT_Default_Config( &config );
    std::size_t required_size = 99u;

    config.max_encoded_frame_size = 533u;
    EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );
    EXPECT_EQ( required_size, 0u );

    HIL_TRANSPORT_Default_Config( &config );
    config.connection_timeout_ms = 1u;
    EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );

    HIL_TRANSPORT_Default_Config( &config );
    config.max_application_message_size = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_UNSUPPORTED_CONFIGURATION );
}

TEST( TransportStorage, ValidatesExactCapacityAlignmentRoleAndAtomicFailure )
{
    HIL_Transport_Config_T config{};
    HIL_TRANSPORT_Default_Config( &config );
    config.session_seed       = UINT64_C( 0x1234 );
    std::size_t required_size = 99u;
    EXPECT_EQ( HIL_TRANSPORT_Required_Storage_Size( nullptr, &required_size ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( required_size, 0u );
    ASSERT_EQ( HIL_TRANSPORT_Required_Storage_Size( &config, &required_size ),
               HIL_TRANSPORT_STATUS_OK );

    alignas( HIL_TRANSPORT_WORKSPACE_ALIGNMENT ) std::array<std::uint8_t, 4097> workspace{};
    ASSERT_LT( required_size, workspace.size() );
    HIL_Transport_Context_T context{};
    HIL_Transport_Storage_T too_small{ workspace.data(), required_size - 1u };
    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &too_small ),
               HIL_TRANSPORT_STATUS_BUFFER_TOO_SMALL );
    EXPECT_EQ( context.implementation, nullptr );
    EXPECT_EQ( context.implementation_size, 0u );
    EXPECT_EQ( context.initialization_cookie, 0u );

    HIL_Transport_Storage_T misaligned{ workspace.data() + 1u, required_size };
    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &misaligned ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    HIL_Transport_Storage_T exact{ workspace.data(), required_size };
    auto                    invalid_host = config;
    invalid_host.session_seed            = HIL_TRANSPORT_SESSION_SEED_INVALID;
    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &invalid_host, &exact ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );
    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_RIG, &config, &exact ),
               HIL_TRANSPORT_STATUS_INVALID_ARGUMENT );

    EXPECT_EQ( HIL_TRANSPORT_Init( &context, HIL_TRANSPORT_ROLE_HOST, &config, &exact ),
               HIL_TRANSPORT_STATUS_OK );
}
