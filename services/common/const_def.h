/*
 * Copyright (c) 2023-2024 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_SHARING_CONST_DEF_H
#define OHOS_SHARING_CONST_DEF_H

#include <string>

namespace OHOS {
namespace Sharing {

enum MediaType { MEDIA_TYPE_AV, MEDIA_TYPE_AUDIO, MEDIA_TYPE_VIDEO };

enum SceneType { FOREGROUND = 0, BACKGROUND = 1 };

enum VideoFormat {
    VIDEO_NONE = -1,
    VIDEO_640X480_25 = 0,
    VIDEO_640X480_30,
    VIDEO_640X480_60,
    VIDEO_1280X720_25,
    VIDEO_1280X720_30,
    VIDEO_1280X720_60,
    VIDEO_1920X1080_25,
    VIDEO_1920X1080_30,
    VIDEO_1920X1080_60
};

enum AudioFormat {
    AUDIO_NONE = -1,
    AUDIO_8000_8_1 = 0,
    AUDIO_8000_8_2,
    AUDIO_8000_16_1,
    AUDIO_8000_16_2,

    AUDIO_11025_8_1 = 10,
    AUDIO_11025_8_2,
    AUDIO_11025_16_1,
    AUDIO_11025_16_2,

    AUDIO_22050_8_1 = 20,
    AUDIO_22050_8_2,
    AUDIO_22050_16_1,
    AUDIO_22050_16_2,

    AUDIO_44100_8_1 = 30,
    AUDIO_44100_8_2,
    AUDIO_44100_16_1,
    AUDIO_44100_16_2,
    AUDIO_44100_16_4,

    AUDIO_48000_8_1 = 40,
    AUDIO_48000_8_2,
    AUDIO_48000_16_1,
    AUDIO_48000_16_2,
    AUDIO_48000_16_4,
    AUDIO_48000_16_6,
    AUDIO_48000_16_8
};

enum class AppCastEventType {
    APP_CAST_CREATE = 1050,
    APP_CAST_DESTROY,
    APP_CAST_RESIZE,
    APP_CAST_MAKE_MIRROR,
    APP_CAST_ENTER_SMALL_WINDOW,
    APP_CAST_EXIT_SMALL_WINDOW
};

constexpr uint32_t WFD_MSEC_IN_SEC = 1000;
constexpr uint32_t TIME_FIVE_SEC = 5000;

constexpr uint32_t AUDIO_SAMPLE_BIT_U8 = 8;
constexpr uint32_t AUDIO_SAMPLE_BIT_S16LE = 16;
constexpr uint32_t AUDIO_CHANNEL_STEREO = 2;
constexpr uint32_t AUDIO_SAMPLE_RATE_44100 = 44100;
constexpr uint32_t AUDIO_SAMPLE_RATE_48000 = 48000;
constexpr uint32_t AUDIO_BIT_RATE_12800 = 128000;

constexpr uint32_t INVALID_ID = 0;
constexpr int32_t DEFAULT_CHANNEL = 2;
constexpr int32_t DEFAULT_SAMPLERATE = 48000;
constexpr uint32_t SAMPLE_RATE_90K = 90000;

constexpr uint16_t MIN_PORT = 6700;
constexpr uint16_t MAX_PORT = 7000;
constexpr uint16_t DEFAULT_WFD_CTRLPORT = 7236;

constexpr uint32_t DEFAULT_VIDEO_WIDTH = 1920;
constexpr uint32_t DEFAULT_VIDEO_HEIGHT = 1080;
constexpr uint32_t DEFAULT_WINDOW_WIDTH = 1920;
constexpr uint32_t DEFAULT_WINDOW_HEIGHT = 1080;
constexpr uint32_t DEFAULT_CAPTURE_VIDEO_WIDTH = 1280;
constexpr uint32_t DEFAULT_CAPTURE_VIDEO_HEIGHT = 720;
constexpr double HIVISION_FRAME_RATE = 60.0;
constexpr double DEFAULT_FRAME_RATE = 30.0;
constexpr uint32_t DEFAULT_SAMPLE_BIT = 16;

constexpr int32_t MAX_CONTEXT_NUM = 20;
constexpr int32_t MAX_SRC_AGENT_NUM = 20;
constexpr int32_t MAX_SINK_AGENT_NUM = 20;
constexpr uint32_t MAX_YUV420_BUFFER_SIZE = 2560 * 1600 * (3 / 2) * 2;
constexpr int32_t MAX_RTSP_PLAYER_NUM = 2;

constexpr int64_t SCREEN_CAPTURE_ENCODE_BITRATE = 5 * 1000 * 1000;
constexpr int64_t HIVISION_ENCODE_BITRATE = 8 * 1000 * 1000;
constexpr uint64_t SCREEN_ID_INVALID = -1ULL;
constexpr float DEFAULT_SCREEN_DENSITY = 2.0;
constexpr int32_t DEFAULT_SCREEN_FLAGS = 0;

inline const std::string DUMMY_EMPTY = "";

constexpr int32_t DISCOVERY_PEERS_CHANNEL = 20000;

constexpr uint32_t BITRATE_MODE_VBR = 1;
constexpr int32_t INITIAL_MAX_QP_HISI = 37;
constexpr int32_t INITIAL_MIN_QP_HISI = 23;

constexpr int32_t MILLISECOND = 1000;
constexpr int32_t REPEAT_DIFF = 16;
constexpr int32_t REPEAT_DIFF_HIVISION = 8;
constexpr uint64_t LARGE_AVPTS_DIFF = 100;
constexpr uint64_t LOW_PTS_INTERVAL_HIVISON = 16;
constexpr uint64_t HIGH_PTS_INTERVAL_HIVISON = 17;
constexpr uint64_t LOW_PTS_INTERVAL = 33;
constexpr uint64_t HIGH_PTS_INTERVAL = 34;
constexpr uint64_t FRAME_RATE_30 = 30;
constexpr uint64_t FRAME_RATE_60 = 60;

constexpr int32_t MAX_RTP_PAYLOAD_SIZE = 1316;
constexpr int32_t MAX_VTP_PAYLOAD_SIZE = 188 * 348;
#ifdef WIFI_DEVICE_ABILITY_ID
#undef WIFI_DEVICE_ABILITY_ID
#endif
constexpr int32_t WIFI_DEVICE_ABILITY_ID = 1120;

constexpr int32_t CONNECT_FAIL_UDP = -2;
constexpr int32_t CONNECT_FAIL_VTP = -3;

constexpr int SEND_DATA_PRINT_MODE = 100;

constexpr int THREAD_PRIORITY = -16;

constexpr int32_t NOTIFY_EVENT_REFRESH_RATE_VOTE = 1101;
constexpr uint32_t VIDEO_FPS_30 = 30;
constexpr int32_t MIN_DATA_SIZE = 5;

constexpr int32_t UPDATE_BITRATE = 1017;

constexpr int32_t DEVICE_LIST_MAX_SIZE = 1000;
constexpr int32_t EVENT_ID_SCREEN_CHANGE = 1080;
constexpr int32_t EVENT_ID_SCREEN_PARAM = 1081;
constexpr int32_t DEAL_VIRTUAL_SCREEN = 1083;

inline constexpr char PARAM_MEDIA_DUMP[] = "debug.cast.media.dump";
inline constexpr char PARAM_YUV_SUPPORT[] = "debug.cast.yuv.support";

enum TrackType { TRACK_INVALID = -1, TRACK_VIDEO = 0, TRACK_AUDIO, TRACK_TITLE, TRACK_APPLICATION, TRACK_MAX };

#define CODEC_MAP(XX)                                                \
    XX(CODEC_DEFAULT, TRACK_VIDEO, 0, "default", PSI_STREAM_DEFAULT) \
    XX(CODEC_H264, TRACK_VIDEO, 1, "H264", PSI_STREAM_H264)          \
    XX(CODEC_H265, TRACK_VIDEO, 2, "H265", PSI_STREAM_H265)          \
    XX(CODEC_AAC, TRACK_AUDIO, 3, "mpeg4-generic", PSI_STREAM_AAC)   \
    XX(CODEC_G711A, TRACK_AUDIO, 4, "PCMA", PSI_STREAM_AUDIO_G711A)  \
    XX(CODEC_G711U, TRACK_AUDIO, 5, "PCMU", PSI_STREAM_AUDIO_G711U)  \
    XX(CODEC_OPUS, TRACK_AUDIO, 6, "opus", PSI_STREAM_AUDIO_OPUS)    \
    XX(CODEC_L16, TRACK_AUDIO, 7, "L16", PSI_STREAM_RESERVED)        \
    XX(CODEC_VP8, TRACK_VIDEO, 8, "VP8", PSI_STREAM_VP8)             \
    XX(CODEC_VP9, TRACK_VIDEO, 9, "VP9", PSI_STREAM_VP9)             \
    XX(CODEC_AV1, TRACK_VIDEO, 10, "AV1X", PSI_STREAM_AV1)           \
    XX(CODEC_PCM, TRACK_AUDIO, 11, "PCM_S16LE", PSI_STREAM_AUDIO_PCM_S16LE)

typedef enum {
    CODEC_NONE = -1,
#define XX(name, type, value, str, mpeg_id) name = (value),
    CODEC_MAP(XX)
#undef XX
        CODEC_MAX
} CodecId;

#define ABSTRACT_ERR_BASE(errorCode) ((errorCode) / 100 * 100)

enum SharingErrorCode {
    // common
    ERR_OK = 0,

    // err in map
    ERR_GENERAL_ERROR = 437452801,
    ERR_BAD_PARAMETER = 437452802,
    ERR_STATE_EXCEPTION = 437452803,
    ERR_CONNECTION_FAILURE = 437452804,
    ERR_CONNECTION_TIMEOUT = 437452805,
    ERR_NETWORK_ERROR = 437452806,
    ERR_TCP_KEEPALIVE_TIMEOUT = 437452807,
    ERR_PROTOCOL_CONNECTION_TIMEOUT = 437452808,
    ERR_PROTOCOL_INTERACTION_TIMEOUT = 437452809,
    ERR_SOCKET_ERROR = 437452810,
    ERR_P2P_OPT_FAILED = 437452811,                  /* failed */
    ERR_P2P_OPT_NOT_SUPPORTED = 437452812,           /* not supported */
    ERR_P2P_OPT_INVALID_PARAM = 437452813,           /* invalid params */
    ERR_P2P_OPT_FORBID_AIRPLANE = 437452814,         /* forbid when current airplane opened */
    ERR_P2P_OPT_FORBID_POWSAVING = 437452815,        /* forbid when current powersaving opened */
    ERR_P2P_OPT_PERMISSION_DENIED = 437452816,       /* permission denied */
    ERR_P2P_OPT_OPEN_FAIL_WHEN_CLOSING = 437452817,  /* open failed when current is closing */
    ERR_P2P_OPT_OPEN_SUCC_WHEN_OPENED = 437452818,   /* open success when current has been opened */
    ERR_P2P_OPT_CLOSE_FAIL_WHEN_OPENING = 437452819, /* close failed when current is opening */
    ERR_P2P_OPT_CLOSE_SUCC_WHEN_CLOSED = 437452820,  /* close success when current has been closed */
    ERR_P2P_OPT_STA_NOT_OPENED = 437452821,          /* sta service not opened */
    ERR_P2P_OPT_SCAN_NOT_OPENED = 437452822,         /* scan service not opened */
    ERR_P2P_OPT_AP_NOT_OPENED = 437452823,           /* ap service not opened */
    ERR_P2P_OPT_INVALID_CONFIG = 437452824,          /* invalid config */
    ERR_P2P_OPT_P2P_NOT_OPENED = 437452825,          /* p2p service not opened */
    ERR_P2P_OPT_P2P_MAC_NOT_FOUND = 437452826,
    ERR_P2P_OPT_P2P_ERR_MAC_FORMAT = 437452827,
    ERR_P2P_OPT_P2P_ERR_INTENT = 437452828,
    ERR_P2P_OPT_P2P_ERR_SIZE_NW_NAME = 437452829,
    ERR_P2P_OPT_MOVING_FREEZE_CTRL = 437452830,      /* moving freeze scanning control */
    ERR_P2P_OPT_NON_SYSTEMAPP = 437452831,           /* not system app denied */
    ERR_P2P_OPT_ENTERPRISE_DENIED = 437452832,       /* mdm forbidden */
    ERR_P2P_DISCONNECTED_STATE_CLOSED = 437452833,
    ERR_P2P_DISCONNECTED_DEVICE_CHANGE = 437452834,
    ERR_P2P_DISCONNECTED_CONNECTION_CHANGE = 437452835,
    ERR_PROSUMER_INIT = 437452836,
    ERR_PROSUMER_CREATE = 437452837,
    ERR_PROSUMER_START = 437452838,
    ERR_PROSUMER_VIDEO_CAPTURE = 437452839,
    ERR_PROSUMER_RTP_CONNECT = 437452840,
    ERR_P2P_GO_NEGOTIATION_REJECT = 437452841,
    ERR_P2P_GO_NEGOTIATION_TIMEOUT = 437452842,
    ERR_VTP_CONNECT_FAIL = 437452843,
    ERR_UDP_CONNECT_FAIL = 437452844,
    ERR_P2P_DISCONNECTED_AFTER_CONNECTED = 437452845,
    ERR_PROTOCOL_CONNECTION_TIMEOUT_GC = 437452846,
    ERR_SOCKET_ERROR_GC = 437452847,
    ERR_UDP_SEND_RETRY_EXCEEDED = 437452848,

    // detailed general error
    ERR_STOP_ERROR,
    ERR_START_ERROR,
    ERR_APPEND_SURFACE_ERROR,
    ERR_SET_MEDIA_FORMAT_ERROR,
    ERR_PLAY_ERROR,
    ERR_PAUSE_ERROR,
    ERR_CLOSE_ERROR,
    ERR_TOMGR_ERROR,
    ERR_AGENT_STEP_DESTROY,
    ERR_EVENT_REPEAT,
    ERR_AGENT_STATUS_ERROR,
    ERR_NULL_SCREEN,
    ERR_NULL_ENCODERSURFACE,
    ERR_REGISTER_FAILURE,
    ERR_NULL_LISTENER,
    ERR_UNREGISTER_FAILURE,
    ERR_INVALID_SCREENID,
    ERR_NULL_SURFACE,
    ERR_SET_SURFACE_FAILURE,
    ERR_NULL_EVENTMSG,
    ERR_APPEND_CAST_ERROR,
    ERR_CREATE_SCREEN_CAPTURE_ERROR,
    ERR_CREATE_CONSUMER_ERROR,
    ERR_UNKNOW_MSG,
    ERR_APPEND_SURFACE,
    ERR_CONSUMER_NOT_INITED,
    ERR_CREATE_CONSUMER_FAILURE,
    ERR_CONSUMER_RUNNING,
    ERR_NULL_CONSUMER,
    ERR_PAUSE_CONSUMER_ERROR,
    ERR_RESUME_CONSUMER_ERROR,
    ERR_START_PRODUCER_END,
    ERR_STOP_PRODUCER_END,
    ERR_PAUSE_PRODUCER_END,
    ERR_RESUME_PRODUCER_END,

    ERR_WIFI_DEVICE_ABILITY_DIED,

    // framework 18800001~18800099
    ERR_INVALID_ID = 18800001,
    ERR_SERVICE_LIMIT,
    ERR_INTERACTION_FAILURE,
    ERR_CONNECTION_REFUSED,
    ERR_BAD_PROTOCOL,
    ERR_UNAUTHORIZED,
    ERR_INVALID_URL,
    ERR_REMOTE_DIED,
    ERR_UDP_KEEPALIVE_TIMEOUT,

    // interaction 18800101~18800199
    ERR_INVALID_SURFACE_ID = 18800101,
    ERR_SURFACE_FAILURE,

    // context + agent 18800201~18800299
    ERR_CONTEXT_AGENT_BASE = 18800200,
    ERR_CONTEXT_CREATE = 18800201,
    ERR_AGENT_CREATE,
    ERR_AGENT_START,
    ERR_AGENT_PAUSE,
    ERR_AGENT_RESUME,

    // session 18800301~18800399
    ERR_SESSION_BASE = 18800300,
    ERR_SESSION_CREATE = 18800301,
    ERR_SESSION_START,
    ERR_SESSION_PAUSE,
    ERR_SESSION_RESUME,
    ERR_INTAKE_TIMEOUT,

    // prosumer 18800501~18800599
    ERR_PROSUMER_BASE = 18800500,
    ERR_PROSUMER_PAUSE,
    ERR_PROSUMER_RESUME,
    ERR_PROSUMER_TIMEOUT,
    ERR_PROSUMER_STOP,
    ERR_PROSUMER_DESTROY,
    ERR_PROSUMER_AUDIO_CAPTURE,

    // decode + encode + play 18800601~18800699
    ERR_DECODE_ERROR = 18800601,
    ERR_DECODE_FORMAT,
    ERR_DECODE_SCARCE_CAPACITY,
    ERR_DECODE_DISABLE_ACCELERATION,
    ERR_ENCODE_ERROR,
    ERR_ENCODE_FORMAT,
    ERR_ENCODE_SCARCE_CAPACITY,
    ERR_PLAY_START,
    ERR_PLAY_STOP,

    // application: wfd 18802000~18802999
    ERR_WFD_START_CODE = 18802000,
    ERR_RECEIVING_LIMIT = SharingErrorCode::ERR_WFD_START_CODE + 101,
    ERR_P2P_SINK_DISCONNECTED,
    ERR_P2P_SOURCE_DISCONNECTED,
    ERR_P2P_SOURCE_HEARTBEAT_ERROR,
    ERR_P2P_DHCP_INVALID_IP,
};

inline const std::string STRING_SOCKET_ERR = "Socket error";
inline const std::string STRING_P2P_OPT_FAILED = "P2P failed";
inline const std::string STRING_P2P_OPT_NOT_SUPPORTED = "P2P not supported";
inline const std::string STRING_P2P_OPT_INVALID_PARAM = "P2P invalid params";
inline const std::string STRING_P2P_OPT_FORBID_AIRPLANE = "P2P forbid when current airplane opened";
inline const std::string STRING_P2P_OPT_FORBID_POWSAVING = "P2P forbid when current powersaving opened";
inline const std::string STRING_P2P_OPT_PERMISSION_DENIED = "P2P permission denied";
inline const std::string STRING_P2P_OPT_OPEN_FAIL_WHEN_CLOSING = "P2P open failed when current is closing";
inline const std::string STRING_P2P_OPT_OPEN_SUCC_WHEN_OPENED = "P2P open success when current has been opened";
inline const std::string STRING_P2P_OPT_CLOSE_FAIL_WHEN_OPENING = "P2P close failed when current is opening";
inline const std::string STRING_P2P_OPT_CLOSE_SUCC_WHEN_CLOSED = "P2P close success when current has been closed";
inline const std::string STRING_P2P_OPT_STA_NOT_OPENED = "P2P sta service not opened";
inline const std::string STRING_P2P_OPT_SCAN_NOT_OPENED = "P2P scan service not opened";
inline const std::string STRING_P2P_OPT_AP_NOT_OPENED = "P2P ap service not opened";
inline const std::string STRING_P2P_OPT_INVALID_CONFIG = "P2P invalid config";
inline const std::string STRING_P2P_OPT_P2P_NOT_OPENED = "P2P p2p service not opened";
inline const std::string STRING_P2P_OPT_P2P_MAC_NOT_FOUND = "P2P mac not found";
inline const std::string STRING_P2P_OPT_P2P_ERR_MAC_FORMAT = "P2P invalid mac format";
inline const std::string STRING_P2P_OPT_P2P_ERR_INTENT = "P2P invalid intent";
inline const std::string STRING_P2P_OPT_P2P_ERR_SIZE_NW_NAME = "P2P invalid size of NW name";
inline const std::string STRING_P2P_OPT_MOVING_FREEZE_CTRL = "P2P moving freeze scanning control";
inline const std::string STRING_P2P_OPT_NON_SYSTEMAPP = "P2P not system app denied";
inline const std::string STRING_P2P_OPT_ENTERPRISE_DENIED = "P2P mdm forbidden";
inline const std::string KEY_PROJECT_MODE = "mode";
inline const std::string KEY_PROJECT_RECT = "rect";
inline const std::string KEY_PROJECT_RECT_TOP = "top";
inline const std::string KEY_PROJECT_RECT_LEFT = "left";
inline const std::string KEY_PROJECT_RECT_WIDTH = "width";
inline const std::string KEY_PROJECT_RECT_HEIGHT = "height";
inline const std::string KEY_PROJECT_ROTATION = "rotation";
inline const std::string KEY_APPCAST_WIDTH = "width";
inline const std::string KEY_APPCAST_HEIGHT = "height";
inline const std::string KEY_APPCAST_DPI = "dpi";
inline const std::string KEY_IS_APPCAST = "isAppCast";
inline const std::string KEY_VOTING = "voting";
inline const std::string KEY_MIN_REFRESH_RATE = "minRefreshRate";
inline const std::string KEY_MXA_REFRESH_RATE = "maxRefreshRate";
inline const std::string KEY_BITRATE = "bitrate";

inline const std::string HUAWEI_CAST_BUNDLE_NAME = "com.huawei.hmos.huaweicast";
inline const std::string VOTER_NAME = "VOTER_VIRTUALDISPLAY";
inline const std::string VIRTUAL_DISPLAY_NAME = "CastEngine";

enum SharingMsgId {
    // service: 0 ~ 999
    // chat: 1000 ~ 1999
    CHAT_MSG_ID_START = 1000,
    // wfd: 2000 ~ 2999
    WFD_MSG_ID_START = 2000,
    // player: 3000 ~ 3999
    PLAYER_MSG_ID_START = 3000,
    // any push: 4000 ~ 4999
    ANY_PUSH_MSG_ID_START = 4000
};

} // namespace Sharing
} // namespace OHOS
#endif
